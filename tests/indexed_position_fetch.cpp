// Standalone Windows check: clang++ indexed_position_fetch.cpp -o check.exe
//   -ld3d11 -ld3dcompiler; check.exe path/to/shader_common.h [crowd.hlsl]
// Runs the actual HLSL fetch helper on D3D11 WARP, without launching a game.
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <array>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
using Microsoft::WRL::ComPtr;
static void check(HRESULT hr) {
    if (FAILED(hr)) throw std::runtime_error("Direct3D HRESULT " + std::to_string(hr));
}
int main(int argc, char** argv) try {
    if (argc < 2 || argc > 3) throw std::runtime_error("Pass shader_common.h [crowd.hlsl]");
    if (argc == 3) {
        std::ifstream dump(argv[2]);
        std::string hlsl{std::istreambuf_iterator<char>(dump), {}};
        for (uint32_t row = 0; row < 3; ++row) {
            const std::string fetch = "loadIndexedPosition(g_IndexedPosition" + std::to_string(row+1) +
                ", g_IndexedPosition(" + std::to_string(row) + "u), trunc(r0.z))";
            if (hlsl.find(fetch) == std::string::npos)
                throw std::runtime_error("Crowd shader does not fetch bone row " + std::to_string(row) + " by r0.z");
        }
    }
    std::ifstream file(argv[1]);
    std::string common{std::istreambuf_iterator<char>(file), {}};
    auto start = common.find("float4 loadIndexedPosition(");
    if (start == std::string::npos) throw std::runtime_error("Missing indexed fetch helper");
    auto end = common.find("\n#endif", start);
    std::string source = common.substr(start, end - start) + R"(
ByteAddressBuffer bones : register(t0);
RWStructuredBuffer<float4> output : register(u0);
[numthreads(32, 1, 1)] void main(uint3 tid : SV_DispatchThreadID) {
    uint i = tid.x;
    if (i >= 21u) return;
    float4 result = 0.0;
    if (i < 6u) result = loadIndexedPosition(bones, uint4(24,48,0x20,(i%3)*8), float(i/3));
    else if (i < 8u) {
        float4 p = float4(2,3,4,1);
        result = float4(dot(loadIndexedPosition(bones,uint4(24,48,0x20,0),float(i-6)),p),
                        dot(loadIndexedPosition(bones,uint4(24,48,0x20,8),float(i-6)),p),
                        dot(loadIndexedPosition(bones,uint4(24,48,0x20,16),float(i-6)),p),1);
    }
    else if (i == 8u) result = loadIndexedPosition(bones,uint4(24,48,0x20,0),2);
    else if (i == 9u) result = loadIndexedPosition(bones,uint4(24,48,0x20,44),0);
    else if (i == 10u) result = loadIndexedPosition(bones,uint4(24,48,0x20,0),-1);
    else if (i == 11u) result = loadIndexedPosition(bones,uint4(24,48,0x20,0),asfloat(0x7FC00000u));
    else if (i == 12u) result = loadIndexedPosition(bones,uint4(24,48,0x20,0),1e30);
    else if (i == 13u) result = loadIndexedPosition(bones,uint4(0,48,0x20,0),999);
    else if (i == 14u) result = loadIndexedPosition(bones,uint4(24,48,0x1F,0),1);
    else if (i == 15u) result = loadIndexedPosition(bones,uint4(16,64,0x24,48),0);
    else if (i == 16u) result = loadIndexedPosition(bones,uint4(16,64,0x25,48),0);
    else if (i == 17u) result = loadIndexedPosition(bones,uint4(16,64,0x39,48),0);
    else if (i == 18u) result = loadIndexedPosition(bones,uint4(16,64,0x26,48),0);
    else if (i == 19u) result = loadIndexedPosition(bones,uint4(24,48,0,0),0);
    else if (i == 20u) result = loadIndexedPosition(bones,uint4(24,0,0x20,0),0);
    output[i] = result;
})";
    ComPtr<ID3DBlob> shader, errors;
    HRESULT hr = D3DCompile(source.data(), source.size(), nullptr, nullptr, nullptr,
                           "main", "cs_5_0", D3DCOMPILE_ENABLE_STRICTNESS, 0, &shader, &errors);
    if (errors) std::cerr << static_cast<const char*>(errors->GetBufferPointer());
    check(hr);
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    check(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0,
                           D3D11_SDK_VERSION, &device, nullptr, &context));
    ComPtr<ID3D11ComputeShader> compute;
    check(device->CreateComputeShader(shader->GetBufferPointer(), shader->GetBufferSize(), nullptr, &compute));
    // DWORD-swapped guest half4 rows: identity + (1,2,3), and Z rotation + (10,20,30).
    std::array<uint32_t,16> data{0x3C000000,0x00003C00,0x00003C00,0x00004000,0x00000000,0x3C004200,
                               0x0000BC00,0x00004900,0x3C000000,0x00004D00,0x00000000,0x3C004F80};
    const float full[] = {1.25f,-2.5f,3.75f,4.5f};
    std::memcpy(data.data()+12,full,sizeof(full));
    D3D11_BUFFER_DESC desc{};
    desc.ByteWidth = sizeof(data);
    desc.Usage = D3D11_USAGE_IMMUTABLE;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
    D3D11_SUBRESOURCE_DATA initial{data.data()};
    ComPtr<ID3D11Buffer> input, output, staging;
    check(device->CreateBuffer(&desc,&initial,&input));
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = DXGI_FORMAT_R32_TYPELESS;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
    srvDesc.BufferEx.NumElements = data.size();
    srvDesc.BufferEx.Flags = D3D11_BUFFEREX_SRV_FLAG_RAW;
    ComPtr<ID3D11ShaderResourceView> srv;
    check(device->CreateShaderResourceView(input.Get(),&srvDesc,&srv));
    desc = {};
    desc.ByteWidth = 21*16;
    desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
    desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    desc.StructureByteStride = 16;
    check(device->CreateBuffer(&desc,nullptr,&output));
    ComPtr<ID3D11UnorderedAccessView> uav;
    check(device->CreateUnorderedAccessView(output.Get(),nullptr,&uav));
    desc.Usage = D3D11_USAGE_STAGING;
    desc.BindFlags = 0;
    desc.MiscFlags = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    check(device->CreateBuffer(&desc,nullptr,&staging));
    context->CSSetShader(compute.Get(),nullptr,0);
    context->CSSetShaderResources(0,1,srv.GetAddressOf());
    context->CSSetUnorderedAccessViews(0,1,uav.GetAddressOf(),nullptr);
    context->Dispatch(1,1,1);
    context->CopyResource(staging.Get(),output.Get());
    D3D11_MAPPED_SUBRESOURCE mapped{};
    check(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&mapped));
    const float expected[21][4] = {{1,0,0,1},{0,1,0,2},{0,0,1,3},
        {0,-1,0,10},{1,0,0,20},{0,0,1,30},{3,5,7,1},{7,22,34,1},
        {},{},{},{},{},{1,0,0,1},{0,-1,0,1},
        {1.25f,0,0,1},{1.25f,-2.5f,0,1},{1.25f,-2.5f,3.75f,1},{1.25f,-2.5f,3.75f,4.5f},{},{}};
    auto result = static_cast<const float*>(mapped.pData);
    for (size_t i = 0; i < 21*4; ++i) {
        if (!std::isfinite(result[i]) || std::abs(result[i]-expected[i/4][i%4]) > 0.0001f)
            throw std::runtime_error("Fetch mismatch at case " + std::to_string(i/4) + " lane " + std::to_string(i%4));
    }
    context->Unmap(staging.Get(),0);
    std::cout << "PASS: 21 HLSL palette fetch/transform/bounds cases on WARP\n";
} catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
    return 1;
}
