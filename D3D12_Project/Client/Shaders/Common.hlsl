cbuffer WoldTranslate : register(b1)
{
    float4x4 world;
    float4x4 finalTranforms[90];
    int isAnimation;
    int3 padding0;
    float powValue;
    float ambiantValue;
    float2 padding1;
};

cbuffer SceneConstantBuffer : register(b0)
{
    float4x4 view;
    float4x4 proj;
    float4x4 lightViewProj;
    float4x4 lightTexCoord;
};

// shadow map 용 srv 하나 더
Texture2D Texture : register(t0);
Texture2D ShadowMap : register(t1);

// shdow map 추출용 sampler 하나 더
SamplerState Sampler : register(s0);
SamplerComparisonState SamplerShadowMap : register(s1);


//---------------------------------------------------------------------------------------
// PCF for shadow mapping.
//---------------------------------------------------------------------------------------

float CalcShadowFactor(float4 shadowPosH)
{
    // Complete projection by doing division by w.
    shadowPosH.xyz /= shadowPosH.w;

    // Depth in NDC space.
    float depth = shadowPosH.z;

    uint width, height, numMips;
    ShadowMap.GetDimensions(0, width, height, numMips);

    // Texel size.
    float dx = 1.0f / (float) width;

    float percentLit = 0.0f;
    const float2 offsets[9] =
    {
        float2(-dx, -dx), float2(0.0f, -dx), float2(dx, -dx),
        float2(-dx, 0.0f), float2(0.0f, 0.0f), float2(dx, 0.0f),
        float2(-dx, +dx), float2(0.0f, +dx), float2(dx, +dx)
    };

    [unroll]
    for (int i = 0; i < 9; ++i)
    {
        percentLit += ShadowMap.SampleCmpLevelZero(SamplerShadowMap,
            shadowPosH.xy + offsets[i], depth).r;
    }
    
    return percentLit / 9.0f;
}
