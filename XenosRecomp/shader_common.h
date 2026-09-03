#ifndef SHADER_COMMON_H_INCLUDED
#define SHADER_COMMON_H_INCLUDED

#define SPEC_CONSTANT_R11G11B10_NORMAL   (1 << 0)
#define SPEC_CONSTANT_ALPHA_TEST         (1 << 1)
// Set when the bound vertex declaration packs the tangent basis as unnormalized
// UBYTE4 (delivered as R8G8B8A8_UNORM, i.e. v/255). The guest microcode decodes
// it with v*(1/127)-1 expecting the raw 0..255 byte, so scale back up by 255.
#define SPEC_CONSTANT_UNPACK_UBYTE4_BASIS (1 << 6)
// FM2 (bug-127): set when the bound declaration's POSITION0 is FLOAT16_2/4.
// The input layout exposes the raw 16-bit words zero-extended into uint4, so
// tfetchPos3N must convert with f16tof32 -- the 32-bit bitcast used for FLOAT
// streams turns half bits into denormals (~0) and the mesh collapses.
#define SPEC_CONSTANT_POSITION_F16 (1 << 7)
// PGR4: POSITION0 is SHORT4/USHORT4 (k_16_16_16_16, integer flag): the IA
// delivers sign/zero-extended integers, convert instead of bitcasting.
#define SPEC_CONSTANT_POSITION_INT16 (1 << 8)

#ifdef UNLEASHED_RECOMP
    #define SPEC_CONSTANT_BICUBIC_GI_FILTER (1 << 2)
    #define SPEC_CONSTANT_ALPHA_TO_COVERAGE (1 << 3)
    #define SPEC_CONSTANT_REVERSE_Z         (1 << 4)
#endif

#ifdef MARATHON_RECOMP
    #define SPEC_CONSTANT_CONDITIONAL_RENDERING (1 << 5)
#endif

#if defined(__air__) || !defined(__cplusplus) || defined(__INTELLISENSE__)

#ifndef __air__
#define FLT_MIN asfloat(0xff7fffff)
#define FLT_MAX asfloat(0x7f7fffff)
#endif

#ifdef __spirv__

struct PushConstants
{
    uint64_t VertexShaderConstants;
    uint64_t PixelShaderConstants;
    uint64_t SharedConstants;
};

[[vk::push_constant]] ConstantBuffer<PushConstants> g_PushConstants;

#define g_BooleanWord(INDEX)        vk::RawBufferLoad<uint>(g_PushConstants.SharedConstants + 256 + uint(INDEX) * 4)
#define g_SwappedTexcoords          vk::RawBufferLoad<uint>(g_PushConstants.SharedConstants + 288)
#define g_SwappedNormals            vk::RawBufferLoad<uint>(g_PushConstants.SharedConstants + 292)
#define g_SwappedBinormals          vk::RawBufferLoad<uint>(g_PushConstants.SharedConstants + 296)
#define g_SwappedTangents           vk::RawBufferLoad<uint>(g_PushConstants.SharedConstants + 300)
#define g_SwappedBlendWeights       vk::RawBufferLoad<uint>(g_PushConstants.SharedConstants + 304)
#define g_SwappedPositions          vk::RawBufferLoad<uint>(g_PushConstants.SharedConstants + 316)
#define g_PackedTexcoordsLo         vk::RawBufferLoad<uint>(g_PushConstants.SharedConstants + 484)
#define g_PackedTexcoordsHi         vk::RawBufferLoad<uint>(g_PushConstants.SharedConstants + 488)
#define g_PackedBasis               vk::RawBufferLoad<uint>(g_PushConstants.SharedConstants + 492)
#define g_HalfPixelOffset           vk::RawBufferLoad<float2>(g_PushConstants.SharedConstants + 308)
#define g_NdcScale                  vk::RawBufferLoad<float2>(g_PushConstants.SharedConstants + 496)
#define g_NdcOffset                 vk::RawBufferLoad<float2>(g_PushConstants.SharedConstants + 504)
#define g_ClipPlane                 vk::RawBufferLoad<float4>(g_PushConstants.SharedConstants + 320)
#define g_ClipPlaneEnabled          vk::RawBufferLoad<bool>(g_PushConstants.SharedConstants + 336)
#define g_AlphaThreshold            vk::RawBufferLoad<float>(g_PushConstants.SharedConstants + 340)
#define g_conditionalSurveyIndex    vk::RawBufferLoad<uint>(g_PushConstants.SharedConstants + 344)
#define g_conditionalRenderingIndex vk::RawBufferLoad<uint>(g_PushConstants.SharedConstants + 348)
// bug-140: Xenos PA_CL_VTE_CNTL vertex-export modes, mirrored from Xenia's
// xe_flags (bit1<<1=XY already divided by W, bit2=Z divided by W, bit3=W is W
// not 1/W). Default 8 = standard homogeneous output (no-op tail).
#define g_VteFlags vk::RawBufferLoad<uint>(g_PushConstants.SharedConstants + 352)
#define g_LoopConstant(INDEX) vk::RawBufferLoad<uint>(g_PushConstants.SharedConstants + 356 + uint(INDEX) * 4)

[[vk::constant_id(0)]] const uint g_SpecConstants = 0;

#define g_SpecConstants() g_SpecConstants

#elif defined(__air__)

#include <metal_stdlib>

using namespace metal;

constant uint G_SPEC_CONSTANTS [[function_constant(0)]];
constant uint G_SPEC_CONSTANTS_VAL = is_function_constant_defined(G_SPEC_CONSTANTS) ? G_SPEC_CONSTANTS : 0;

uint g_SpecConstants()
{
    return G_SPEC_CONSTANTS_VAL;
}

struct PushConstants
{
    ulong VertexShaderConstants;
    ulong PixelShaderConstants;
    ulong SharedConstants;
};

#define g_BooleanWord(INDEX) (*(reinterpret_cast<device uint*>(g_PushConstants.SharedConstants + 256 + uint(INDEX) * 4)))
#define g_SwappedTexcoords (*(reinterpret_cast<device uint*>(g_PushConstants.SharedConstants + 288)))
#define g_SwappedNormals (*(reinterpret_cast<device uint*>(g_PushConstants.SharedConstants + 292)))
#define g_SwappedBinormals (*(reinterpret_cast<device uint*>(g_PushConstants.SharedConstants + 296)))
#define g_SwappedTangents (*(reinterpret_cast<device uint*>(g_PushConstants.SharedConstants + 300)))
#define g_SwappedBlendWeights (*(reinterpret_cast<device uint*>(g_PushConstants.SharedConstants + 304)))
#define g_SwappedPositions (*(reinterpret_cast<device uint*>(g_PushConstants.SharedConstants + 316)))
#define g_PackedTexcoordsLo (*(reinterpret_cast<device uint*>(g_PushConstants.SharedConstants + 484)))
#define g_PackedTexcoordsHi (*(reinterpret_cast<device uint*>(g_PushConstants.SharedConstants + 488)))
#define g_PackedBasis (*(reinterpret_cast<device uint*>(g_PushConstants.SharedConstants + 492)))
#define g_HalfPixelOffset (*(reinterpret_cast<device float2*>(g_PushConstants.SharedConstants + 308)))
#define g_NdcScale (*(reinterpret_cast<device float2*>(g_PushConstants.SharedConstants + 496)))
#define g_NdcOffset (*(reinterpret_cast<device float2*>(g_PushConstants.SharedConstants + 504)))
#define g_ClipPlane (*(reinterpret_cast<device float4*>(g_PushConstants.SharedConstants + 320)))
#define g_ClipPlaneEnabled (*(reinterpret_cast<device bool*>(g_PushConstants.SharedConstants + 336)))
#define g_AlphaThreshold (*(reinterpret_cast<device float*>(g_PushConstants.SharedConstants + 340)))
#define g_conditionalSurveyIndex (*(reinterpret_cast<device uint*>(g_PushConstants.SharedConstants + 344)))
#define g_conditionalRenderingIndex (*(reinterpret_cast<device uint*>(g_PushConstants.SharedConstants + 348)))
#define g_VteFlags (*(reinterpret_cast<device uint*>(g_PushConstants.SharedConstants + 352)))
#define g_LoopConstant(INDEX) (*(reinterpret_cast<device uint*>(g_PushConstants.SharedConstants + 356 + uint(INDEX) * 4)))

#else

#define DEFINE_SHARED_CONSTANTS() \
    uint4 g_BooleanWords[2] : packoffset(c16); \
    uint g_SwappedTexcoords : packoffset(c18.x); \
    uint g_SwappedNormals : packoffset(c18.y); \
    uint g_SwappedBinormals : packoffset(c18.z); \
    uint g_SwappedTangents : packoffset(c18.w);  \
    uint g_SwappedBlendWeights : packoffset(c19.x); \
    float2 g_HalfPixelOffset : packoffset(c19.y); \
    uint g_SwappedPositions : packoffset(c19.w); \
    float4 g_ClipPlane : packoffset(c20.x); \
    bool g_ClipPlaneEnabled : packoffset(c21.x); \
    float g_AlphaThreshold : packoffset(c21.y); \
    uint g_conditionalSurveyIndex : packoffset(c21.z); \
    uint g_conditionalRenderingIndex : packoffset(c21.w); \
    uint4 g_VteAndLoopConstants[9] : packoffset(c22); \
    float2 g_NdcScale : packoffset(c31.x); \
    float2 g_NdcOffset : packoffset(c31.z);

#define g_BooleanWord(INDEX) g_BooleanWords[uint(INDEX) / 4u][uint(INDEX) & 3u]
#define g_VteFlags g_VteAndLoopConstants[0].x
// PGR4 packed vertex modes live in the trailing padding of the loop-constant
// array (offsets 484/488/492 = dwords 33-35).
#define g_PackedTexcoordsLo g_VteAndLoopConstants[8].y
#define g_PackedTexcoordsHi g_VteAndLoopConstants[8].z
#define g_PackedBasis g_VteAndLoopConstants[8].w
#define g_LoopConstant(INDEX) g_VteAndLoopConstants[(uint(INDEX) + 1u) >> 2][(uint(INDEX) + 1u) & 3u]

uint g_SpecConstants();

#endif

#define BOOL_BIT(ADDRESS) ((g_BooleanWord(uint(ADDRESS) >> 5u) & (1u << (uint(ADDRESS) & 31u))) != 0u)

float4 cube(float4 value)
{
    float3 src = value.zwx;
    float3 abs_src = abs(src);

    float sc, tc, ma, id;

    if (abs_src.z >= abs_src.x && abs_src.z >= abs_src.y)
    {
        // Z major axis
        tc = -src.y;
        sc = src.z < 0.0 ? -src.x : src.x;
        ma = 2.0 * src.z;
        id = src.z < 0.0 ? 5.0 : 4.0;
    }
    else if (abs_src.y >= abs_src.x)
    {
        // Y major axis
        tc = src.y < 0.0 ? -src.z : src.z;
        sc = src.x;
        ma = 2.0 * src.y;
        id = src.y < 0.0 ? 3.0 : 2.0;
    }
    else
    {
        // X major axis
        tc = -src.y;
        sc = src.x < 0.0 ? src.z : -src.z;
        ma = 2.0 * src.x;
        id = src.x < 0.0 ? 1.0 : 0.0;
    }

    // Return as per Xbox 360 cube instruction output format:
    // x = t coordinate
    // y = s coordinate
    // z = 2 * major axis
    // w = face ID
    return float4(tc, sc, ma, id);
}

float3 cubeDir(float3 texCoord)
{
    // Move from 1...2 to -1...1
    float sc = (texCoord.x * 2.0) - 3.0;
    float tc = (texCoord.y * 2.0) - 3.0;

    uint face = uint(clamp(texCoord.z, 0.0, 5.0));

    // Split face into axis and sign
    uint axis = face >> 1;
    uint neg = face & 1;

    float3 dir;

    switch(axis)
    {
    case 0: // X major axis
        dir.y = -tc;
        dir.z = neg ? sc : -sc;
        dir.x = neg ? -1.0 : 1.0;
        break;

    case 1: // Y major axis
        dir.x = sc;
        dir.z = neg ? -tc : tc;
        dir.y = neg ? -1.0 : 1.0;
        break;

    default: // Z major axis
        dir.x = neg ? -sc : sc;
        dir.y = -tc;
        dir.z = neg ? -1.0 : 1.0;
        break;
    }

    return dir;
}

#ifdef __air__

struct Texture2DDescriptorHeap
{
    texture2d<float> tex;
};

struct Texture2DArrayDescriptorHeap
{
    texture2d_array<float> tex;
};

struct TextureCubeDescriptorHeap
{
    texturecube<float> tex;
};

struct SamplerDescriptorHeap
{
    sampler samp;
};

struct AtomicUintBuffer
{
    device atomic_uint* buffer;
};

uint2 getTexture2DDimensions(texture2d<float> texture)
{
    return uint2(texture.get_width(), texture.get_height());
}

uint3 getTexture2DArrayDimensions(texture2d_array<float> texture)
{
    return uint3(texture.get_width(), texture.get_height(), texture.get_array_size());
}

float4 tfetch2D(constant Texture2DDescriptorHeap* textureHeap,
                constant SamplerDescriptorHeap* samplerHeap,
                uint resourceDescriptorIndex,
                uint samplerDescriptorIndex,
                float2 texCoord, float2 offset)
{
    texture2d<float> texture = textureHeap[resourceDescriptorIndex].tex;
    sampler sampler = samplerHeap[samplerDescriptorIndex].samp;
    return texture.sample(sampler, texCoord + (offset + float2(0.00146484375, 0.00146484375)) / (float2)getTexture2DDimensions(texture));
}

float4 tfetch1D(constant Texture2DDescriptorHeap* textureHeap,
                constant SamplerDescriptorHeap* samplerHeap,
                uint resourceDescriptorIndex,
                uint samplerDescriptorIndex,
                float texCoord)
{
    return tfetch2D(textureHeap, samplerHeap, resourceDescriptorIndex, samplerDescriptorIndex,
        float2(texCoord, 0.5), float2(0.0));
}

float4 tfetch2DArray(constant Texture2DArrayDescriptorHeap* textureHeap,
                     constant SamplerDescriptorHeap* samplerHeap,
                     uint resourceDescriptorIndex,
                     uint samplerDescriptorIndex,
                     float3 texCoord, float3 offset)
{
    texture2d_array<float> texture = textureHeap[resourceDescriptorIndex].tex;
    sampler sampler = samplerHeap[samplerDescriptorIndex].samp;
    uint3 dimensions = getTexture2DArrayDimensions(texture);
    return texture.sample(sampler, texCoord.xy + (offset.xy + float2(0.00146484375, 0.00146484375)) / float2(dimensions.xy), uint(texCoord.z * dimensions.z));
}

float4 tfetchCube(constant TextureCubeDescriptorHeap* textureHeap,
                  constant SamplerDescriptorHeap* samplerHeap,
                  uint resourceDescriptorIndex,
                  uint samplerDescriptorIndex,
                  float3 texCoord)
{
    texturecube<float> texture = textureHeap[resourceDescriptorIndex].tex;
    sampler sampler = samplerHeap[samplerDescriptorIndex].samp;
    float3 dir = cubeDir(texCoord);
    return texture.sample(sampler, dir);
}

float4 tfetch2DL(constant Texture2DDescriptorHeap* textureHeap,
                 constant SamplerDescriptorHeap* samplerHeap,
                 uint resourceDescriptorIndex, uint samplerDescriptorIndex,
                 float2 texCoord, float2 offset, float lod)
{
    texture2d<float> texture = textureHeap[resourceDescriptorIndex].tex;
    sampler sampler = samplerHeap[samplerDescriptorIndex].samp;
    return texture.sample(sampler, texCoord + (offset + float2(0.00146484375, 0.00146484375)) / (float2)getTexture2DDimensions(texture), level(lod));
}

float4 tfetch1DL(constant Texture2DDescriptorHeap* textureHeap,
                 constant SamplerDescriptorHeap* samplerHeap,
                 uint resourceDescriptorIndex, uint samplerDescriptorIndex,
                 float texCoord, float lod)
{
    return tfetch2DL(textureHeap, samplerHeap, resourceDescriptorIndex,
                     samplerDescriptorIndex, float2(texCoord, 0.5), float2(0.0), lod);
}

float4 tfetch2DArrayL(constant Texture2DArrayDescriptorHeap* textureHeap,
                      constant SamplerDescriptorHeap* samplerHeap,
                      uint resourceDescriptorIndex, uint samplerDescriptorIndex,
                      float3 texCoord, float3 offset, float lod)
{
    texture2d_array<float> texture = textureHeap[resourceDescriptorIndex].tex;
    sampler sampler = samplerHeap[samplerDescriptorIndex].samp;
    uint3 dimensions = getTexture2DArrayDimensions(texture);
    return texture.sample(sampler, texCoord.xy + (offset.xy + float2(0.00146484375, 0.00146484375)) / float2(dimensions.xy),
                          uint(texCoord.z * dimensions.z), level(lod));
}

float4 tfetchCubeL(constant TextureCubeDescriptorHeap* textureHeap,
                   constant SamplerDescriptorHeap* samplerHeap,
                   uint resourceDescriptorIndex, uint samplerDescriptorIndex,
                   float3 texCoord, float lod)
{
    texturecube<float> texture = textureHeap[resourceDescriptorIndex].tex;
    sampler sampler = samplerHeap[samplerDescriptorIndex].samp;
    return texture.sample(sampler, cubeDir(texCoord), level(lod));
}

float2 getWeights2D(constant Texture2DDescriptorHeap* textureHeap,
                    constant SamplerDescriptorHeap* samplerHeap,
                    uint resourceDescriptorIndex,
                    uint samplerDescriptorIndex,
                    float2 texCoord, float2 offset)
{
    texture2d<float> texture = textureHeap[resourceDescriptorIndex].tex;
    return select(fract(texCoord * float2(getTexture2DDimensions(texture)) + offset - 0.5), 0.0, isnan(texCoord));
}

float getWeights1D(constant Texture2DDescriptorHeap* textureHeap,
                   constant SamplerDescriptorHeap* samplerHeap,
                   uint resourceDescriptorIndex,
                   uint samplerDescriptorIndex,
                   float texCoord)
{
    return getWeights2D(textureHeap, samplerHeap, resourceDescriptorIndex, samplerDescriptorIndex,
        float2(texCoord, 0.5), float2(0.0)).x;
}

float3 getWeights2DArray(constant Texture2DArrayDescriptorHeap* textureHeap,
                         constant SamplerDescriptorHeap* samplerHeap,
                         uint resourceDescriptorIndex,
                         uint samplerDescriptorIndex,
                         float3 texCoord, float3 offset)
{
    texture2d_array<float> texture = textureHeap[resourceDescriptorIndex].tex;
    return select(fract(texCoord * float3(getTexture2DArrayDimensions(texture)) + offset - 0.5), 0.0, isnan(texCoord));
}

#else

Texture2D<float4> g_Texture2DDescriptorHeap[] : register(t0, space0);
Texture2DArray<float4> g_Texture2DArrayDescriptorHeap[] : register(t0, space1);
TextureCube<float4> g_TextureCubeDescriptorHeap[] : register(t0, space2);
SamplerState g_SamplerDescriptorHeap[] : register(s0, space3);

#ifdef MARATHON_RECOMP
RWStructuredBuffer<uint> g_ConditionalSurveyBuffer : register(u0, space4);
#endif

uint2 getTexture2DDimensions(Texture2D<float4> texture)
{
    uint2 dimensions;
    texture.GetDimensions(dimensions.x, dimensions.y);
    return dimensions;
}

uint3 getTexture2DArrayDimensions(Texture2DArray<float4> texture)
{
    uint4 dimensions;
    texture.GetDimensions(0, dimensions.x, dimensions.y, dimensions.z, dimensions.w);
    return dimensions.xyz;
}

float4 tfetch2D(uint resourceDescriptorIndex, uint samplerDescriptorIndex, float2 texCoord, float2 offset)
{
    Texture2D<float4> texture = g_Texture2DDescriptorHeap[resourceDescriptorIndex];
    return texture.SampleLevel(g_SamplerDescriptorHeap[samplerDescriptorIndex], texCoord + (offset + float2(0.00146484375, 0.00146484375)) / getTexture2DDimensions(texture), 0.0);
}

float4 tfetch1D(uint resourceDescriptorIndex, uint samplerDescriptorIndex, float texCoord)
{
    return tfetch2D(resourceDescriptorIndex, samplerDescriptorIndex, float2(texCoord, 0.5), float2(0.0, 0.0));
}

float4 tfetch2DArray(uint resourceDescriptorIndex, uint samplerDescriptorIndex, float3 texCoord, float3 offset)
{
    Texture2DArray<float4> texture = g_Texture2DArrayDescriptorHeap[resourceDescriptorIndex];
    uint3 dimensions = getTexture2DArrayDimensions(texture);
    return texture.SampleLevel(g_SamplerDescriptorHeap[samplerDescriptorIndex], float3(texCoord.xy + (offset.xy + float2(0.00146484375, 0.00146484375)) / dimensions.xy, texCoord.z * dimensions.z), 0.0);
}

float4 tfetchCube(uint resourceDescriptorIndex, uint samplerDescriptorIndex, float3 texCoord)
{
    float3 dir = cubeDir(texCoord);
    return g_TextureCubeDescriptorHeap[resourceDescriptorIndex].SampleLevel(
        g_SamplerDescriptorHeap[samplerDescriptorIndex], dir, 0.0);
}

float4 tfetch2DL(uint resourceDescriptorIndex, uint samplerDescriptorIndex,
                 float2 texCoord, float2 offset, float lod)
{
    Texture2D<float4> texture = g_Texture2DDescriptorHeap[resourceDescriptorIndex];
    return texture.SampleLevel(g_SamplerDescriptorHeap[samplerDescriptorIndex],
                               texCoord + (offset + float2(0.00146484375, 0.00146484375)) / getTexture2DDimensions(texture), lod);
}

float4 tfetch1DL(uint resourceDescriptorIndex, uint samplerDescriptorIndex,
                 float texCoord, float lod)
{
    return tfetch2DL(resourceDescriptorIndex, samplerDescriptorIndex,
                     float2(texCoord, 0.5), float2(0.0, 0.0), lod);
}

float4 tfetch2DArrayL(uint resourceDescriptorIndex, uint samplerDescriptorIndex,
                      float3 texCoord, float3 offset, float lod)
{
    Texture2DArray<float4> texture = g_Texture2DArrayDescriptorHeap[resourceDescriptorIndex];
    uint3 dimensions = getTexture2DArrayDimensions(texture);
    return texture.SampleLevel(g_SamplerDescriptorHeap[samplerDescriptorIndex],
                               float3(texCoord.xy + (offset.xy + float2(0.00146484375, 0.00146484375)) / dimensions.xy,
                                      texCoord.z * dimensions.z), lod);
}

float4 tfetchCubeL(uint resourceDescriptorIndex, uint samplerDescriptorIndex,
                   float3 texCoord, float lod)
{
    return g_TextureCubeDescriptorHeap[resourceDescriptorIndex].SampleLevel(
        g_SamplerDescriptorHeap[samplerDescriptorIndex], cubeDir(texCoord), lod);
}

float2 getWeights2D(uint resourceDescriptorIndex, uint samplerDescriptorIndex, float2 texCoord, float2 offset)
{
    Texture2D<float4> texture = g_Texture2DDescriptorHeap[resourceDescriptorIndex];
    return select(isnan(texCoord), 0.0, frac(texCoord * getTexture2DDimensions(texture) + offset - 0.5));
}

float getWeights1D(uint resourceDescriptorIndex, uint samplerDescriptorIndex, float texCoord)
{
    return getWeights2D(resourceDescriptorIndex, samplerDescriptorIndex, float2(texCoord, 0.5), float2(0.0, 0.0)).x;
}

float3 getWeights2DArray(uint resourceDescriptorIndex, uint samplerDescriptorIndex, float3 texCoord, float3 offset)
{
    Texture2DArray<float4> texture = g_Texture2DArrayDescriptorHeap[resourceDescriptorIndex];
    return select(isnan(texCoord), 0.0, frac(texCoord * getTexture2DArrayDimensions(texture) + offset - 0.5));
}

#endif

#ifdef __air__
#define selectWrapper(a, b, c) select(c, b, a)
#else
#define selectWrapper(a, b, c) select(a, b, c)
#endif

#ifdef __air__
#define frac(X) fract(X)
#define asuint(X) as_type<uint4>(X)

template<typename T>
void clip(T a)
{
    if (a < 0.0) {
        discard_fragment();
    }
}

template<typename T>
float rcp(T a)
{
    return 1.0 / a;
}

template<typename T>
float4x4 mul(T a, T b)
{
    return b * a;
}
#endif

// Xenos RECIP_FF / RSQ_FF ("fast"/flush) scalar ops flush +-Inf to 0 instead of
// clamping to +-FLT_MAX (the RECIP_CLAMP/RSQ_CLAMP variants) or leaving the raw
// IEEE result. Matches Xenia's DxbcShaderTranslator (rsqrt(0)/rcp(0) => 0), so a
// degenerate normalize (length 0) contributes no light rather than exploding to
// FLT_MAX (root cause of the FM2 main-menu car's flashing polys, VS 4fff9681).
float rcpFF(float a) { float r = rcp(a); return isinf(r) ? 0.0 : r; }
float rsqFF(float a) { float r = rsqrt(a); return isinf(r) ? 0.0 : r; }

// Shader Model 3 ALU semantics, matching Xenia's DxbcShaderTranslator
// (dxbc_translator_alu.cpp). These only differ from a plain op for degenerate
// operands (+-Inf / NaN, i.e. downstream of a clamped Logc/Rcpc/Rsqc); for
// finite inputs they are identical to a * b / max / min / dot.
//   * multiply: +-0 (or denormal) * anything = +0, so 0 * Inf = +0 (not NaN).
//   * max/min : a>=b?a:b / a<b?a:b (propagate the 2nd operand on NaN, not fmax/fmin).
float  mulSM3(float  a, float  b) { return selectWrapper(min(abs(a), abs(b)) == 0.0, (float )0.0, a * b); }
float2 mulSM3(float2 a, float2 b) { return selectWrapper(min(abs(a), abs(b)) == 0.0, (float2)0.0, a * b); }
float3 mulSM3(float3 a, float3 b) { return selectWrapper(min(abs(a), abs(b)) == 0.0, (float3)0.0, a * b); }
float4 mulSM3(float4 a, float4 b) { return selectWrapper(min(abs(a), abs(b)) == 0.0, (float4)0.0, a * b); }
float  maxSM3(float  a, float  b) { return selectWrapper(a >= b, a, b); }
float2 maxSM3(float2 a, float2 b) { return selectWrapper(a >= b, a, b); }
float3 maxSM3(float3 a, float3 b) { return selectWrapper(a >= b, a, b); }
float4 maxSM3(float4 a, float4 b) { return selectWrapper(a >= b, a, b); }
float  minSM3(float  a, float  b) { return selectWrapper(a <  b, a, b); }
float2 minSM3(float2 a, float2 b) { return selectWrapper(a <  b, a, b); }
float3 minSM3(float3 a, float3 b) { return selectWrapper(a <  b, a, b); }
float4 minSM3(float4 a, float4 b) { return selectWrapper(a <  b, a, b); }
float dotSM3(float2 a, float2 b) { return mulSM3(a.x, b.x) + mulSM3(a.y, b.y); }
float dotSM3(float3 a, float3 b) { return mulSM3(a.x, b.x) + mulSM3(a.y, b.y) + mulSM3(a.z, b.z); }
float dotSM3(float4 a, float4 b) { return mulSM3(a.x, b.x) + mulSM3(a.y, b.y) + mulSM3(a.z, b.z) + mulSM3(a.w, b.w); }

#ifdef __air__
#define UNROLL
#define BRANCH
#else
#define UNROLL [unroll]
#define BRANCH [branch]
#endif

float4 getGradients2D(float2 value)
{
#ifdef __air__
    return float4(dfdx(value.x), dfdy(value.x), dfdx(value.y), dfdy(value.y));
#else
    return float4(ddx_coarse(value.x), ddy_coarse(value.x), ddx_coarse(value.y), ddy_coarse(value.y));
#endif
}

float w0(float a)
{
    return (1.0f / 6.0f) * (a * (a * (-a + 3.0f) - 3.0f) + 1.0f);
}

float w1(float a)
{
    return (1.0f / 6.0f) * (a * a * (3.0f * a - 6.0f) + 4.0f);
}

float w2(float a)
{
    return (1.0f / 6.0f) * (a * (a * (-3.0f * a + 3.0f) + 3.0f) + 1.0f);
}

float w3(float a)
{
    return (1.0f / 6.0f) * (a * a * a);
}

float g0(float a)
{
    return w0(a) + w1(a);
}

float g1(float a)
{
    return w2(a) + w3(a);
}

float h0(float a)
{
    return -1.0f + w1(a) / (w0(a) + w1(a)) + 0.5f;
}

float h1(float a)
{
    return 1.0f + w3(a) / (w2(a) + w3(a)) + 0.5f;
}

#ifdef __air__

float4 tfetch2DBicubic(constant Texture2DDescriptorHeap* textureHeap,
                       constant SamplerDescriptorHeap* samplerHeap,
                       uint resourceDescriptorIndex,
                       uint samplerDescriptorIndex,
                       float2 texCoord, float2 offset)
{
    texture2d<float> texture = textureHeap[resourceDescriptorIndex].tex;
    sampler sampler = samplerHeap[samplerDescriptorIndex].samp;
    uint2 dimensions = getTexture2DDimensions(texture);

    float x = texCoord.x * dimensions.x + offset.x;
    float y = texCoord.y * dimensions.y + offset.y;

    x -= 0.5f;
    y -= 0.5f;
    float px = floor(x);
    float py = floor(y);
    float fx = x - px;
    float fy = y - py;

    float g0x = g0(fx);
    float g1x = g1(fx);
    float h0x = h0(fx);
    float h1x = h1(fx);
    float h0y = h0(fy);
    float h1y = h1(fy);

    float4 r =
        g0(fy) * (g0x * texture.sample(sampler, float2(px + h0x, py + h0y) / float2(dimensions)) +
              g1x * texture.sample(sampler, float2(px + h1x, py + h0y) / float2(dimensions))) +
        g1(fy) * (g0x * texture.sample(sampler, float2(px + h0x, py + h1y) / float2(dimensions)) +
              g1x * texture.sample(sampler, float2(px + h1x, py + h1y) / float2(dimensions)));

    return r;
}

#else

float4 tfetch2DBicubic(uint resourceDescriptorIndex, uint samplerDescriptorIndex, float2 texCoord, float2 offset)
{
    Texture2D<float4> texture = g_Texture2DDescriptorHeap[resourceDescriptorIndex];
    SamplerState samplerState = g_SamplerDescriptorHeap[samplerDescriptorIndex];
    uint2 dimensions = getTexture2DDimensions(texture);
    
    float x = texCoord.x * dimensions.x + offset.x;
    float y = texCoord.y * dimensions.y + offset.y;

    x -= 0.5f;
    y -= 0.5f;
    float px = floor(x);
    float py = floor(y);
    float fx = x - px;
    float fy = y - py;

    float g0x = g0(fx);
    float g1x = g1(fx);
    float h0x = h0(fx);
    float h1x = h1(fx);
    float h0y = h0(fy);
    float h1y = h1(fy);

    float4 r =
        g0(fy) * (g0x * texture.Sample(samplerState, float2(px + h0x, py + h0y) / float2(dimensions)) +
            g1x * texture.Sample(samplerState, float2(px + h1x, py + h0y) / float2(dimensions))) +
        g1(fy) * (g0x * texture.Sample(samplerState, float2(px + h0x, py + h1y) / float2(dimensions)) +
            g1x * texture.Sample(samplerState, float2(px + h1x, py + h1y) / float2(dimensions)));

    return r;
}

#endif

// bug-140 (computed LOD): Xenos pixel-shader texture fetches default to
// UseComputedLOD=true -- implicit-LOD sampling plus the fetch's 1/32-step LOD
// bias -- but the base tfetch* helpers force SampleLevel(0) (required for
// vertex shaders, which have no derivatives). These PS-only variants restore
// the hardware mip selection; XENOS_PS is defined by the recompiler for pixel
// shaders only, keeping Sample/SampleBias intrinsics out of VS compilations.
#ifdef XENOS_PS
#ifdef __air__
float4 tfetch2DCL(constant Texture2DDescriptorHeap* textureHeap,
                  constant SamplerDescriptorHeap* samplerHeap,
                  uint resourceDescriptorIndex, uint samplerDescriptorIndex,
                  float2 texCoord, float2 offset, float lodBias)
{
    texture2d<float> texture = textureHeap[resourceDescriptorIndex].tex;
    sampler samp = samplerHeap[samplerDescriptorIndex].samp;
    return texture.sample(samp, texCoord + (offset + float2(0.00146484375, 0.00146484375)) / (float2)getTexture2DDimensions(texture), bias(lodBias));
}

float4 tfetch1DCL(constant Texture2DDescriptorHeap* textureHeap,
                  constant SamplerDescriptorHeap* samplerHeap,
                  uint resourceDescriptorIndex, uint samplerDescriptorIndex,
                  float texCoord, float lodBias)
{
    return tfetch2DCL(textureHeap, samplerHeap, resourceDescriptorIndex,
                      samplerDescriptorIndex, float2(texCoord, 0.5), float2(0.0), lodBias);
}

float4 tfetch2DArrayCL(constant Texture2DArrayDescriptorHeap* textureHeap,
                       constant SamplerDescriptorHeap* samplerHeap,
                       uint resourceDescriptorIndex, uint samplerDescriptorIndex,
                       float3 texCoord, float3 offset, float lodBias)
{
    texture2d_array<float> texture = textureHeap[resourceDescriptorIndex].tex;
    sampler samp = samplerHeap[samplerDescriptorIndex].samp;
    uint3 dimensions = getTexture2DArrayDimensions(texture);
    return texture.sample(samp, texCoord.xy + (offset.xy + float2(0.00146484375, 0.00146484375)) / float2(dimensions.xy),
                          uint(texCoord.z * dimensions.z), bias(lodBias));
}

float4 tfetchCubeCL(constant TextureCubeDescriptorHeap* textureHeap,
                    constant SamplerDescriptorHeap* samplerHeap,
                    uint resourceDescriptorIndex, uint samplerDescriptorIndex,
                    float3 texCoord, float lodBias)
{
    texturecube<float> texture = textureHeap[resourceDescriptorIndex].tex;
    sampler samp = samplerHeap[samplerDescriptorIndex].samp;
    return texture.sample(samp, cubeDir(texCoord), bias(lodBias));
}
#else
float4 tfetch2DCL(uint resourceDescriptorIndex, uint samplerDescriptorIndex,
                  float2 texCoord, float2 offset, float lodBias)
{
    Texture2D<float4> texture = g_Texture2DDescriptorHeap[resourceDescriptorIndex];
    return texture.SampleBias(g_SamplerDescriptorHeap[samplerDescriptorIndex],
                              texCoord + (offset + float2(0.00146484375, 0.00146484375)) / getTexture2DDimensions(texture), lodBias);
}

float4 tfetch1DCL(uint resourceDescriptorIndex, uint samplerDescriptorIndex,
                  float texCoord, float lodBias)
{
    return tfetch2DCL(resourceDescriptorIndex, samplerDescriptorIndex,
                      float2(texCoord, 0.5), float2(0.0, 0.0), lodBias);
}

float4 tfetch2DArrayCL(uint resourceDescriptorIndex, uint samplerDescriptorIndex,
                       float3 texCoord, float3 offset, float lodBias)
{
    Texture2DArray<float4> texture = g_Texture2DArrayDescriptorHeap[resourceDescriptorIndex];
    uint3 dimensions = getTexture2DArrayDimensions(texture);
    return texture.SampleBias(g_SamplerDescriptorHeap[samplerDescriptorIndex],
                              float3(texCoord.xy + (offset.xy + float2(0.00146484375, 0.00146484375)) / dimensions.xy,
                                     texCoord.z * dimensions.z), lodBias);
}

float4 tfetchCubeCL(uint resourceDescriptorIndex, uint samplerDescriptorIndex,
                    float3 texCoord, float lodBias)
{
    return g_TextureCubeDescriptorHeap[resourceDescriptorIndex].SampleBias(
        g_SamplerDescriptorHeap[samplerDescriptorIndex], cubeDir(texCoord), lodBias);
}
#endif
#endif

float4 tfetchR11G11B10(uint4 value)
{
    if (g_SpecConstants() & SPEC_CONSTANT_R11G11B10_NORMAL)
    {
        return float4(
            (value.x & 0x00000400 ? -1.0 : 0.0) + ((value.x & 0x3FF) / 1024.0),
            (value.x & 0x00200000 ? -1.0 : 0.0) + (((value.x >> 11) & 0x3FF) / 1024.0),
            (value.x & 0x80000000 ? -1.0 : 0.0) + (((value.x >> 22) & 0x1FF) / 512.0),
            0.0);
    }
    else
    {
#ifdef __air__
        return as_type<float4>(value);
#else
        return asfloat(value);
#endif
    }
}

float4 unpackUByte4Basis(float4 value)
{
    if (g_SpecConstants() & SPEC_CONSTANT_UNPACK_UBYTE4_BASIS)
        return value * 255.0;
    return value;
}

float4 tfetchPos3N(uint4 value)
{
    // Raw-integer position stream: bitcast the fetched words back to float.
    // (.w defaults to 1 from the input assembler for 3-component positions.)
    // SPEC_CONSTANT_POSITION_F16: the stream is FLOAT16_2/4 -- the IA delivers
    // zero-extended 16-bit words, so convert each half properly; the 32-bit
    // bitcast below would produce denormals (~0) and collapse the mesh.
    // bug-136/137 (2026-07-09, Xenia ucode dump diff): the guest microcode
    // fetches 16-bit positions with a .yxwz DESTINATION SWIZZLE
    // ("vfetch_full r1.yxwz ... FMT_16_16_16_16_FLOAT") -- the standard 360
    // fix-up that undoes the half-order swap of 16-bit components inside
    // byteswapped 32-bit words. The fork's translator drops all vfetch
    // attributes; texcoords are patched at runtime via g_SwappedTexcoords but
    // POSITION had no swap path, so x<->y and z<->w (the per-part dequant
    // SCALE!) were exchanged -- negative/garbage scales exploded the FM2 car
    // mesh. Consume the halves in .yxwz order.
    if (g_SpecConstants() & SPEC_CONSTANT_POSITION_F16)
    {
#ifdef __air__
        float4 position = float4(float(as_type<half>(ushort(value.y))),
                                 float(as_type<half>(ushort(value.x))),
                                 float(as_type<half>(ushort(value.w))),
                                 float(as_type<half>(ushort(value.z))));
#else
        float4 position = float4(f16tof32(value.y), f16tof32(value.x),
                                 f16tof32(value.w), f16tof32(value.z));
#endif
        // FLOAT16_2 stream: the IA defaults .zw to (0, 1); undo the swap the
        // defaults went through and keep the classic (x, y, 0, 1) shape.
        if (value.z == 0 && value.w == 1)
        {
#ifdef __air__
            position = float4(float(as_type<half>(ushort(value.y))),
                              float(as_type<half>(ushort(value.x))), 0.0, 1.0);
#else
            position = float4(f16tof32(value.y), f16tof32(value.x), 0.0, 1.0);
#endif
        }
        return position;
    }
    if (g_SpecConstants() & SPEC_CONSTANT_POSITION_INT16)
    {
        // Same .yxwz half-order fix-up as the FLOAT16 path; the game scales
        // these integers with its own constants.
        return float4(float(int(value.y)), float(int(value.x)), float(int(value.w)), float(int(value.z)));
    }
#ifdef __air__
    float4 position = as_type<float4>(value);
#else
    float4 position = asfloat(value);
#endif
    position.w = value.w == 1 ? 1.0 : position.w;
    return position;
}

float4 swapFloats(uint swappedFloats, float4 value, uint semanticIndex)
{
    return (swappedFloats & (1ull << semanticIndex)) != 0 ? value.yxwz : value;
}

// PGR4 (2026-09-03): vertex element formats the host input assembler cannot
// convert (Xenos k_10_11_11 / k_11_11_10 / k_2_10_10_10 in unorm, uint and
// snorm flavours). The host feeds such elements as a raw uint (R32_UINT) and
// publishes a 4-bit mode per input here; mode 0 is a passthrough.
//   mode = 1 + family * 3 + kind
//   family: 0 = 10:11:11 (x 11 bits low), 1 = 11:11:10 (x 10 bits low), 2 = 2:10:10:10
//   kind:   0 = unorm, 1 = uint, 2 = snorm
float unpackSnormField(uint bits, uint width)
{
    int v = int(bits << (32u - width)) >> (32u - width);
    return max(float(v) / float((1u << (width - 1u)) - 1u), -1.0);
}

float4 unpackVertexMode(uint mode, float4 value)
{
    if (mode == 0u)
        return value;
    // 10 / 11: integer 16-bit (SHORT / USHORT) elements. The input assembler
    // sign/zero-extends them into the float register unconverted.
#ifdef __air__
    if (mode == 10u)
        return float4(as_type<int4>(value));
    if (mode == 11u)
        return float4(as_type<uint4>(value));
    uint b = as_type<uint>(value.x);
#else
    if (mode == 10u)
        return float4(asint(value));
    if (mode == 11u)
        return float4(asuint(value));
    uint b = asuint(value.x);
#endif
    uint family = (mode - 1u) / 3u;
    uint kind = (mode - 1u) % 3u;
    uint w0 = family == 0u ? 11u : 10u;
    uint w1 = family == 2u ? 10u : 11u;
    uint w2 = family == 1u ? 11u : 10u;
    uint x = b & ((1u << w0) - 1u);
    uint y = (b >> w0) & ((1u << w1) - 1u);
    uint z = (b >> (w0 + w1)) & ((1u << w2) - 1u);
    uint w = b >> 30u;
    float4 r;
    if (kind == 0u)
        r = float4(float(x) / float((1u << w0) - 1u), float(y) / float((1u << w1) - 1u),
                   float(z) / float((1u << w2) - 1u), family == 2u ? float(w) / 3.0 : 1.0);
    else if (kind == 1u)
        r = float4(float(x), float(y), float(z), family == 2u ? float(w) : 1.0);
    else
        r = float4(unpackSnormField(x, w0), unpackSnormField(y, w1), unpackSnormField(z, w2),
                   family == 2u ? unpackSnormField(w, 2u) : 1.0);
    return r;
}

float4 unpackTexcoord(uint lo, uint hi, float4 value, uint semanticIndex)
{
    uint mode = semanticIndex < 8u ? (lo >> (semanticIndex * 4u)) & 0xFu
                                   : (hi >> ((semanticIndex - 8u) * 4u)) & 0xFu;
    return unpackVertexMode(mode, value);
}

// slot: normal 0-1 -> 0-1, tangent 0-1 -> 2-3, binormal 0-1 -> 4-5
float4 unpackBasis(uint word, uint slot, float4 value)
{
    return unpackVertexMode((word >> (slot * 4u)) & 0xFu, value);
}

float4 dst(float4 src0, float4 src1)
{
    float4 dest;
    dest.x = 1.0;
    dest.y = src0.y * src1.y;
    dest.z = src0.z;
    dest.w = src1.w;
    return dest;
}

float4 max4(float4 src0)
{
    return max(max(src0.x, src0.y), max(src0.z, src0.w));
}

#ifdef __air__

float2 getPixelCoord(constant Texture2DDescriptorHeap* textureHeap,
                     uint resourceDescriptorIndex,
                     float2 texCoord)
{
    texture2d<float> texture = textureHeap[resourceDescriptorIndex].tex;
    return (float2)getTexture2DDimensions(texture) * texCoord;
}

#else

float2 getPixelCoord(uint resourceDescriptorIndex, float2 texCoord)
{
    return getTexture2DDimensions(g_Texture2DDescriptorHeap[resourceDescriptorIndex]) * texCoord;
}

#endif

float computeMipLevel(float2 pixelCoord)
{
#ifdef __air__
    float2 dx = dfdx(pixelCoord);
    float2 dy = dfdy(pixelCoord);
#else
    float2 dx = ddx(pixelCoord);
    float2 dy = ddy(pixelCoord);
#endif
    float deltaMaxSqr = max(dot(dx, dx), dot(dy, dy));
    return max(0.0, 0.5 * log2(deltaMaxSqr));
}

#ifdef __air__

uint atomicLoadUint(device AtomicUintBuffer* buffer, uint index)
{
    return atomic_load_explicit(&buffer->buffer[index], memory_order_relaxed);
}

uint atomicFetchAddUint(device AtomicUintBuffer* buffer, uint index, uint value)
{
    return atomic_fetch_add_explicit(&buffer->buffer[index], value, memory_order_relaxed);
}

#else

uint atomicLoadUint(RWStructuredBuffer<uint> buffer, uint index)
{
    return buffer[index];
}

uint atomicFetchAddUint(RWStructuredBuffer<uint> buffer, uint index, uint value)
{
    uint originalValue;
    InterlockedAdd(buffer[index], value, originalValue);
    return originalValue;
}

#endif

#endif

#endif
