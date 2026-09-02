#!/usr/bin/env python
"""Static semantic differential checks for the FM2 Xenia oracle corpus.

This does not compare formatting. It compares decoded control predicates, fetch
routing, write masks, exports, addressing modes and special semantic paths with
the specialized HLSL emitted by XenosRecomp.
"""

from __future__ import annotations

import argparse
import json
import re
import struct
import subprocess
from collections import Counter
from pathlib import Path

KNOWN_OPCODES = {
    # Control flow and pseudo operations.
    "alloc", "cexec", "cjmp", "cnop", "exec", "exece", "jmp", "label", "retain_prev", "serialize",
    # Fetch operations used by FM2.
    "getGradients", "setTexLOD", "tfetch2D", "tfetchCube", "vfetch_full", "vfetch_mini",
    # Vector ALU.
    "add", "add_sat", "cndeq", "cndeq_sat", "cndge", "cndge_sat", "cndgt", "cndgt_sat",
    "cube", "dp2add", "dp3", "dp3_sat", "dp4", "floor", "frc", "mad", "mad_sat",
    "max", "max_sat", "min", "mul", "mul_sat", "seq", "seq_sat", "sge", "sge_sat",
    "sgt", "sgt_sat", "sne", "sne_sat", "trunc",
    # Scalar ALU.
    "addsc", "addsc_sat", "adds", "adds_sat", "adds_prev", "adds_prev_sat", "exp", "floors", "frcs",
    "log", "maxas", "maxs", "maxs_sat", "mins", "mulsc", "mulsc_sat", "muls", "muls_prev",
    "muls_prev_sat", "muls_sat", "rcp", "rcp_sat", "rsq", "seqs", "setp_eq", "setp_ge", "setp_gt",
    "setp_ne", "sges", "sgts", "snes", "sqrt", "subsc", "subsc_sat", "subs",
    "subs_prev", "truncs",
}
NON_WRITING = {"alloc", "cexec", "cjmp", "cnop", "exec", "exece", "jmp", "label", "serialize", "setTexLOD"}


def strip_disasm_prefix(line: str) -> str:
    line = re.sub(r"^/\*.*?\*/\s*", "", line).strip()
    line = re.sub(r"^\+\s*", "", line)
    line = re.sub(r"^\([^)]*\)\s*", "", line)
    return line


def opcode_of(line: str) -> str | None:
    text = strip_disasm_prefix(line)
    match = re.match(r"([A-Za-z][A-Za-z0-9_]*)", text)
    return match.group(1) if match else None


def body_of(hlsl: str) -> str:
    marker = "float textureLod = 0.0;"
    offset = hlsl.rfind(marker)
    if offset < 0:
        raise ValueError("generated shader body marker is missing")
    return hlsl[offset:]


def build_synthetic_container(raw: bytes, stage: str) -> bytes:
    """Wrap Xenia's raw little-endian ucode in the container XenosRecomp reads."""
    if len(raw) % 4:
        raise ValueError("ucode size is not dword-aligned")
    physical = b"".join(raw[offset : offset + 4][::-1] for offset in range(0, len(raw), 4))
    shader = struct.pack(">6I", 0, len(physical), 0, 0, 0, 16 << 5)
    if stage == "frag":
        metadata = struct.pack(">2I", 0, 0x1F)
        metadata += b"".join(
            struct.pack(">I", index | (5 << 4) | (index << 8)) for index in range(16)
        )
        flags = 0x102A1100
    else:
        metadata = struct.pack(">3I", 0, 256, 0)
        metadata += b"".join(struct.pack(">I", address | (5 << 12)) for address in range(256))
        metadata += b"".join(
            struct.pack(">I", index | (5 << 4) | (index << 8)) for index in range(16)
        )
        flags = 0x102A1101
    virtual_size = 36 + len(shader) + len(metadata)
    header = struct.pack(">9I", flags, virtual_size, len(physical), 0, 0, 0, 36, 0, 0)
    return header + shader + metadata + physical


def generate_oracle_hlsl(oracle: Path, executable: Path, shader_common: Path, work: Path) -> Path:
    containers = work / "containers"
    generated = work / "generated"
    containers.mkdir(parents=True, exist_ok=True)
    generated.mkdir(parents=True, exist_ok=True)
    oracle_files = sorted(list(oracle.glob("*.ucode.vert")) + list(oracle.glob("*.ucode.frag")))
    for disasm in oracle_files:
        stage = disasm.suffix[1:]
        raw_name = disasm.name.replace(".ucode.", ".ucode.bin.")
        raw_path = oracle / raw_name
        if not raw_path.exists():
            raise FileNotFoundError(raw_path)
        container_name = raw_name + ".container"
        container_path = containers / container_name
        container_path.write_bytes(build_synthetic_container(raw_path.read_bytes(), stage))
        output_path = generated / (container_name + ".hlsl")
        result = subprocess.run(
            [str(executable), str(container_path), str(output_path), str(shader_common)],
            text=True,
            capture_output=True,
        )
        if result.returncode:
            raise RuntimeError(
                f"translation failed for {disasm.name} ({result.returncode}):\n{result.stderr}"
            )
    return generated


def write_components(line: str, output_only: bool = False) -> int:
    text = strip_disasm_prefix(line)
    opcode = opcode_of(line)
    if not opcode or opcode in NON_WRITING:
        return 0
    target = r"o(?:Pos|C\d+|\d+)" if output_only else r"(?:r\d+|oPos|oC\d+|o\d+)"
    match = re.match(rf"[A-Za-z][A-Za-z0-9_]*\s+({target})(?:\.([xyzw01_]+))?", text)
    if not match:
        return 0
    swizzle = match.group(2)
    return 4 if swizzle is None else sum(component != "_" for component in swizzle)


def generated_write_components(body: str, output_only: bool = False) -> int:
    total = 0
    for line in body.splitlines():
        if any(name in line for name in ("g_VteFlags", "g_ClipPlane", "g_HalfPixelOffset")):
            continue
        target = r"output\.[A-Za-z0-9_]+" if output_only else r"(?:r\d+|output\.[A-Za-z0-9_]+)"
        match = re.match(rf"\s*{target}(?:\.([xyzw]+))?\s*=", line)
        if match:
            total += len(match.group(1)) if match.group(1) else 4
    return total


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--oracle", required=True, type=Path)
    parser.add_argument("--generated", type=Path)
    parser.add_argument("--xenos-recomp", type=Path)
    parser.add_argument("--shader-common", type=Path)
    parser.add_argument("--work", type=Path)
    parser.add_argument("--json", type=Path)
    args = parser.parse_args()

    if args.generated is None:
        if not args.xenos_recomp or not args.shader_common or not args.work:
            parser.error("use --generated, or provide --xenos-recomp, --shader-common and --work")
        args.generated = generate_oracle_hlsl(
            args.oracle, args.xenos_recomp, args.shader_common, args.work
        )

    oracle_files = sorted(list(args.oracle.glob("*.ucode.vert")) + list(args.oracle.glob("*.ucode.frag")))
    failures: list[str] = []
    opcode_counts: Counter[str] = Counter()
    metrics: Counter[str] = Counter()

    def fail(shader: str, category: str, expected, actual) -> None:
        failures.append(f"{shader}: {category}: expected {expected!r}, got {actual!r}")

    for oracle in oracle_files:
        generated_name = oracle.name.replace(".ucode.", ".ucode.bin.") + ".container.hlsl"
        generated = args.generated / generated_name
        if not generated.exists():
            failures.append(f"{oracle.name}: generated file is missing: {generated_name}")
            continue

        disasm = oracle.read_text(errors="replace")
        hlsl = generated.read_text(errors="replace")
        try:
            body = body_of(hlsl)
        except ValueError as error:
            failures.append(f"{oracle.name}: {error}")
            continue

        stage = oracle.suffix[1:]
        for line in disasm.splitlines():
            opcode = opcode_of(line)
            if opcode:
                opcode_counts[opcode] += 1

        # Boolean CondExec / CondJmp guards. Structured forward jumps guard the
        # fall-through body with the inverse condition; PC-loop jumps test the
        # jump condition directly.
        direct_pc_loop = "while (true)" in body
        expected_bool_guards = []
        for line in disasm.splitlines():
            text = strip_disasm_prefix(line)
            match = re.match(r"(cexec|cjmp)\s+(!?)b(\d+)", text)
            if not match:
                continue
            opcode, negated, address = match.group(1), bool(match.group(2)), int(match.group(3))
            truth = not negated
            if opcode == "cjmp" and not direct_pc_loop:
                truth = not truth
            expected_bool_guards.append((address, truth))
        actual_bool_guards = [
            (int(address), not bool(negated))
            for negated, address in re.findall(
                r"if \((!?)BOOL_BIT\((\d+)\)\)", body
            )
        ]
        if actual_bool_guards != expected_bool_guards:
            fail(oracle.name, "boolean control flow", expected_bool_guards, actual_bool_guards)
        metrics["boolean_guards"] += len(expected_bool_guards)

        # Instruction and predicate-jump guards, in execution order.
        expected_predicates = []
        for line in disasm.splitlines():
            match = re.search(r"\((!?p0)\)\s+([A-Za-z_]+)", line)
            if not match:
                continue
            predicate, opcode = match.group(1), match.group(2)
            if opcode in {"exec", "exece"}:
                continue
            if opcode in {"jmp", "cjmp"}:
                if not direct_pc_loop:
                    predicate = "p0" if predicate == "!p0" else "!p0"
                expected_predicates.append(predicate)
                continue
            # A '+' continuation is the scalar half of the same ALU instruction.
            if "+" not in line[: line.find("(") + 1]:
                expected_predicates.append(predicate)
        actual_predicates = []
        for line in body.splitlines():
            stripped = line.strip()
            if stripped == "if (p0)":
                actual_predicates.append("p0")
            elif stripped == "if (!p0)":
                actual_predicates.append("!p0")
        if actual_predicates != expected_predicates:
            fail(oracle.name, "predicate guards", expected_predicates, actual_predicates)
        metrics["predicate_guards"] += len(expected_predicates)

        # Texture dimension, sampler slot and coordinate source routing.
        expected_fetches = []
        for line in disasm.splitlines():
            text = strip_disasm_prefix(line)
            match = re.match(r"tfetch(2D|Cube)\s+\S+,\s*(r\d+\.[xyzw]+),\s*tf(\d+)", text)
            if match:
                expected_fetches.append((match.group(1), int(match.group(3)), match.group(2)))
        flat_body = re.sub(r"\s+", " ", body)
        actual_fetches = [
            (dimension, int(slot), source)
            for dimension, slot, source in re.findall(
                r"=\s*tfetch(2D|Cube)(?:CL|L)?\(.*?s(\d+)_Texture(?:2D|Cube)DescriptorIndex,\s*"
                r"s\d+_SamplerDescriptorIndex,\s*(r\d+\.[xyzw]+)",
                flat_body,
            )
        ]
        if actual_fetches != expected_fetches:
            fail(oracle.name, "texture fetch routing", expected_fetches, actual_fetches)
        metrics["texture_fetches"] += len(expected_fetches)

        # Cube consumes src1.  The second disassembly operand is the source
        # whose register swizzle must reach cube() unchanged.
        expected_cube_sources = []
        for line in disasm.splitlines():
            text = strip_disasm_prefix(line)
            match = re.match(r"cube\s+\S+,\s*(r\d+\.[xyzw]+),", text)
            if match:
                expected_cube_sources.append(match.group(1))
        actual_cube_sources = re.findall(r"\bcube\((r\d+\.[xyzw]+)\)", body)
        if actual_cube_sources != expected_cube_sources:
            fail(oracle.name, "cube src1 swizzles", expected_cube_sources, actual_cube_sources)
        metrics["cube_src1_swizzles"] += len(expected_cube_sources)

        register_lod_count = disasm.count("UseRegisterLOD=true")
        explicit_lod_calls = len(re.findall(r"=\s*tfetch(?:1D|2D|2DArray|Cube)L\(", body))
        if register_lod_count != explicit_lod_calls:
            fail(oracle.name, "register LOD SampleLevel calls", register_lod_count, explicit_lod_calls)
        metrics["register_lod_fetches"] += register_lod_count

        gradient_count = sum("getGradients " in line for line in disasm.splitlines())
        emitted_gradients = len(re.findall(r"=\s*getGradients2D\(", body))
        if gradient_count != emitted_gradients:
            fail(oracle.name, "gradient fetches", gradient_count, emitted_gradients)
        metrics["gradient_fetches"] += gradient_count

        set_lod_count = sum("setTexLOD " in line for line in disasm.splitlines())
        emitted_set_lod = len(re.findall(r"^\s*textureLod\s*=\s*r\d+\.[xyzw];", body, re.MULTILINE))
        if set_lod_count != emitted_set_lod:
            fail(oracle.name, "setTexLOD writes", set_lod_count, emitted_set_lod)
        metrics["set_lod_writes"] += set_lod_count

        # Vertex fetches are specialized through the linked declaration, but no
        # decoded fetch may disappear.
        vertex_fetches = sum(bool(re.search(r"\bvfetch_(?:full|mini)\b", line)) for line in disasm.splitlines())
        input_reads = sum("(input.i" in line and "=" in line for line in body.splitlines())
        if vertex_fetches != input_reads:
            fail(oracle.name, "vertex fetch count", vertex_fetches, input_reads)
        metrics["vertex_fetches"] += vertex_fetches

        expected_writes = sum(write_components(line) for line in disasm.splitlines())
        actual_writes = generated_write_components(body)
        if expected_writes != actual_writes:
            fail(oracle.name, "written register/export components", expected_writes, actual_writes)
        metrics["written_components"] += expected_writes

        expected_exports = sum(write_components(line, True) for line in disasm.splitlines())
        actual_exports = generated_write_components(body, True)
        if expected_exports != actual_exports:
            fail(oracle.name, "export components", expected_exports, actual_exports)
        metrics["export_components"] += expected_exports

        expected_predicate_writes = sum(
            bool(re.search(r"\bsetp_(?:eq|ne|gt|ge)(?:_push)?\b", line)) for line in disasm.splitlines()
        )
        actual_predicate_writes = sum(bool(re.match(r"\s*p0\s*=", line)) for line in body.splitlines())
        if expected_predicate_writes != actual_predicate_writes:
            fail(oracle.name, "predicate writes", expected_predicate_writes, actual_predicate_writes)
        metrics["predicate_writes"] += expected_predicate_writes

        expected_a0_writes = sum(bool(re.search(r"\bmaxas\b", line)) for line in disasm.splitlines())
        actual_a0_writes = sum(bool(re.match(r"\s*a0\s*=", line)) for line in body.splitlines())
        if expected_a0_writes != actual_a0_writes:
            fail(oracle.name, "a0 writes", expected_a0_writes, actual_a0_writes)
        metrics["a0_writes"] += expected_a0_writes

        expected_saturates = sum(bool(re.search(r"\b[A-Za-z0-9_]+_sat\b", line)) for line in disasm.splitlines())
        actual_saturates = sum("saturate(" in line for line in body.splitlines())
        if expected_saturates != actual_saturates:
            fail(oracle.name, "saturate operations", expected_saturates, actual_saturates)
        metrics["saturate_operations"] += expected_saturates

        if "+a0]" in disasm and "+ a0)" not in body:
            failures.append(f"{oracle.name}: relative constant addressing was dropped")
        if ("_abs[" in disasm or "r_abs[" in disasm) and "abs(" not in body:
            failures.append(f"{oracle.name}: absolute-value operand modifier was dropped")

        for forbidden in ("clamp(log2(", "clamp(rcp(", "clamp(rsqrt(", "sqrt(max(0.0"):
            if forbidden in body:
                failures.append(f"{oracle.name}: IEEE scalar operation incorrectly emitted as {forbidden}")

    unknown = sorted(set(opcode_counts) - KNOWN_OPCODES)
    if unknown:
        failures.append(f"unsupported opcode vocabulary: {unknown}")

    # Every generated shader includes the shared sampling implementation. Xenia
    # adds this fixed sub-texel bias before normalized 2D sampling.
    if oracle_files:
        first_generated = args.generated / (oracle_files[0].name.replace(".ucode.", ".ucode.bin.") + ".container.hlsl")
        if first_generated.exists() and "float2(0.00146484375, 0.00146484375)" not in first_generated.read_text(errors="replace"):
            failures.append("Xenia 3/2048-texel coordinate epsilon is missing")

    report = {
        "status": "pass" if not failures else "fail",
        "shader_count": len(oracle_files),
        "vertex_shaders": sum(path.suffix == ".vert" for path in oracle_files),
        "pixel_shaders": sum(path.suffix == ".frag" for path in oracle_files),
        "opcode_counts": dict(sorted(opcode_counts.items())),
        "metrics": dict(sorted(metrics.items())),
        "failures": failures,
    }
    rendered = json.dumps(report, indent=2)
    print(rendered)
    if args.json:
        args.json.parent.mkdir(parents=True, exist_ok=True)
        args.json.write_text(rendered + "\n")
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
