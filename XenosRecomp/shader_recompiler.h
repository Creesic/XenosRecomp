#pragma once

#include "shader.h"
#include "shader_code.h"

struct StringBuffer
{
    std::string out;

    template<class... Args>
    void print(fmt::format_string<Args...> fmt, Args&&... args)
    {
        fmt::vformat_to(std::back_inserter(out), fmt.get(), fmt::make_format_args(args...));
    }

    template<class... Args>
    void println(fmt::format_string<Args...> fmt, Args&&... args)
    {
        fmt::vformat_to(std::back_inserter(out), fmt.get(), fmt::make_format_args(args...));
        out += '\n';
    }
};

struct ShaderRecompiler : StringBuffer
{
    uint32_t indentation = 0;
    bool isPixelShader = false;

    // PGR4: vertex shaders address their textures through a separate 4-slot
    // table appended to SharedConstants (2D @512, 3D @528, cube @544,
    // samplers @560). D3DVERTEXTEXTURESAMPLER0..3 are samplers 16..19 and
    // the microcode fetches them as constants 16..19 (aliased onto s0..s3
    // below), so a vertex-shader sampler n < 4 means vertex slot n.
    uint32_t textureIndexOffset(size_t dim, uint32_t reg) const
    {
        return (!isPixelShader && reg < 4) ? 512 + uint32_t(dim) * 16 + reg * 4
                                           : uint32_t(dim) * 64 + reg * 4;
    }
    uint32_t samplerIndexOffset(uint32_t reg) const
    {
        return (!isPixelShader && reg < 4) ? 560 + reg * 4 : 192 + reg * 4;
    }
    const uint8_t* constantTableData = nullptr;
    std::unordered_map<uint32_t, VertexElement> vertexElements;
    std::unordered_map<uint32_t, std::string> interpolators;
    std::unordered_map<uint32_t, const ConstantInfo*> float4Constants;
    std::unordered_set<uint32_t> float4Definitions;
    std::unordered_map<uint32_t, const char*> boolConstants;
    std::unordered_map<uint32_t, const char*> samplers;
    std::unordered_map<uint32_t, uint32_t> ifEndLabels;
    std::unordered_set<uint32_t> elseLabels;
    uint32_t specConstantsMask = 0;

#ifdef UNLEASHED_RECOMP
    bool hasMtxProjection = false;
    bool hasMtxPrevInvViewProjection = false;
#endif

    void indent()
    {
        for (uint32_t i = 0; i < indentation; i++)
            out += '\t';
    }

    uint32_t printDstSwizzle(uint32_t dstSwizzle, bool operand);
    void printDstSwizzle01(uint32_t dstRegister, uint32_t dstSwizzle);

    void recompile(const VertexFetchInstruction& instr, uint32_t address);
    void recompile(const TextureFetchInstruction& instr, bool bicubic);
    void recompile(const AluInstruction& instr);

    void recompile(const uint8_t* shaderData, const std::string_view& include);
};
