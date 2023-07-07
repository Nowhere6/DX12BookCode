//***************************************************************************************
// color.hlsl by Frank Luna (C) 2015 All Rights Reserved.
//
// Transforms and colors geometry.
//***************************************************************************************

cbuffer cbPerObject : register(b0)
{
  float4x4 WorldViewProjection;
  float4x4 InverseWorld;
};

StructuredBuffer<float4x4> Bind : register(t0); // Local to bind space
StructuredBuffer<float4x4> BoneFinalTransform : register(t1); // Assure root bone space = world space

struct VertexIn
{
  float3 PosL : POSITION;
  float3 NormalL : NORMAL;
};

struct VertexOut
{
  float4 PosH : SV_POSITION;
  float3 NormalW : NORMAL;
};

static const float3 lightDir = normalize(float3(0, 1, 0.5));
static const float3 Color1 = float3(0.79f, 1.0f, 0.72f);
static const float3 Color2 = float3(0.3f, 0.24f, 0.17f);

VertexOut VS(VertexIn vin, uint InstanceID : SV_InstanceID)
{
  VertexOut vout;
  
  float4 pos = mul(float4(vin.PosL, 1.0f), Bind[InstanceID]);
  pos = mul(pos, BoneFinalTransform[InstanceID]);
  pos = mul(pos, WorldViewProjection);
  vout.PosH = pos;
  
  float4 n = mul(float4(vin.NormalL, 0.0f), Bind[InstanceID]);
  n = mul(n, BoneFinalTransform[InstanceID]);
  vout.NormalW = n.xyz;
  
  return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
  float3 c = lerp(Color2, Color1, 0.5f * dot(normalize(pin.NormalW), lightDir) + 0.5f);
  
  return float4(c, 1);
}


