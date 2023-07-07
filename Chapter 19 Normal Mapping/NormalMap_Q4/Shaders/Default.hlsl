//***************************************************************************************
// Default.hlsl by Frank Luna (C) 2015 All Rights Reserved.
//***************************************************************************************

// Defaults for number of lights.
#ifndef NUM_DIR_LIGHTS
    #define NUM_DIR_LIGHTS 3
#endif

#ifndef NUM_POINT_LIGHTS
    #define NUM_POINT_LIGHTS 0
#endif

#ifndef NUM_SPOT_LIGHTS
    #define NUM_SPOT_LIGHTS 0
#endif

// Include common HLSL code.
#include "Common.hlsl"

struct VertexIn
{
  float3 PosL : POSITION;
  float3 NormalL : NORMAL;
  float2 TexC : TEXCOORD;
  float3 TangentU : TANGENT;
};

struct VertexOut
{
  float4 PosH : SV_POSITION;
  float3 PosW : POSITION;
  float3 NormalL : NORMAL; // suffix -L = Local Space
  float3 TangentL : TANGENT;
  float2 TexC : TEXCOORD;
};

VertexOut VS(VertexIn vin)
{
  VertexOut vout = (VertexOut) 0.0f;

	// Fetch the material data.
  MaterialData matData = gMaterialData[gMaterialIndex];
	
    // Transform to world space.
  float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);
  vout.PosW = posW.xyz;

    // Assumes nonuniform scaling; otherwise, need to use inverse-transpose of world matrix.
  vout.NormalL = vin.NormalL;
	
  vout.TangentL = vin.TangentU;

    // Transform to homogeneous clip space.
  vout.PosH = mul(posW, gViewProj);
	
	// Output vertex attributes for interpolation across triangle.
  float4 texC = mul(float4(vin.TexC, 0.0f, 1.0f), gTexTransform);
  vout.TexC = mul(texC, matData.MatTransform).xy;
	
  return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
	// Fetch the material data.
  MaterialData matData = gMaterialData[gMaterialIndex];
  float4 diffuseAlbedo = matData.DiffuseAlbedo;
  float3 fresnelR0 = matData.FresnelR0;
  float roughness = matData.Roughness;
  uint diffuseMapIndex = matData.DiffuseMapIndex;
  uint normalMapIndex = matData.NormalMapIndex;

	// Interpolating normal can unnormalize it, so renormalize it.
  pin.NormalL = normalize(pin.NormalL);
  pin.TangentL = normalize(pin.TangentL - dot(pin.TangentL, pin.NormalL) * pin.NormalL);
  float3 BiTangent = cross(pin.NormalL, pin.TangentL);
  float3x3 Tangent2Local = float3x3(pin.TangentL, BiTangent, pin.NormalL);
  float3x3 Local2Tangent = transpose(Tangent2Local);
  
  float4 RawSampleNormalT = gTextureMaps[normalMapIndex].Sample(gsamAnisotropicWrap, pin.TexC);
  float3 SampleNormalT = normalize(2.0 * RawSampleNormalT.xyz - 1.0);

	// Uncomment to turn off normal mapping.
	//bumpedNormalW = pin.NormalW;

	// Dynamically look up the texture in the array.
  diffuseAlbedo *= gTextureMaps[diffuseMapIndex].Sample(gsamAnisotropicWrap, pin.TexC);

  float3x3 World2Local = 
    { gWorld[0][0], gWorld[1][0], gWorld[2][0],
      gWorld[0][1], gWorld[1][1], gWorld[2][1],
      gWorld[0][2], gWorld[1][2], gWorld[2][2]
    };
  float3 toEyeT = normalize(mul(mul(gEyePosW - pin.PosW, World2Local), Local2Tangent));

    // Light terms.
  float4 ambient = gAmbientLight * diffuseAlbedo;

  const float shininess = (1.0f - roughness) * RawSampleNormalT.a;
  Material mat = { diffuseAlbedo, fresnelR0, shininess };
  float3 shadowFactor = 1.0f;
  
  Light lights[MaxLights] = gLights;
  [unroll]
  for (int i = 0; i < NUM_DIR_LIGHTS; ++i)
  {
    //现在只考虑切线空间平行光照，只有方向较为简单
    lights[i].Direction = mul(mul(lights[i].Direction, World2Local),Local2Tangent);
  }
  
  //全部位于切线空间
  float4 directLight = ComputeLighting(lights, mat, float3(0, 0, 0),
        SampleNormalT, toEyeT, shadowFactor);

  float4 litColor = ambient + directLight;

	// Add in specular reflections.
  float3 rT = reflect(-toEyeT, SampleNormalT);
  float3 rW = mul(float4(mul(rT, Tangent2Local), 0), gWorld).xyz;
  float4 reflectionColor = gCubeMap.Sample(gsamLinearWrap, rW);
  float3 fresnelFactor = SchlickFresnel(fresnelR0, SampleNormalT, rT);
  litColor.rgb += shininess * fresnelFactor * reflectionColor.rgb;
	
    // Common convention to take alpha from diffuse albedo.
  litColor.a = diffuseAlbedo.a;

  return litColor;
  //return mul(float4(mul(SampleNormalT, Tangent2Local), 0), gWorld);

}


