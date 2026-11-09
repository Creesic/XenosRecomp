#include "dxbc_compiler.h"
#include <d3dcompiler.h>
#include <stdexcept>

std::vector<uint8_t> DxbcCompiler::compile(const std::string& source, bool pixelShader)
{
    std::vector<D3D_SHADER_MACRO> defines = {
        { "__DXBC__", "1" },
#ifdef UNLEASHED_RECOMP
        { "UNLEASHED_RECOMP", "1" },
#endif
#ifdef MARATHON_RECOMP
        { "MARATHON_RECOMP", "1" },
#endif
        { nullptr, nullptr }
    };
    if (!pixelShader)
        defines.insert(defines.begin() + 1, { "__DXBC_VERTEX__", "1" });
    ID3DBlob* bytecode = nullptr;
    ID3DBlob* errors = nullptr;
    const HRESULT hr = D3DCompile(source.data(), source.size(), nullptr, defines.data(), nullptr,
        "shaderMain", pixelShader ? "ps_5_1" : "vs_5_1",
        D3DCOMPILE_OPTIMIZATION_LEVEL3,
        0, &bytecode, &errors);
    std::string diagnostic;
    if (errors)
    {
        diagnostic.assign(static_cast<const char*>(errors->GetBufferPointer()), errors->GetBufferSize());
        errors->Release();
    }
    if (FAILED(hr))
    {
        if (bytecode) bytecode->Release();
        throw std::runtime_error("DXBC compilation failed: " + diagnostic);
    }
    const auto* bytes = static_cast<const uint8_t*>(bytecode->GetBufferPointer());
    std::vector<uint8_t> result(bytes, bytes + bytecode->GetBufferSize());
    bytecode->Release();
    return result;
}
