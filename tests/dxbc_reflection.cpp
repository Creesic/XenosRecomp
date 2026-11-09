// Compile translated guest shaders and check the renderer's stage-specific ABI.
#include "dxbc_compiler.h"
#include <d3dcompiler.h>
#include <d3d11shader.h>
#include <d3d12shader.h>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>

static void checkBoundedBindings(const std::string& translated, bool pixelShader)
{
    // Keep the real translated declarations, and force all array kinds live with
    // uniform indices from a constant buffer, matching per-draw descriptor remaps.
    const std::string source = "#define shaderMain translatedShaderMain\n" + translated +
        "\n#undef shaderMain\ncbuffer BindingProbe : register(b7) { uint textureIndex; uint samplerIndex; };\n"
        "float4 shaderMain() : " + (pixelShader ? "SV_Target" : "SV_Position") + " {\n"
        "return g_Texture2DDescriptorHeap[textureIndex].SampleLevel(g_SamplerDescriptorHeap[samplerIndex], float2(0,0), 0) +\n"
        "g_Texture2DArrayDescriptorHeap[textureIndex].SampleLevel(g_SamplerDescriptorHeap[samplerIndex], float3(0,0,0), 0) +\n"
        "g_TextureCubeDescriptorHeap[textureIndex].SampleLevel(g_SamplerDescriptorHeap[samplerIndex], float3(1,0,0), 0); }\n";
    const auto code = DxbcCompiler::compile(source, pixelShader);
    ID3D12ShaderReflection* reflection = nullptr;
    if (FAILED(D3DReflect(code.data(), code.size(), IID_ID3D12ShaderReflection,
            reinterpret_cast<void**>(&reflection))))
        throw std::runtime_error("Bounded DXBC reflection failed");
    D3D12_SHADER_DESC shader{};
    reflection->GetDesc(&shader);
    unsigned textures = 0, samplers = 0;
    for (unsigned i = 0; i != shader.BoundResources; ++i)
    {
        D3D12_SHADER_INPUT_BIND_DESC binding{};
        reflection->GetResourceBindingDesc(i, &binding);
        if (binding.Type != D3D_SIT_TEXTURE && binding.Type != D3D_SIT_SAMPLER) continue;
        const bool sampler = binding.Type == D3D_SIT_SAMPLER;
        const unsigned count = sampler ? (pixelShader ? 16 : 4) : 20;
        const unsigned space = sampler ? (pixelShader ? 3 : 5) :
            binding.Dimension == D3D_SRV_DIMENSION_TEXTURE2D ? 0 :
            binding.Dimension == D3D_SRV_DIMENSION_TEXTURE2DARRAY ? 1 : 2;
        if (binding.BindCount != count || binding.BindPoint != 0 || binding.Space != space)
        {
            const std::string name = binding.Name;
            reflection->Release();
            throw std::runtime_error(name + " count=" + std::to_string(binding.BindCount) +
                " space=" + std::to_string(binding.Space) + " expected count=" + std::to_string(count) +
                " space=" + std::to_string(space));
        }
        sampler ? ++samplers : ++textures;
    }
    reflection->Release();
    if (textures != 3 || samplers != 1)
        throw std::runtime_error("Bounded probe did not retain all texture and sampler arrays");
}

int main(int argc, char** argv) try
{
    if (argc != 3) throw std::runtime_error("Expected vertex and pixel HLSL");
    for (int stage = 0; stage != 2; ++stage)
    {
        std::ifstream input(argv[stage + 1]);
        const std::string source{std::istreambuf_iterator<char>(input), {}};
        const auto code = DxbcCompiler::compile(source, stage != 0);
        if (code.size() < 4 || std::string(code.begin(), code.begin() + 4) != "DXBC")
            throw std::runtime_error("Compiler did not produce a DXBC container");
        ID3D11ShaderReflection* reflection = nullptr;
        if (FAILED(D3DReflect(code.data(), code.size(), IID_ID3D11ShaderReflection,
                reinterpret_cast<void**>(&reflection))))
            throw std::runtime_error("DXBC reflection failed");
        auto* variable = reflection->GetConstantBufferByName("SharedConstants")
            ->GetVariableByName("g_DxbcSpecConstants");
        D3D11_SHADER_VARIABLE_DESC description{};
        const HRESULT hr = variable->GetDesc(&description);
        reflection->Release();
        if (FAILED(hr) || description.StartOffset != (stage ? 884u : 880u) ||
            !(description.uFlags & D3D_SVF_USED))
            throw std::runtime_error("Stage " + std::to_string(stage) + " specialization constant offset=" + std::to_string(description.StartOffset) + " flags=" + std::to_string(description.uFlags) + " hr=" + std::to_string(hr));
        checkBoundedBindings(source, stage != 0);
    }
    // The production compiler must surface a useful diagnostic for invalid HLSL.
    try { DxbcCompiler::compile("invalid shader", false); }
    catch (const std::exception& error)
    {
        if (std::string(error.what()).find("DXBC compilation failed") != std::string::npos)
            return 0;
        throw;
    }
    throw std::runtime_error("Invalid HLSL unexpectedly compiled");
}
catch (const std::exception& error)
{
    std::cerr << error.what() << '\n';
    return 1;
}
