#include "Common.hlsl"

struct VertexIn
{
    float3 position : POSITION;
    float4 weight : WEIGHT;
    int4 boneIndex : BONEINDEX;
};

struct VertexOut
{
    float4 position : SV_POSITION;
};

VertexOut VS(VertexIn input)
{
    if (isAnimation == 1)
    {
        float3 pos = 0.0f;
        
        for (int i = 0; i < 4; ++i)
        {
            pos += input.weight[i] * mul(float4(input.position, 1.0f), finalTranforms[input.boneIndex[i]]).xyz;
        }
        input.position = pos;
    }

    VertexOut vertexOut = (VertexOut)0.0f;
    float4 PosW = mul(float4(input.position, 1.0f), world);
    vertexOut.position = mul(PosW, lightViewProj);
    
    return vertexOut;
}

void PS(VertexOut input)
{
}
