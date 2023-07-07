#pragma once

#include "../../Common/d3dUtil.h"

using namespace std;
using namespace DirectX;

struct Bone
{
  XMMATRIX Offset;
};

struct Skeleton
{
  vector<Bone> Bones;
  vector<INT32> ParentBoneIndex;
};

struct SkinPart
{
  string MeshName;
  XMMATRIX Bind;
};

struct Skin
{
  vector<SkinPart> SkinMesh;
};

struct KeyFrame
{
  vector<XMVECTOR> Translation;
  vector<XMVECTOR> Quaternion;
  XMMATRIX ToParent(int i)
  {
    return XMMatrixMultiply(XMMatrixRotationQuaternion(Quaternion[i]), XMMatrixTranslationFromVector(Translation[i]));
  }
};

class Animation
{
private:
  
  vector<KeyFrame> KeyFrames;
  
  float PlayTime;
  
  float Interval;

public:
  Animation(float interval = 1) : 
    PlayTime(0), 
    Interval(interval) 
  {};
  
  void AddKey(KeyFrame& key)
  {
    KeyFrames.push_back(key);
  }

  float GetLastTime()
  {
    if (KeyFrames.size() != 0)
    {
      return (KeyFrames.size() - 1) * Interval;
    }
    else
      return 0;
  }

  KeyFrame GetLerpKeyFrame(float deltaTime)
  {
    PlayTime += deltaTime;
    if (PlayTime > GetLastTime())
    {
      PlayTime = 0;
    }

    int leftI = (int)(PlayTime / Interval);
    float t = (PlayTime - leftI * Interval) / Interval;
    int count = KeyFrames[0].Translation.size();
    
    KeyFrame output;
    for (int i = 0; i < count; ++i)
    {
      XMVECTOR tra = XMVectorLerp(KeyFrames[leftI].Translation[i], KeyFrames[leftI + 1].Translation[i], t);
      XMVECTOR rot = XMQuaternionSlerp(KeyFrames[leftI].Quaternion[i], KeyFrames[leftI + 1].Quaternion[i], t);
      output.Translation.push_back(tra);
      output.Quaternion.push_back(rot);
    }

    return output;
  }
};