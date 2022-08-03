#include <Windows.h>
#include <DirectXMath.h>
#include <DirectXPackedVector.h>
#include <iostream>
#include "02_DXMathTest.h"
using namespace std;
using namespace DirectX;
using namespace DirectX::PackedVector;


#define O 0.0f


ostream& XM_CALLCONV operator << (ostream& os, FXMVECTOR v)
{
  XMFLOAT4 unpack;
  XMStoreFloat4(&unpack, v);
  os << unpack.x << ' ' << unpack.y << ' ' << unpack.z << ' ' << unpack.w;
  return os;
}

void PerformanceTimerTest()
{
  __int64 currTime = 0;
  __int64 lastTime = 0;
  __int64 countsPerSecond = 0;
  QueryPerformanceFrequency((LARGE_INTEGER*)&countsPerSecond);
  cout << "Frequency " << countsPerSecond << endl;
  while (true)
  {
    Sleep(500);
    QueryPerformanceCounter((LARGE_INTEGER*)&lastTime);
    system("cls");
    QueryPerformanceCounter((LARGE_INTEGER*)&currTime);
    cout <<"Delta Count "<< currTime - lastTime << endl;
    cout << "Delta Time " << (double)(currTime - lastTime) * (1.0 / countsPerSecond) * 1000.0 << "ms" << endl;
  }

}

int main()
{
  cout << "Hello World!\n";
  if (!XMVerifyCPUSupport())
  {
    cout << "DX math NOT supported" << endl;
    return 0;
  }
  else
  {
    cout << "DX math supported" << endl;
  }

  XMMATRIX A
  (1.0f, 2.0f, O, O,
    O, 2.0f, O, O,
    O, O, 3.0f, O,
    O, O, O, 4.0f);

  XMFLOAT4 _B(1.0f, 2.0f, 3.0f, 4.0f);
  XMVECTOR B = XMLoadFloat4(&_B);

  XMVECTOR det = XMMatrixDeterminant(A);
  cout << det << endl;

  // vector * matrix
  XMVECTOR multiply = XMVector4Transform(B, A);
  cout << multiply << endl;
  PerformanceTimerTest();
  return 0;
}
