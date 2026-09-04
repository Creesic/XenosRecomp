#include "shader_recompiler.h"
#include "shader_common.h"
#include <array>
#include <cstdio>

static constexpr char SWIZZLES[] = 
{ 
    'x',
    'y', 
    'z', 
    'w', 
    '0', 
    '1',
    '_',
    '_'
};

static constexpr const char* USAGE_TYPES[] =
{
    "float4", // POSITION
    "float4", // BLENDWEIGHT
    "uint4", // BLENDINDICES
    "float4", // NORMAL
    "float4", // PSIZE
    "float4", // TEXCOORD
    "float4", // TANGENT
    "float4", // BINORMAL
    "float4", // TESSFACTOR
    "float4", // POSITIONT
    "float4", // COLOR
    "float4", // FOG
    "float4", // DEPTH
    "float4", // SAMPLE
};

static constexpr const char* USAGE_VARIABLES[] =
{
    "Position",
    "BlendWeight",
    "BlendIndices",
    "Normal",
    "PointSize",
    "TexCoord",
    "Tangent",
    "Binormal",
    "TessFactor",
    "PositionT",
    "Color",
    "Fog",
    "Depth",
    "Sample"
};

static constexpr const char* USAGE_SEMANTICS[] =
{
    "POSITION",
    "BLENDWEIGHT",
    "BLENDINDICES",
    "NORMAL",
    "PSIZE",
    "TEXCOORD",
    "TANGENT",
    "BINORMAL",
    "TESSFACTOR",
    "POSITIONT",
    "COLOR",
    "FOG",
    "DEPTH",
    "SAMPLE"
};

struct DeclUsageLocation
{
    DeclUsage usage;
    uint32_t usageIndex;
    uint32_t location;
};

#ifdef UNLEASHED_RECOMP
// NOTE: These are specialized Vulkan locations for Unleashed Recompiled. Change as necessary. Likely not going to work with other games.
static constexpr DeclUsageLocation USAGE_LOCATIONS[] =
{
    { DeclUsage::Position, 0, 0 },
    { DeclUsage::Position, 1, 1 },
    { DeclUsage::Position, 2, 2 },
    { DeclUsage::Position, 3, 3 },
    { DeclUsage::Normal, 0, 4 },
    { DeclUsage::Normal, 1, 5 },
    { DeclUsage::Normal, 2, 6 },
    { DeclUsage::Normal, 3, 7 },
    { DeclUsage::Tangent, 0, 8 },
    { DeclUsage::Tangent, 1, 9 },
    { DeclUsage::Tangent, 2, 10 },
    { DeclUsage::Tangent, 3, 11 },
    { DeclUsage::Binormal, 0, 12 },
    { DeclUsage::TexCoord, 0, 13 },
    { DeclUsage::TexCoord, 1, 14 },
    { DeclUsage::TexCoord, 2, 15 },
    { DeclUsage::TexCoord, 3, 16 },
    { DeclUsage::Color, 0, 17 },
    { DeclUsage::BlendIndices, 0, 18 },
    { DeclUsage::BlendWeight, 0, 19 },
    { DeclUsage::TexCoord, 5, 20 },
    { DeclUsage::TexCoord, 6, 21 },
    { DeclUsage::TexCoord, 7, 22 },
    { DeclUsage::Color, 1, 23 },
    { DeclUsage::Depth, 1, 24 },
    { DeclUsage::Tangent, 4, 25 },
    { DeclUsage::Tangent, 5, 26 },
    { DeclUsage::Tangent, 6, 27 },
    { DeclUsage::Tangent, 7, 28 },
    { DeclUsage::Binormal, 1, 29 },
    { DeclUsage::Binormal, 2, 30 },
    { DeclUsage::Binormal, 3, 31 },
};
#else
// FM2 (bug-128): aligned with the title's kSemanticLocations table
// (FM2/src/render/render_state.cpp) -- the two MUST agree for Vulkan location
// binding, and the old Unleashed table was missing TEXCOORD4 entirely (the car
// VS's r8 vfetch fell into the float4(0) hole -> unlit black car) and
// disagreed on COLOR1/BLENDINDICES/TEXCOORD5-7. Extra usages the title table
// doesn't map yet live in the free 5-12/25-31 slots.
static constexpr DeclUsageLocation USAGE_LOCATIONS[] =
{
    { DeclUsage::Position, 0, 0 },
    { DeclUsage::Normal, 0, 1 },
    { DeclUsage::Tangent, 0, 2 },
    { DeclUsage::Binormal, 0, 3 },
    { DeclUsage::Position, 1, 4 },
    { DeclUsage::TexCoord, 0, 13 },
    { DeclUsage::TexCoord, 1, 14 },
    { DeclUsage::TexCoord, 2, 15 },
    { DeclUsage::TexCoord, 3, 16 },
    { DeclUsage::Color, 0, 17 },
    { DeclUsage::Color, 1, 18 },
    { DeclUsage::BlendWeight, 0, 19 },
    { DeclUsage::BlendIndices, 0, 20 },
    { DeclUsage::TexCoord, 4, 21 },
    { DeclUsage::TexCoord, 5, 22 },
    { DeclUsage::TexCoord, 6, 23 },
    { DeclUsage::TexCoord, 7, 24 },
    // Usages FM2's title table doesn't bind yet, parked in free locations so
    // shaders that read them still get declared inputs (IA default zeros).
    { DeclUsage::Normal, 1, 5 },
    { DeclUsage::Normal, 2, 6 },
    { DeclUsage::Normal, 3, 7 },
    { DeclUsage::Tangent, 1, 8 },
    { DeclUsage::Tangent, 2, 9 },
    { DeclUsage::Tangent, 3, 10 },
    { DeclUsage::Binormal, 1, 11 },
    { DeclUsage::Binormal, 2, 12 },
    { DeclUsage::Binormal, 3, 25 },
    { DeclUsage::Position, 2, 26 },
    { DeclUsage::Position, 3, 27 },
    { DeclUsage::Tangent, 4, 28 },
    { DeclUsage::Tangent, 5, 29 },
    { DeclUsage::Tangent, 6, 30 },
    { DeclUsage::Tangent, 7, 31 },
};
#endif

static const DeclUsageLocation* findUsageLocation(DeclUsage usage, uint32_t usageIndex)
{
    for (auto& usageLocation : USAGE_LOCATIONS)
    {
        if (usageLocation.usage == usage && usageLocation.usageIndex == usageIndex)
            return &usageLocation;
    }

    return nullptr;
}

static bool isPosition0(DeclUsage usage, uint32_t usageIndex)
{
    return usage == DeclUsage::Position && usageIndex == 0;
}

static constexpr std::pair<DeclUsage, size_t> INTERPOLATORS[] =
{
    { DeclUsage::TexCoord, 0 },
    { DeclUsage::TexCoord, 1 },
    { DeclUsage::TexCoord, 2 },
    { DeclUsage::TexCoord, 3 },
    { DeclUsage::TexCoord, 4 },
    { DeclUsage::TexCoord, 5 },
    { DeclUsage::TexCoord, 6 },
    { DeclUsage::TexCoord, 7 },
    { DeclUsage::TexCoord, 8 },
    { DeclUsage::TexCoord, 9 },
    { DeclUsage::TexCoord, 10 },
    { DeclUsage::TexCoord, 11 },
    { DeclUsage::TexCoord, 12 },
    { DeclUsage::TexCoord, 13 },
    { DeclUsage::TexCoord, 14 },
    { DeclUsage::TexCoord, 15 },
    { DeclUsage::Color, 0 },
    { DeclUsage::Color, 1 },
    { DeclUsage::Color, 2 },
    // FM2 fix 2026-07-04: FM2 pixel shaders read NORMAL interpolators (e.g. the
    // logo shader B60DDF84 reads input.iNormal0), but the VS->PS interpolator
    // struct was declared only from TexCoord/Color, so the register-naming
    // emitted iNormal0 with no matching struct member -> DXC "no member named
    // iNormal0". Extend the interpolator ABI to carry NORMAL0-3.
    { DeclUsage::Normal, 0 },
    { DeclUsage::Normal, 1 },
    { DeclUsage::Normal, 2 },
    { DeclUsage::Normal, 3 }
};

static constexpr std::string_view TEXTURE_DIMENSIONS[] = 
{
    "2D",
    "2DArray",
    "Cube" 
};

static FetchDestinationSwizzle getDestSwizzle(uint32_t dstSwizzle, uint32_t index)
{
    return FetchDestinationSwizzle((dstSwizzle >> (index * 3)) & 0x7);
}

uint32_t ShaderRecompiler::printDstSwizzle(uint32_t dstSwizzle, bool operand)
{
    uint32_t size = 0;

    for (size_t i = 0; i < 4; i++)
    {
        const auto swizzle = getDestSwizzle(dstSwizzle, i);
        if (swizzle >= FetchDestinationSwizzle::X && swizzle <= FetchDestinationSwizzle::W)
        {
            out += SWIZZLES[operand ? uint32_t(swizzle) : i];
            size++;
        }
    }

    return size;
}

void ShaderRecompiler::printDstSwizzle01(uint32_t dstRegister, uint32_t dstSwizzle)
{
    for (size_t i = 0; i < 4; i++)
    {
        const auto swizzle = getDestSwizzle(dstSwizzle, i);
        if (swizzle == FetchDestinationSwizzle::Zero)
        {
            indent();
            println("r{}.{} = 0.0;", dstRegister, SWIZZLES[i]);
        }
        else if (swizzle == FetchDestinationSwizzle::One)
        {
            indent();
            println("r{}.{} = 1.0;", dstRegister, SWIZZLES[i]);
        }
    }
}

void ShaderRecompiler::recompile(const VertexFetchInstruction& instr, uint32_t address)
{
    if (instr.isPredicated)
    {
        indent();
        println("if ({}p0)", instr.predicateCondition ? "" : "!");

        indent();
        out += "{\n";
        ++indentation;
    }

    size_t assignmentStart = out.size();
    indent();
    print("r{}.", instr.dstRegister);
    uint32_t size = printDstSwizzle(instr.dstSwizzle, false);
    if (size == 0)
    {
        out.resize(assignmentStart);
        printDstSwizzle01(instr.dstRegister, instr.dstSwizzle);

        if (instr.isPredicated)
        {
            --indentation;
            indent();
            out += "}\n";
        }

        return;
    }

    out += " = ";

    if (size <= 1)
        out += "(float)(";
    else
        print("(float{})(", size);

    auto findResult = vertexElements.find(address);
    const VertexElement* vertexElement = findResult != vertexElements.end() ? &findResult->second : nullptr;
    const DeclUsageLocation* usageLocation = vertexElement != nullptr
        ? findUsageLocation(vertexElement->usage, uint32_t(vertexElement->usageIndex))
        : nullptr;

    if (vertexElement == nullptr || usageLocation == nullptr ||
        uint32_t(vertexElement->usage) >= std::size(USAGE_VARIABLES))
    {
        // bug-128: a silently dropped vfetch zeroes a shader input (this is how
        // the FM2 car went black). Shout so cache regens enumerate every gap.
        if (vertexElement == nullptr)
            fprintf(stderr, "XenosRecomp WARNING: vfetch at 0x%X has no vertex "
                    "element mapping; emitting zeros\n", address);
        else
            fprintf(stderr, "XenosRecomp WARNING: vfetch usage %u.%u has no "
                    "location mapping; emitting zeros\n",
                    uint32_t(vertexElement->usage),
                    uint32_t(vertexElement->usageIndex));
        out += "float4(0.0, 0.0, 0.0, 0.0)";
    }
    else
    {

        const uint32_t usageIndex = uint32_t(vertexElement->usageIndex);
        const bool position0 = isPosition0(vertexElement->usage, usageIndex);

#ifndef UNLEASHED_RECOMP
        const bool byteBasis =
            vertexElement->usage == DeclUsage::Normal ||
            vertexElement->usage == DeclUsage::Tangent ||
            vertexElement->usage == DeclUsage::Binormal;
#else
        const bool byteBasis = false;
#endif
        // Xbox 360 DEC3N/k_10_11_11/k_11_11_10 packed normals arrive in a
        // float4 input whose R32_FLOAT layout preserves the raw word for
        // asuint below. POSITION0 instead uses a matching uint4 input/layout.
        // Unwrapped,
        // this is exactly asfloat(rawBits) -- a huge garbage vector, which
        // flattens per-pixel lighting into a near-uniform result. tfetchR11G11B10
        // unpacks it correctly when SPEC_CONSTANT_R11G11B10_NORMAL is set; when
        // it's not (FLOAT3 or UBYTE4-basis normals), its else branch is
        // asfloat(asuint(x)) == x, a guaranteed no-op round-trip, so wrapping
        // unconditionally (same pattern as byteBasis) cannot break those cases.
        const bool packedNormalBasis = byteBasis;
        // FM2 strips vfetch format fields from its embedded shader containers.
        // The bound declaration restores the per-TEXCOORD packed-format mask.
        const bool packedTexcoord = vertexElement->usage == DeclUsage::TexCoord;

        if (position0)
        {
            out += "tfetchPos3N(";
            // bug-127: the helper reads the F16-position spec constant; mark
            // the shader as responsive to it so the host builds the variant.
            specConstantsMask |= SPEC_CONSTANT_POSITION_F16;
        }
        if (byteBasis)
        {
            out += "unpackUByte4Basis(";
            specConstantsMask |= SPEC_CONSTANT_UNPACK_UBYTE4_BASIS;
        }

        switch (vertexElement->usage)
        {
        case DeclUsage::Normal:
            print("swapFloats(g_SwappedNormals, ");
            break;
        case DeclUsage::Tangent:
            print("swapFloats(g_SwappedTangents, ");
            break;
        case DeclUsage::Binormal:
            print("swapFloats(g_SwappedBinormals, ");
            break;
        case DeclUsage::BlendWeight:
            print("swapFloats(g_SwappedBlendWeights, ");
            break;
        case DeclUsage::TexCoord:
            print("swapFloats(g_SwappedTexcoords, ");
            break;
        }

        if (packedNormalBasis)
        {
            out += "tfetchR11G11B10(asuint(";
            specConstantsMask |= SPEC_CONSTANT_R11G11B10_NORMAL;
        }
        else if (packedTexcoord)
        {
            out += "tfetchR11G11B10(asuint(";
        }

        print("(input.i{}{})", USAGE_VARIABLES[uint32_t(vertexElement->usage)], usageIndex);

        if (packedNormalBasis)
            out += "))";
        else if (packedTexcoord)
            print("), (g_R11G11B10Texcoords & (1u << {})) != 0)", usageIndex);

        switch (vertexElement->usage)
        {
        case DeclUsage::Normal:
        case DeclUsage::Tangent:
        case DeclUsage::Binormal:
        case DeclUsage::BlendWeight:
        case DeclUsage::TexCoord:
            print(", {})", usageIndex);
            break;
        }

        if (byteBasis)
            out += ")";

        if (position0)
            out += ")";
    }

    out += ").";
    printDstSwizzle(instr.dstSwizzle, true);

    out += ";\n";

    printDstSwizzle01(instr.dstRegister, instr.dstSwizzle);

    if (instr.isPredicated)
    {
        --indentation;
        indent();
        out += "}\n";
    }
}

void ShaderRecompiler::recompile(const TextureFetchInstruction& instr, bool bicubic)
{
    if (instr.opcode != FetchOpcode::TextureFetch &&
        instr.opcode != FetchOpcode::GetTextureWeights &&
        instr.opcode != FetchOpcode::GetTextureGradients)
        return;

    if (instr.isPredicated)
    {
        indent();
        println("if ({}p0)", instr.predCondition ? "" : "!");

        indent();
        out += "{\n";
        ++indentation;
    }

    auto printSrcRegister = [&](size_t componentCount)
        {
            print("r{}.", instr.srcRegister);

            for (size_t i = 0; i < componentCount; i++)
                out += SWIZZLES[((instr.srcSwizzle >> (i * 2))) & 0x3];
        };

    std::string constName;
    const char* constNamePtr = nullptr;
#ifdef UNLEASHED_RECOMP
    bool subtractFromOne = false;
#endif

    auto findResult = samplers.find(instr.constIndex);
    if (findResult != samplers.end())
    {
        constNamePtr = findResult->second;

    #ifdef UNLEASHED_RECOMP
        subtractFromOne = hasMtxPrevInvViewProjection && strcmp(constNamePtr, "sampZBuffer") == 0;
    #endif
    }
    else
    {
        constName = fmt::format("s{}", instr.constIndex);
        constNamePtr = constName.c_str();
    }

#ifdef UNLEASHED_RECOMP
    if (instr.constIndex == 0 && instr.dimension == TextureDimension::Texture2D)
    {
        indent();
        println("pixelCoord = getPixelCoord(");
        println("#ifdef __air__");
        indent();
        println("g_Texture2DDescriptorHeap,");
        println("#endif");
        indent();
        print("{}_Texture2DDescriptorIndex, ", constNamePtr);
        printSrcRegister(2);
        out += ");\n";
    }
#endif

    size_t textureAssignmentStart = out.size();
    indent();
    print("r{}.", instr.dstRegister);
    uint32_t textureWriteSize = printDstSwizzle(instr.dstSwizzle, false);
    if (textureWriteSize == 0)
    {
        out.resize(textureAssignmentStart);
        printDstSwizzle01(instr.dstRegister, instr.dstSwizzle);

        if (instr.isPredicated)
        {
            --indentation;
            indent();
            out += "}\n";
        }

        return;
    }

    out += " = ";
    if (instr.opcode == FetchOpcode::GetTextureGradients)
    {
        out += "getGradients2D(";
        printSrcRegister(2);
        out += ").";
        printDstSwizzle(instr.dstSwizzle, true);
        out += ";\n";

        printDstSwizzle01(instr.dstRegister, instr.dstSwizzle);

        if (instr.isPredicated)
        {
            --indentation;
            indent();
            out += "}\n";
        }
        return;
    }

    if (instr.opcode == FetchOpcode::GetTextureWeights)
        out += "float4(";

    switch (instr.opcode)
    {
    case FetchOpcode::TextureFetch:
    {
    #ifdef UNLEASHED_RECOMP
        if (subtractFromOne)
            out += "1.0 - ";
    #endif

        out += "tfetch";
        break;
    }
    case FetchOpcode::GetTextureWeights:
    {
        out += "getWeights";
        break;
    }
    }

    std::string_view dimension;
    uint32_t componentCount = 0;

    switch (instr.dimension)
    {
    case TextureDimension::Texture1D:
        dimension = "1D";
        componentCount = 1;
        break;
    case TextureDimension::Texture2D:
        dimension = "2D";
        componentCount = 2;
        break;
    case TextureDimension::Texture3D:
        dimension = "2DArray";
        componentCount = 3;
        break;
    case TextureDimension::TextureCube:
        dimension = "Cube";
        componentCount = 3;
        break;
    }

    out += dimension;

    // Computed LOD uses implicit derivatives plus bias (SampleBias). Register
    // LOD is explicit and must use SampleLevel instead.
    const bool computedLod = isPixelShader &&
                             instr.opcode == FetchOpcode::TextureFetch &&
                             instr.useCompLod && !instr.useRegLod;
    const bool registerLod = isPixelShader &&
                             instr.opcode == FetchOpcode::TextureFetch &&
                             instr.useRegLod;
    if (computedLod)
        out += "CL";
    else if (registerLod)
        out += "L";

#ifdef UNLEASHED_RECOMP
    if (bicubic)
        out += "Bicubic";
#endif

    println("(");

    println("#ifdef __air__");
    indent();
    println("\tg_Texture{}DescriptorHeap,", dimension);
    indent();
    println("\tg_SamplerDescriptorHeap,");
    println("#endif");

    indent();
    print("\t{0}_Texture{1}DescriptorIndex, {0}_SamplerDescriptorIndex, ", constNamePtr, dimension);
    printSrcRegister(componentCount);

    switch (instr.dimension)
    {
        case TextureDimension::Texture2D:
            print(", float2({}, {})", instr.offsetX * 0.5f, instr.offsetY * 0.5f);
            break;
        case TextureDimension::Texture3D:
            print(", float3({}, {}, {})", instr.offsetX * 0.5f, instr.offsetY * 0.5f, instr.offsetZ * 0.5f);
            break;
    }

    // Both explicit and computed paths apply the instruction's 1/32-step bias.
    if (computedLod || registerLod)
    {
        print(", {}", float(instr.lodBias) / 32.0f);
        if (registerLod)
            out += " + textureLod";
    }

    out += ")";

    if (instr.opcode == FetchOpcode::GetTextureWeights)
    {
        for (uint32_t i = componentCount; i < 4; i++)
            out += ", 0.0";
        out += ")";
    }

    out += ".";

    printDstSwizzle(instr.dstSwizzle, true);

    out += ";\n";

    printDstSwizzle01(instr.dstRegister, instr.dstSwizzle);

    if (instr.isPredicated)
    {
        --indentation;
        indent();
        out += "}\n";
    }
}

void ShaderRecompiler::recompile(const AluInstruction& instr)
{
    if (instr.isPredicated)
    {
        indent();
        println("if ({}p0)", instr.predicateCondition ? "" : "!");

        indent(); 
        out += "{\n";
        ++indentation;
    }

    enum
    {
        VECTOR_0,
        VECTOR_1,
        VECTOR_2,
        SCALAR_0,
        SCALAR_1,
        SCALAR_CONSTANT_0,
        SCALAR_CONSTANT_1
    };

    struct OperationResult
    {
        std::string expression;
        size_t componentCount;
    };

    auto op = [&](size_t operand)
        {
            size_t reg = 0;
            size_t swizzle = 0;
            size_t sourceIndex = 0;
            bool select = true;
            bool negate = false;
            bool abs = false;

            switch (operand)
            {
            case SCALAR_CONSTANT_0:
                reg = instr.src3Register;
                swizzle = instr.src3Swizzle;
                sourceIndex = 3;
                select = false;
                negate = instr.src3Negate;
                abs = instr.absConstants;
                break;

            case SCALAR_CONSTANT_1:
                reg = (uint32_t(instr.scalarOpcode) & 1) | (instr.src3Select << 1) | (instr.src3Swizzle & 0x3C);
                swizzle = instr.src3Swizzle;
                select = true;
                negate = instr.src3Negate;
                abs = instr.absConstants;
                break;

            default:
                switch (operand)
                {
                case VECTOR_0:
                    reg = instr.src1Register;
                    swizzle = instr.src1Swizzle;
                    sourceIndex = 1;
                    select = instr.src1Select;
                    negate = instr.src1Negate;
                    break;
                case VECTOR_1:
                    reg = instr.src2Register;
                    swizzle = instr.src2Swizzle;
                    sourceIndex = 2;
                    select = instr.src2Select;
                    negate = instr.src2Negate;
                    break;
                case VECTOR_2:
                case SCALAR_0:
                case SCALAR_1:
                    reg = instr.src3Register;
                    swizzle = instr.src3Swizzle;
                    sourceIndex = 3;
                    select = instr.src3Select;
                    negate = instr.src3Negate;
                    break;
                }

                if (select)
                {
                    abs = (reg & 0x80) != 0;
                    reg &= 0x3F;
                }
                else
                {
                    abs = instr.absConstants;
                }

                break;
            }

            std::string regFormatted;

            if (select)
            {
                regFormatted = fmt::format("r{}", reg);
            }
            else
            {
                // The two addressing bits are assigned by the ordinal of the
                // constant operand, not directly by source slot. Match Xenia's
                // AluInstruction::src_const_is_addressed mapping.
                bool relative = false;
                switch (sourceIndex)
                {
                case 1:
                    relative = instr.const0Relative;
                    break;
                case 2:
                    relative = instr.src1Select ? instr.const0Relative : instr.const1Relative;
                    break;
                case 3:
                    relative = (instr.src1Select && instr.src2Select)
                        ? instr.const0Relative
                        : instr.const1Relative;
                    break;
                }

                if (relative)
                {
                    regFormatted = fmt::format("Load{}ShaderConstant({} + {})",
                        isPixelShader ? "Pixel" : "Vertex", reg,
                        instr.constAddressRegisterRelative ? "a0" : "aL");
                }
                else
                {
                auto findResult = float4Constants.find(reg);
                if (findResult != float4Constants.end())
                {
                    const char* constantName = reinterpret_cast<const char*>(constantTableData + findResult->second->name);
                    if (findResult->second->registerCount > 1)
                    {
                    #ifdef UNLEASHED_RECOMP
                        if (hasMtxProjection && strcmp(constantName, "g_MtxProjection") == 0)
                        {
                            regFormatted = fmt::format("(iterationIndex == 0 ? mtxProjectionReverseZ[{0}] : mtxProjection[{0}])",
                                reg - findResult->second->registerIndex);
                        }
                        else
                    #endif
                        {
                            regFormatted = fmt::format("{}({})", constantName,
                                reg - findResult->second->registerIndex);
                        }
                    }
                    else
                    {
                        regFormatted = constantName;
                    }
                }
                else if (float4Definitions.count(reg) != 0)
                {
                    regFormatted = fmt::format("c{}", reg);
                }
                else
                {
                    regFormatted = fmt::format("Load{}ShaderConstant({})",
                        isPixelShader ? "Pixel" : "Vertex", reg);
                }
                }
            }

            OperationResult opResult {};

            if (negate)
                opResult.expression += '-';

            if (abs)
                opResult.expression += "abs(";

            opResult.expression += regFormatted;
            opResult.expression += '.';

            switch (operand)
            {
            case VECTOR_0:
            case VECTOR_1:
            case VECTOR_2:
            {
                uint32_t mask;

                switch (instr.vectorOpcode)
                {
                case AluVectorOpcode::Dp2Add:
                    mask = (operand == VECTOR_2) ? 0b1 : 0b11;
                    break;

                case AluVectorOpcode::Dp3:
                    mask = 0b111;
                    break;

                case AluVectorOpcode::Dp4:
                case AluVectorOpcode::Max4:
                    mask = 0b1111;
                    break;

                default:
                    mask = instr.vectorWriteMask != 0 ? instr.vectorWriteMask : 0b1;
                    break;
                }

                for (size_t i = 0; i < 4; i++)
                {
                    if ((mask >> i) & 0x1) {
                        opResult.componentCount++;
                        opResult.expression += SWIZZLES[((swizzle >> (i * 2)) + i) & 0x3];
                    }
                }

                break;
            }

            case SCALAR_0:
            case SCALAR_CONSTANT_0:
                opResult.componentCount = 1;
                opResult.expression += SWIZZLES[((swizzle >> 6) + 3) & 0x3];
                break;

            case SCALAR_1:
            case SCALAR_CONSTANT_1:
                opResult.componentCount = 1;
                opResult.expression += SWIZZLES[swizzle & 0x3];
                break;
            }

            if (abs)
                opResult.expression += ")";

            return opResult;
        };

    switch (instr.vectorOpcode)
    {
    case AluVectorOpcode::KillEq:
        if (isPixelShader)
        {
            indent();
            println("clip(any({} == {}) ? -1 : 1);", op(VECTOR_0).expression, op(VECTOR_1).expression);
        }
        break;
    
    case AluVectorOpcode::KillGt:
        if (isPixelShader)
        {
            indent();
            println("clip(any({} > {}) ? -1 : 1);", op(VECTOR_0).expression, op(VECTOR_1).expression);
        }
        break;
    
    case AluVectorOpcode::KillGe:
        if (isPixelShader)
        {
            indent();
            println("clip(any({} >= {}) ? -1 : 1);", op(VECTOR_0).expression, op(VECTOR_1).expression);
        }
        break;
    
    case AluVectorOpcode::KillNe:
        if (isPixelShader)
        {
            indent();
            println("clip(any({} != {}) ? -1 : 1);", op(VECTOR_0).expression, op(VECTOR_1).expression);
        }
        break;
    }

    bool closeIfBracket = false;

    std::string_view exportRegister;
    bool vectorRegister = true;

    if (instr.exportData)
    {
        if (isPixelShader)
        {
            switch (ExportRegister(instr.vectorDest))
            {
            case ExportRegister::PSColor0:
                exportRegister = "output.oC0";
                break;        
            case ExportRegister::PSColor1:
                exportRegister = "output.oC1";
                break;        
            case ExportRegister::PSColor2:
                exportRegister = "output.oC2";
                break;            
            case ExportRegister::PSColor3:
                exportRegister = "output.oC3";
                break;           
            case ExportRegister::PSDepth:
                exportRegister = "output.oDepth";
                vectorRegister = false;
                break;
            }
        }
        else
        {
            switch (ExportRegister(instr.vectorDest))
            {
            case ExportRegister::VSPosition:
                exportRegister = "output.oPos";

            #ifdef UNLEASHED_RECOMP
                if (hasMtxProjection)
                {
                    indent();
                    out += "if ((g_SpecConstants() & SPEC_CONSTANT_REVERSE_Z) == 0 || iterationIndex == 0)\n";
                    indent();
                    out += "{\n";
                    ++indentation;

                    closeIfBracket = true;
                }
            #endif

                break;

            default:
            {
                auto findResult = interpolators.find(instr.vectorDest);
                assert(findResult != interpolators.end());
                exportRegister = findResult->second;
                break;
            }
            }
        }
    }

    if (instr.vectorOpcode >= AluVectorOpcode::SetpEqPush && instr.vectorOpcode <= AluVectorOpcode::SetpGePush)
    {
        indent();
        print("p0 = all({} == 0.0) && all({} ", op(VECTOR_0).expression, op(VECTOR_1).expression);

        switch (instr.vectorOpcode)
        {
        case AluVectorOpcode::SetpEqPush:
            out += "==";
            break;
        case AluVectorOpcode::SetpNePush:
            out += "!=";
            break;
        case AluVectorOpcode::SetpGtPush:
            out += ">";
            break;
        case AluVectorOpcode::SetpGePush:
            out += ">=";
            break;
        }

        out += " 0.0);\n";
    }
    else if (instr.vectorOpcode >= AluVectorOpcode::MaxA)
    {
        auto v0 = op(VECTOR_0);
        indent();
        if (v0.componentCount >= 4)
            println("a0 = (int)clamp(floor(({}).w + 0.5), -256.0, 255.0);", v0.expression);
        else if (v0.componentCount > 1)
            println("a0 = (int)clamp(floor(({}).x + 0.5), -256.0, 255.0);", v0.expression);
        else
            println("a0 = (int)clamp(floor({} + 0.5), -256.0, 255.0);", v0.expression);
    }

    uint32_t vectorWriteMask = instr.vectorWriteMask;
    vectorWriteMask &= 0b1111;
    if (instr.exportData)
        vectorWriteMask &= ~instr.scalarWriteMask;

    if (vectorWriteMask != 0)
    {
        indent();
        if (!exportRegister.empty())
        {
            out += exportRegister;
            if (vectorRegister)
                out += '.';
        }
        else
        {
            print("r{}.", instr.vectorDest);
        }

        uint32_t vectorWriteSize = 0;

        for (size_t i = 0; i < 4; i++)
        {
            if ((vectorWriteMask >> i) & 0x1)
            {
                if (vectorRegister)
                    out += SWIZZLES[i];
                vectorWriteSize++;
            }
        }

        out += " = ";

        if (vectorWriteSize > 1)
            print("(float{})((", vectorWriteSize);
        else
            out += "(float)((";

        if (instr.vectorSaturate)
            out += "saturate(";

        size_t operationResultComponentCount = 1;

        switch (instr.vectorOpcode)
        {
        case AluVectorOpcode::Add:
            {
                auto v0 = op(VECTOR_0);
                auto v1 = op(VECTOR_1);
                operationResultComponentCount = std::max(v0.componentCount, v1.componentCount);

                print("{} + {}", v0.expression, v1.expression);
                break;
            }

        case AluVectorOpcode::Mul:
            {
                auto v0 = op(VECTOR_0);
                auto v1 = op(VECTOR_1);
                operationResultComponentCount = std::max(v0.componentCount, v1.componentCount);

                print("mulSM3({}, {})", v0.expression, v1.expression);
                break;
            }

        case AluVectorOpcode::Max:
        case AluVectorOpcode::MaxA:
            {
                auto v0 = op(VECTOR_0);
                auto v1 = op(VECTOR_1);
                operationResultComponentCount = std::max(v0.componentCount, v1.componentCount);

                print("maxSM3({}, {})", v0.expression, v1.expression);
                break;
            }

        case AluVectorOpcode::Min:
            {
                auto v0 = op(VECTOR_0);
                auto v1 = op(VECTOR_1);
                operationResultComponentCount = std::max(v0.componentCount, v1.componentCount);

                print("minSM3({}, {})", v0.expression, v1.expression);
                break;
            }

        case AluVectorOpcode::Seq:
            {
                auto v0 = op(VECTOR_0);
                auto v1 = op(VECTOR_1);
                operationResultComponentCount = std::max(v0.componentCount, v1.componentCount);

                print("{} == {}", v0.expression, v1.expression);
                break;
            }

        case AluVectorOpcode::Sgt:
            {
                auto v0 = op(VECTOR_0);
                auto v1 = op(VECTOR_1);
                operationResultComponentCount = std::max(v0.componentCount, v1.componentCount);

                print("{} > {}", v0.expression, v1.expression);
                break;
            }

        case AluVectorOpcode::Sge:
            {
                auto v0 = op(VECTOR_0);
                auto v1 = op(VECTOR_1);
                operationResultComponentCount = std::max(v0.componentCount, v1.componentCount);

                print("{} >= {}", v0.expression, v1.expression);
                break;
            }

        case AluVectorOpcode::Sne:
            {
                auto v0 = op(VECTOR_0);
                auto v1 = op(VECTOR_1);
                operationResultComponentCount = std::max(v0.componentCount, v1.componentCount);

                print("{} != {}", v0.expression, v1.expression);
                break;
            }

        case AluVectorOpcode::Frc:
            {
                auto v0 = op(VECTOR_0);
                operationResultComponentCount = v0.componentCount;

                print("frac({})", v0.expression);
                break;
            }

        case AluVectorOpcode::Trunc:
            {
                auto v0 = op(VECTOR_0);
                operationResultComponentCount = v0.componentCount;

                print("trunc({})", v0.expression);
                break;
            }

        case AluVectorOpcode::Floor:
            {
                auto v0 = op(VECTOR_0);
                operationResultComponentCount = v0.componentCount;

                print("floor({})", v0.expression);
                break;
            }

        case AluVectorOpcode::Mad:
            {
                auto v0 = op(VECTOR_0);
                auto v1 = op(VECTOR_1);
                auto v2 = op(VECTOR_2);
                operationResultComponentCount = std::max(std::max(v0.componentCount, v1.componentCount), v2.componentCount);

                print("mulSM3({}, {}) + {}", v0.expression, v1.expression, v2.expression);
                break;
            }

        case AluVectorOpcode::CndEq:
            {
                auto v0 = op(VECTOR_0);
                auto v1 = op(VECTOR_1);
                auto v2 = op(VECTOR_2);
                operationResultComponentCount = std::max(v1.componentCount, v2.componentCount);

                print("selectWrapper({} == 0.0, {}, {})", v0.expression, v1.expression, v2.expression);
                break;
            }

        case AluVectorOpcode::CndGe:
            {
                auto v0 = op(VECTOR_0);
                auto v1 = op(VECTOR_1);
                auto v2 = op(VECTOR_2);
                operationResultComponentCount = std::max(v1.componentCount, v2.componentCount);

                print("selectWrapper({} >= 0.0, {}, {})", v0.expression, v1.expression, v2.expression);
                break;
            }

        case AluVectorOpcode::CndGt:
            {
                auto v0 = op(VECTOR_0);
                auto v1 = op(VECTOR_1);
                auto v2 = op(VECTOR_2);
                operationResultComponentCount = std::max(v1.componentCount, v2.componentCount);

                print("selectWrapper({} > 0.0, {}, {})", v0.expression, v1.expression, v2.expression);
                break;
            }

        case AluVectorOpcode::Dp4:
        case AluVectorOpcode::Dp3:
            operationResultComponentCount = 1;
            print("dotSM3({}, {})", op(VECTOR_0).expression, op(VECTOR_1).expression);
            break;

        case AluVectorOpcode::Dp2Add:
            {
                auto v2 = op(VECTOR_2);
                operationResultComponentCount = v2.componentCount;

                print("dotSM3({}, {}) + {}", op(VECTOR_0).expression, op(VECTOR_1).expression, v2.expression);
                break;
            }

        case AluVectorOpcode::Cube:
            operationResultComponentCount = 4;
            print("cube({})", op(VECTOR_0).expression);
            break;

        case AluVectorOpcode::Max4:
            operationResultComponentCount = 4;
            print("max4({})", op(VECTOR_0).expression);
            break;

        case AluVectorOpcode::SetpEqPush:
        case AluVectorOpcode::SetpNePush:
        case AluVectorOpcode::SetpGtPush:
        case AluVectorOpcode::SetpGePush:
            {
                auto v0 = op(VECTOR_0);
                operationResultComponentCount = v0.componentCount;

                print("p0 ? 0.0 : {} + 1.0", v0.expression);
                break;
            }

        case AluVectorOpcode::KillEq:
            operationResultComponentCount = 1;
            print("any({} == {})", op(VECTOR_0).expression, op(VECTOR_1).expression);
            break;

        case AluVectorOpcode::KillGt:
            operationResultComponentCount = 1;
            print("any({} > {})", op(VECTOR_0).expression, op(VECTOR_1).expression);
            break;

        case AluVectorOpcode::KillGe:
            operationResultComponentCount = 1;
            print("any({} >= {})", op(VECTOR_0).expression, op(VECTOR_1).expression);
            break;

        case AluVectorOpcode::KillNe:
            operationResultComponentCount = 1;
            print("any({} != {})", op(VECTOR_0).expression, op(VECTOR_1).expression);
            break;

        case AluVectorOpcode::Dst:
            operationResultComponentCount = 4;
            print("dst({}, {})", op(VECTOR_0).expression, op(VECTOR_1).expression);
            break;

        default:
            out += "0.0";
            break;
        }

		out += ")";

        if (operationResultComponentCount > vectorWriteSize) {
            if (vectorWriteSize == 1) {
                out += ".x";
            } else if (vectorWriteSize == 2) {
                out += ".xy";
            } else if (vectorWriteSize == 3) {
                out += ".xyz";
            }
        }

        out += ")";

        if (instr.vectorSaturate)
            out += ')';

        out += ";\n";
    }

    if (instr.scalarOpcode != AluScalarOpcode::RetainPrev)
    {
        if (instr.scalarOpcode >= AluScalarOpcode::SetpEq && instr.scalarOpcode <= AluScalarOpcode::SetpRstr)
        {
            indent();
            out += "p0 = ";

            switch (instr.scalarOpcode)
            {
            case AluScalarOpcode::SetpEq:
                print("{} == 0.0", op(SCALAR_0).expression);
                break;

            case AluScalarOpcode::SetpNe:
                print("{} != 0.0", op(SCALAR_0).expression);
                break;

            case AluScalarOpcode::SetpGt:
                print("{} > 0.0", op(SCALAR_0).expression);
                break;

            case AluScalarOpcode::SetpGe:
                print("{} >= 0.0", op(SCALAR_0).expression);
                break;

            case AluScalarOpcode::SetpInv:
                print("{} == 1.0", op(SCALAR_0).expression);
                break;

            case AluScalarOpcode::SetpPop:
                print("{} - 1.0 <= 0.0", op(SCALAR_0).expression);
                break;

            case AluScalarOpcode::SetpClr:
                out += "false";
                break;

            case AluScalarOpcode::SetpRstr:
                print("{} == 0.0", op(SCALAR_0).expression);
                break;
            }

            out += ";\n";
        }

        indent();
        out += "ps = ";
        if (instr.scalarSaturate)
            out += "saturate((float)(";

        switch (instr.scalarOpcode)
        {
        case AluScalarOpcode::Adds:
            print("{} + {}", op(SCALAR_0).expression, op(SCALAR_1).expression);
            break;

        case AluScalarOpcode::AddsPrev:
            print("{} + ps", op(SCALAR_0).expression);
            break;

        case AluScalarOpcode::Muls:
            print("mulSM3({}, {})", op(SCALAR_0).expression, op(SCALAR_1).expression);
            break;

        case AluScalarOpcode::MulsPrev:
        case AluScalarOpcode::MulsPrev2:
            print("mulSM3({}, ps)", op(SCALAR_0).expression);
            break;

        case AluScalarOpcode::Maxs:
        case AluScalarOpcode::MaxAs:
        case AluScalarOpcode::MaxAsf:
            print("maxSM3({}, {})", op(SCALAR_0).expression, op(SCALAR_1).expression);
            break;

        case AluScalarOpcode::Mins:
            print("minSM3({}, {})", op(SCALAR_0).expression, op(SCALAR_1).expression);
            break;

        case AluScalarOpcode::Seqs:
            print("{} == 0.0", op(SCALAR_0).expression);
            break;

        case AluScalarOpcode::Sgts:
            print("{} > 0.0", op(SCALAR_0).expression);
            break;

        case AluScalarOpcode::Sges:
            print("{} >= 0.0", op(SCALAR_0).expression);
            break;

        case AluScalarOpcode::Snes:
            print("{} != 0.0", op(SCALAR_0).expression);
            break;

        case AluScalarOpcode::Frcs:
            print("frac({})", op(SCALAR_0).expression);
            break;

        case AluScalarOpcode::Truncs:
            print("trunc({})", op(SCALAR_0).expression);
            break;

        case AluScalarOpcode::Floors:
            print("floor({})", op(SCALAR_0).expression);
            break;

        case AluScalarOpcode::Exp:
            print("exp2({})", op(SCALAR_0).expression);
            break;

        // Xenos has separate clamp, flush-to-zero and IEEE variants. The SM3
        // multiply helpers above already prevent 0*Inf from becoming NaN, so the
        // IEEE variants must remain raw exactly as in Xenia.
        case AluScalarOpcode::Logc:
            print("clamp(log2({}), -FLT_MAX, FLT_MAX)", op(SCALAR_0).expression);
            break;
        case AluScalarOpcode::Log:
            print("log2({})", op(SCALAR_0).expression);
            break;

        case AluScalarOpcode::Rcpc:
            print("clamp(rcp({}), -FLT_MAX, FLT_MAX)", op(SCALAR_0).expression);
            break;
        case AluScalarOpcode::Rcp:
            print("rcp({})", op(SCALAR_0).expression);
            break;
        case AluScalarOpcode::Rcpf: // RECIP_FF (flush +-Inf -> 0)
            print("rcpFF({})", op(SCALAR_0).expression);
            break;

        case AluScalarOpcode::Rsqc:
            print("clamp(rsqrt({}), -FLT_MAX, FLT_MAX)", op(SCALAR_0).expression);
            break;
        case AluScalarOpcode::Rsq:
            print("rsqrt({})", op(SCALAR_0).expression);
            break;
        case AluScalarOpcode::Rsqf: // RSQ_FF (flush +-Inf -> 0)
            print("rsqFF({})", op(SCALAR_0).expression);
            break;

        case AluScalarOpcode::Subs:
            print("{} - {}", op(SCALAR_0).expression, op(SCALAR_1).expression);
            break;

        case AluScalarOpcode::SubsPrev:
            print("{} - ps", op(SCALAR_0).expression);
            break;

        case AluScalarOpcode::SetpEq:
        case AluScalarOpcode::SetpNe:
        case AluScalarOpcode::SetpGt:
        case AluScalarOpcode::SetpGe:
            out += "p0 ? 0.0 : 1.0";
            break;

        case AluScalarOpcode::SetpInv:
            print("p0 ? 0.0 : {0} == 0.0 ? 1.0 : {0}", op(SCALAR_0).expression);
            break;

        case AluScalarOpcode::SetpPop:
            print("p0 ? 0.0 : ({} - 1.0)", op(SCALAR_0).expression);
            break;

        case AluScalarOpcode::SetpClr:
            out += "FLT_MAX";
            break;

        case AluScalarOpcode::SetpRstr:
            print("p0 ? 0.0 : {}", op(SCALAR_0).expression);
            break;

        case AluScalarOpcode::KillsEq:
            print("{} == 0.0", op(SCALAR_0).expression);
            break;

        case AluScalarOpcode::KillsGt:
            print("{} > 0.0", op(SCALAR_0).expression);
            break;

        case AluScalarOpcode::KillsGe:
            print("{} >= 0.0", op(SCALAR_0).expression);
            break;

        case AluScalarOpcode::KillsNe:
            print("{} != 0.0", op(SCALAR_0).expression);
            break;

        case AluScalarOpcode::KillsOne:
            print("{} == 1.0", op(SCALAR_0).expression);
            break;

        case AluScalarOpcode::Sqrt:
            print("sqrt({})", op(SCALAR_0).expression);
            break;

        case AluScalarOpcode::Mulsc0:
        case AluScalarOpcode::Mulsc1:
            print("mulSM3({}, {})", op(SCALAR_CONSTANT_0).expression, op(SCALAR_CONSTANT_1).expression);
            break;

        case AluScalarOpcode::Addsc0:
        case AluScalarOpcode::Addsc1:
            print("{} + {}", op(SCALAR_CONSTANT_0).expression, op(SCALAR_CONSTANT_1).expression);
            break;

        case AluScalarOpcode::Subsc0:
        case AluScalarOpcode::Subsc1:
            print("{} - {}", op(SCALAR_CONSTANT_0).expression, op(SCALAR_CONSTANT_1).expression);
            break;

        case AluScalarOpcode::Sin:
            print("sin({})", op(SCALAR_0).expression);
            break;

        case AluScalarOpcode::Cos:
            print("cos({})", op(SCALAR_0).expression);
            break;

        default:
            out += "0.0";
            break;
        }

        if (instr.scalarSaturate)
            out += "))";

        out += ";\n";

        switch (instr.scalarOpcode)
        {
        case AluScalarOpcode::MaxAs:
            indent();
            println("a0 = (int)clamp(floor({} + 0.5), -256.0, 255.0);", op(SCALAR_0).expression);
            break;     
        case AluScalarOpcode::MaxAsf:
            indent();
            println("a0 = (int)clamp(floor({}), -256.0, 255.0);", op(SCALAR_0).expression);
            break;
        }
    }

    uint32_t scalarWriteMask = instr.scalarWriteMask;
    scalarWriteMask &= 0b1111;
    if (instr.exportData)
        scalarWriteMask &= ~instr.vectorWriteMask;

    if (scalarWriteMask != 0)
    {
        indent();
        if (!exportRegister.empty())
        {
            out += exportRegister;
            if (vectorRegister)
                out += '.';
        }
        else
        {
            print("r{}.", instr.scalarDest);
        }

        for (size_t i = 0; i < 4; i++)
        {
            if (((scalarWriteMask >> i) & 0x1) && vectorRegister)
                out += SWIZZLES[i];
        }

        out += " = ps;\n";
    }

    if (instr.exportData)
    {
        uint32_t zeroMask = instr.scalarDestRelative ? (0b1111 & ~(instr.vectorWriteMask | instr.scalarWriteMask)) : 0;
        uint32_t oneMask = instr.vectorWriteMask & instr.scalarWriteMask;

        for (size_t i = 0; i < 4; i++)
        {
            uint32_t mask = 1 << i;
            if (zeroMask & mask)
            {
                indent();
                println("{}.{} = 0.0;", exportRegister, SWIZZLES[i]);
            }
            else if (oneMask & mask)
            {
                indent();
                println("{}.{} = 1.0;", exportRegister, SWIZZLES[i]);
            }
        }
    }

    if (isPixelShader && instr.scalarOpcode >= AluScalarOpcode::KillsEq && instr.scalarOpcode <= AluScalarOpcode::KillsOne)
    {
        indent();
        out += "clip(ps != 0.0 ? -1 : 1);\n";
    }

    if (closeIfBracket)
    {
        --indentation;
        indent();
        out += "}\n";
    }

    if (instr.isPredicated)
    {
        --indentation;
        indent();
        out += "}\n";
    }
}

void ShaderRecompiler::recompile(const uint8_t* shaderData, const std::string_view& include)
{
    const bool trace = std::getenv("XENOS_RECOMP_TRACE") != nullptr;
    if (trace)
        fmt::println(stderr, "trace: begin recompile");

    const auto shaderContainer = reinterpret_cast<const ShaderContainer*>(shaderData);

    assert((shaderContainer->flags & 0xFFFFFF00) == 0x102A1100);

    isPixelShader = (shaderContainer->flags & 0x1) == 0;
    // bug-140: gate the PS-only computed-LOD tfetch variants (Sample/SampleBias
    // are illegal in vertex shaders); must precede the shader_common.h include.
    if (isPixelShader)
        out += "#define XENOS_PS 1\n";

    out += include;
    out += '\n';

    // Constant-less shaders carry constantTableOffset == 0 (trivial pixel/vertex
    // shaders such as 05A43916/082CFF62 that reference no constants). Point at a
    // zeroed table so every downstream reflection loop sees zero constants,
    // instead of reinterpreting the shader header as a constant-table container
    // (which faults on these and, in --dump-hlsl, aborts the whole run).
    static const ConstantTableContainer kEmptyConstantTable{};
    const ConstantTableContainer* constantTableContainer = &kEmptyConstantTable;
    if (shaderContainer->constantTableOffset != 0)
    {
        constantTableContainer = reinterpret_cast<const ConstantTableContainer*>(shaderData + shaderContainer->constantTableOffset);
        constantTableData = reinterpret_cast<const uint8_t*>(&constantTableContainer->constantTable);
        const uint32_t constantTableContainerSize = constantTableContainer->size;
        if (constantTableContainerSize < sizeof(ConstantTableContainer) ||
            shaderContainer->constantTableOffset > shaderContainer->virtualSize ||
            constantTableContainerSize > shaderContainer->virtualSize - shaderContainer->constantTableOffset)
        {
            throw std::runtime_error(fmt::format(
                "invalid constant table range: offset={} size={} virtual_size={}",
                shaderContainer->constantTableOffset.get(), constantTableContainerSize,
                shaderContainer->virtualSize.get()));
        }

        const uint32_t constantTableDataSize = constantTableContainerSize - sizeof(be<uint32_t>);
        const uint32_t constantCount = constantTableContainer->constantTable.constants;
        const uint32_t constantInfoOffset = constantTableContainer->constantTable.constantInfo;
        if (constantInfoOffset > constantTableDataSize ||
            constantCount > (constantTableDataSize - constantInfoOffset) / sizeof(ConstantInfo))
        {
            throw std::runtime_error(fmt::format(
                "invalid constant info range: offset={} count={} table_size={}",
                constantInfoOffset, constantCount, constantTableDataSize));
        }

        for (uint32_t i = 0; i < constantCount; i++)
        {
            const auto constantInfo = reinterpret_cast<const ConstantInfo*>(
                constantTableData + constantInfoOffset + i * sizeof(ConstantInfo));
            const uint32_t nameOffset = constantInfo->name;
            if (nameOffset >= constantTableDataSize ||
                std::memchr(constantTableData + nameOffset, '\0', constantTableDataSize - nameOffset) == nullptr)
            {
                throw std::runtime_error(fmt::format(
                    "invalid constant name range: index={} name_offset={} table_size={}",
                    i, nameOffset, constantTableDataSize));
            }
        }
    }
    else
    {
        constantTableData = reinterpret_cast<const uint8_t*>(&constantTableContainer->constantTable);
    }

    if (trace)
        fmt::println(stderr, "trace: constant table validated");

    const auto shader = reinterpret_cast<const Shader*>(shaderData + shaderContainer->shaderOffset);
    if (shader->physicalOffset > shaderContainer->physicalSize ||
        shader->size > shaderContainer->physicalSize - shader->physicalOffset ||
        shader->size > 4096 ||
        (shader->size % 12) != 0)
    {
        throw std::runtime_error(fmt::format(
            "invalid shader bytecode range: physical_offset={} shader_size={} physical_size={}",
            shader->physicalOffset.get(), shader->size.get(), shaderContainer->physicalSize.get()));
    }

    const be<uint32_t>* code = reinterpret_cast<const be<uint32_t>*>(shaderData + shaderContainer->virtualSize + shader->physicalOffset);
    if (trace)
        fmt::println(stderr, "trace: shader range validated size={}", shader->size.get());
    std::map<uint32_t, uint32_t> unreflectedSamplers;
    for (uint32_t i = 0; i < 16; i++)
        unreflectedSamplers.emplace(i, 0);

    // Reflection may name a single register inside a wider array. Resolve the
    // physical register owner once so every backend and ALU operand picks the
    // same widest declaration; equal spans retain constant-table order.
    for (uint32_t i = 0; i < constantTableContainer->constantTable.constants; i++)
    {
        const auto constantInfo = reinterpret_cast<const ConstantInfo*>(
            constantTableData + constantTableContainer->constantTable.constantInfo + i * sizeof(ConstantInfo));
        if (constantInfo->registerSet != RegisterSet::Float4)
            continue;

        for (uint16_t j = 0; j < constantInfo->registerCount; j++)
        {
            const uint32_t reg = constantInfo->registerIndex + j;
            auto owner = float4Constants.find(reg);
            if (owner == float4Constants.end() ||
                constantInfo->registerCount > owner->second->registerCount)
            {
                float4Constants[reg] = constantInfo;
            }
        }
    }

    out += "#ifdef __spirv__\n\n";
    out += "#define LoadVertexShaderConstant(INDEX) vk::RawBufferLoad<float4>(g_PushConstants.VertexShaderConstants + min(uint(INDEX), 255u) * 16, 0x10)\n";
    out += "#define LoadPixelShaderConstant(INDEX) vk::RawBufferLoad<float4>(g_PushConstants.PixelShaderConstants + min(uint(INDEX), 223u) * 16, 0x10)\n\n";

#ifdef UNLEASHED_RECOMP
    bool isMetaInstancer = false;
    bool hasIndexCount = false;
#endif

    for (uint32_t i = 0; i < constantTableContainer->constantTable.constants; i++)
    {
        const auto constantInfo = reinterpret_cast<const ConstantInfo*>(
            constantTableData + constantTableContainer->constantTable.constantInfo + i * sizeof(ConstantInfo));

        const char* constantName = reinterpret_cast<const char*>(constantTableData + constantInfo->name);

    #ifdef UNLEASHED_RECOMP
        if (!isPixelShader)
        {
            if (strcmp(constantName, "g_MtxProjection") == 0)
                hasMtxProjection = true;
            else if (strcmp(constantName, "g_InstanceTypes") == 0)
                isMetaInstancer = true;
            else if (strcmp(constantName, "g_IndexCount") == 0)
                hasIndexCount = true;
        }
        else
        {
            if (strcmp(constantName, "g_MtxPrevInvViewProjection") == 0)
                hasMtxPrevInvViewProjection = true;
        }
    #endif

        switch (constantInfo->registerSet)
        {
        case RegisterSet::Float4:
        {
            const char* shaderName = isPixelShader ? "Pixel" : "Vertex";

            if (constantInfo->registerCount > 1)
            {
                uint32_t tailCount = (isPixelShader ? 224 : 256) - constantInfo->registerIndex;

                println("#define {}(INDEX) selectWrapper((INDEX) < {}, vk::RawBufferLoad<float4>(g_PushConstants.{}ShaderConstants + ({} + min(INDEX, {})) * 16, 0x10), 0.0)",
                    constantName, tailCount, shaderName, constantInfo->registerIndex.get(), tailCount - 1);
            }
            else
            {
                println("#define {} vk::RawBufferLoad<float4>(g_PushConstants.{}ShaderConstants + {}, 0x10)",
                    constantName, shaderName, constantInfo->registerIndex * 16);
            }
            
            break;
        }

        case RegisterSet::Sampler:
        {
            unreflectedSamplers.erase(constantInfo->registerIndex);

            for (size_t j = 0; j < std::size(TEXTURE_DIMENSIONS); j++)
            {
                println("#define {}_Texture{}DescriptorIndex vk::RawBufferLoad<uint>(g_PushConstants.SharedConstants + {})",
                    constantName, TEXTURE_DIMENSIONS[j], j * 64 + constantInfo->registerIndex * 4);
            }

            println("#define {}_SamplerDescriptorIndex vk::RawBufferLoad<uint>(g_PushConstants.SharedConstants + {})",
                constantName, std::size(TEXTURE_DIMENSIONS) * 64 + constantInfo->registerIndex * 4);

            samplers.emplace(constantInfo->registerIndex, constantName);
            break;
        }

        }
    }

    for (auto& [samplerIndex, _] : unreflectedSamplers)
    {
        for (size_t j = 0; j < std::size(TEXTURE_DIMENSIONS); j++)
        {
            println("#define s{}_Texture{}DescriptorIndex vk::RawBufferLoad<uint>(g_PushConstants.SharedConstants + {})",
                samplerIndex, TEXTURE_DIMENSIONS[j], j * 64 + samplerIndex * 4);
        }

        println("#define s{}_SamplerDescriptorIndex vk::RawBufferLoad<uint>(g_PushConstants.SharedConstants + {})",
            samplerIndex, std::size(TEXTURE_DIMENSIONS) * 64 + samplerIndex * 4);
    }

    out += "\n#elif defined(__air__)\n\n";
    out += "#define LoadVertexShaderConstant(INDEX) (*(reinterpret_cast<device float4*>(g_PushConstants.VertexShaderConstants + min(uint(INDEX), 255u) * 16)))\n";
    out += "#define LoadPixelShaderConstant(INDEX) (*(reinterpret_cast<device float4*>(g_PushConstants.PixelShaderConstants + min(uint(INDEX), 223u) * 16)))\n\n";

    for (uint32_t i = 0; i < constantTableContainer->constantTable.constants; i++)
    {
        const auto constantInfo = reinterpret_cast<const ConstantInfo*>(
            constantTableData + constantTableContainer->constantTable.constantInfo + i * sizeof(ConstantInfo));

        const char* constantName = reinterpret_cast<const char*>(constantTableData + constantInfo->name);

    #ifdef UNLEASHED_RECOMP
        if (!isPixelShader)
        {
            if (strcmp(constantName, "g_MtxProjection") == 0)
                hasMtxProjection = true;
            else if (strcmp(constantName, "g_InstanceTypes") == 0)
                isMetaInstancer = true;
            else if (strcmp(constantName, "g_IndexCount") == 0)
                hasIndexCount = true;
        }
        else
        {
            if (strcmp(constantName, "g_MtxPrevInvViewProjection") == 0)
                hasMtxPrevInvViewProjection = true;
        }
    #endif

        switch (constantInfo->registerSet)
        {
        case RegisterSet::Float4:
        {
            const char* shaderName = isPixelShader ? "Pixel" : "Vertex";

            if (constantInfo->registerCount > 1)
            {
                uint32_t tailCount = (isPixelShader ? 224 : 256) - constantInfo->registerIndex;

                println("#define {}(INDEX) selectWrapper((INDEX) < {}, (*(reinterpret_cast<device float4*>(g_PushConstants.{}ShaderConstants + ({} + min(INDEX, {})) * 16))), 0.0)",
                    constantName, tailCount, shaderName, constantInfo->registerIndex.get(), tailCount - 1);
            }
            else
            {
                println("#define {} (*(reinterpret_cast<device float4*>(g_PushConstants.{}ShaderConstants + {})))",
                    constantName, shaderName, constantInfo->registerIndex * 16);
            }

            break;
        }

        case RegisterSet::Sampler:
        {
            for (size_t j = 0; j < std::size(TEXTURE_DIMENSIONS); j++)
            {
                println("#define {}_Texture{}DescriptorIndex (*(reinterpret_cast<device uint*>(g_PushConstants.SharedConstants + {})))",
                    constantName, TEXTURE_DIMENSIONS[j], j * 64 + constantInfo->registerIndex * 4);
            }

            println("#define {}_SamplerDescriptorIndex (*(reinterpret_cast<device uint*>(g_PushConstants.SharedConstants + {})))",
                constantName, std::size(TEXTURE_DIMENSIONS) * 64 + constantInfo->registerIndex * 4);

            samplers.emplace(constantInfo->registerIndex, constantName);
            break;
        }

        }
    }

    for (auto& [samplerIndex, _] : unreflectedSamplers)
    {
        for (size_t j = 0; j < std::size(TEXTURE_DIMENSIONS); j++)
        {
            println("#define s{}_Texture{}DescriptorIndex (*(reinterpret_cast<device uint*>(g_PushConstants.SharedConstants + {})))",
                samplerIndex, TEXTURE_DIMENSIONS[j], j * 64 + samplerIndex * 4);
        }

        println("#define s{}_SamplerDescriptorIndex (*(reinterpret_cast<device uint*>(g_PushConstants.SharedConstants + {})))",
            samplerIndex, std::size(TEXTURE_DIMENSIONS) * 64 + samplerIndex * 4);
    }

    out += "\n#else\n\n";

    const char* shaderName = isPixelShader ? "Pixel" : "Vertex";
    const uint32_t shaderConstantCount = isPixelShader ? 224 : 256;

    println("cbuffer {}ShaderConstants : register(b{}, space4)", shaderName, isPixelShader ? 1 : 0);
    out += "{\n";
    println("\tfloat4 g_{}ShaderConstants[{}] : packoffset(c0);", shaderName, shaderConstantCount);
    out += "};\n\n";
    println("#define Load{}ShaderConstant(INDEX) g_{}ShaderConstants[min(uint(INDEX), {}u)]",
        shaderName, shaderName, shaderConstantCount - 1);
    out += "\n";

    for (uint32_t i = 0; i < constantTableContainer->constantTable.constants; i++)
    {
        const auto constantInfo = reinterpret_cast<const ConstantInfo*>(
            constantTableData + constantTableContainer->constantTable.constantInfo + i * sizeof(ConstantInfo));

        if (constantInfo->registerSet == RegisterSet::Float4)
        {
            const char* constantName = reinterpret_cast<const char*>(constantTableData + constantInfo->name);

            if (constantInfo->registerCount > 1)
            {
                uint32_t tailCount = (isPixelShader ? 224 : 256) - constantInfo->registerIndex;
                println("#define {0}(INDEX) selectWrapper((INDEX) < {1}, g_{2}ShaderConstants[{3} + min(uint(INDEX), {4}u)], 0.0)",
                    constantName, tailCount, shaderName, constantInfo->registerIndex.get(), tailCount - 1);
            }
            else
            {
                println("#define {} g_{}ShaderConstants[{}]", constantName, shaderName,
                    constantInfo->registerIndex.get());
            }
        }
    }

    out += "cbuffer SharedConstants : register(b2, space4)\n";
    out += "{\n";

    bool printedSamplerConstants[16] {};
    std::vector<std::string> samplerConstantNames;

    for (uint32_t i = 0; i < constantTableContainer->constantTable.constants; i++)
    {
        const auto constantInfo = reinterpret_cast<const ConstantInfo*>(
            constantTableData + constantTableContainer->constantTable.constantInfo + i * sizeof(ConstantInfo));

        if (constantInfo->registerSet == RegisterSet::Sampler)
        {
            const char* constantName = reinterpret_cast<const char*>(constantTableData + constantInfo->name);
            samplerConstantNames.emplace_back(constantName);
            if (constantInfo->registerIndex < std::size(printedSamplerConstants))
                printedSamplerConstants[constantInfo->registerIndex] = true;

            for (size_t j = 0; j < std::size(TEXTURE_DIMENSIONS); j++)
            {
                println("\tuint {}_Texture{}DescriptorIndex : packoffset(c{}.{});",
                    constantName, TEXTURE_DIMENSIONS[j], j * 4 + constantInfo->registerIndex / 4, SWIZZLES[constantInfo->registerIndex % 4]);
            }

            println("\tuint {}_SamplerDescriptorIndex : packoffset(c{}.{});",
                constantName, 4 * std::size(TEXTURE_DIMENSIONS) + constantInfo->registerIndex / 4, SWIZZLES[constantInfo->registerIndex % 4]);
        }
    }

    for (auto& [samplerIndex, _] : unreflectedSamplers)
    {
        if (samplerIndex >= std::size(printedSamplerConstants))
            continue;

        printedSamplerConstants[samplerIndex] = true;

        for (size_t j = 0; j < std::size(TEXTURE_DIMENSIONS); j++)
        {
            println("\tuint s{}_Texture{}DescriptorIndex : packoffset(c{}.{});",
                samplerIndex, TEXTURE_DIMENSIONS[j], j * 4 + samplerIndex / 4, SWIZZLES[samplerIndex % 4]);
        }

        println("\tuint s{}_SamplerDescriptorIndex : packoffset(c{}.{});",
            samplerIndex, 4 * std::size(TEXTURE_DIMENSIONS) + samplerIndex / 4, SWIZZLES[samplerIndex % 4]);
    }

    out += "\tDEFINE_SHARED_CONSTANTS();\n";
    out += "};\n\n";

    out += "#endif\n";

    for (const std::string& samplerName : samplerConstantNames)
    {
        println("#define {}_Texture1DDescriptorIndex {}_Texture2DDescriptorIndex",
            samplerName, samplerName);
    }

    for (uint32_t samplerIndex = 0; samplerIndex < 16; samplerIndex++)
    {
        println("#define s{}_Texture1DDescriptorIndex s{}_Texture2DDescriptorIndex",
            samplerIndex, samplerIndex);
    }

    for (uint32_t samplerIndex = 16; samplerIndex < 32; samplerIndex++)
    {
        uint32_t aliasedSamplerIndex = samplerIndex & 15;
        println("#define s{}_Texture1DDescriptorIndex s{}_Texture2DDescriptorIndex",
            samplerIndex, aliasedSamplerIndex);
        for (size_t j = 0; j < std::size(TEXTURE_DIMENSIONS); j++)
        {
            println("#define s{}_Texture{}DescriptorIndex s{}_Texture{}DescriptorIndex",
                samplerIndex, TEXTURE_DIMENSIONS[j], aliasedSamplerIndex, TEXTURE_DIMENSIONS[j]);
        }

        println("#define s{}_SamplerDescriptorIndex s{}_SamplerDescriptorIndex",
            samplerIndex, aliasedSamplerIndex);
    }

    for (uint32_t i = 0; i < constantTableContainer->constantTable.constants; i++)
    {
        const auto constantInfo = reinterpret_cast<const ConstantInfo*>(
            constantTableData + constantTableContainer->constantTable.constantInfo + i * sizeof(ConstantInfo));

        if (constantInfo->registerSet == RegisterSet::Bool)
        {
            const char* constantName = reinterpret_cast<const char*>(constantTableData + constantInfo->name);
            const uint32_t address = constantInfo->registerIndex + (isPixelShader ? 128u : 0u);
            println("#define {} BOOL_BIT({})", constantName, address);
            boolConstants.emplace(address, constantName);
        }
    }

    out += '\n';

    println("struct {}", isPixelShader ? "Interpolators" : "VertexShaderInput");
    out += "{\n";

    if (isPixelShader)
    {
        out += "#ifdef __air__\n";

        out += "\tfloat4 iPos [[position]];\n";

        for (auto& [usage, usageIndex] : INTERPOLATORS)
            println("\tfloat4 i{0}{1} [[user({2}{1})]];", USAGE_VARIABLES[uint32_t(usage)], usageIndex, USAGE_SEMANTICS[uint32_t(usage)]);

        out += "#else\n";

        out += "\tfloat4 iPos : SV_Position;\n";

        for (auto& [usage, usageIndex] : INTERPOLATORS)
            println("\tfloat4 i{0}{1} : {2}{1};", USAGE_VARIABLES[uint32_t(usage)], usageIndex, USAGE_SEMANTICS[uint32_t(usage)]);

        out += "#endif\n";
    }
    else
    {
        auto vertexShader = reinterpret_cast<const VertexShader*>(shader);
        bool emittedAirVertexInputs[std::size(USAGE_TYPES)][16]{};
        bool emittedHlslVertexInputs[std::size(USAGE_TYPES)][16]{};
        out += "#ifdef __air__\n";

        for (uint32_t i = 0; i < vertexShader->vertexElementCount; i++)
        {
            union
            {
                VertexElement vertexElement;
                uint32_t value;
            };

            value = vertexShader->vertexElementsAndInterpolators[vertexShader->field18 + i];

            if (uint32_t(vertexElement.usage) >= std::size(USAGE_TYPES))
            {
                vertexElements.emplace(uint32_t(vertexElement.address), vertexElement);
                continue;
            }

            const char* usageType = USAGE_TYPES[uint32_t(vertexElement.usage)];
            const uint32_t usageIndex = uint32_t(vertexElement.usageIndex);
            const auto usageLocation = findUsageLocation(vertexElement.usage, usageIndex);
            if (usageLocation == nullptr)
            {
                vertexElements.emplace(uint32_t(vertexElement.address), vertexElement);
                continue;
            }

            if (emittedAirVertexInputs[uint32_t(vertexElement.usage)][usageIndex])
            {
                vertexElements.emplace(uint32_t(vertexElement.address), vertexElement);
                continue;
            }
            emittedAirVertexInputs[uint32_t(vertexElement.usage)][usageIndex] = true;

            bool useUintInput = isPosition0(vertexElement.usage, usageIndex);
#ifdef UNLEASHED_RECOMP
            useUintInput = useUintInput ||
                (vertexElement.usage == DeclUsage::TexCoord && vertexElement.usageIndex == 2 && isMetaInstancer) ||
                (vertexElement.usage == DeclUsage::Position && vertexElement.usageIndex == 1);
#endif
            if (useUintInput)
            {
                usageType = "uint4";
            }

            out += '\t';

            print("{0} i{1}{2}", usageType, USAGE_VARIABLES[uint32_t(vertexElement.usage)],
                usageIndex);

            println(" [[attribute({})]];", usageLocation->location);

            vertexElements.emplace(uint32_t(vertexElement.address), vertexElement);
        }

        out += "#else\n";

        for (uint32_t i = 0; i < vertexShader->vertexElementCount; i++)
        {
            union
            {
                VertexElement vertexElement;
                uint32_t value;
            };

            value = vertexShader->vertexElementsAndInterpolators[vertexShader->field18 + i];

            if (uint32_t(vertexElement.usage) >= std::size(USAGE_TYPES))
            {
                continue;
            }

            const char* usageType = USAGE_TYPES[uint32_t(vertexElement.usage)];
            const uint32_t usageIndex = uint32_t(vertexElement.usageIndex);
            const auto usageLocation = findUsageLocation(vertexElement.usage, usageIndex);
            if (usageLocation == nullptr)
            {
                continue;
            }

            if (emittedHlslVertexInputs[uint32_t(vertexElement.usage)][usageIndex])
            {
                continue;
            }
            emittedHlslVertexInputs[uint32_t(vertexElement.usage)][usageIndex] = true;

            bool useUintInput = isPosition0(vertexElement.usage, usageIndex);
#ifdef UNLEASHED_RECOMP
            useUintInput = useUintInput ||
                (vertexElement.usage == DeclUsage::TexCoord && vertexElement.usageIndex == 2 && isMetaInstancer) ||
                (vertexElement.usage == DeclUsage::Position && vertexElement.usageIndex == 1);
#endif
            if (useUintInput)
            {
                usageType = "uint4";
            }

            out += '\t';

            print("[[vk::location({})]] ", usageLocation->location);

            println("{0} i{1}{2} : {3}{2};", usageType, USAGE_VARIABLES[uint32_t(vertexElement.usage)],
                usageIndex, USAGE_SEMANTICS[uint32_t(vertexElement.usage)]);
        }

        out += "#endif\n";
    }

    out += "};\n";

    println("struct {}", isPixelShader ? "PixelShaderOutput" : "Interpolators");
    out += "{\n";

    if (isPixelShader)
    {
        out += "#ifdef __air__\n";

        auto pixelShader = reinterpret_cast<const PixelShader*>(shader);
        if (pixelShader->outputs & PIXEL_SHADER_OUTPUT_COLOR0)
            out += "\tfloat4 oC0 [[color(0)]];\n";
        if (pixelShader->outputs & PIXEL_SHADER_OUTPUT_COLOR1)
            out += "\tfloat4 oC1 [[color(1)]];\n";
        if (pixelShader->outputs & PIXEL_SHADER_OUTPUT_COLOR2)
            out += "\tfloat4 oC2 [[color(2)]];\n";
        if (pixelShader->outputs & PIXEL_SHADER_OUTPUT_COLOR3)
            out += "\tfloat4 oC3 [[color(3)]];\n";
        if (pixelShader->outputs & PIXEL_SHADER_OUTPUT_DEPTH)
            out += "\tfloat oDepth [[depth(any)]];\n";

        out += "#else\n";

        if (pixelShader->outputs & PIXEL_SHADER_OUTPUT_COLOR0)
            out += "\tfloat4 oC0 : SV_Target0;\n";
        if (pixelShader->outputs & PIXEL_SHADER_OUTPUT_COLOR1)
            out += "\tfloat4 oC1 : SV_Target1;\n";
        if (pixelShader->outputs & PIXEL_SHADER_OUTPUT_COLOR2)
            out += "\tfloat4 oC2 : SV_Target2;\n";
        if (pixelShader->outputs & PIXEL_SHADER_OUTPUT_COLOR3)
            out += "\tfloat4 oC3 : SV_Target3;\n";
        if (pixelShader->outputs & PIXEL_SHADER_OUTPUT_DEPTH)
            out += "\tfloat oDepth : SV_Depth;\n";

        out += "#endif\n";
    }
    else
    {
        out += "#ifdef __air__\n";

        out += "\tfloat4 oPos [[position]] [[invariant]];\n";

        for (auto& [usage, usageIndex] : INTERPOLATORS)
            print("\tfloat4 o{0}{1} [[user({2}{1})]];\n", USAGE_VARIABLES[uint32_t(usage)], usageIndex, USAGE_SEMANTICS[uint32_t(usage)]);

        out += "\tfloat clipDistance [[clip_distance]];\n";

        out += "#else\n";

        out += "\tprecise float4 oPos : SV_Position;\n";

        for (auto& [usage, usageIndex] : INTERPOLATORS)
            print("\tfloat4 o{0}{1} : {2}{1};\n", USAGE_VARIABLES[uint32_t(usage)], usageIndex, USAGE_SEMANTICS[uint32_t(usage)]);

        out += "\tfloat clipDistance : SV_ClipDistance;\n";

        out += "#endif\n";
    }

    out += "};\n";

    out += "#ifdef __air__\n";

    if (isPixelShader)
        out += "[[fragment]]\n";
    else
        out += "[[vertex]]\n";

    out += "#elif !defined(__spirv__)\n";

    if (isPixelShader)
        out += "[shader(\"pixel\")]\n";
    else
        out += "[shader(\"vertex\")]\n";

    out += "#endif\n";

    println("{} shaderMain(", isPixelShader ? "PixelShaderOutput" : "Interpolators");

    if (isPixelShader)
    {
        out += "#ifdef __air__\n";

        out += "\tInterpolators input [[stage_in]],\n";
        out += "\tbool iFace [[front_facing]],\n";

        out += "\tconstant Texture2DDescriptorHeap* g_Texture2DDescriptorHeap [[buffer(0)]],\n";
        out += "\tconstant Texture2DArrayDescriptorHeap* g_Texture2DArrayDescriptorHeap [[buffer(1)]],\n";
        out += "\tconstant TextureCubeDescriptorHeap* g_TextureCubeDescriptorHeap [[buffer(2)]],\n";
        out += "\tconstant SamplerDescriptorHeap* g_SamplerDescriptorHeap [[buffer(3)]],\n";
#ifdef MARATHON_RECOMP
        out += "\tdevice AtomicUintBuffer* g_ConditionalSurveyBuffer [[buffer(4)]],\n";
#endif
        out += "\tconstant PushConstants& g_PushConstants [[buffer(8)]]\n";

        out += "#else\n";

        out += "\tInterpolators input,\n";

        out += "#ifdef __spirv__\n";
        out += "\tin bool iFace : SV_IsFrontFace\n";
        out += "#else\n";
        out += "\tin uint iFace : SV_IsFrontFace\n";
        out += "#endif\n";

        out += "\n#endif\n";
    }
    else
    {
        out += "#ifdef __air__\n";
        out += "\tconstant PushConstants& g_PushConstants [[buffer(8)]],\n";
        out += "\tVertexShaderInput input [[stage_in]]\n";
        out += "#else\n";
        out += "\tVertexShaderInput input\n";
        out += "#endif\n";

    #ifdef UNLEASHED_RECOMP
        if (hasIndexCount)
        {
            out += "\t,\n";
            out += "#ifdef __air__\n";
            out += "\tuint iVertexId [[vertex_id]],\n";
            out += "\tuint iInstanceId [[instance_id]]\n";
            out += "#else\n";
            out += "\tin uint iVertexId : SV_VertexID,\n";
            out += "\tin uint iInstanceId : SV_InstanceID\n";
            out += "#endif\n";
        }
    #endif
    }

    out += ")\n";
    out += "{\n";

    std::string outputName = isPixelShader ? "PixelShaderOutput" : "Interpolators";

    out += "#ifdef __air__\n";
    println("\t{0} output = {0}{{}};", outputName);
    out += "#else\n";
    println("\t{0} output = ({0})0;", outputName);
    out += "#endif\n";

#ifdef UNLEASHED_RECOMP
    if (hasMtxProjection)
    {
        specConstantsMask |= SPEC_CONSTANT_REVERSE_Z;

        out += "\toutput.oPos = 0.0;\n";

        out += "\tfloat4x4 mtxProjection = float4x4(g_MtxProjection(0), g_MtxProjection(1), g_MtxProjection(2), g_MtxProjection(3));\n";
        out += "\tfloat4x4 mtxProjectionReverseZ = mul(mtxProjection, float4x4(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, -1, 0, 0, 0, 1, 1));\n";

        out += "\tUNROLL for (int iterationIndex = 0; iterationIndex < 2; iterationIndex++)\n";
        out += "\t{\n";
    }
#endif

    bool printedIntConstants[32]{};

    if (shaderContainer->definitionTableOffset != NULL)
    {
        auto definitionTable = reinterpret_cast<const DefinitionTable*>(shaderData + shaderContainer->definitionTableOffset);
        auto definitions = definitionTable->definitions;
        while (*definitions != 0)
        {
            auto definition = reinterpret_cast<const Float4Definition*>(definitions);
            auto value = reinterpret_cast<const be<uint32_t>*>(shaderData + shaderContainer->virtualSize + definition->physicalOffset);
            for (uint16_t i = 0; i < (definition->count + 3) / 4; i++)
            {
                println("#ifdef __air__");
                println("\tfloat4 c{} = as_type<float4>(uint4(0x{:X}, 0x{:X}, 0x{:X}, 0x{:X}));",
                    definition->registerIndex + i - (isPixelShader ? 256 : 0), value[0].get(), value[1].get(), value[2].get(), value[3].get());
                println("#else");
                println("\tfloat4 c{} = asfloat(uint4(0x{:X}, 0x{:X}, 0x{:X}, 0x{:X}));",
                    definition->registerIndex + i - (isPixelShader ? 256 : 0), value[0].get(), value[1].get(), value[2].get(), value[3].get());
                println("#endif");

                // Record this register so direct (non-relative) constant reads resolve to the
                // local `c{N}` default instead of the zero runtime constant buffer.
                float4Definitions.emplace(definition->registerIndex + i - (isPixelShader ? 256 : 0));

                value += 4;
            }
            definitions += 2;
        }
        ++definitions;
        while (*definitions != 0)
        {
            auto definition = reinterpret_cast<const Int4Definition*>(definitions);
            for (uint16_t i = 0; i < definition->count; i++)
            {
                union
                {
                    uint32_t value;
                    struct
                    {
                        int8_t x;
                        int8_t y;
                        int8_t z;
                        int8_t w;
                    };
                };

                value = definition->values[i].get();

                println("\tint4 i{} = int4({}, {}, {}, {});",
                    (definition->registerIndex - 8992) / 4 + i, x, y, z, w);
                const uint32_t intRegister = (definition->registerIndex - 8992) / 4 + i;
                if (intRegister < std::size(printedIntConstants))
                    printedIntConstants[intRegister] = true;
            }
            definitions += 2;
            definitions += definition->count;
        }

        out += "\n";
    }

    for (uint32_t i = 0; i < std::size(printedIntConstants); i++)
    {
        if (!printedIntConstants[i])
            println("\tint4 i{} = int4(1, 0, 0, 0);", i);
    }

    out += "\n";

    bool printedRegisters[64]{};

    uint32_t interpolatorCount = (shader->interpolatorInfo >> 5) & 0x1F;

    for (uint32_t i = 0; i < interpolatorCount; i++)
    {
        union
        {
            Interpolator interpolator;
            uint32_t value;
        };
    
        if (isPixelShader)
        {
            value = reinterpret_cast<const PixelShader*>(shader)->interpolators[i];
            println("\tfloat4 r{} = input.i{}{};", uint32_t(interpolator.reg), USAGE_VARIABLES[uint32_t(interpolator.usage)], uint32_t(interpolator.usageIndex));
            printedRegisters[interpolator.reg] = true;
        }
        else
        {
            auto vertexShader = reinterpret_cast<const VertexShader*>(shader);
            value = vertexShader->vertexElementsAndInterpolators[vertexShader->field18 + vertexShader->vertexElementCount + i];
            interpolators.emplace(i, fmt::format("output.o{}{}", USAGE_VARIABLES[uint32_t(interpolator.usage)], uint32_t(interpolator.usageIndex)));
        }
    }

    if (!isPixelShader)
    {
    #ifdef UNLEASHED_RECOMP
        if (!hasMtxProjection)
            out += "\toutput.oPos = 0.0;\n";
    #endif

        for (auto& [usage, usageIndex] : INTERPOLATORS)
            println("\toutput.o{}{} = 0.0;", USAGE_VARIABLES[uint32_t(usage)], usageIndex);

        out += "\n";
    }

    for (size_t i = 0; i < std::size(printedRegisters); i++)
    {
        if (!printedRegisters[i])
        {
            print("\tfloat4 r{} = ", i);
            if (isPixelShader && i == ((shader->fieldC >> 8) & 0xFF))
            {
                out += "float4((input.iPos.xy - 0.5) * float2(iFace ? 1.0 : -1.0, 1.0), 0.0, 0.0);\n";
            }
        #ifdef UNLEASHED_RECOMP
            else if (!isPixelShader && hasIndexCount && i == 0)
            {
                out += "float4(iVertexId + g_IndexCount.x * iInstanceId, 0.0, 0.0, 0.0);\n";
            }
        #endif
            else
            {
                out += "0.0;\n";
            }
        }
    }

    out += "\tint a0 = 0;\n";
    out += "\tint aL = 0;\n";
    out += "\tbool p0 = false;\n";
    out += "\tfloat ps = 0.0;\n";
    out += "\tfloat textureLod = 0.0;\n";
    if (trace)
        fmt::println(stderr, "trace: declarations emitted");
    if (isPixelShader)
    {
#ifdef UNLEASHED_RECOMP
        out += "\tfloat2 pixelCoord = 0.0;\n";
#endif
#ifdef MARATHON_RECOMP
        specConstantsMask |= SPEC_CONSTANT_CONDITIONAL_RENDERING;

        out += "\tBRANCH if ((g_SpecConstants() & SPEC_CONSTANT_CONDITIONAL_RENDERING))\n";
        out += "\t{\n";

        out += "\t\tuint sampleCount = atomicLoadUint(g_ConditionalSurveyBuffer, g_conditionalSurveyIndex);\n";
        out += "\t\tBRANCH if (sampleCount == 0)\n";
        out += "\t\t{\n";

        println("#ifdef __air__");
        println("\t\t\tdiscard_fragment();");
        println("#else");
        println("\t\t\tdiscard;");
        println("#endif");

        out += "\t\t}\n";

        out += "\t}\n";
#endif
    }

    union
    {
        ControlFlowInstruction controlFlow[2];
        struct
        {
            uint32_t code0;
            uint32_t code1;
            uint32_t code2;
            uint32_t code3;
        };
    };

    auto controlFlowCode = code;
    uint32_t instrAddress = 0;
    uint32_t instrSize = shader->size;
    bool simpleControlFlow = true;

    enum class CfKind : uint8_t { None, Cond, Uncond, LoopStart, LoopEnd };
    struct CfSummary
    {
        CfKind kind = CfKind::None;
        uint32_t target = 0;
    };
    std::vector<CfSummary> cfSummaries;
    std::vector<uint32_t> loopStarts;
    std::unordered_map<uint32_t, uint32_t> loopEndFor;
    uint32_t cfIndex = 0;

    while (instrAddress < instrSize)
    {
        code0 = controlFlowCode[0];
        code1 = controlFlowCode[1] & 0xFFFF;
        code2 = (controlFlowCode[1] >> 16) | (controlFlowCode[2] << 16);
        code3 = controlFlowCode[2] >> 16;

        for (auto& cfInstr : controlFlow)
        {
            uint32_t address = 0;

            switch (cfInstr.opcode)
            {
            case ControlFlowOpcode::Nop:
            case ControlFlowOpcode::Alloc:
            case ControlFlowOpcode::CondCall:
            case ControlFlowOpcode::Return:
            case ControlFlowOpcode::MarkVsFetchDone:
                break;

            case ControlFlowOpcode::Exec:
            case ControlFlowOpcode::ExecEnd:
                address = cfInstr.exec.address;
                break;

            case ControlFlowOpcode::CondExec:
            case ControlFlowOpcode::CondExecEnd:
            case ControlFlowOpcode::CondExecPredClean:
            case ControlFlowOpcode::CondExecPredCleanEnd:
                address = cfInstr.condExec.address;
                break;

            case ControlFlowOpcode::CondExecPred:
            case ControlFlowOpcode::CondExecPredEnd:
                address = cfInstr.condExecPred.address;
                break;

            case ControlFlowOpcode::LoopStart:
                loopStarts.push_back(cfIndex);
                cfSummaries.push_back({ CfKind::LoopStart, 0 });
                break;

            case ControlFlowOpcode::LoopEnd:
                if (loopStarts.empty())
                    simpleControlFlow = false;
                else
                {
                    loopEndFor[loopStarts.back()] = cfIndex;
                    loopStarts.pop_back();
                }
                cfSummaries.push_back({ CfKind::LoopEnd, 0 });
                break;

            case ControlFlowOpcode::CondJmp:
            {
                if (cfInstr.condJmp.direction || cfInstr.condJmp.address <= cfIndex)
                    simpleControlFlow = false;

                cfSummaries.push_back({
                    cfInstr.condJmp.isUnconditional ? CfKind::Uncond : CfKind::Cond,
                    uint32_t(cfInstr.condJmp.address) });

                break;
            }
            }

            if (cfSummaries.size() == cfIndex)
                cfSummaries.push_back({});

            if (address != 0)
                instrSize = std::min<uint32_t>(instrSize, address * 12);

            ++cfIndex;
        }

        controlFlowCode += 3;
        instrAddress += 12;
    }

    if (!loopStarts.empty())
        simpleControlFlow = false;

    if (simpleControlFlow)
    {
        const uint32_t instrCount = (instrSize / 12) * 2;
        if (cfSummaries.size() > instrCount)
            cfSummaries.resize(instrCount);

        std::function<bool(uint32_t, uint32_t, uint32_t)> structure =
            [&](uint32_t lo, uint32_t hi, uint32_t cont) -> bool
        {
            uint32_t i = lo;
            while (i < hi)
            {
                const CfSummary& summary = cfSummaries[i];
                switch (summary.kind)
                {
                case CfKind::Uncond:
                    if (i + 1 == hi && (summary.target == cont || summary.target == hi))
                        i = hi;
                    else
                        return false;
                    break;

                case CfKind::Cond:
                {
                    const uint32_t target = summary.target;
                    if (target > hi)
                    {
                        if (target != cont)
                            return false;
                        ++ifEndLabels[hi];
                        if (!structure(i + 1, hi, cont))
                            return false;
                        i = hi;
                    }
                    else if (target < hi && target - 1 > i &&
                        cfSummaries[target - 1].kind == CfKind::Uncond &&
                        cfSummaries[target - 1].target != target)
                    {
                        const uint32_t merge = cfSummaries[target - 1].target;
                        if (merge == cont || merge == hi)
                        {
                            elseLabels.insert(target);
                            ++ifEndLabels[hi];
                            if (!structure(i + 1, target - 1, merge == cont ? cont : hi) ||
                                !structure(target, hi, cont))
                                return false;
                            i = hi;
                        }
                        else if (merge < hi)
                        {
                            elseLabels.insert(target);
                            ++ifEndLabels[merge];
                            if (!structure(i + 1, target - 1, merge) ||
                                !structure(target, merge, merge))
                                return false;
                            i = merge;
                        }
                        else
                        {
                            return false;
                        }
                    }
                    else
                    {
                        ++ifEndLabels[target];
                        if (!structure(i + 1, target, target == hi ? cont : target))
                            return false;
                        i = target;
                    }
                    break;
                }

                case CfKind::LoopStart:
                {
                    auto loopEnd = loopEndFor.find(i);
                    if (loopEnd == loopEndFor.end() || loopEnd->second >= hi ||
                        !structure(i + 1, loopEnd->second, loopEnd->second))
                        return false;
                    i = loopEnd->second + 1;
                    break;
                }

                case CfKind::LoopEnd:
                    return false;

                default:
                    ++i;
                    break;
                }
            }
            return true;
        };

        simpleControlFlow = structure(0, instrCount, instrCount);
    }

    if (!simpleControlFlow)
    {
        ifEndLabels.clear();
        elseLabels.clear();
    }

    if (trace)
        fmt::println(stderr, "trace: control-flow scan done simple={} instr_size={}", simpleControlFlow, instrSize);

    if (simpleControlFlow)
    {
        out += '\n';
        indentation = 1;
    }
    else
    {
        out += "\n\tuint pc = 0;\n";
        out += "\tuint loopDepth = 0;\n";
        out += "\tuint4 loopIterator = 0;\n";
        out += "\tuint4 loopCount = 0;\n";
        out += "\tint4 loopStartAddress = 0;\n";
        out += "\tint4 loopStep = 0;\n";
        out += "\tuint loopConstant = 0;\n";
        out += "\tuint loopIndex = 0;\n";
        out += "\twhile (true)\n";
        out += "\t{\n";
        out += "\t\tswitch (pc)\n";
        out += "\t\t{\n";
    }

    controlFlowCode = code;
    instrAddress = 0;
    uint32_t pc = 0;
    uint32_t emittedInstructionCount = 0;
    uint32_t loopDepth = 0;
    std::array<uint32_t, 4> loopIds{};

    while (instrAddress < instrSize)
    {
        code0 = controlFlowCode[0];
        code1 = controlFlowCode[1] & 0xFFFF;
        code2 = (controlFlowCode[1] >> 16) | (controlFlowCode[2] << 16);
        code3 = controlFlowCode[2] >> 16;

        for (auto& cfInstr : controlFlow)
        {
            if (trace)
                fmt::println(stderr, "trace: cf top pc={} opcode={}", pc, uint32_t(cfInstr.opcode));
            if (!simpleControlFlow)
            {
                indentation = 3;
                println("\t\tcase {}:", pc);
            }
            else
            {
                auto findResult = ifEndLabels.find(pc);
                if (findResult != ifEndLabels.end())
                {
                    for (uint32_t i = 0; i < findResult->second; i++)
                    {
                        if (indentation > 1)
                        {
                            --indentation;
                            indent();
                            out += "}\n";
                        }
                    }
                }
                if (elseLabels.count(pc) != 0)
                {
                    --indentation;
                    indent();
                    out += "}\n";
                    indent();
                    out += "else\n";
                    indent();
                    out += "{\n";
                    ++indentation;
                }
            }

            ++pc;

            uint32_t address = 0;
            uint32_t count = 0;
            uint32_t sequence = 0;
            bool shouldReturn = false;
            bool condExecGuard = false;

            switch (cfInstr.opcode)
            {
            case ControlFlowOpcode::Nop:
            case ControlFlowOpcode::Alloc:
            case ControlFlowOpcode::CondCall:
            case ControlFlowOpcode::MarkVsFetchDone:
                continue;

            case ControlFlowOpcode::Return:
                shouldReturn = true;
                break;

            case ControlFlowOpcode::Exec:
            case ControlFlowOpcode::ExecEnd:
                address = cfInstr.exec.address;
                count = cfInstr.exec.count;
                sequence = cfInstr.exec.sequence;
                shouldReturn = (cfInstr.opcode == ControlFlowOpcode::ExecEnd);
                break;

            case ControlFlowOpcode::CondExec:
            case ControlFlowOpcode::CondExecEnd:
            case ControlFlowOpcode::CondExecPredClean:
            case ControlFlowOpcode::CondExecPredCleanEnd:
            {
                address = cfInstr.condExec.address;
                count = cfInstr.condExec.count;
                sequence = cfInstr.condExec.sequence;
                shouldReturn = (cfInstr.opcode == ControlFlowOpcode::CondExecEnd ||
                                cfInstr.opcode == ControlFlowOpcode::CondExecPredCleanEnd);
                // CondExec addresses the unified VS 0..127 / PS 128..255 Xenos
                // boolean file. Keep the raw address visible in generated HLSL.
                indent();
                println("if ({}BOOL_BIT({}))", cfInstr.condExec.condition ? "" : "!",
                    uint32_t(cfInstr.condExec.boolAddress));
                indent();
                out += "{\n";
                ++indentation;
                condExecGuard = true;
                break;
            }

            case ControlFlowOpcode::CondExecPred:
            case ControlFlowOpcode::CondExecPredEnd:
                address = cfInstr.condExecPred.address;
                count = cfInstr.condExecPred.count;
                sequence = cfInstr.condExecPred.sequence;
                shouldReturn = (cfInstr.opcode == ControlFlowOpcode::CondExecPredEnd);
                break;

            case ControlFlowOpcode::LoopStart:
                if (simpleControlFlow)
                {
                    if (loopDepth >= loopIds.size())
                        throw std::runtime_error("shader loop nesting exceeds four levels");
                    const uint32_t depth = loopDepth++;
                    loopIds[depth] = cfInstr.loopStart.loopId;
                    indent();
                    println("uint loopConstant{} = g_LoopConstant({});", depth,
                        uint32_t(cfInstr.loopStart.loopId));
                    indent();
                    println("uint loopCount{} = loopConstant{} & 0xFFu;", depth, depth);
                    indent();
                    println("int loopStart{} = int((loopConstant{} >> 8) & 0xFFu);", depth, depth);
                    indent();
                    println("int loopStep{} = int(loopConstant{} << 8) >> 24;", depth, depth);
                    indent();
                    println("for (uint loopIterator{} = 0; loopIterator{} < loopCount{}; ++loopIterator{})",
                        depth, depth, depth, depth);
                    indent();
                    out += "{\n";
                    ++indentation;
                    indent();
                    println("int loopAddress{} = int(loopIterator{}) * loopStep{} + loopStart{};",
                        depth, depth, depth, depth);
                    indent();
                    println("aL = (loopAddress{} << 22) >> 22;", depth);
                }
                else
                {
                    indent();
                    println("loopConstant = g_LoopConstant({});", uint32_t(cfInstr.loopStart.loopId));
                    indent();
                    out += "loopIterator[loopDepth] = 0;\n";
                    indent();
                    out += "loopCount[loopDepth] = loopConstant & 0xFFu;\n";
                    indent();
                    out += "loopStartAddress[loopDepth] = int((loopConstant >> 8) & 0xFFu);\n";
                    indent();
                    out += "loopStep[loopDepth] = int(loopConstant << 8) >> 24;\n";
                    indent();
                    out += "aL = (loopStartAddress[loopDepth] << 22) >> 22;\n";
                    indent();
                    out += "++loopDepth;\n";
                }
                break;

            case ControlFlowOpcode::LoopEnd:
                if (simpleControlFlow)
                {
                    if (loopDepth == 0)
                        throw std::runtime_error("loop end has no matching loop start");
                    const uint32_t depth = --loopDepth;
                    if (loopIds[depth] != cfInstr.loopEnd.loopId)
                        throw std::runtime_error("loop start/end IDs do not match");
                    if (cfInstr.loopEnd.isPredicatedBreak)
                    {
                        indent();
                        println("if ({}p0)", cfInstr.loopEnd.condition ? "" : "!");
                        ++indentation;
                        indent();
                        out += "break;\n";
                        --indentation;
                    }
                    if (indentation > 1)
                    {
                        --indentation;
                        indent();
                        out += "}\n";
                    }
                }
                else
                {
                    indent();
                    out += "loopIndex = loopDepth - 1u;\n";
                    if (cfInstr.loopEnd.isPredicatedBreak)
                    {
                        indent();
                        println("if ({}p0)", cfInstr.loopEnd.condition ? "" : "!");
                        indent();
                        out += "{\n";
                        ++indentation;
                        indent();
                        out += "--loopDepth;\n";
                        --indentation;
                        indent();
                        out += "}\n";
                        indent();
                        out += "else\n";
                        indent();
                        out += "{\n";
                        ++indentation;
                    }
                    indent();
                    out += "++loopIterator[loopIndex];\n";
                    indent();
                    out += "if (loopIterator[loopIndex] < loopCount[loopIndex])\n";
                    indent();
                    out += "{\n";
                    ++indentation;
                    indent();
                    out += "aL = ((int(loopIterator[loopIndex]) * loopStep[loopIndex] + loopStartAddress[loopIndex]) << 22) >> 22;\n";
                    indent();
                    println("pc = {};", uint32_t(cfInstr.loopEnd.address));
                    indent();
                    out += "continue;\n";
                    --indentation;
                    indent();
                    out += "}\n";
                    indent();
                    out += "--loopDepth;\n";
                    if (cfInstr.loopEnd.isPredicatedBreak)
                    {
                        --indentation;
                        indent();
                        out += "}\n";
                    }
                }
                break;

            case ControlFlowOpcode::CondJmp:
            {
                if (cfInstr.condJmp.isUnconditional)
                {
                    if (!simpleControlFlow)
                    {
                        println("\t\t\tpc = {};", uint32_t(cfInstr.condJmp.address));
                        out += "\t\t\tcontinue;\n";
                    }
                }
                else
                {
                    indent();
                    if (cfInstr.condJmp.isPredicated)
                    {
                        println("if ({}p0)", cfInstr.condJmp.condition ^ simpleControlFlow ? "" : "!");
                    }
                    else
                    {
                        const bool jumpIfSet = cfInstr.condJmp.condition ^ simpleControlFlow;
                        println("if ({}BOOL_BIT({}))", jumpIfSet ? "" : "!",
                            uint32_t(cfInstr.condJmp.boolAddress));
                    }

                    if (simpleControlFlow)
                    {
                        indent();
                        out += "{\n";
                        ++indentation;
                    }
                    else
                    {
                        out += "\t\t\t{\n";
                        println("\t\t\t\tpc = {};", uint32_t(cfInstr.condJmp.address));
                        out += "\t\t\t\tcontinue;\n";
                        out += "\t\t\t}\n";
                    }
                }
                break;
            }
            }

            if (trace)
                fmt::println(stderr, "trace: cf switch done pc={} address={} count={} indentation={}",
                    pc - 1, address, count, indentation);
            auto instructionCode = code + address * 3;
            if (address * 12 > shader->size || count > (shader->size - address * 12) / 12)
            {
                // bug-139: don't leak an opened CondExec guard brace on the skip path.
                if (condExecGuard)
                {
                    --indentation;
                    indent();
                    out += "}\n";
                }
                continue;
            }
            if (trace)
                fmt::println(stderr, "trace: emit cf pc={} opcode={} address={} count={} sequence={:08X}",
                    pc - 1, uint32_t(cfInstr.opcode), address, count, sequence);
            
            for (uint32_t i = 0; i < count; i++)
            {
                union
                {
                    VertexFetchInstruction vertexFetch;
                    TextureFetchInstruction textureFetch;
                    AluInstruction alu;
                    struct
                    {
                        uint32_t code0;
                        uint32_t code1;
                        uint32_t code2;
                    };
                };
            
                code0 = instructionCode[0];
                code1 = instructionCode[1];
                code2 = instructionCode[2];
            
                if ((sequence & 0x1) != 0)
                {
                    if (trace)
                        fmt::println(stderr, "trace: emit fetch address={} i={} opcode={} sequence={:08X}",
                            address, i, uint32_t(vertexFetch.opcode), sequence);
                    if (vertexFetch.opcode == FetchOpcode::VertexFetch)
                    {
                        recompile(vertexFetch, address + i);
                    }
                    else if (textureFetch.opcode == FetchOpcode::SetTextureLod)
                    {
                        if (textureFetch.isPredicated)
                        {
                            indent();
                            println("if ({}p0)", textureFetch.predCondition ? "" : "!");
                            indent();
                            out += "{\n";
                            ++indentation;
                        }

                        indent();
                        print("textureLod = r{}.", uint32_t(textureFetch.srcRegister));
                        out += SWIZZLES[textureFetch.srcSwizzle & 0x3];
                        out += ";\n";

                        if (textureFetch.isPredicated)
                        {
                            --indentation;
                            indent();
                            out += "}\n";
                        }
                    }
                    else
                    {
                    #ifdef UNLEASHED_RECOMP
                        if (textureFetch.constIndex == 10) // g_GISampler
                        {
                            specConstantsMask |= SPEC_CONSTANT_BICUBIC_GI_FILTER;

                            indent();
                            out += "if (g_SpecConstants() & SPEC_CONSTANT_BICUBIC_GI_FILTER)\n";
                            indent();
                            out += "{\n";

                            ++indentation;
                            recompile(textureFetch, true);
                            --indentation;

                            indent();
                            out += "}\n";
                            indent();
                            out += "else\n";
                            indent();
                            out += "{\n";

                            ++indentation;
                            recompile(textureFetch, false);
                            --indentation;

                            indent();
                            out += "}\n";
                        }
                        else
                    #endif
                        {
                            recompile(textureFetch, false);
                        }
                    }
                }
                else
                {
                    if (trace)
                        fmt::println(stderr, "trace: emit alu address={} i={} vector={} scalar={} sequence={:08X}",
                            address, i, uint32_t(alu.vectorOpcode), uint32_t(alu.scalarOpcode), sequence);
                    recompile(alu);
                }
            
                sequence >>= 2;
                instructionCode += 3;
                ++emittedInstructionCount;
                if (emittedInstructionCount > 1024 || out.size() > 96 * 1024)
                    throw std::runtime_error("shader generated too much HLSL");
            }

            // bug-139: close the CondExec boolean guard. The end-of-shader tail
            // (alpha test / half-pixel / return) below runs UNCONDITIONALLY on
            // hardware even for CondExecEnd, so it stays outside the guard.
            if (condExecGuard)
            {
                --indentation;
                indent();
                out += "}\n";
            }

            if (shouldReturn)
            {
                if (isPixelShader)
                {
                    specConstantsMask |= SPEC_CONSTANT_ALPHA_TEST;

                    indent();
                    out += "BRANCH if (g_SpecConstants() & SPEC_CONSTANT_ALPHA_TEST)\n";
                    indent();
                    out += "{\n";

                    indent();
                    out += "\tclip(output.oC0.w - g_AlphaThreshold);\n";

                    indent();
                    out += "}\n";

                #ifdef UNLEASHED_RECOMP
                    specConstantsMask |= SPEC_CONSTANT_ALPHA_TO_COVERAGE;

                    indent();
                    out += "else if (g_SpecConstants() & SPEC_CONSTANT_ALPHA_TO_COVERAGE)\n";
                    indent();
                    out += "{\n";

                    indent();
                    out += "\toutput.oC0.w *= 1.0 + computeMipLevel(pixelCoord) * 0.25;\n";
                    indent();
                    out += "\toutput.oC0.w = 0.5 + (output.oC0.w - g_AlphaThreshold) / max(fwidth(output.oC0.w), 1e-6);\n";

                    indent();
                    out += "}\n";
                #endif
                }
                else
                {
                    // bug-140 (VTE flags, from the Xenia-edge diff): Xenos honors
                    // PA_CL_VTE_CNTL's vertex-export formats -- the shader may output
                    // pre-divided XY/Z and reciprocal W. Mirror Xenia's tail exactly
                    // (order matters: W is converted first, then XY/Z use the
                    // converted W). FM2 currently programs 0x43F => flags=8 => all
                    // three are no-ops; this makes the semantics faithful for any
                    // pass that switches to screen-space output.
                    out += "\tif ((g_VteFlags & 8u) == 0u) output.oPos.w = 1.0 / output.oPos.w;\n";
                    out += "\tif ((g_VteFlags & 2u) != 0u) output.oPos.xy *= output.oPos.w;\n";
                    out += "\tif ((g_VteFlags & 4u) != 0u) output.oPos.z *= output.oPos.w;\n";
                    out += "\tif (g_ClipPlaneEnabled) output.clipDistance = dot(output.oPos, g_ClipPlane);\n";
                #if defined(UNLEASHED_RECOMP)
                    if (!hasMtxProjection)
                #endif
                    {
                        specConstantsMask |= SPEC_CONSTANT_REVERSE_Z;
                        out += "\tif (g_SpecConstants() & SPEC_CONSTANT_REVERSE_Z) output.oPos.z = output.oPos.w - output.oPos.z;\n";
                    }
                    out += "\toutput.oPos.xy += g_HalfPixelOffset * output.oPos.w;\n";
                }

                if (simpleControlFlow)
                {
                    indent();
                #ifdef UNLEASHED_RECOMP
                    if (hasMtxProjection)
                    {
                        out += "continue;\n";
                    }
                    else
                #endif
                    {
                        out += "return output;\n";
                    }
                }
                else
                {
                    out += "\t\t\tbreak;\n";
                }
            }
        }

        controlFlowCode += 3;
        instrAddress += 12;
    }

    if (trace)
        fmt::println(stderr, "trace: control-flow emit done output_size={}", out.size());

    if (!simpleControlFlow)
    {
        out += "\t\t\tbreak;\n";
        out += "\t\t}\n";
        out += "\t\tbreak;\n";
        out += "\t}\n";
    }

#ifdef UNLEASHED_RECOMP
    if (hasMtxProjection)
        out += "\t}\n";
#endif

    if (simpleControlFlow)
    {
        while (indentation > 1)
        {
            --indentation;
            indent();
            out += "}\n";
        }
    }

    if (!simpleControlFlow)
        out += "\treturn output;\n";
#ifdef UNLEASHED_RECOMP
    else if (hasMtxProjection)
        out += "\treturn output;\n";
#endif
    else
        out += "\treturn output;\n";

    out += "}";
}
