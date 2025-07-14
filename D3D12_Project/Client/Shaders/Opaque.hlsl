#include "Common.hlsl"

struct VSInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
    float4 weight : WEIGHT;
    int4 boneIndex : BONEINDEX;
};

struct VSOutput
{
    float4 position : SV_POSITION;
    float4 shadowPosH : POSITION0;
    float4 normal : NORMAL;
    float2 uv : TEXCOORD;
};

VSOutput VS(VSInput input)
{    
    if (isAnimation == 1)
    {
        float3 pos = 0.0f;
        float3 normal = 0.0f;
        
        for (int i = 0; i < 4; ++i)
        {
            pos += input.weight[i] * mul(float4(input.position, 1.0f), finalTranforms[input.boneIndex[i]]).xyz;
            normal += input.weight[i] * mul(float4(input.normal, 0.0f), finalTranforms[input.boneIndex[i]]).xyz;
        }
        input.position = pos;
        normal = normalize(normal);
        input.normal = normal;
    }
    
    VSOutput output;
    float4 posW = mul(float4(input.position, 1.0f), world);
    output.position = mul(posW, mul(view, proj));
    float3 n = mul(input.normal, (float3x3) world);
    output.normal = float4(normalize(n), 0);
    output.uv = input.uv;
    output.shadowPosH = mul(posW, mul(lightViewProj, lightTexCoord));

    return output;
}

float4 PS(VSOutput input) : SV_TARGET
{
    float shadow = CalcShadowFactor(input.shadowPosH);
    shadow = max(shadow, 0.6f);
    //float4 result = Texture.Sample(Sampler, input.uv);
    float4 lightVector = float4(0.3, 1, 0, 0);
    lightVector = normalize(lightVector);
    float4 result = Texture.Sample(Sampler, input.uv) * (pow(max(dot(input.normal, lightVector), 0.f), powValue) + ambiantValue) * shadow;
    //float4 result = Texture.Sample(Sampler, input.uv) * max(dot(input.normal, lightVector), 0.2f);
    return result;
}
