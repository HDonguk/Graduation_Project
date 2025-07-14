#pragma once
#include "stdafx.h"
#include "Info.h"

struct State {
	State() : GenerateNormals{ false }, UV{ true } {}
	void Reset() {
		GenerateNormals = false;
		UV = true;
	}

	bool GenerateNormals;
	bool UV;
};

class FbxExtractor
{
public:
	FbxExtractor();
	~FbxExtractor();
	bool ImportFbxFile(const std::string&);
	bool ConvertScenePolygonsTriangulate();
	void ExtractMeshData(std::vector<Vertex>&);
	State GetState();
private:
	void Clear();
	void RemoveBadPolygons();
	bool ConvertNodeAttributePolygonsTriangulate(ptr<FbxNode>&);
	void CheckMeshDataState(ptr<FbxMesh>&);

	ptr<FbxManager> mFbxManager;
	ptr<FbxIOSettings> mFbxIOS;
	ptr<FbxImporter> mFbxImporter;
	ptr<FbxScene> mFbxScene;
	
	State mState;
};

