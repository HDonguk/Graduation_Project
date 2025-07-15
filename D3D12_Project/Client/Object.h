#pragma once
#include "stdafx.h"
#include "Component.h"
#include <variant>

class GameTimer;
class Scene;

enum class ePass;  // Forward declaration

// 기본 Object 클래스만 정의
class Object
{
public:
	Object() = default;
	Object(Scene* root);
	virtual ~Object() = default;

	virtual void OnUpdate(GameTimer& gTimer) = 0;
	virtual void LateUpdate(GameTimer& gTimer) = 0;
	virtual void OnRender(ID3D12Device* device, ID3D12GraphicsCommandList* commandList) = 0;

	void BuildConstantBuffer(ID3D12Device* device);

	template<typename T>
	void AddComponent(T&& component) { m_components.emplace(typeid(T).name(), move(component)); }

	template <typename T>
	T& GetComponent() { return get<T>(m_components.at(typeid(T).name())); }

	template <typename T>
	bool FindComponent() {
		auto& it = m_components.find(typeid(T).name());
		return it != m_components.end();
	}

	void SetActive(bool active) { m_isActive = active; }
	bool IsActive() const { return m_isActive; }

	// 오브젝트 마다 독립적인 CB
	UINT8* m_mappedData;
	ComPtr<ID3D12Resource> m_constantBuffer;

protected:
	Scene* m_root;
	unordered_map<string, ComponentVariant> m_components;

private:
	bool m_isActive = true;
};

class PlayerObject : public Object
{
public:
	PlayerObject() = default;
	PlayerObject(Scene* root);
	void OnUpdate(GameTimer& gTimer) override;
	void LateUpdate(GameTimer& gTimer) override;
	void OnRender(ID3D12Device* device, ID3D12GraphicsCommandList* commandList) override;
	void OnKeyboardInput(const GameTimer& gTimer);
private:
	XMMATRIX mRotation; // 키보드 인풋 함수에서 구했던 카메라 좌표계를 기준으로 하는 회전행렬이다. 이 값을 함수 내부에서 컴포넌트의 rotate에 곱하고 그 결과를 다시 컴포넌트에 저장하면 이 변수는 없어도 될듯. 나중에 고치자.
};

class CameraObject : public Object
{
public:
	CameraObject() = default;
	CameraObject(float radius, Scene* root);
	void OnUpdate(GameTimer& gTimer) override;
	void LateUpdate(GameTimer& gTimer) override;
	void OnRender(ID3D12Device* device, ID3D12GraphicsCommandList* commandList) override;
	void OnMouseInput(WPARAM wParam, HWND hWnd);
	void SetXMMATRIX(XMMATRIX& m);
	XMMATRIX GetXMMATRIX();
private:
	int mLastPosX;
	int mLastPosY;
	float mTheta;
	float mPhi;
	float mRadius;
	XMFLOAT4X4 mViewMatrix;
};

class TerrainObject : public Object
{
public:
	TerrainObject() = default;
	TerrainObject(Scene* root);
	void OnUpdate(GameTimer& gTimer) override;
	void LateUpdate(GameTimer& gTimer) override;
	void OnRender(ID3D12Device* device, ID3D12GraphicsCommandList* commandList) override;
};

class TestObject : public Object
{
public:
	TestObject() = default;
	TestObject(Scene* root);
	void OnUpdate(GameTimer& gTimer) override;
	void LateUpdate(GameTimer& gTimer) override;
	void OnRender(ID3D12Device* device, ID3D12GraphicsCommandList* commandList) override;
};

class TreeObject : public Object
{
public:
	TreeObject() = default;
	TreeObject(Scene* root);
	void OnUpdate(GameTimer& gTimer) override;
	void LateUpdate(GameTimer& gTimer) override;
	void OnRender(ID3D12Device* device, ID3D12GraphicsCommandList* commandList) override;
};

class TigerObject : public Object
{
public:
	TigerObject(Scene* root);
	virtual void OnUpdate(GameTimer& gTimer) override;
	virtual void LateUpdate(GameTimer& gTimer) override;
	virtual void OnRender(ID3D12Device* device, ID3D12GraphicsCommandList* commandList) override;

private:
	XMMATRIX mRotation;
};

class StoneObject : public Object
{
public:
	StoneObject() = default;
	StoneObject(Scene* root);
	void OnUpdate(GameTimer& gTimer) override;
	void LateUpdate(GameTimer& gTimer) override;
	void OnRender(ID3D12Device* device, ID3D12GraphicsCommandList* commandList) override;
};

using ObjectVariant = variant<PlayerObject, CameraObject, TestObject, TerrainObject, TreeObject, TigerObject, StoneObject>;