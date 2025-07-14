#pragma once
#include <variant>
#include <DirectXCollision.h>
#include "stdafx.h"
#include "Info.h"
#include "FbxExtractor.h"

class Object;

struct Component // ��ü�� ������ �ʴ� Ŭ����
{
	Component() = default;
	Component(Object* root) : mRoot{root} {}
	virtual ~Component() = default;
	Object* mRoot;
};

struct NeedVector // ��ü�� ������ �ʴ� Ŭ����
{
	NeedVector() = default;
	NeedVector(float x, float y, float z, float w) : mFloat4 { x, y, z, w } {}
	XMVECTOR GetXMVECTOR() { return XMLoadFloat4(&mFloat4); }
	void SetXMVECTOR(XMVECTOR& v) {
		XMStoreFloat4(&mFloat4, v);
	}
	XMFLOAT4 mFloat4;
};

//struct World : public Component
//{
//	World() = default;
//	World(Object* root) : Component{ root }, mWorld{} {}
//	XMMATRIX GetXMMATRIX() { return XMLoadFloat4x4(&mWorld); }
//	void SetXMMATRIX(XMMATRIX& m) { XMStoreFloat4x4(&mWorld, m); }
//	XMFLOAT4X4 mWorld;
//};

struct Mesh : public Component, public NeedVector
{ 
	Mesh() = default;
	Mesh(SubMeshData& subMeshData, Object* root) : Component{ root }, mSubMeshData{ subMeshData } {}
	SubMeshData mSubMeshData;
};

struct Texture : public Component
{
	Texture() = default;
	Texture(int descriptorStartIndex, Object* root) : Component{ root }, mDescriptorStartIndex{descriptorStartIndex} {}
	int mDescriptorStartIndex;
};

struct Animation : public Component
{
	Animation() = default;
	Animation(unordered_map<string, SkinnedData>& animData, Object* root) : Component{ root }, mAnimData{ &animData }, mAnimationTime{ 0.f }, mSleepTime{ 0.f }, mCurrentFileName{} {}
	unordered_map<string, SkinnedData>* mAnimData;
	float mAnimationTime;
	float mSleepTime;
	string mCurrentFileName;
};

struct Position : public Component, public NeedVector
{
	Position() = default;
	Position(float x, float y, float z, float w, Object* root) : Component{ root }, NeedVector { x, y, z, w}, mModify(true) {}
	bool mModify;
};

struct Velocity : public Component, public NeedVector
{
	Velocity() = default;
	Velocity(float x, float y, float z, float w, Object* root) : Component{ root }, NeedVector{ x, y, z, w } {}
};

struct Rotation : public Component, public NeedVector
{
	Rotation() = default;
	Rotation(float x, float y, float z, float w, Object* root) : Component{ root }, NeedVector{ x, y, z, w }, mModify(true) {}
	bool mModify;
};

struct Rotate : public Component, public NeedVector
{
	Rotate() = default;
	Rotate(float x, float y, float z, float w, Object* root) : Component{ root }, NeedVector{ x, y, z, w } {}
};

struct Scale : public Component, public NeedVector
{
	Scale() = default;
	Scale(float x, Object* root) : Component{ root }, NeedVector{ x, x, x, 1 }, mScaleValue{x} {}
	float mScaleValue;
};

struct Gravity : public Component, public NeedVector
{
	Gravity() = default;
	Gravity(float value, Object* root) : Component{ root }, NeedVector{ 0.f, -value, 0.f, 0.f}, mGravityTime{0.f} {}
	float mGravityTime;
};

struct Collider : public Component
{
	Collider() = default;
	Collider(float centerX, float centerY, float centerZ, float extentsX, float extentsY, float extentsZ, Object* root) :
		Component{ root },
		mAABB{ XMFLOAT3{centerX, centerY, centerZ}, XMFLOAT3{extentsX, extentsY, extentsZ} },
		mLocalAABB{ XMFLOAT3{centerX, centerY, centerZ}, XMFLOAT3{extentsX, extentsY, extentsZ} } {}
	bool FindCollisionObj(Object* objAddress){
		auto it = mCollisionStates.find(objAddress);
		return it != mCollisionStates.end();
	}
	BoundingBox mAABB;
	BoundingBox mLocalAABB;
	unordered_map<Object*, CollisionState> mCollisionStates;
};

//struct CameraPosition : NeedVector
//{
//	CameraPosition() = default;
//	CameraPosition(float x, float y, float z, float w, Object* root) :
//		NeedVector{ x,y,z,w,root }, mLastPosX{}, mLastPosY{}, mTheta{}, mPhi{} {}
//	int mLastPosX;
//	int mLastPosY;
//	float mTheta;
//	float mPhi;
//};

using ComponentVariant = variant<Mesh, Position, Velocity, Rotation, Rotate, Scale, Texture, Animation, Gravity, Collider>;
