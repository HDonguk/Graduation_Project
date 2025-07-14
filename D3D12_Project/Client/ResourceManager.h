#pragma once
#include "stdafx.h"
#include "FbxExtractor.h"
#include "Info.h"

class Scene;  // 전방 선언

struct TerrainData {
	int terrainWidth;
	int terrainHeight;
	int terrainScale;
};

class ResourceManager
{
	friend class Scene;  // Scene을 friend 클래스로 선언
public:
	ResourceManager();  // private 생성자
	~ResourceManager();
	void LoadFbx(const string& fileName, bool onlyAnimation, bool zUp);
	void CreatePlane(const string& name, float size);
	void CreateTerrain(const string& name, int maxheight, int scale, int maxUV);
	vector<Vertex>& GetVertexBuffer();
	vector<uint32_t>& GetIndexBuffer();
	unordered_map<string, SubMeshData>& GetSubMeshData();
	unordered_map<string, SkinnedData>& GetAnimationData();
	TerrainData& GetTerrainData();

private:
	unique_ptr<FbxExtractor> mFbxExtractor;
	vector<Vertex> mVertexBuffer;
	vector<uint32_t> mIndexBuffer;
	unordered_map<string, SubMeshData> mSubMeshData;
	unordered_map<string, SkinnedData> mAnimData;

	TerrainData mTerrainData;
};

