//***************************************************************************************
// BoxApp.cpp by Frank Luna (C) 2015 All Rights Reserved.
//
// Shows how to draw a box in Direct3D 12.
//
// Controls:
//   Hold the left mouse button down and move the mouse to rotate.
//   Hold the right mouse button down and move the mouse to zoom in and out.
//***************************************************************************************

#include "../../Common/d3dApp.h"
#include "../../Common/MathHelper.h"
#include "../../Common/UploadBuffer.h"
#include "../../Common/GeometryGenerator.h"
#include "BoneAnimation.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;

struct Vertex
{
  XMFLOAT3 Pos;
  XMFLOAT3 Normal;
};

struct ObjectConstants
{
  XMFLOAT4X4 WorldViewProjection = MathHelper::Identity4x4();
  XMFLOAT4X4 InverseWorld = MathHelper::Identity4x4();
};

class BoxApp : public D3DApp
{
public:
  BoxApp(HINSTANCE hInstance);
  BoxApp(const BoxApp& rhs) = delete;
  BoxApp& operator=(const BoxApp& rhs) = delete;
  ~BoxApp();

  virtual bool Initialize()override;

private:
  virtual void OnResize()override;
  virtual void Update(const GameTimer& gt)override;
  virtual void Draw(const GameTimer& gt)override;
  virtual void DrawSkin(ID3D12GraphicsCommandList* cmdList, const GameTimer& gt);

  virtual void OnMouseDown(WPARAM btnState, int x, int y)override;
  virtual void OnMouseUp(WPARAM btnState, int x, int y)override;
  virtual void OnMouseMove(WPARAM btnState, int x, int y)override;

  void BuildDescriptorHeaps();
  void BuildConstantBuffersAndStructuredBuffers();
  void BuildRootSignature();
  void BuildShadersAndInputLayout();
  void BuildGeometry();
  void BuildBoneAndAnimation();
  void BuildPSO();

private:

  ComPtr<ID3D12RootSignature> mRootSignature = nullptr;
  ComPtr<ID3D12DescriptorHeap> mCbvHeap = nullptr;

  std::unique_ptr<UploadBuffer<ObjectConstants>> mObjectCB = nullptr;
  std::unique_ptr<UploadBuffer<XMFLOAT4X4[5]>> mBindMatrix = nullptr;
  std::unique_ptr<UploadBuffer<XMFLOAT4X4[5]>> mFinalTransform = nullptr;

  std::unique_ptr<MeshGeometry> mCylinderGeo = nullptr;
  std::unique_ptr<Skin> mSkin = nullptr;
  std::unique_ptr<Skeleton> mSkeleton = nullptr;
  std::unique_ptr<Animation> mAnimation = nullptr;

  ComPtr<ID3DBlob> mvsByteCode = nullptr;
  ComPtr<ID3DBlob> mpsByteCode = nullptr;

  std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;

  ComPtr<ID3D12PipelineState> mPSO = nullptr;

  XMFLOAT4X4 mWorld = MathHelper::Identity4x4();
  XMFLOAT4X4 mView = MathHelper::Identity4x4();
  XMFLOAT4X4 mProj = MathHelper::Identity4x4();

  float mTheta = 1.5f * XM_PI;
  float mPhi = XM_PIDIV4;
  float mRadius = 5.0f;

  POINT mLastMousePos;
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance,
  PSTR cmdLine, int showCmd)
{
  // Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
  _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

  try
  {
    BoxApp theApp(hInstance);
    if (!theApp.Initialize())
      return 0;

    return theApp.Run();
  }
  catch (DxException& e)
  {
    MessageBox(nullptr, e.ToString().c_str(), L"HR Failed", MB_OK);
    return 0;
  }
}

BoxApp::BoxApp(HINSTANCE hInstance)
  : D3DApp(hInstance)
{
}

BoxApp::~BoxApp()
{
}

bool BoxApp::Initialize()
{
  if (!D3DApp::Initialize())
    return false;

  // Reset the command list to prep for initialization commands.
  ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

  BuildDescriptorHeaps();
  BuildConstantBuffersAndStructuredBuffers();
  BuildRootSignature();
  BuildShadersAndInputLayout();
  BuildGeometry();
  BuildBoneAndAnimation();
  BuildPSO();

  // Execute the initialization commands.
  ThrowIfFailed(mCommandList->Close());
  ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
  mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

  // Wait until initialization is complete.
  FlushCommandQueue();

  return true;
}

void BoxApp::OnResize()
{
  D3DApp::OnResize();

  // The window resized, so update the aspect ratio and recompute the projection matrix.
  XMMATRIX P = XMMatrixPerspectiveFovLH(0.25f * MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);
  XMStoreFloat4x4(&mProj, P);
}

void BoxApp::Update(const GameTimer& gt)
{
  // Convert Spherical to Cartesian coordinates.
  float x = mRadius * sinf(mPhi) * cosf(mTheta);
  float z = mRadius * sinf(mPhi) * sinf(mTheta);
  float y = mRadius * cosf(mPhi);

  // Build the view matrix.
  XMVECTOR pos = XMVectorSet(x, y, z, 1.0f);
  XMVECTOR target = XMVectorZero();
  XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

  XMMATRIX view = XMMatrixLookAtLH(pos, target, up);
  XMStoreFloat4x4(&mView, view);

  XMMATRIX world = XMLoadFloat4x4(&mWorld);
  XMMATRIX invWorld = XMMatrixInverse(&XMMatrixDeterminant(world), world);
  XMMATRIX proj = XMLoadFloat4x4(&mProj);
  XMMATRIX worldViewProj = world * view * proj;

  // Update the constant buffer with the latest worldViewProj matrix.
  ObjectConstants objConstants;
  XMStoreFloat4x4(&objConstants.WorldViewProjection, XMMatrixTranspose(worldViewProj));
  XMStoreFloat4x4(&objConstants.InverseWorld, XMMatrixTranspose(invWorld));
  mObjectCB->CopyData(0, objConstants);

  // Bind Matrix
  XMFLOAT4X4 bind[5];
  for (int i = 0; i < 5; ++i)
  {
    XMStoreFloat4x4(&bind[i], XMMatrixTranspose(mSkin->SkinMesh[i].Bind));
  }
  mBindMatrix->CopyData(0, bind);

  // Final Matrix
  KeyFrame keyFrame = mAnimation->GetLerpKeyFrame(gt.DeltaTime());
  XMFLOAT4X4 final[5];
  XMMATRIX ToRoot[5];

  for (int i = 0; i < 5; ++i)
  {
    int parent = mSkeleton->ParentBoneIndex[i];
    
    // Compute every ToRoot
    ToRoot[i] = (i == 0) ? keyFrame.ToParent(0) : XMMatrixMultiply(keyFrame.ToParent(i), ToRoot[parent]);

    // Compute every FinalTransform
    XMStoreFloat4x4(&final[i], XMMatrixTranspose(XMMatrixMultiply(mSkeleton->Bones[i].Offset, ToRoot[i])));
    // XMStoreFloat4x4(&final[i], XMMatrixTranspose(XMMatrixIdentity()));
  }
  mFinalTransform->CopyData(0, final);
}

void BoxApp::Draw(const GameTimer& gt)
{
  // Reuse the memory associated with command recording.
  // We can only reset when the associated command lists have finished execution on the GPU.
  ThrowIfFailed(mDirectCmdListAlloc->Reset());

  // A command list can be reset after it has been added to the command queue via ExecuteCommandList.
    // Reusing the command list reuses memory.
  ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), mPSO.Get()));

  mCommandList->RSSetViewports(1, &mScreenViewport);
  mCommandList->RSSetScissorRects(1, &mScissorRect);

  // Indicate a state transition on the resource usage.
  mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
    D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

  // Clear the back buffer and depth buffer.
  mCommandList->ClearRenderTargetView(CurrentBackBufferView(), Colors::LightSteelBlue, 0, nullptr);
  mCommandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

  // Specify the buffers we are going to render to.
  mCommandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());

  ID3D12DescriptorHeap* descriptorHeaps[] = { mCbvHeap.Get() };
  mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

  mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

  mCommandList->IASetVertexBuffers(0, 1, &mCylinderGeo->VertexBufferView());
  mCommandList->IASetIndexBuffer(&mCylinderGeo->IndexBufferView());
  mCommandList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

  mCommandList->SetGraphicsRootDescriptorTable(0, mCbvHeap->GetGPUDescriptorHandleForHeapStart());

  DrawSkin(mCommandList.Get(), gt);

  // Indicate a state transition on the resource usage.
  mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
    D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

  // Done recording commands.
  ThrowIfFailed(mCommandList->Close());

  // Add the command list to the queue for execution.
  ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
  mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

  // swap the back and front buffers
  ThrowIfFailed(mSwapChain->Present(0, 0));
  mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

  // Wait until frame commands are complete.  This waiting is inefficient and is
  // done for simplicity.  Later we will show how to organize our rendering code
  // so we do not have to wait per frame.
  FlushCommandQueue();
}

void BoxApp::DrawSkin(ID3D12GraphicsCommandList* cmdList, const GameTimer& gt)
{
  cmdList->SetGraphicsRootShaderResourceView(1, mBindMatrix->Resource()->GetGPUVirtualAddress());
  cmdList->SetGraphicsRootShaderResourceView(2, mFinalTransform->Resource()->GetGPUVirtualAddress());

  int count = mSkin->SkinMesh.size();
  cmdList->DrawIndexedInstanced(mCylinderGeo->DrawArgs["cylinder"].IndexCount, count, 0, 0, 0);
}

void BoxApp::OnMouseDown(WPARAM btnState, int x, int y)
{
  mLastMousePos.x = x;
  mLastMousePos.y = y;

  SetCapture(mhMainWnd);
}

void BoxApp::OnMouseUp(WPARAM btnState, int x, int y)
{
  ReleaseCapture();
}

void BoxApp::OnMouseMove(WPARAM btnState, int x, int y)
{
  if ((btnState & MK_LBUTTON) != 0)
  {
    // Make each pixel correspond to a quarter of a degree.
    float dx = XMConvertToRadians(0.25f * static_cast<float>(x - mLastMousePos.x));
    float dy = XMConvertToRadians(0.25f * static_cast<float>(y - mLastMousePos.y));

    // Update angles based on input to orbit camera around box.
    mTheta += dx;
    mPhi += dy;

    // Restrict the angle mPhi.
    mPhi = MathHelper::Clamp(mPhi, 0.1f, MathHelper::Pi - 0.1f);
  }
  else if ((btnState & MK_RBUTTON) != 0)
  {
    // Make each pixel correspond to 0.005 unit in the scene.
    float dx = 0.005f * static_cast<float>(x - mLastMousePos.x);
    float dy = 0.005f * static_cast<float>(y - mLastMousePos.y);

    // Update the camera radius based on input.
    mRadius += dx - dy;

    // Restrict the radius.
    mRadius = MathHelper::Clamp(mRadius, 3.0f, 15.0f);
  }

  mLastMousePos.x = x;
  mLastMousePos.y = y;
}

void BoxApp::BuildDescriptorHeaps()
{
  D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc;
  cbvHeapDesc.NumDescriptors = 1;
  cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
  cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
  cbvHeapDesc.NodeMask = 0;
  ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&cbvHeapDesc,
    IID_PPV_ARGS(&mCbvHeap)));
}

void BoxApp::BuildConstantBuffersAndStructuredBuffers()
{
  mObjectCB = std::make_unique<UploadBuffer<ObjectConstants>>(md3dDevice.Get(), 1, true);

  UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));

  D3D12_GPU_VIRTUAL_ADDRESS cbAddress = mObjectCB->Resource()->GetGPUVirtualAddress();
  // Offset to the ith object constant buffer in the buffer.
  int boxCBufIndex = 0;
  cbAddress += boxCBufIndex * objCBByteSize;

  D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
  cbvDesc.BufferLocation = cbAddress;
  cbvDesc.SizeInBytes = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));

  md3dDevice->CreateConstantBufferView(
    &cbvDesc,
    mCbvHeap->GetCPUDescriptorHandleForHeapStart());

  mBindMatrix = std::make_unique<UploadBuffer<XMFLOAT4X4[5]>>(md3dDevice.Get(), 1, false);
  mFinalTransform = std::make_unique<UploadBuffer<XMFLOAT4X4[5]>>(md3dDevice.Get(), 1, false);
}

void BoxApp::BuildRootSignature()
{
  // Shader programs typically require resources as input (constant buffers,
  // textures, samplers).  The root signature defines the resources the shader
  // programs expect.  If we think of the shader programs as a function, and
  // the input resources as function parameters, then the root signature can be
  // thought of as defining the function signature.  

  // Root parameter can be a table, root descriptor or root constants.
  CD3DX12_ROOT_PARAMETER slotRootParameter[3];

  // Create a single descriptor table of CBVs.
  CD3DX12_DESCRIPTOR_RANGE cbvTable1;
  cbvTable1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
  slotRootParameter[0].InitAsDescriptorTable(1, &cbvTable1);
  slotRootParameter[1].InitAsShaderResourceView(0);
  slotRootParameter[2].InitAsShaderResourceView(1);

  // A root signature is an array of root parameters.
  CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(3, slotRootParameter, 0, nullptr,
    D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

  // create a root signature with a single slot which points to a descriptor range consisting of a single constant buffer
  ComPtr<ID3DBlob> serializedRootSig = nullptr;
  ComPtr<ID3DBlob> errorBlob = nullptr;
  HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
    serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

  if (errorBlob != nullptr)
  {
    ::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
  }
  ThrowIfFailed(hr);

  ThrowIfFailed(md3dDevice->CreateRootSignature(
    0,
    serializedRootSig->GetBufferPointer(),
    serializedRootSig->GetBufferSize(),
    IID_PPV_ARGS(&mRootSignature)));
}

void BoxApp::BuildShadersAndInputLayout()
{
  HRESULT hr = S_OK;

  mvsByteCode = d3dUtil::CompileShader(L"Shaders\\color.hlsl", nullptr, "VS", "vs_5_0");
  mpsByteCode = d3dUtil::CompileShader(L"Shaders\\color.hlsl", nullptr, "PS", "ps_5_0");

  mInputLayout =
  {
      { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
      { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
  };
}

void BoxApp::BuildGeometry()
{
  GeometryGenerator geoGenerator;
  GeometryGenerator::MeshData mesh = geoGenerator.CreateCylinder(0.1, 0.1, 1, 16, 1);
  std::vector<Vertex> vertices;
  for (GeometryGenerator::Vertex& vData : mesh.Vertices)
  {
    Vertex v{ vData.Position, vData.Normal };
    vertices.push_back(v);
  }

  const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
  const UINT ibByteSize = (UINT)mesh.GetIndices16().size() * sizeof(std::uint16_t);

  mCylinderGeo = std::make_unique<MeshGeometry>();
  mCylinderGeo->Name = "cylinderGeo";

  mCylinderGeo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
    mCommandList.Get(), vertices.data(), vbByteSize, mCylinderGeo->VertexBufferUploader);

  mCylinderGeo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
    mCommandList.Get(), mesh.GetIndices16().data(), ibByteSize, mCylinderGeo->IndexBufferUploader);

  mCylinderGeo->VertexByteStride = sizeof(Vertex);
  mCylinderGeo->VertexBufferByteSize = vbByteSize;
  mCylinderGeo->IndexFormat = DXGI_FORMAT_R16_UINT;
  mCylinderGeo->IndexBufferByteSize = ibByteSize;

  SubmeshGeometry submesh;
  submesh.IndexCount = (UINT)mesh.GetIndices16().size();
  submesh.StartIndexLocation = 0;
  submesh.BaseVertexLocation = 0;

  mCylinderGeo->DrawArgs["cylinder"] = submesh;

  // Build skin
  mSkin = std::make_unique<Skin>();

  SkinPart part1;
  part1.MeshName = "cylinder";
  part1.Bind = XMMatrixTranslation(0, 0.5f, 0);
  mSkin->SkinMesh.push_back(part1);

  SkinPart part2;
  part2.MeshName = "cylinder";
  part2.Bind = XMMatrixMultiply(XMMatrixRotationZ(MathHelper::Pi * 0.5f), XMMatrixTranslation(-0.5f, 1.0f, 0));
  mSkin->SkinMesh.push_back(part2);

  SkinPart part3;
  part3.MeshName = "cylinder";
  part3.Bind = XMMatrixMultiply(XMMatrixRotationZ(MathHelper::Pi * 0.5f), XMMatrixTranslation(0.5f, 1.0f, 0));
  mSkin->SkinMesh.push_back(part3);

  SkinPart part4;
  part4.MeshName = "cylinder";
  part4.Bind = XMMatrixTranslation(-1.0f, 1.5f, 0);
  mSkin->SkinMesh.push_back(part4);

  SkinPart part5;
  part5.MeshName = "cylinder";
  part5.Bind = XMMatrixTranslation(1.0f, 1.5f, 0);
  mSkin->SkinMesh.push_back(part5);
}

void BoxApp::BuildBoneAndAnimation()
{
  XMMATRIX translation;

  mSkeleton = std::make_unique<Skeleton>();
  Bone bone1;
  translation = XMMatrixIdentity();
  bone1.Offset = XMMatrixInverse(&XMMatrixDeterminant(translation), translation);
  mSkeleton->Bones.push_back(bone1);
  mSkeleton->ParentBoneIndex.push_back(-1);

  Bone bone2;
  translation = XMMatrixMultiply(XMMatrixRotationZ(MathHelper::Pi * 0.5f), XMMatrixTranslation(0, 1.0f, 0));
  bone2.Offset = XMMatrixInverse(&XMMatrixDeterminant(translation), translation);
  mSkeleton->Bones.push_back(bone2);
  mSkeleton->ParentBoneIndex.push_back(0);

  Bone bone3;
  translation = XMMatrixMultiply(XMMatrixRotationZ(MathHelper::Pi * -0.5f), XMMatrixTranslation(0, 1.0f, 0));
  bone3.Offset = XMMatrixInverse(&XMMatrixDeterminant(translation), translation);
  mSkeleton->Bones.push_back(bone3);
  mSkeleton->ParentBoneIndex.push_back(0);

  Bone bone4;
  translation = XMMatrixTranslation(-1, 1, 0);
  bone4.Offset = XMMatrixInverse(&XMMatrixDeterminant(translation), translation);
  mSkeleton->Bones.push_back(bone4);
  mSkeleton->ParentBoneIndex.push_back(1);

  Bone bone5;
  translation = XMMatrixTranslation(1, 1, 0);
  bone5.Offset = XMMatrixInverse(&XMMatrixDeterminant(translation), translation);
  mSkeleton->Bones.push_back(bone5);
  mSkeleton->ParentBoneIndex.push_back(2);

  // Make animation
  mAnimation = std::make_unique<Animation>(1.0f);

  KeyFrame key[5];
  key[0].Translation.push_back(XMVectorSet(0, 0, 0, 1));
  key[0].Translation.push_back(XMVectorSet(0, 1, 0, 1));
  key[0].Translation.push_back(XMVectorSet(0, 1, 0, 1));
  key[0].Translation.push_back(XMVectorSet(0, 1, 0, 1));
  key[0].Translation.push_back(XMVectorSet(0, 1, 0, 1));
  key[0].Quaternion.push_back(XMQuaternionIdentity());
  key[0].Quaternion.push_back(XMQuaternionRotationRollPitchYaw(0, 0, MathHelper::Pi * 0.5f));
  key[0].Quaternion.push_back(XMQuaternionRotationRollPitchYaw(0, 0, MathHelper::Pi * -0.5f));
  key[0].Quaternion.push_back(XMQuaternionRotationRollPitchYaw(0, 0, MathHelper::Pi * -0.5f));
  key[0].Quaternion.push_back(XMQuaternionRotationRollPitchYaw(0, 0, MathHelper::Pi * 0.5f));
  key[1].Translation.push_back(XMVectorSet(0, 0, 0, 1));
  key[1].Translation.push_back(XMVectorSet(0, 1, 0, 1));
  key[1].Translation.push_back(XMVectorSet(0, 1, 0, 1));
  key[1].Translation.push_back(XMVectorSet(0, 1, 0, 1));
  key[1].Translation.push_back(XMVectorSet(0, 1, 0, 1));
  key[1].Quaternion.push_back(XMQuaternionRotationRollPitchYaw(MathHelper::Pi * 0.25f, 0, 0));
  key[1].Quaternion.push_back(XMQuaternionRotationRollPitchYaw(0, MathHelper::Pi * 0.15f, MathHelper::Pi * 0.5f));
  key[1].Quaternion.push_back(XMQuaternionRotationRollPitchYaw(0, MathHelper::Pi * -0.15f, MathHelper::Pi * -0.5f));
  key[1].Quaternion.push_back(XMQuaternionRotationRollPitchYaw(0, 0, MathHelper::Pi * -0.35f));
  key[1].Quaternion.push_back(XMQuaternionRotationRollPitchYaw(0, 0, MathHelper::Pi * 0.65f));
  key[2] = key[0];
  key[3].Translation.push_back(XMVectorSet(0, 0, 0, 1));
  key[3].Translation.push_back(XMVectorSet(0, 1, 0, 1));
  key[3].Translation.push_back(XMVectorSet(0, 1, 0, 1));
  key[3].Translation.push_back(XMVectorSet(0, 1, 0, 1));
  key[3].Translation.push_back(XMVectorSet(0, 1, 0, 1));
  key[3].Quaternion.push_back(XMQuaternionRotationRollPitchYaw(MathHelper::Pi * -0.25f, 0, 0));
  key[3].Quaternion.push_back(XMQuaternionRotationRollPitchYaw(0, MathHelper::Pi * -0.15f, MathHelper::Pi * 0.5f));
  key[3].Quaternion.push_back(XMQuaternionRotationRollPitchYaw(0, MathHelper::Pi * 0.15f, MathHelper::Pi * -0.5f));
  key[3].Quaternion.push_back(XMQuaternionRotationRollPitchYaw(0, 0, MathHelper::Pi * -0.65f));
  key[3].Quaternion.push_back(XMQuaternionRotationRollPitchYaw(0, 0, MathHelper::Pi * 0.35f));
  key[4] = key[0];
  
  for (KeyFrame& k : key)
  {
    mAnimation->AddKey(k);
  }
}

void BoxApp::BuildPSO()
{
  D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc;
  ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
  psoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
  psoDesc.pRootSignature = mRootSignature.Get();
  psoDesc.VS =
  {
    reinterpret_cast<BYTE*>(mvsByteCode->GetBufferPointer()),
    mvsByteCode->GetBufferSize()
  };
  psoDesc.PS =
  {
    reinterpret_cast<BYTE*>(mpsByteCode->GetBufferPointer()),
    mpsByteCode->GetBufferSize()
  };
  psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
  psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
  psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
  psoDesc.SampleMask = UINT_MAX;
  psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  psoDesc.NumRenderTargets = 1;
  psoDesc.RTVFormats[0] = mBackBufferFormat;
  psoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
  psoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
  psoDesc.DSVFormat = mDepthStencilFormat;
  ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSO)));
}