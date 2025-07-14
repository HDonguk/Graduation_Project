#include "stdafx.h"
#include "FbxExtractor.h"
#include <Windows.h>

FbxExtractor::FbxExtractor() : 
	mFbxManager{nullptr}, 
	mFbxIOS{nullptr}, 
	mFbxImporter{nullptr}, 
	mFbxScene{nullptr},
	mUV{true},
	mIsFirst{true},
	mBone{false},
	mOnlyAnimation{false}
{
	mFbxManager = FbxManager::Create();
	mFbxIOS = FbxIOSettings::Create(mFbxManager, IOSROOT);
	mFbxManager->SetIOSettings(mFbxIOS);
}

FbxExtractor::~FbxExtractor()
{
	mFbxManager->Destroy();
}

void FbxExtractor::ImportFbxFile(const std::string& fileName, bool onlyAnimation, bool zUp)
{

	mFbxImporter = FbxImporter::Create(mFbxManager, "");
	if(mFbxImporter->Initialize(fileName.c_str()) == false) throw;

	mFbxScene = FbxScene::Create(mFbxManager, "");
	if(mFbxImporter->Import(mFbxScene) == false) throw;
	
	if (zUp) ConvertSceneAxisSystem(FbxAxisSystem::eZAxis, FbxAxisSystem::eParityOdd, FbxAxisSystem::eLeftHanded);
	else ConvertSceneAxisSystem(FbxAxisSystem::eYAxis, FbxAxisSystem::eParityOdd, FbxAxisSystem::eLeftHanded);

	mOnlyAnimation = onlyAnimation;

	mFbxImporter->Destroy();
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

void FbxExtractor::ExtractDataFromFbx()
{

	ptr<FbxNode> rootNode = mFbxScene->GetRootNode();

	int childCount = rootNode->GetChildCount();
	for (int i = 0; i < childCount; ++i) {
		TraverseNodeForSkeleton(rootNode->GetChild(i));
	}

	for (int i = 0; i < childCount; ++i) {
		TraverseNodeForMesh(rootNode->GetChild(i));
	}

	if(mBone) ExtractAnimationData(rootNode);

}

void FbxExtractor::ConvertSceneAxisSystem(FbxAxisSystem::EUpVector u, FbxAxisSystem::EFrontVector f, FbxAxisSystem::ECoordSystem c)
{
	FbxAxisSystem axisSystem{ u, f, c };
	axisSystem.DeepConvertScene(mFbxScene);

	int up{ -2 };
	int upSign{ -2 };
	int front{ -2 };
	int frontSign{ -2 };
	int coor{ -2 };
	up = mFbxScene->GetGlobalSettings().GetAxisSystem().GetUpVector(upSign);
	front = mFbxScene->GetGlobalSettings().GetAxisSystem().GetFrontVector(frontSign);
	coor = mFbxScene->GetGlobalSettings().GetAxisSystem().GetCoorSystem();
	OutputDebugStringA((" 상하축 = " + to_string(up) + " 전방축 = " + to_string(front) + " 좌표계의 시스템 =  " + to_string(coor) + "\n").c_str());
	OutputDebugStringA((" 상하축의 방향 = " + to_string(upSign) + " 전방축의 방향 = " + to_string(frontSign) + "\n").c_str());
}

//void FbxExtractor::TraverseNode(ptr<FbxNode> node)
//{
//	if (node == nullptr) return;
//	ptr<FbxNodeAttribute> nodeAttribute = node->GetNodeAttribute();
//	if (nodeAttribute != nullptr) {
//		FbxNodeAttribute::EType type = nodeAttribute->GetAttributeType();
//		switch (type)
//		{
//		case FbxNodeAttribute::eSkeleton:
//			mBone = true;
//			ExtractBoneHierarchy(node);
//
//			break;
//		case FbxNodeAttribute::eMesh:
//			if (mIsFirst != true) break;
//			mIsFirst = false;
//			ExtractMeshData(node);
//			break;
//		default:
//			break;
//		}
//	}
//
//	int childCount = node->GetChildCount();
//	for (int i = 0; i < childCount; ++i) {
//		TraverseNode(node->GetChild(i));
//	}
//}

inline void FbxExtractor::TraverseNodeForSkeleton(ptr<FbxNode> node)
{
	if (node == nullptr) return;
	ptr<FbxNodeAttribute> nodeAttribute = node->GetNodeAttribute();
	if (nodeAttribute != nullptr) {
		FbxNodeAttribute::EType type = nodeAttribute->GetAttributeType();
		switch (type)
		{
		case FbxNodeAttribute::eSkeleton:
			mBone = true;
			ExtractBoneHierarchy(node);
			break;
		default:
			break;
		}
	}

	int childCount = node->GetChildCount();
	for (int i = 0; i < childCount; ++i) {
		TraverseNodeForSkeleton(node->GetChild(i));
	}

}

inline void FbxExtractor::TraverseNodeForMesh(ptr<FbxNode> node)
{
	if (node == nullptr) return;
	ptr<FbxNodeAttribute> nodeAttribute = node->GetNodeAttribute();
	if (nodeAttribute != nullptr) {
		FbxNodeAttribute::EType type = nodeAttribute->GetAttributeType();
		switch (type)
		{
		case FbxNodeAttribute::eMesh:
			if (mIsFirst != true) break;
			mIsFirst = false;
			ExtractMeshData(node);
			break;
		default:
			break;
		}
	}

	int childCount = node->GetChildCount();
	for (int i = 0; i < childCount; ++i) {
		TraverseNodeForMesh(node->GetChild(i));
	}
}

void FbxExtractor::TraverseNodeForAnimation(ptr<FbxNode> node, ptr<FbxTakeInfo> takeInfo , FbxTime::EMode timeMode)
{
	if (node == nullptr) return;

	ptr<FbxNodeAttribute> nodeAttribute = node->GetNodeAttribute();
	if (nodeAttribute != nullptr && nodeAttribute->GetAttributeType() == FbxNodeAttribute::eSkeleton) {

		FbxLongLong startFrame = takeInfo->mLocalTimeSpan.GetStart().GetFrameCount(timeMode);
		FbxLongLong stopFrame = takeInfo->mLocalTimeSpan.GetStop().GetFrameCount(timeMode);
		Keyframe keyframe;
		FbxTime currentTime;
		BoneAnimation boneAnimation;
		for (FbxLongLong frame = startFrame; frame < stopFrame; ++frame) {
			currentTime.SetFrame(frame, timeMode);
			keyframe.TimePos = static_cast<float>(currentTime.GetSecondDouble());
			FbxAMatrix& localMatrix = node->EvaluateLocalTransform(currentTime);
			keyframe.Scale = ToXMFloat3(localMatrix.GetS());
			keyframe.RotationQuat = ToXMFloat4(localMatrix.GetQ());
			keyframe.Translation = ToXMFloat3(localMatrix.GetT());
			boneAnimation.Keyframes.push_back(keyframe);
		}
		mAnimations[takeInfo->mName.Buffer()].BoneAnimations.push_back(boneAnimation);
		//OutputDebugStringA(("take info name buffer = " + string{ takeInfo->mName.Buffer() } + "\n").c_str());
	};

	for (int i = 0; i < node->GetChildCount(); ++i) {
		TraverseNodeForAnimation(node->GetChild(i), takeInfo, timeMode);
	}
}

void FbxExtractor::ExtractMeshData(ptr<FbxNode> node)
{
	ptr<FbxMesh> mesh = node->GetMesh();
	if (mesh->GetPolygonSize(0) != 3) {
		OutputDebugStringA("Polygon size is not 3\n");
		if (!ConvertNodeAttributePolygonsTriangulate(node)) throw std::runtime_error("Triangulate failed");
		mesh = node->GetMesh(); // 삼각형화 했다면 FbxMesh 다시 얻어오기
	}

	ExtractWeightAndOffsetMatrix(mesh);
	if (mNormalize == true) NormalizeWeight();

	CheckMeshDataState(mesh);

	ptr<int> lVertices = mesh->GetPolygonVertices();
	int polygonVerteCount = mesh->GetPolygonVertexCount();
	ptr<FbxGeometryElementNormal> lNormal = mesh->GetElementNormal();
	ptr<FbxGeometryElementUV> lUV = mesh->GetElementUV();
	mVertices.resize(polygonVerteCount);
	
	for (int i = 0; i < polygonVerteCount; ++i) {

		// position
		FbxVector4 lVector4 = mesh->GetControlPointAt(lVertices[i]);
		mVertices[i].position.x = (float)lVector4[0];
		mVertices[i].position.y = (float)lVector4[1];
		mVertices[i].position.z = (float)lVector4[2];

		// normal
		if (lNormal->GetReferenceMode() == FbxGeometryElement::eDirect) {
			lVector4 = lNormal->GetDirectArray().GetAt(i);
		}
		else {
			int j = lNormal->GetIndexArray().GetAt(i);
			lVector4 = lNormal->GetDirectArray().GetAt(j);
		}
		mVertices[i].normal.x = (float)lVector4[0];
		mVertices[i].normal.y = (float)lVector4[1];
		mVertices[i].normal.z = (float)lVector4[2];

		// uv
		if (mUV == true) {
			FbxVector2 lVector2;
			if (lUV->GetReferenceMode() == FbxGeometryElement::eDirect) {
				lVector2 = lUV->GetDirectArray().GetAt(i);
			}
			else {
				int j = lUV->GetIndexArray().GetAt(i);
				lVector2 = lUV->GetDirectArray().GetAt(j);
			}
			mVertices[i].uv.x = (float)lVector2[0];
			mVertices[i].uv.y = 1 - (float)lVector2[1];
		}
		else {
			mVertices[i].uv.x = 0.5f;
			mVertices[i].uv.y = 0.5f;
		}

		// weight, bone index
		if (mBone == true) {

			//weight
			mVertices[i].weight.x = mControlPointsWeight[lVertices[i]][0].second;
			mVertices[i].weight.y = mControlPointsWeight[lVertices[i]][1].second;
			mVertices[i].weight.z = mControlPointsWeight[lVertices[i]][2].second;
			mVertices[i].weight.w = mControlPointsWeight[lVertices[i]][3].second;
			//bone index
			mVertices[i].boneIndex[0] = mControlPointsWeight[lVertices[i]][0].first;
			mVertices[i].boneIndex[1] = mControlPointsWeight[lVertices[i]][1].first;
			mVertices[i].boneIndex[2] = mControlPointsWeight[lVertices[i]][2].first;
			mVertices[i].boneIndex[3] = mControlPointsWeight[lVertices[i]][3].first;
		}
	}
}

void FbxExtractor::ExtractBoneHierarchy(ptr<FbxNode> node)
{
	ptr<FbxSkeleton> skeleton = node->GetSkeleton();
	if (skeleton == nullptr) return;

	ptr<FbxNode> parentNode = node->GetParent();
	string currentNodeName = node->GetName();
	//OutputDebugStringA((currentNodeName + "\n").c_str());

	int parentIndex{};
	if (parentNode != nullptr && parentNode->GetNodeAttribute() != nullptr && parentNode->GetNodeAttribute()->GetAttributeType() == FbxNodeAttribute::eSkeleton) {
		string parentNodeName = parentNode->GetName();
		parentIndex = FindBoneIndex(parentNodeName);
	}
	else {
		parentIndex = -1;
	}
	mBoneHierarchyIndex.push_back(parentIndex);
	mBoneHierarchyName.push_back(currentNodeName);
	//mBoneHierarchy.emplace_back(pair<string, int>{currentNodeName, parentIndex});
}

void FbxExtractor::ExtractAnimationData(ptr<FbxNode> rootNode)
{
	int animStackCount = mFbxScene->GetSrcObjectCount<FbxAnimStack>();
	OutputDebugStringA((string{ "Animation stack count = " } + to_string(animStackCount) + "\n").c_str());
	if (animStackCount <= 0) return;

	for (int clipIndex = 0; clipIndex < animStackCount; ++clipIndex) {
		ptr<FbxAnimStack> animStack = mFbxScene->GetSrcObject<FbxAnimStack>(clipIndex);
		string currentClipName = animStack->GetName();
		OutputDebugStringA((string{"Animation name = "} + currentClipName + "\n").c_str());

		int animLayerCount = animStack->GetMemberCount<FbxAnimLayer>();
		OutputDebugStringA((currentClipName + " animation stack Layer count = " + to_string(animLayerCount) + "\n").c_str());
		if (animLayerCount <= 0) break;
		
		mAnimations[currentClipName];

		mFbxScene->SetCurrentAnimationStack(animStack);
		
		FbxTime::EMode timeMode = mFbxScene->GetGlobalSettings().GetTimeMode();
		OutputDebugStringA(("Scene time mode : " + to_string(timeMode) + "\n").c_str());

		ptr<FbxTakeInfo> takeInfo = mFbxScene->GetTakeInfo(currentClipName.c_str());
		FbxTime start = takeInfo->mLocalTimeSpan.GetStart();
		OutputDebugStringA((" strat time = " + to_string(start.GetSecondDouble()) + "\n").c_str());
		//OutputDebugStringA((" strat frame count = " + to_string(start.GetFrameCount(timeMode)) + "\n").c_str());
		FbxTime stop = takeInfo->mLocalTimeSpan.GetStop();
		OutputDebugStringA((" stop time = " + to_string(stop.GetSecondDouble()) + "\n").c_str());
		//OutputDebugStringA((" stop frame count = " + to_string(stop.GetFrameCount(timeMode)) + "\n").c_str());
		//FbxTime duration = takeInfo->mLocalTimeSpan.GetDuration();
		//OutputDebugStringA((" duration = " + to_string(duration.GetSecondDouble()) + "\n").c_str());
		//OutputDebugStringA((" duration frame count = " + to_string(duration.GetFrameCount(timeMode)) + "\n").c_str());

		//double frameRate = FbxTime::GetFrameRate(timeMode);
		//OutputDebugStringA((" frame Rate = " + to_string(frameRate) + "\n").c_str());

		for (int i = 0; i < rootNode->GetChildCount(); ++i) {
			TraverseNodeForAnimation(rootNode->GetChild(i), takeInfo, timeMode);
		}
	}
}

vector<Vertex>& FbxExtractor::GetVertices()
{
	return mVertices;
}

//vector<pair<string, int>>& FbxExtractor::GetBoneHierarchy()
//{
//	return mBoneHierarchy;
//}

vector<int>& FbxExtractor::GetBoneHierarchyIndex()
{
	return mBoneHierarchyIndex;
}

vector<vector<pair<int,float>>>& FbxExtractor::GetControlPointsWeight()
{
	return mControlPointsWeight;
}

vector<XMFLOAT4X4>& FbxExtractor::GetOffsetMatrix()
{
	return mOffsetMatrix;
}

unordered_map<string, AnimationClip>& FbxExtractor::GetAnimation()
{
	return mAnimations;
}

void FbxExtractor::ResetAndClear()
{
	mFbxScene->Destroy();
	mBone = false;
	mIsFirst = true;
	mAnimations.clear();
	mOffsetMatrix.clear();
	mBoneHierarchyName.clear();
	mBoneHierarchyIndex.clear();
	mVertices.clear();
	mControlPointsWeight.clear();
}

bool FbxExtractor::ConvertNodeAttributePolygonsTriangulate(ptr<FbxNode> node)
{
	FbxGeometryConverter lConverter{ mFbxManager };
	ptr<FbxNodeAttribute> lNewNodeAttribute = lConverter.Triangulate(node->GetNodeAttribute(), true);
	if (!lNewNodeAttribute) return false;
	node->SetNodeAttribute(lNewNodeAttribute);
	return true;
}

void FbxExtractor::CheckMeshDataState(ptr<FbxMesh> pMesh)
{

	int lVertexCount = pMesh->GetPolygonVertexCount();
	
	// check normal
	ptr<FbxGeometryElementNormal> lNormal = pMesh->GetElementNormal();
	if (lNormal == nullptr || lNormal->GetMappingMode() != FbxGeometryElement::eByPolygonVertex) {
		OutputDebugStringA("No normal or invailed mapping mode\n");
		if (!pMesh->GenerateNormals(true, false, false)) 
			throw std::runtime_error("GenerateNormals failed");
		OutputDebugStringA("Generate normals\n");
	}

	if (lNormal->GetMappingMode() == FbxGeometryElement::eByPolygonVertex) {
		OutputDebugStringA("Normal mapping mode = by polygon vertex\n");
		if (lNormal->GetReferenceMode() == FbxGeometryElement::eDirect) {
			OutputDebugStringA("Normal Ref mode = direct\n");
			if (lVertexCount != lNormal->GetDirectArray().GetCount()) 
				throw std::runtime_error("Normal(Direct) != vertex count");
		}
		else if (lNormal->GetReferenceMode() == FbxGeometryElement::eIndexToDirect) {
			OutputDebugStringA("Normal Ref mode = index to direct\n");
			if (lVertexCount != lNormal->GetIndexArray().GetCount())
				throw std::runtime_error("Normal(Index) != vertex count");
		}
		else {
			throw std::runtime_error("Normal Reference Mode");
		}
	}
	else {
		throw std::runtime_error("Invailed Normal mapping mode");
	}

	// check uv
	ptr<FbxGeometryElementUV> lUV = pMesh->GetElementUV();
	if (!lUV) {
		mUV = false;
		OutputDebugStringA("No uv\n");
	}
	else {
		mUV = true;
		if (lUV->GetMappingMode() != FbxGeometryElement::eByPolygonVertex) {
			throw std::runtime_error("Invalid uv mapping mode");
		}

		OutputDebugStringA("UV mapping mode = by polygon vertex\n");

		if (lUV->GetReferenceMode() == FbxGeometryElement::eDirect) {
			OutputDebugStringA("UV ref mode = direct\n");
			if (lVertexCount != lUV->GetDirectArray().GetCount())
				throw std::runtime_error("UV(Direct) count");
		}
		else if (lUV->GetReferenceMode() == FbxGeometryElement::eIndexToDirect) {
			OutputDebugStringA("UV ref mode = index to direct\n");
			if (lVertexCount != lUV->GetIndexArray().GetCount())
				throw std::runtime_error("UV(Index) count");
		}
		else {
			throw std::runtime_error("Invalied uv ref mode");
		}
	}
}

void FbxExtractor::ProcessAnimationLayer(ptr<FbxNode> node, ptr<FbxAnimLayer> layer)
{
	if (!node) return;
	ptr<FbxNodeAttribute> nodeAttribute = node->GetNodeAttribute();
	if (nodeAttribute && nodeAttribute->GetAttributeType() == FbxNodeAttribute::eSkeleton) {
		OutputDebugStringA((string{ node->GetName() } + "\n").c_str());
		ptr<FbxAnimCurve> curves = nullptr;
		curves = node->LclTranslation.GetCurve(layer, FBXSDK_CURVENODE_COMPONENT_X);
		if (curves) { OutputDebugStringA("Translation X "); ProcessCurves(curves); }
		curves = node->LclTranslation.GetCurve(layer, FBXSDK_CURVENODE_COMPONENT_Y);
		if (curves) { OutputDebugStringA("Translation Y "); ProcessCurves(curves); }
		curves = node->LclTranslation.GetCurve(layer, FBXSDK_CURVENODE_COMPONENT_Z);
		if (curves) { OutputDebugStringA("Translation Z "); ProcessCurves(curves); }
		curves = node->LclRotation.GetCurve(layer, FBXSDK_CURVENODE_COMPONENT_X);
		if (curves) { OutputDebugStringA("Rotation X "); ProcessCurves(curves); }
		curves = node->LclRotation.GetCurve(layer, FBXSDK_CURVENODE_COMPONENT_Y);
		if (curves) { OutputDebugStringA("Rotation Y "); ProcessCurves(curves); }
		curves = node->LclRotation.GetCurve(layer, FBXSDK_CURVENODE_COMPONENT_Z);
		if (curves) { OutputDebugStringA("Rotation Z "); ProcessCurves(curves); }
		curves = node->LclScaling.GetCurve(layer, FBXSDK_CURVENODE_COMPONENT_X);
		if (curves) { OutputDebugStringA("Scale X "); ProcessCurves(curves); }
		curves = node->LclScaling.GetCurve(layer, FBXSDK_CURVENODE_COMPONENT_Y);
		if (curves) { OutputDebugStringA("Scale Y "); ProcessCurves(curves); }
		curves = node->LclScaling.GetCurve(layer, FBXSDK_CURVENODE_COMPONENT_Z);
		if (curves) { OutputDebugStringA("Scale Z "); ProcessCurves(curves); }
		OutputDebugStringA("\n");
	}
	for (int i = 0; i < node->GetChildCount(); ++i) {
		ProcessAnimationLayer(node->GetChild(i), layer);
	}
}

void FbxExtractor::ProcessCurves(ptr<FbxAnimCurve> curve)
{
	int keyCount = curve->KeyGetCount();
	OutputDebugStringA((string{ "key count = " } + to_string(keyCount) + "\n").c_str());
	for (int i = 0; i < keyCount; ++i) {
		float keyValue = curve->KeyGetValue(i);
		OutputDebugStringA((string{ "key value = " } + to_string(keyValue) + "\n").c_str());
		float keyTime = (float)curve->KeyGetTime(i).GetSecondDouble();
		OutputDebugStringA((string{ "key time = " } + to_string(keyTime) + "\n").c_str());

	}

}

int FbxExtractor::FindBoneIndex(const string& name)
{
	int index{ 0 };
	for (auto& n : mBoneHierarchyName) {
		if (n == name) return index;
		++index;
	}
	return -1;
}

void FbxExtractor::ExtractWeightAndOffsetMatrix(ptr<FbxMesh> mesh)
{
	if (mBone == false) return;

	int controlPointsCount = mesh->GetControlPointsCount();
	mControlPointsWeight.resize(controlPointsCount);
	mOffsetMatrix.resize(mBoneHierarchyIndex.size());


	for (int deformerIndex = 0; deformerIndex < mesh->GetDeformerCount(); ++deformerIndex) {
		ptr<FbxDeformer> deformer = mesh->GetDeformer(deformerIndex);
		//OutputDebugStringA(("디포머 타입" + to_string(deformer->GetDeformerType()) + "\n").c_str());

		if (deformer->GetDeformerType() != FbxDeformer::eSkin) continue;

		ptr<FbxSkin> skin = static_cast<ptr<FbxSkin>>(deformer);
		int clusterCount = skin->GetClusterCount();
		//OutputDebugStringA(("cluster 갯수" + to_string(clusterCount) + "\n").c_str());
		for (int clusterIndex = 0; clusterIndex < clusterCount; ++clusterIndex) {
			ptr<FbxCluster> cluster = skin->GetCluster(clusterIndex);

			ptr<FbxNode> linkNode = cluster->GetLink();
			if (linkNode == nullptr) continue;

			int linkNodeIndex = FindBoneIndex(linkNode->GetName());
			//OutputDebugStringA((to_string(linkNodeIndex) + "\n").c_str());
			if (linkNodeIndex < 0) continue;

			FbxAMatrix transformMatrix;
			transformMatrix.SetIdentity();
			FbxAMatrix transformLinkMatrix{};
			transformLinkMatrix.SetIdentity();

			cluster->GetTransformMatrix(transformMatrix);
			cluster->GetTransformLinkMatrix(transformLinkMatrix);

			mOffsetMatrix[linkNodeIndex] = ToXMFloat4x4(transformLinkMatrix.Inverse() * transformMatrix);

			ptr<int> indices = cluster->GetControlPointIndices();
			ptr<double> weights = cluster->GetControlPointWeights();
			int indexCount = cluster->GetControlPointIndicesCount();

			for (int i = 0; i < indexCount; ++i) {
				mControlPointsWeight[indices[i]].emplace_back(pair<int, float>{linkNodeIndex, static_cast<float>(weights[i])});
			}

			mNormalize = cluster->GetLinkMode() == FbxCluster::eNormalize ? true : false;
			//OutputDebugStringA(("클러스터 링크 모드" + to_string(cluster->GetLinkMode()) + "\n").c_str());

		}
	}
}

void FbxExtractor::NormalizeWeight()
{
	for (auto& controlPointWeight : mControlPointsWeight) {

		if (controlPointWeight.size() > 4) {
			partial_sort(controlPointWeight.begin(), controlPointWeight.begin() + 4, controlPointWeight.end(),
				[](const pair<int, float>& a, const pair<int, float>& b) {
					return a.second > b.second;
				});
		}

		controlPointWeight.resize(4);

		float totalWeight{ 0.f };
		for (auto& weight : controlPointWeight) {
			totalWeight += weight.second;
		}
		for (auto& weight : controlPointWeight) {
			weight.second /= totalWeight;
		}
	}
}

XMFLOAT4X4 FbxExtractor::ToXMFloat4x4(FbxAMatrix& fbxmatrix)
{
	XMFLOAT4X4 xmMatrix;
	XMStoreFloat4x4(&xmMatrix, XMMatrixIdentity());

	// FbxMatrix는 4x4 double 배열이므로 XMFLOAT4X4에 복사
	for (int row = 0; row < 4; ++row)
	{
		for (int col = 0; col < 4; ++col)
		{
			xmMatrix.m[row][col] = static_cast<float>(fbxmatrix.Get(row, col));
		}
	}
	return xmMatrix;
}

XMFLOAT3 FbxExtractor::ToXMFloat3(FbxVector4& fbxV4)
{
	XMFLOAT3 xmV3;
	xmV3.x = static_cast<float>(fbxV4[0]);
	xmV3.y = static_cast<float>(fbxV4[1]);
	xmV3.z = static_cast<float>(fbxV4[2]);
	return xmV3;
}

XMFLOAT4 FbxExtractor::ToXMFloat4(FbxVector4& fbxV4)
{
	XMFLOAT4 xmV4;
	xmV4.x = static_cast<float>(fbxV4[0]);
	xmV4.y = static_cast<float>(fbxV4[1]);
	xmV4.z = static_cast<float>(fbxV4[2]);
	xmV4.w = static_cast<float>(fbxV4[3]);
	return xmV4;
}

XMFLOAT4 FbxExtractor::ToXMFloat4(FbxQuaternion& fbxQ)
{
	XMFLOAT4 xmV4;
	xmV4.x = fbxQ[0];
	xmV4.y = fbxQ[1];
	xmV4.z = fbxQ[2];
	xmV4.w = fbxQ[3];
	return xmV4;
}