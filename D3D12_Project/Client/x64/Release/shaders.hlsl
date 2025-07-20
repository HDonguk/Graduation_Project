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
};

Texture2D Texture : register(t0);
SamplerState Sampler : register(s0);

struct VSInput
{
    float4 position : POSITION;
    float4 normal : NORMAL;
    float2 uv : TEXCOORD;
    float4 weight : WEIGHT;
    int4 boneIndex : BONEINDEX;
};

struct VSOutput
{
    float4 position : SV_POSITION;
    float4 normal : NORMAL;
    float2 uv : TEXCOORD;
};

VSOutput VSMain(VSInput input)
{    
    if (isAnimation == 1)
    {
        float3 pos = float3(0.f, 0.f, 0.f);
        float3 normal = float3(0.f, 0.f, 0.f);
        
        for (int i = 0; i < 4; ++i)
        {
            pos += input.weight[i] * mul(input.position, finalTranforms[input.boneIndex[i]]).xyz;
            normal += input.weight[i] * mul(input.normal.xyz, (float3x3)finalTranforms[input.boneIndex[i]]);
        }
        
        input.position = float4(pos, 1);
        normal = normalize(normal);
        input.normal = float4(normal, 0);
    }
    
    VSOutput output;    
    output.position = mul(input.position, mul(world, mul(view, proj)));
    float3 n = mul(input.normal.xyz, (float3x3) world); // fbx 파일에서 읽어올때 회전을 시켜서 읽어 오는게 더 좋을지도...
    n = normalize(n);
    output.normal = float4(n, 0);
    output.uv = input.uv;

    return output;
}

float4 PSMain(VSOutput input) : SV_TARGET
{
    //float4 result = Texture.Sample(Sampler, input.uv);

    float4 lightVector = float4(0, 1, 0, 0);
    float4 result = Texture.Sample(Sampler, input.uv) * (pow(max(dot(input.normal, lightVector), 0.f), powValue) + ambiantValue);
    //float4 result = Texture.Sample(Sampler, input.uv) * max(dot(input.normal, lightVector), 0.2f);
    return result;
}
