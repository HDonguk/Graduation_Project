#include "FbxExtractor.h"

FbxExtractor::FbxExtractor() : 
	mFbxManager{nullptr}, 
	mFbxIOS{nullptr}, 
	mFbxImporter{nullptr}, 
	mFbxScene{nullptr},
	mState{}
{
	mFbxManager = FbxManager::Create();
	mFbxIOS = FbxIOSettings::Create(mFbxManager, IOSROOT);
	mFbxManager->SetIOSettings(mFbxIOS);
}

FbxExtractor::~FbxExtractor()
{
	mFbxManager->Destroy();
}

bool FbxExtractor::ImportFbxFile(const std::string& fileName)
{
	mFbxImporter = FbxImporter::Create(mFbxManager, "");
	if(!mFbxImporter->Initialize(fileName.c_str())) return false;

	mFbxScene = FbxScene::Create(mFbxManager, "");
	if(!mFbxImporter->Import(mFbxScene)) return false;

	mFbxImporter->Destroy();
	return true;
}

//If the cleaned-up mesh becomes invalid, it is removed entirely.
void FbxExtractor::RemoveBadPolygons()
{
	FbxGeometryConverter lConverter{ mFbxManager };
	lConverter.RemoveBadPolygonsFromMeshes(mFbxScene);
}

bool FbxExtractor::ConvertScenePolygonsTriangulate()
{
	FbxGeometryConverter lConverter{ mFbxManager };
	return lConverter.Triangulate(mFbxScene, true);
}

bool FbxExtractor::ConvertNodeAttributePolygonsTriangulate(ptr<FbxNode>& node)
{
	FbxGeometryConverter lConverter{ mFbxManager };
	ptr<FbxNodeAttribute> lNewNodeAttribute = lConverter.Triangulate(node->GetNodeAttribute(), true);
	if (!lNewNodeAttribute) return false;
	node->SetNodeAttribute(lNewNodeAttribute);
	return true;

}

void FbxExtractor::ExtractMeshData(std::vector<Vertex>& pVertices)
{
	ptr<FbxNode> lRootNode = mFbxScene->GetRootNode();
	for (int i = 0; i < lRootNode->GetChildCount(); ++i) {
		ptr<FbxNode> lNode = lRootNode->GetChild(i);
		if (!lNode || !lNode->GetNodeAttribute() || lNode->GetNodeAttribute()->GetAttributeType() != FbxNodeAttribute::eMesh) continue;

		ptr<FbxMesh> lMesh = lNode->GetMesh();
		if (lMesh->GetPolygonSize(0) != 3) {
			if (!ConvertNodeAttributePolygonsTriangulate(lNode)) throw std::runtime_error("Failed triangulate");
			lMesh = lNode->GetMesh(); // 삼각형화 했다면 FbxMesh 다시 얻어오기
		}

		CheckMeshDataState(lMesh);
		
		ptr<int> lVertices = lMesh->GetPolygonVertices();
		ptr<FbxGeometryElementNormal> lNormal = lMesh->GetElementNormal();
		ptr<FbxGeometryElementUV> lUV = lMesh->GetElementUV();

		int polygonVertexCount = lMesh->GetPolygonVertexCount();
		for (int i = 0; i < polygonVertexCount; ++i) {
			Vertex lVertex{};
			FbxVector4 lVector4 = lMesh->GetControlPointAt(lVertices[i]);
			lVertex.position.x = (float)lVector4[0];
			lVertex.position.y = (float)lVector4[1];
			lVertex.position.z = (float)lVector4[2];

			if (lNormal->GetReferenceMode() == FbxGeometryElement::eDirect) {
				lVector4 = lNormal->GetDirectArray().GetAt(i);
			}
			else {
				int j = lNormal->GetIndexArray().GetAt(i);
				lVector4 = lNormal->GetDirectArray().GetAt(j);
			}
			lVertex.normal.x = (float)lVector4[0];
			lVertex.normal.y = (float)lVector4[1];
			lVertex.normal.z = (float)lVector4[2];

			if (mState.UV) {
				FbxVector2 lVector2;
				if (lUV->GetReferenceMode() == FbxGeometryElement::eDirect) {
					lVector2 = lUV->GetDirectArray().GetAt(i);
				}
				else {
					int j = lUV->GetIndexArray().GetAt(i);
					lVector2 = lUV->GetDirectArray().GetAt(j);
				}
				lVertex.uv.x = (float)lVector2[0];
				lVertex.uv.y = 1.0f - (float)lVector2[1];
			}
			else {
				lVertex.uv.x = 0.5f;//(float)(i / polygonVertexCount);
				lVertex.uv.y = 0.5f;//(float)(i / polygonVertexCount);
			}
			pVertices.push_back(lVertex);
		}

		break;
	}
	Clear();
	mFbxScene->Destroy();
}

State FbxExtractor::GetState()
{
	return mState;
}

void FbxExtractor::Clear()
{
}

void FbxExtractor::CheckMeshDataState(ptr<FbxMesh>& pMesh)
{

	int lVertexCount = pMesh->GetPolygonVertexCount();
	
	// check normal
	ptr<FbxGeometryElementNormal> lNormal = pMesh->GetElementNormal();
	if (!lNormal || lNormal->GetMappingMode() != FbxGeometryElement::eByPolygonVertex) {
		if (!pMesh->GenerateNormals(true, false, false)) 
			throw std::runtime_error("Error At GenerateNormals");
		mState.GenerateNormals = true;
	}

	if (lNormal->GetMappingMode() == FbxGeometryElement::eByPolygonVertex) {
		if (lNormal->GetReferenceMode() == FbxGeometryElement::eDirect) {
			if (lVertexCount != lNormal->GetDirectArray().GetCount()) 
				throw std::runtime_error("Normal(Direct) count");
		}
		else if (lNormal->GetReferenceMode() == FbxGeometryElement::eIndexToDirect) {
			if (lVertexCount != lNormal->GetIndexArray().GetCount())
				throw std::runtime_error("Normal(Index) count");
		}
		else {
			throw std::runtime_error("Normal Reference Mode");
		}
	}
	else {
		throw std::runtime_error("Normal mapping mode");
	}

	// check uv
	ptr<FbxGeometryElementUV> lUV = pMesh->GetElementUV();
	if (!lUV) {
		mState.UV = false;
		OutputDebugStringA("No uv\n");
	}
	else {
		mState.UV = true;
		if (lUV->GetMappingMode() != FbxGeometryElement::eByPolygonVertex) {
			throw std::runtime_error("UV mapping mode");
		}

		if (lUV->GetReferenceMode() == FbxGeometryElement::eDirect) {
			if (lVertexCount != lUV->GetDirectArray().GetCount())
				throw std::runtime_error("UV(Direct) count");
		}
		else if (lUV->GetReferenceMode() == FbxGeometryElement::eIndexToDirect) {
			if (lVertexCount != lUV->GetIndexArray().GetCount())
				throw std::runtime_error("UV(Index) count");
		}
		else {
			throw std::runtime_error("UV reference mode");
		}
	}
}
