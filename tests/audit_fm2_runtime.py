#!/usr/bin/env python
"""Audit the FM2 runtime shader containers, including runtime-only opcodes."""

from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
from collections import Counter
from pathlib import Path

AUDITED_OPCODES = {
    "cf": {0, 1, 2, 5, 7, 8, 11, 12, 13},
    "vector": {0, 1, 2, 3, 4, 5, 6, 8, 9, 10, 11, 12, 14, 15, 16, 17, 18, 25},
    "scalar": {0, 1, 2, 3, 5, 6, 8, 9, 10, 11, 12, 13, 14, 16, 19, 22, 23, 25,
               27, 28, 29, 30, 40, 42, 43, 44, 45, 46, 47, 50},
    "fetch": {0, 1, 18, 24},
}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--runtime", required=True, type=Path)
    parser.add_argument("--xenos-recomp", required=True, type=Path)
    parser.add_argument("--shader-common", required=True, type=Path)
    parser.add_argument("--work", required=True, type=Path)
    parser.add_argument("--json", type=Path)
    args = parser.parse_args()

    args.work.mkdir(parents=True, exist_ok=True)
    failures: list[str] = []
    counts = {kind: Counter() for kind in AUDITED_OPCODES}
    shader_count = 0
    loop_shader_count = 0
    structured_else_shader_count = 0
    nested_branch_shader_count = 0
    fallback_shader_count = 0
    unnamed_sampler_shader_count = 0
    reflected_alias_shader_count = 0

    env = os.environ.copy()
    env["XENOS_RECOMP_TRACE"] = "1"

    for shader in sorted(args.runtime.glob("*.bin")):
        shader_count += 1
        output = args.work / (shader.stem + ".hlsl")
        result = subprocess.run(
            [str(args.xenos_recomp), str(shader), str(output), str(args.shader_common)],
            env=env,
            text=True,
            capture_output=True,
        )
        if result.returncode:
            failures.append(f"{shader.name}: translation failed ({result.returncode}): {result.stderr[-500:]}")
            continue
        warnings = [line for line in result.stderr.splitlines() if "WARNING" in line]
        if warnings:
            failures.append(f"{shader.name}: translator warnings: {warnings}")

        cf = [int(value) for value in re.findall(r"trace: cf top pc=\d+ opcode=(\d+)", result.stderr)]
        alu = [(int(vector), int(scalar)) for vector, scalar in re.findall(
            r"trace: emit alu .*? vector=(\d+) scalar=(\d+)", result.stderr
        )]
        fetch = [int(value) for value in re.findall(
            r"trace: emit fetch .*? opcode=(\d+)", result.stderr
        )]
        counts["cf"].update(cf)
        counts["vector"].update(vector for vector, _ in alu)
        counts["scalar"].update(scalar for _, scalar in alu)
        counts["fetch"].update(fetch)

        hlsl = output.read_text(errors="replace")
        marker = "float textureLod = 0.0;"
        body_offset = hlsl.rfind(marker)
        if body_offset < 0:
            failures.append(f"{shader.name}: generated body marker is missing")
            continue
        body = hlsl[body_offset:]

        if re.search(r"(?m)^\s*else\s*$", body):
            structured_else_shader_count += 1
        if re.search(r"(?m)^\t{2,}if \(", body):
            nested_branch_shader_count += 1
        if "switch (pc)" in body:
            fallback_shader_count += 1

        unnamed_samplers = set(
            re.findall(r"\bs([0-9]|1[0-5])_SamplerDescriptorIndex\b", body)
        )
        if unnamed_samplers:
            unnamed_sampler_shader_count += 1
            for sampler in unnamed_samplers:
                definition = rf"(?m)^#define s{sampler}_SamplerDescriptorIndex\b"
                dxil_field = f"uint s{sampler}_SamplerDescriptorIndex : packoffset("
                if len(re.findall(definition, hlsl)) < 2 or dxil_field not in hlsl:
                    failures.append(
                        f"{shader.name}: unnamed sampler s{sampler} is not defined for all backends"
                    )

        # This FM2 vertex shader reflects several overlapping Float4 matrix
        # aliases.  They must share the one physical constant-file declaration
        # while remaining usable by name in the translated body.
        if shader.stem == "0191395EEF439898":
            aliases = ("matWVP", "matWV", "matWInvT", "matW", "matVInv")
            if hlsl.count("float4 g_VertexShaderConstants[256]") != 1:
                failures.append(
                    f"{shader.name}: expected one physical VS Float4 constant declaration"
                )
            missing_aliases = [name for name in aliases if f"{name}(" not in body]
            if missing_aliases:
                failures.append(
                    f"{shader.name}: reflected aliases missing from body: {missing_aliases}"
                )
            else:
                reflected_alias_shader_count += 1

        loop_starts = cf.count(7)
        loop_ends = cf.count(8)
        emitted_loops = len(re.findall(r"g_LoopConstant\(\d+\)", body))
        emitted_for_loops = len(re.findall(r"for \(uint loopIterator\d+ = 0;", body))
        fallback_loops = len(re.findall(r"loopIterator\[loopDepth\] = 0;", body))
        if loop_starts or loop_ends:
            loop_shader_count += 1
        if (loop_starts != loop_ends or loop_starts != emitted_loops or
                loop_starts != emitted_for_loops + fallback_loops):
            failures.append(
                f"{shader.name}: loop control mismatch start={loop_starts} end={loop_ends} "
                f"constants={emitted_loops} for_loops={emitted_for_loops} "
                f"fallback_loops={fallback_loops}"
            )

        gradient_fetches = fetch.count(18)
        emitted_gradients = len(re.findall(r"=\s*getGradients2D\(", body))
        if gradient_fetches != emitted_gradients:
            failures.append(
                f"{shader.name}: gradient fetch mismatch expected={gradient_fetches} emitted={emitted_gradients}"
            )

        set_lod = fetch.count(24)
        emitted_set_lod = len(re.findall(r"^\s*textureLod\s*=", body, re.MULTILINE))
        if set_lod != emitted_set_lod:
            failures.append(
                f"{shader.name}: setTexLOD mismatch expected={set_lod} emitted={emitted_set_lod}"
            )

    for kind, audited in AUDITED_OPCODES.items():
        unknown = sorted(set(counts[kind]) - audited)
        if unknown:
            failures.append(f"runtime uses unaudited {kind} opcodes: {unknown}")

    for label, count in (
        ("structured if/else", structured_else_shader_count),
        ("nested branch", nested_branch_shader_count),
        ("switch(pc) fallback", fallback_shader_count),
        ("unnamed sampler fallback", unnamed_sampler_shader_count),
        ("overlapping reflected aliases", reflected_alias_shader_count),
    ):
        if count == 0:
            failures.append(f"runtime corpus did not exercise {label}")

    cache_output = args.work / "shader_cache.cpp"
    cache_result = subprocess.run(
        [
            str(args.xenos_recomp),
            str(args.runtime),
            str(cache_output),
            str(args.shader_common),
            "--jobs",
            "7",
        ],
        text=True,
        capture_output=True,
    )
    if cache_result.returncode:
        failures.append(
            "full runtime cache compilation failed "
            f"({cache_result.returncode}): {cache_result.stderr[-2000:]}"
        )

    report = {
        "status": "pass" if not failures else "fail",
        "shader_count": shader_count,
        "loop_shader_count": loop_shader_count,
        "structured_else_shader_count": structured_else_shader_count,
        "nested_branch_shader_count": nested_branch_shader_count,
        "fallback_shader_count": fallback_shader_count,
        "unnamed_sampler_shader_count": unnamed_sampler_shader_count,
        "reflected_alias_shader_count": reflected_alias_shader_count,
        "opcode_counts": {
            kind: dict(sorted(counter.items())) for kind, counter in counts.items()
        },
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
