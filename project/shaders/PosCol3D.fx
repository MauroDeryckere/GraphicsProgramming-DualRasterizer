// Worldview
float4x4 gWorldViewProj : WorldViewProjection;
// World
float4x4 gWorldMatrix : World;

// Light
float3 gLightDirection = float3(0.577f, -0.577f, 0.577f);
float gLightIntensity = 7.0f;
float4 gAmbientColor = float4(0.025f, 0.025f, 0.025f, 0);

// Camera
float3 gCameraPosition = float3(0, 0, 0);

// Util
float gPI = 3.14159265359f;
float gShininess = 25.0f;

// Texture
Texture2D gDiffuseMap : DiffuseMap;
Texture2D gNormalMap : NormalMap;
Texture2D gSpecularMap : SpecularMap;
Texture2D gGlossinessMap : GlossinessMap;

    //Sampling
    SamplerState gSamplePoint : SampleState
    {
        Filter = MIN_MAG_MIP_POINT;
        AddressU = Wrap;
        AddressV = Wrap;
    };
    SamplerState gSampleLinear : SampleState
    {
        Filter = MIN_MAG_MIP_LINEAR;
        AddressU = Wrap;
        AddressV = Wrap;
    };
    SamplerState gSampleAni : SampleState
    {
        Filter = ANISOTROPIC;
        AddressU = Wrap;
        AddressV = Wrap;
        MaxAnisotropy = 16;
    };
//Rasterizer
RasterizerState gRasterizerState
{
    CullMode = none;
    FrontCounterClockwise = false; 
};

//Blending (disable it)
BlendState gBlendState
{
    BlendEnable[0] = false;
    RenderTargetWriteMask[0] = 0x0F;
};

//Depth
DepthStencilState gDepthStencilState
{
    DepthEnable = true;
    DepthWriteMask = 1;
    DepthFunc = less;
    StencilEnable = false;

    //Others are reduntdant because StencilEnable is false
    StencilReadMask = 0x0F;
    StencilWriteMask = 0x0F;

    FrontFaceStencilFunc = always;
    BackFaceStencilFunc = always;
    
    FrontFaceStencilDepthFail = keep;
    BackFaceStencilDepthFail = keep;

    FrontFaceStencilPass = keep;
    BackFaceStencilPass = keep;

    FrontFaceStencilFail = keep;
    BackFaceStencilFail = keep;
};

// Input / Output Structs
struct VS_INPUT
{
    float3 Position : POSITION;
    float2 TexCoord : TEXCOORD;
    
    float3 Normal : NORMAL;
    float3 Tangent : TANGENT;
};

struct VS_OUTPUT
{
    float4 Position : SV_POSITION;
    float4 WorldPosition : WORLD;
    float2 TexCoord : TEXCOORD;
    
    float3 Normal : NORMAL;
    float3 Tangent : TANGENT;
};

// "private functions"
float4 Lambert(float kd, float4 cd)
{
    return cd * kd / gPI;
}
float Phong(float ks, float exp, float3 lightDir, float3 viewDir, float3 normal)
{
    const float3 refl = reflect(-lightDir, normal);
    const float a = saturate(dot(refl, viewDir));
    return ks * pow(a, exp);
}


// Vertex Shader
VS_OUTPUT VS(VS_INPUT input)
{
    VS_OUTPUT output = (VS_OUTPUT)0;
    output.Position = mul(float4(input.Position, 1.0f), gWorldViewProj);
    output.WorldPosition = mul(float4(input.Position, 1.0f), gWorldMatrix);
    output.TexCoord = input.TexCoord;
    
    output.Tangent = mul(normalize(input.Tangent), (float3x3) gWorldMatrix);
    output.Normal = mul(normalize(input.Normal), (float3x3) gWorldMatrix);
    return output;
}

// Pixel Shader (phong)
float4 PS(VS_OUTPUT input, SamplerState s) : SV_TARGET
{ 
    const float3 normalMap = 2.0f * gNormalMap.Sample(s, input.TexCoord).rgb - float3(1.0f, 1.0f, 1.0f);
    const float3x3 TBN = float3x3(input.Tangent,
                                    cross(input.Normal, input.Tangent),
                                    input.Normal);
    const float3 normal = normalize(mul(normalMap, TBN));
    const float observedArea = saturate(dot(normal, -gLightDirection));
    
    // skip further calculations if possible
    if (observedArea <= 0.f)
    {
        return float4(0.f, 0.f, 0.f, 0.f);
    }
    
    const float3 viewDir = normalize(input.WorldPosition.xyz - gCameraPosition); 
    const float4 lambert = Lambert(gLightIntensity, gDiffuseMap.Sample(s, input.TexCoord));
    const float4 specular = gSpecularMap.Sample(s, input.TexCoord) * Phong(1.0f,
                                                                            gShininess * gGlossinessMap.Sample(s, input.TexCoord).b,
                                                                            gLightDirection,
                                                                            viewDir,
                                                                            normal);
    return observedArea * lambert + specular + gAmbientColor;
}

float4 PS_P(VS_OUTPUT input) : SV_TARGET { return PS(input, gSamplePoint); }
float4 PS_L(VS_OUTPUT input) : SV_TARGET { return PS(input, gSampleLinear); }
float4 PS_A(VS_OUTPUT input) : SV_TARGET { return PS(input, gSampleAni); }

// Technique 
technique11 PointSampling
{
    pass P0
    {
        SetRasterizerState(gRasterizerState);
        SetDepthStencilState(gDepthStencilState, 0);
        SetBlendState(gBlendState, float4(0.0f, 0.0f, 0.0f, 0.0f), 0xFFFFFFFF); // it's necessary to reset this since the other shaders set it to something else
        SetVertexShader(CompileShader(vs_5_0, VS()));
        SetGeometryShader( NULL );
        SetPixelShader(CompileShader(ps_5_0, PS_P()));
    }
}
technique11 LinearSampling
{
    pass P0
    {
        SetRasterizerState(gRasterizerState);
        SetDepthStencilState(gDepthStencilState, 0);
        SetBlendState(gBlendState, float4(0.0f, 0.0f, 0.0f, 0.0f), 0xFFFFFFFF); // it's necessary to reset this since the other shaders set it to something else
        SetVertexShader(CompileShader(vs_5_0, VS()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_5_0, PS_L()));
    }
}
technique11 AniSampling
{
    pass P0
    {
        SetRasterizerState(gRasterizerState);
        SetDepthStencilState(gDepthStencilState, 0);
        SetBlendState(gBlendState, float4(0.0f, 0.0f, 0.0f, 0.0f), 0xFFFFFFFF); // it's necessary to reset this since the other shaders set it to something else
        SetVertexShader(CompileShader(vs_5_0, VS()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_5_0, PS_A()));
    }
}