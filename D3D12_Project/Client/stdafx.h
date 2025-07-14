#pragma once

// 반드시 가장 먼저 정의되어야 함
#define _WINSOCKAPI_    // windows.h에서 winsock.h를 포함하지 않도록 함
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

// 순서 중요
#include <winsock2.h>
#include <ws2tcpip.h>
#include <mswsock.h>

// windows.h는 winsock2.h 다음에
#include <windows.h>

#pragma comment(lib, "ws2_32.lib")

// DirectX 관련 헤더들
#include <d3d12.h>
#include <dxgi1_6.h>
#include <D3Dcompiler.h>
#include <DirectXMath.h>
#include "d3dx12.h"

#include <string>
#include <wrl.h>
#include <shellapi.h>

#include <unordered_map>
#include <vector>
#include <array>

#include <fbxsdk.h>
#include "DirectXTex.h"
#include "DDSTextureLoader12.h"

using namespace std;
using namespace DirectX;
using Microsoft::WRL::ComPtr;

template<typename T> using ptr = T*;

