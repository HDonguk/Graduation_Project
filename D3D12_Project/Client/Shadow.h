#pragma once
#include <DirectXCollision.h>
#include "stdafx.h"

class Scene;

class Shadow
{
public:
	Shadow(Scene* parent, UINT width, UINT height);
	Shadow(const Shadow&) = delete;
	Shadow& operator=(const Shadow&) = delete;
	~Shadow() = default;

	void UpdateShadow();
	void DrawShadowMap();
	Scene* GetScene();
	D3D12_GPU_DESCRIPTOR_HANDLE& GetGpuDescHandleForShadow();
	D3D12_GPU_DESCRIPTOR_HANDLE& GetGpuDescHandleForNullShadow();
private:
	void BuildResource();
	void BuildDescView();
private:
	Scene* mParent = nullptr;

	BoundingSphere mSceneSphere;
	XMFLOAT3 mLightDirection;
	XMFLOAT4X4 mViewMatrix;
	XMFLOAT4X4 mProjMatrix;
	XMFLOAT4X4 mTextureMatrix;
	XMFLOAT4X4 mFinalMatrix;

	D3D12_CPU_DESCRIPTOR_HANDLE mSrvCpuHandle;
	D3D12_GPU_DESCRIPTOR_HANDLE mSrvGpuHandle;
	D3D12_CPU_DESCRIPTOR_HANDLE mNullSrvCpuHandle;
	D3D12_GPU_DESCRIPTOR_HANDLE mNullSrvGpuHandle;
	D3D12_CPU_DESCRIPTOR_HANDLE mDsvCpuHandle;

	D3D12_VIEWPORT mViewport;
	D3D12_RECT mScissorRect;

	UINT mWidth = 0;
	UINT mHeight = 0;

	ComPtr<ID3D12Resource> mShadowMap = nullptr;
	ComPtr<ID3D12Resource> mShadowPassBuffer = nullptr;
};

