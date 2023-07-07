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
  float3 NormalW : NORMAL;
  float3 TangentW : TANGENT;
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
  vout.NormalW = mul(vin.NormalL, (float3x3) gWorld);
	
  vout.TangentW = mul(vin.TangentU, (float3x3) gWorld);

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
  pin.NormalW = normalize(pin.NormalW);
	
  float4 normalMapSample = gTextureMaps[normalMapIndex].Sample(gsamAnisotropicWrap, pin.TexC);
  float3 bumpedNormalW = NormalSampleToWorldSpace(normalMapSample.rgb, pin.NormalW, pin.TangentW);

	// Uncomment to turn off normal mapping.
	//bumpedNormalW = pin.NormalW;

	// Dynamically look up the texture in the array.
  diffuseAlbedo *= gTextureMaps[diffuseMapIndex].Sample(gsamAnisotropicWrap, pin.TexC);

    // Vector from point being lit to eye. 
  float3 toEyeW = normalize(gEyePosW - pin.PosW);

    // Light terms.
  float4 ambient = gAmbientLight * diffuseAlbedo;

  const float shininess = (1.0f - roughness) * normalMapSample.a;
  Material mat = { diffuseAlbedo, fresnelR0, shininess };
  float3 shadowFactor = 1.0f;
  float4 directLight = ComputeLighting(gLights, mat, pin.PosW,
        bumpedNormalW, toEyeW, shadowFactor);

  float4 litColor = ambient + directLight;

	// Add in specular reflections.
  float3 r = reflect(-toEyeW, bumpedNormalW);
  float4 reflectionColor = gCubeMap.Sample(gsamLinearWrap, r);
  float3 fresnelFactor = SchlickFresnel(fresnelR0, bumpedNormalW, r);
  litColor.rgb += shininess * fresnelFactor * reflectionColor.rgb;
	
    // Common convention to take alpha from diffuse albedo.
  litColor.a = diffuseAlbedo.a;

  return litColor;
}

// 曲面细分和几何着色器
struct PatchTess
{
  float EdgeTess[3] : SV_TessFactor;
  float InsideTess[1] : SV_InsideTessFactor;
};

PatchTess ConstantHS(InputPatch<VertexOut, 3> patch, uint patchID : SV_PrimitiveID)
{
  PatchTess pt;
	
  pt.EdgeTess[0] = 8;
  pt.EdgeTess[1] = 8;
  pt.EdgeTess[2] = 8;
	
  pt.InsideTess[0] = 8;
	
  return pt;
}

[domain("tri")]
[partitioning("integer")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(3)]
[patchconstantfunc("ConstantHS")]
[maxtessfactor(64.0f)]
VertexOut HS(InputPatch<VertexOut, 3> p,
           uint i : SV_OutputControlPointID,
           uint patchId : SV_PrimitiveID)
{	
  return p[i];
}

// The domain shader is called for every vertex created by the tessellator.  
// It is like the vertex shader after tessellation.
[domain("tri")]
VertexOut DS(PatchTess patchTess,
             float3 coord : SV_DomainLocation,
             const OutputPatch<VertexOut, 3> tri)
{
  VertexOut dout;
	
  dout.PosW = tri[0].PosW * coord.x + tri[1].PosW * coord.y + tri[2].PosW * coord.z;
  dout.NormalW = tri[0].NormalW * coord.x + tri[1].NormalW * coord.y + tri[2].NormalW * coord.z;
  dout.TangentW = tri[0].TangentW * coord.x + tri[1].TangentW * coord.y + tri[2].TangentW * coord.z;
  dout.TexC = tri[0].TexC * coord.x + tri[1].TexC * coord.y + tri[2].TexC * coord.z;
  
  MaterialData matData = gMaterialData[gMaterialIndex];
  float4 diffuseColor = gTextureMaps[matData.DiffuseMapIndex].SampleLevel(gsamLinearWrap, dout.TexC, 0);
  float height = 0.299 * diffuseColor.r + 0.587 * diffuseColor.g + 0.114 * diffuseColor.b;
  height = height * 0.5;
  dout.PosW.y += height;
  dout.PosH = mul(float4(dout.PosW, 1), gViewProj);
  
  return dout;
}
