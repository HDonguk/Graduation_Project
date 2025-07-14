#include "NetworkManager.h"
#include "Scene.h"
#include "DXSampleHelper.h"
#include "GameTimer.h"
#include "string"
#include "info.h"
#include "OtherPlayerManager.h"
#include "Framework.h"
#include <array>
#include <psapi.h>

Scene::Scene(Framework* parent, UINT width, UINT height, std::wstring name) :
    m_parent{ parent },
    m_viewport(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)),
    m_scissorRect(0, 0, static_cast<LONG>(width), static_cast<LONG>(height)),
    m_name(name),
    m_mappedData(nullptr)
{
}

Scene::Scene(Scene&& other) noexcept :
    m_parent(other.m_parent),
    m_viewport(other.m_viewport),
    m_scissorRect(other.m_scissorRect),
    m_name(std::move(other.m_name)),
    m_objects(std::move(other.m_objects)),
    m_resourceManager(std::move(other.m_resourceManager)),
    m_rootSignature(std::move(other.m_rootSignature)),
    m_PSOs(std::move(other.m_PSOs)),
    m_descriptorHeap(std::move(other.m_descriptorHeap)),
    m_cbvsrvuavDescriptorSize(other.m_cbvsrvuavDescriptorSize),
    m_vertexBuffer_default(std::move(other.m_vertexBuffer_default)),
    m_indexBuffer_default(std::move(other.m_indexBuffer_default)),
    m_vertexBuffer_upload(std::move(other.m_vertexBuffer_upload)),
    m_indexBuffer_upload(std::move(other.m_indexBuffer_upload)),
    m_vertexBufferView(other.m_vertexBufferView),
    m_indexBufferView(other.m_indexBufferView),
    m_subTextureData(std::move(other.m_subTextureData)),
    m_DDSFileName(std::move(other.m_DDSFileName)),
    m_textureBuffer_defaults(std::move(other.m_textureBuffer_defaults)),
    m_textureBuffer_uploads(std::move(other.m_textureBuffer_uploads)),
    m_constantBuffer(std::move(other.m_constantBuffer)),
    m_mappedData(other.m_mappedData),
    m_proj(other.m_proj),
    m_device(other.m_device),
    m_pendingTigerSpawns(std::move(other.m_pendingTigerSpawns)),
    m_tigerInterpolationData(std::move(other.m_tigerInterpolationData)),
    m_shadow(std::move(other.m_shadow)),
    m_shaders(std::move(other.m_shaders)),
    m_inputElement(std::move(other.m_inputElement))
{
    other.m_parent = nullptr;
    other.m_mappedData = nullptr;
    other.m_device = nullptr;
}

Scene& Scene::operator=(Scene&& other) noexcept
{
    if (this != &other)
    {
        m_parent = other.m_parent;
        m_viewport = other.m_viewport;
        m_scissorRect = other.m_scissorRect;
        m_name = std::move(other.m_name);
        m_objects = std::move(other.m_objects);
        m_resourceManager = std::move(other.m_resourceManager);
        m_rootSignature = std::move(other.m_rootSignature);
        m_PSOs = std::move(other.m_PSOs);
        m_descriptorHeap = std::move(other.m_descriptorHeap);
        m_cbvsrvuavDescriptorSize = other.m_cbvsrvuavDescriptorSize;
        m_vertexBuffer_default = std::move(other.m_vertexBuffer_default);
        m_indexBuffer_default = std::move(other.m_indexBuffer_default);
        m_vertexBuffer_upload = std::move(other.m_vertexBuffer_upload);
        m_indexBuffer_upload = std::move(other.m_indexBuffer_upload);
        m_vertexBufferView = other.m_vertexBufferView;
        m_indexBufferView = other.m_indexBufferView;
        m_subTextureData = std::move(other.m_subTextureData);
        m_DDSFileName = std::move(other.m_DDSFileName);
        m_textureBuffer_defaults = std::move(other.m_textureBuffer_defaults);
        m_textureBuffer_uploads = std::move(other.m_textureBuffer_uploads);
        m_constantBuffer = std::move(other.m_constantBuffer);
        m_mappedData = other.m_mappedData;
        m_proj = other.m_proj;
        m_device = other.m_device;
        m_pendingTigerSpawns = std::move(other.m_pendingTigerSpawns);
        m_tigerInterpolationData = std::move(other.m_tigerInterpolationData);
        m_shadow = std::move(other.m_shadow);
        m_shaders = std::move(other.m_shaders);
        m_inputElement = std::move(other.m_inputElement);

        other.m_parent = nullptr;
        other.m_mappedData = nullptr;
        other.m_device = nullptr;
    }
    return *this;
}

void Scene::OnInit(ID3D12Device* device, ID3D12GraphicsCommandList* commandList)
{
    NetworkManager::LogToFile("[Scene] OnInit - Starting");
    
    m_device = device;
    Initialize();
    
    NetworkManager::LogToFile("[Scene] OnInit - Loading Mesh/Animation/Texture");
    LoadMeshAnimationTexture();
    NetworkManager::LogToFile("[Scene] OnInit - Building Projection Matrix");
    BuildProjMatrix();
    NetworkManager::LogToFile("[Scene] OnInit - Building Objects");
    BuildObjects(device);
    NetworkManager::LogToFile("[Scene] OnInit - Building Root Signature");
    BuildRootSignature(device);
    NetworkManager::LogToFile("[Scene] OnInit - Building Input Element");
    BuildInputElement();
    NetworkManager::LogToFile("[Scene] OnInit - Building Shaders");
    BuildShaders();
    NetworkManager::LogToFile("[Scene] OnInit - Building PSO");
    BuildPSO(device);
    NetworkManager::LogToFile("[Scene] OnInit - Building Vertex Buffer");
    BuildVertexBuffer(device, commandList);
    NetworkManager::LogToFile("[Scene] OnInit - Building Index Buffer");
    BuildIndexBuffer(device, commandList);
    NetworkManager::LogToFile("[Scene] OnInit - Building Texture Buffer");
    BuildTextureBuffer(device, commandList);
    NetworkManager::LogToFile("[Scene] OnInit - Building Constant Buffer");
    BuildConstantBuffer(device);
    NetworkManager::LogToFile("[Scene] OnInit - Building Descriptor Heap");
    BuildDescriptorHeap(device);
    NetworkManager::LogToFile("[Scene] OnInit - Building Vertex Buffer View");
    BuildVertexBufferView();
    NetworkManager::LogToFile("[Scene] OnInit - Building Index Buffer View");
    BuildIndexBufferView();
    NetworkManager::LogToFile("[Scene] OnInit - Building Constant Buffer View");
    BuildConstantBufferView(device);
    NetworkManager::LogToFile("[Scene] OnInit - Building Texture Buffer View");
    BuildTextureBufferView(device);
    NetworkManager::LogToFile("[Scene] OnInit - Building Shadow");
    BuildShadow();
    NetworkManager::LogToFile("[Scene] OnInit - Complete");
}

void Scene::BuildObjects(ID3D12Device* device)
{
    ResourceManager& rm = GetResourceManager();
    auto& subMeshData = rm.GetSubMeshData();
    auto& animData = rm.GetAnimationData();

    Object* objectPtr = nullptr;

    NetworkManager::LogToFile("[Debug] Creating PlayerObject...");
    AddObj(L"PlayerObject", PlayerObject{ this });
    NetworkManager::LogToFile("[Debug] PlayerObject created successfully");
    
    objectPtr = &GetObj<PlayerObject>(L"PlayerObject");
    objectPtr->AddComponent(Position{ 600.f, 0.f, 600.f, 1.f, objectPtr });
    objectPtr->AddComponent(Velocity{ 0.f, 0.f, 0.f, 0.f, objectPtr });
    objectPtr->AddComponent(Rotation{ 0.0f, 180.0f, 0.0f, 0.0f, objectPtr });
    objectPtr->AddComponent(Rotate{ 0.0f, 0.0f, 0.0f, 0.0f, objectPtr });
    objectPtr->AddComponent(Scale{ 0.1f, objectPtr });
    objectPtr->AddComponent(Mesh{ subMeshData.at("1P(boy-idle).fbx"), objectPtr });
    objectPtr->AddComponent(Texture{ m_subTextureData.at(L"boy"), objectPtr });
    objectPtr->AddComponent(Animation{ animData, objectPtr });
    objectPtr->AddComponent(Gravity{ 2.f, objectPtr });
    objectPtr->AddComponent(Collider{ 0.f, 0.f, 0.f, 4.f, 50.f, 4.f, objectPtr });

    // 다른 플레이어 미리 생성 (초기에는 비활성화)
    AddObj(L"OtherPlayer", PlayerObject{ this });
    objectPtr = &GetObj<PlayerObject>(L"OtherPlayer");
    objectPtr->AddComponent(Position{ 600.f, 0.f, 600.f, 1.f, objectPtr });
    objectPtr->AddComponent(Velocity{ 0.f, 0.f, 0.f, 0.f, objectPtr });
    objectPtr->AddComponent(Rotation{ 0.0f, 180.0f, 0.0f, 0.0f, objectPtr });
    objectPtr->AddComponent(Rotate{ 0.0f, 0.0f, 0.0f, 0.0f, objectPtr });
    objectPtr->AddComponent(Scale{ 0.1f, objectPtr });
    objectPtr->AddComponent(Mesh{ subMeshData.at("1P(boy-idle).fbx"), objectPtr });
    objectPtr->AddComponent(Texture{ m_subTextureData.at(L"boy"), objectPtr });
    objectPtr->AddComponent(Animation{ animData, objectPtr });
    objectPtr->AddComponent(Gravity{ 2.f, objectPtr });
    objectPtr->AddComponent(Collider{ 0.f, 0.f, 0.f, 4.f, 50.f, 4.f, objectPtr });
    objectPtr->SetActive(false);  // 초기에는 비활성화
    
    AddObj(L"CameraObject", CameraObject{ 70.f, this });
    objectPtr = &GetObj<CameraObject>(L"CameraObject");
    objectPtr->AddComponent(Position{ 0.f, 0.f, 0.f, 0.f, objectPtr });

    AddObj(L"TerrainObject", TerrainObject{ this });
    objectPtr = &GetObj<TerrainObject>(L"TerrainObject");
    objectPtr->AddComponent(Position{ 0.f, 0.f, 0.f, 1.f, objectPtr });
    objectPtr->AddComponent(Velocity{ 0.f, 0.f, 0.f, 0.f, objectPtr });
    objectPtr->AddComponent(Rotation{ 0.0f, 0.0f, 0.0f, 0.0f, objectPtr });
    objectPtr->AddComponent(Rotate{ 0.0f, 0.0f, 0.0f, 0.0f, objectPtr });
    objectPtr->AddComponent(Scale{ 1.f, objectPtr });
    objectPtr->AddComponent(Mesh{ subMeshData.at("HeightMap.raw") , objectPtr });
    objectPtr->AddComponent(Texture{ m_subTextureData.at(L"grass"), objectPtr });

    //AddObj(L"TestObject", TestObject{ this });
    //objectPtr = &GetObj<TestObject>(L"TestObject");
    //objectPtr->AddComponent(Position{ 0.f, 0.f, 0.f, 1.f, objectPtr });
    //objectPtr->AddComponent(Velocity{ 0.f, 0.f, 0.f, 0.f, objectPtr });
    //objectPtr->AddComponent(Rotation{ 0.0f, 0.0f, 0.0f, 0.0f, objectPtr });
    //objectPtr->AddComponent(Rotate{ 0.0f, 0.0f, 0.0f, 0.0f, objectPtr });
    //objectPtr->AddComponent(Scale{ 0.01f, objectPtr });
    //objectPtr->AddComponent(Mesh{ subMeshData.at("house_1218_attach_fix.fbx") , objectPtr });
    //objectPtr->AddComponent(Texture{ m_subTextureData.at(L"PP_Color_Palette"), objectPtr });

    int repeat{ 17 };
    for (int i = 0; i < repeat; ++i) {
        for (int j = 0; j < repeat; ++j) {
            wstring objectName = L"TreeObject" + to_wstring(j + (repeat * i));
            AddObj(objectName, TreeObject{ this });
            objectPtr = &GetObj<TreeObject>(objectName);
            objectPtr->AddComponent(Position{ 100.f + 150.f * j, 0.f, 100.f + 150.f * i, 1.f, objectPtr });
            objectPtr->AddComponent(Velocity{ 0.f, 0.f, 0.f, 0.f, objectPtr });
            objectPtr->AddComponent(Rotation{ 0.0f, 0.0f, 0.0f, 0.0f, objectPtr });
            objectPtr->AddComponent(Rotate{ 0.0f, 0.0f, 0.0f, 0.0f, objectPtr });
            objectPtr->AddComponent(Scale{ 20.f, objectPtr });
            objectPtr->AddComponent(Mesh{ subMeshData.at("long_tree.fbx") , objectPtr });
            objectPtr->AddComponent(Texture{ m_subTextureData.at(L"longTree"), objectPtr });
            objectPtr->AddComponent(Collider{ 0.f, 0.f, 0.f, 3.f, 50.f, 3.f, objectPtr });
        }
    }
}

void Scene::BuildRootSignature(ID3D12Device* device)
{
    // Create a root signature consisting of a descriptor table with a single CBV.
    D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData{};

    // This is the highest version the sample supports. If CheckFeatureSupport succeeds, the HighestVersion returned will not be greater than this.
    featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

    if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
    {
        featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
    }

    CD3DX12_DESCRIPTOR_RANGE1 ranges[3] = {};
    ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0, 0);
    ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);
    ranges[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 0);

    CD3DX12_ROOT_PARAMETER1 rootParameters[4] = {};
    rootParameters[0].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_VERTEX);
    rootParameters[1].InitAsDescriptorTable(1, &ranges[1], D3D12_SHADER_VISIBILITY_PIXEL);
    rootParameters[2].InitAsConstantBufferView(1);
    rootParameters[3].InitAsDescriptorTable(1, &ranges[2], D3D12_SHADER_VISIBILITY_PIXEL);

    std::array<D3D12_STATIC_SAMPLER_DESC, 2> samplerDesc = {};
    D3D12_STATIC_SAMPLER_DESC* descPtr = nullptr;

    descPtr = &samplerDesc[0];
    descPtr->Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    descPtr->AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    descPtr->AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    descPtr->AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    descPtr->MipLODBias = 0;
    descPtr->MaxAnisotropy = 0;
    descPtr->ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    descPtr->BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
    descPtr->MinLOD = 0.0f;
    descPtr->MaxLOD = D3D12_FLOAT32_MAX;
    descPtr->ShaderRegister = 0;
    descPtr->RegisterSpace = 0;
    descPtr->ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    descPtr = &samplerDesc[1];
    descPtr->Filter = D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
    descPtr->AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
    descPtr->AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
    descPtr->AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
    descPtr->MipLODBias = 0;
    descPtr->MaxAnisotropy = 0;
    descPtr->ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    descPtr->BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
    descPtr->MinLOD = 0.0f;
    descPtr->MaxLOD = 0.0f;
    descPtr->ShaderRegister = 1;
    descPtr->RegisterSpace = 0;
    descPtr->ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_ROOT_SIGNATURE_FLAGS flags = 
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
    rootSignatureDesc.Init_1_1(_countof(rootParameters), rootParameters, samplerDesc.size(), samplerDesc.data(), flags);

    ComPtr<ID3DBlob> signature;
    ComPtr<ID3DBlob> error;
    ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, featureData.HighestVersion, &signature, &error));
    ThrowIfFailed(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)));
}

void Scene::BuildPSO(ID3D12Device* device)
{
    NetworkManager::LogToFile("[Scene] BuildPSO - Starting PSO creation");
    
    // Describe and create the graphics pipeline state object (PSO).
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    
    NetworkManager::LogToFile("[Scene] BuildPSO - Setting up input layout");
    psoDesc.InputLayout = { m_inputElement.data(), static_cast<UINT>(m_inputElement.size())};
    
    NetworkManager::LogToFile("[Scene] BuildPSO - Setting up root signature");
    if (!m_rootSignature) {
        NetworkManager::LogToFile("[Scene] BuildPSO - ERROR: Root signature is null");
        return;
    }
    psoDesc.pRootSignature = m_rootSignature.Get();
    
    NetworkManager::LogToFile("[Scene] BuildPSO - Setting up vertex shader");
    if (!m_shaders.contains("VS_Opaque")) {
        NetworkManager::LogToFile("[Scene] BuildPSO - ERROR: VS_Opaque shader not found");
        return;
    }
    psoDesc.VS = CD3DX12_SHADER_BYTECODE(m_shaders.at("VS_Opaque").Get());
    
    NetworkManager::LogToFile("[Scene] BuildPSO - Setting up pixel shader");
    if (!m_shaders.contains("PS_Opaque")) {
        NetworkManager::LogToFile("[Scene] BuildPSO - ERROR: PS_Opaque shader not found");
        return;
    }
    psoDesc.PS = CD3DX12_SHADER_BYTECODE(m_shaders.at("PS_Opaque").Get());
    
    NetworkManager::LogToFile("[Scene] BuildPSO - Setting up rasterizer state");
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState.DepthEnable = TRUE;
    psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
    psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.SampleDesc.Count = 1;
    psoDesc.SampleDesc.Quality = 0;

    // 추가 검증
    if (!psoDesc.pRootSignature || !psoDesc.VS.pShaderBytecode || !psoDesc.PS.pShaderBytecode) {
        NetworkManager::LogToFile("[Scene] BuildPSO - ERROR: Required PSO components are null");
        return;
    }

    // PSO 상태 로깅
    char buffer[256];
    sprintf_s(buffer, "[Scene] BuildPSO - PSO State: RTV Format: %d, DSV Format: %d", 
        psoDesc.RTVFormats[0], psoDesc.DSVFormat);
    NetworkManager::LogToFile(buffer);
    sprintf_s(buffer, "[Scene] BuildPSO - Sample Count: %d, Quality: %d", 
        psoDesc.SampleDesc.Count, psoDesc.SampleDesc.Quality);
    NetworkManager::LogToFile(buffer);

    NetworkManager::LogToFile("[Scene] BuildPSO - Creating Opaque PSO");
    HRESULT hr = device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(m_PSOs["PSO_Opaque"].GetAddressOf()));
    if (FAILED(hr)) {
        sprintf_s(buffer, "[Scene] BuildPSO - Failed to create Opaque PSO. HRESULT: 0x%08X", hr);
        NetworkManager::LogToFile(buffer);
        
        // 추가 디버그 정보
        NetworkManager::LogToFile("[Scene] BuildPSO - Debug Info:");
        sprintf_s(buffer, "Input Layout Elements: %d", psoDesc.InputLayout.NumElements);
        NetworkManager::LogToFile(buffer);
        sprintf_s(buffer, "VS ByteCode Size: %zu", psoDesc.VS.BytecodeLength);
        NetworkManager::LogToFile(buffer);
        sprintf_s(buffer, "PS ByteCode Size: %zu", psoDesc.PS.BytecodeLength);
        NetworkManager::LogToFile(buffer);
        return;
    }
    NetworkManager::LogToFile("[Scene] BuildPSO - Opaque PSO created successfully");

    // Shadow PSO 설정
    NetworkManager::LogToFile("[Scene] BuildPSO - Setting up shadow PSO");
    psoDesc.RasterizerState.DepthBias = 10000;
    psoDesc.RasterizerState.DepthBiasClamp = 0.0f;
    psoDesc.RasterizerState.SlopeScaledDepthBias = 1.2f;
    
    if (!m_shaders.contains("VS_Shadow")) {
        NetworkManager::LogToFile("[Scene] BuildPSO - ERROR: VS_Shadow shader not found");
        return;
    }
    psoDesc.VS = CD3DX12_SHADER_BYTECODE(m_shaders.at("VS_Shadow").Get());
    
    if (!m_shaders.contains("PS_Shadow")) {
        NetworkManager::LogToFile("[Scene] BuildPSO - ERROR: PS_Shadow shader not found");
        return;
    }
    psoDesc.PS = CD3DX12_SHADER_BYTECODE(m_shaders.at("PS_Shadow").Get());
    
    psoDesc.NumRenderTargets = 0;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_UNKNOWN;

    NetworkManager::LogToFile("[Scene] BuildPSO - Creating Shadow PSO");
    hr = device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(m_PSOs["PSO_Shadow"].GetAddressOf()));
    if (FAILED(hr)) {
        sprintf_s(buffer, "[Scene] BuildPSO - Failed to create Shadow PSO. HRESULT: 0x%08X", hr);
        NetworkManager::LogToFile(buffer);
        return;
    }
    NetworkManager::LogToFile("[Scene] BuildPSO - Shadow PSO created successfully");
    NetworkManager::LogToFile("[Scene] BuildPSO - Complete");
}

void Scene::BuildVertexBuffer(ID3D12Device* device, ID3D12GraphicsCommandList* commandList)
{
    const UINT vertexBufferSize = m_resourceManager->GetVertexBuffer().size() * sizeof(Vertex);
    // Create the vertex buffer.
    ThrowIfFailed(device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize),
        D3D12_RESOURCE_STATE_COMMON,
        nullptr,
        IID_PPV_ARGS(m_vertexBuffer_default.GetAddressOf())));

    ThrowIfFailed(device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize),
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(m_vertexBuffer_upload.GetAddressOf())));

    D3D12_SUBRESOURCE_DATA subResourceData{};
    subResourceData.pData = m_resourceManager->GetVertexBuffer().data();
    subResourceData.RowPitch = vertexBufferSize;
    subResourceData.SlicePitch = subResourceData.RowPitch;

    commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_vertexBuffer_default.Get(),
        D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST));
    UpdateSubresources(commandList, m_vertexBuffer_default.Get(), m_vertexBuffer_upload.Get(), 0, 0, 1, &subResourceData);
    commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_vertexBuffer_default.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ));
}

void Scene::BuildIndexBuffer(ID3D12Device* device, ID3D12GraphicsCommandList* commandList)
{
    // Create the index buffer.
    const UINT indexBufferSize = m_resourceManager->GetIndexBuffer().size() * sizeof(uint32_t);

    ThrowIfFailed(device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(indexBufferSize),
        D3D12_RESOURCE_STATE_COMMON,
        nullptr,
        IID_PPV_ARGS(m_indexBuffer_default.GetAddressOf())));

    ThrowIfFailed(device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(indexBufferSize),
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(m_indexBuffer_upload.GetAddressOf())));

    D3D12_SUBRESOURCE_DATA subResourceData = {};
    subResourceData.pData = m_resourceManager->GetIndexBuffer().data();
    subResourceData.RowPitch = indexBufferSize;
    subResourceData.SlicePitch = subResourceData.RowPitch;

    commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_indexBuffer_default.Get(),
        D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST));
    UpdateSubresources(commandList, m_indexBuffer_default.Get(), m_indexBuffer_upload.Get(), 0, 0, 1, &subResourceData);
    commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_indexBuffer_default.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ));
}

void Scene::BuildVertexBufferView()
{
    // Initialize the vertex buffer view.
    m_vertexBufferView.BufferLocation = m_vertexBuffer_default->GetGPUVirtualAddress();
    m_vertexBufferView.StrideInBytes = sizeof(Vertex);
    m_vertexBufferView.SizeInBytes = m_resourceManager->GetVertexBuffer().size() * sizeof(Vertex);
}

void Scene::BuildIndexBufferView()
{
    m_indexBufferView.BufferLocation = m_indexBuffer_default->GetGPUVirtualAddress();
    m_indexBufferView.Format = DXGI_FORMAT_R32_UINT;
    m_indexBufferView.SizeInBytes = m_resourceManager->GetIndexBuffer().size() * sizeof(uint32_t);
}

void Scene::BuildDescriptorHeap(ID3D12Device* device)
{
    D3D12_DESCRIPTOR_HEAP_DESC HeapDesc = {};
    HeapDesc.NumDescriptors = static_cast<UINT>(1 + m_DDSFileName.size());
    HeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    HeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    ThrowIfFailed(device->CreateDescriptorHeap(&HeapDesc, IID_PPV_ARGS(m_descriptorHeap.GetAddressOf())));

    m_cbvsrvuavDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

void Scene::BuildConstantBuffer(ID3D12Device* device)
{
    const UINT constantBufferSize = CalcConstantBufferByteSize(sizeof(CommonCB));    // CB size is required to be 256-byte aligned.

    ThrowIfFailed(device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(constantBufferSize),
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&m_constantBuffer)));

    // Map and initialize the constant buffer. We don't unmap this until the
    // app closes. Keeping things mapped for the lifetime of the resource is okay.
    CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
    ThrowIfFailed(m_constantBuffer->Map(0, &readRange, &m_mappedData));

    for (auto& [key, value] : m_objects) {
        visit([device](auto& arg) {arg.BuildConstantBuffer(device); }, value);
    }
}

void Scene::BuildConstantBufferView(ID3D12Device* device)
{
    // Describe and create a constant buffer view.
    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc{};
    cbvDesc.BufferLocation = m_constantBuffer->GetGPUVirtualAddress();
    cbvDesc.SizeInBytes = CalcConstantBufferByteSize(sizeof(CommonCB));

    CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(m_descriptorHeap->GetCPUDescriptorHandleForHeapStart());
    hDescriptor.Offset(0, m_cbvsrvuavDescriptorSize);

    device->CreateConstantBufferView(&cbvDesc, hDescriptor);

}

void Scene::BuildTextureBuffer(ID3D12Device* device, ID3D12GraphicsCommandList* commandList)
{
    // Create the texture.
    for (auto& fileName : m_DDSFileName)
    {
        ComPtr<ID3D12Resource> defaultBuffer;
        ComPtr<ID3D12Resource> uploadBuffer;

        // DDSTexture  ϴ 
        unique_ptr<uint8_t[]> ddsData;
        vector<D3D12_SUBRESOURCE_DATA> subresources;
        ThrowIfFailed(LoadDDSTextureFromFile(device, fileName.c_str(), defaultBuffer.GetAddressOf(), ddsData, subresources));

        //// DirectTex ϴ 
        //ScratchImage image;
        //TexMetadata metadata;

        //ThrowIfFailed(LoadFromDDSFile(L"./Textures/grass.dds", DDS_FLAGS_NONE, &metadata, image));
        ////metadata = image.GetMetadata(); // ڵ带 ϰ  ڵ 3° ڸ nullptr ص ȴ.

        //ThrowIfFailed(CreateTexture(device, metadata, m_textureBuffer_default.GetAddressOf()));
        //ThrowIfFailed(PrepareUpload(device, image.GetImages(), image.GetImageCount(), metadata, subresources));

        const UINT64 uploadBufferSize = GetRequiredIntermediateSize(defaultBuffer.Get(), 0, subresources.size());

        OutputDebugStringA(string{ "current texture subresource size = " + to_string(subresources.size()) + "\n" }.c_str());
        OutputDebugStringA(string{ "current texture mip level = " + to_string(defaultBuffer->GetDesc().MipLevels) + "\n" }.c_str());
        OutputDebugStringA(string{ "current texture format = " + to_string(defaultBuffer->GetDesc().Format) + "\n" }.c_str());

        // Create the GPU upload buffer.
        ThrowIfFailed(device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(uploadBuffer.GetAddressOf())));

        UpdateSubresources(commandList, defaultBuffer.Get(), uploadBuffer.Get(), 0, 0, static_cast<UINT>(subresources.size()), subresources.data());
        commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(defaultBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

        m_textureBuffer_defaults.push_back(move(defaultBuffer));
        m_textureBuffer_uploads.push_back(move(uploadBuffer));
    }
}

void Scene::BuildTextureBufferView(ID3D12Device* device)
{
    // Describe and create a SRV for the texture.
    for (int i = 0; i < m_DDSFileName.size(); ++i)
    {
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Format = m_textureBuffer_defaults[i]->GetDesc().Format;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = m_textureBuffer_defaults[i]->GetDesc().MipLevels;

        CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(m_descriptorHeap->GetCPUDescriptorHandleForHeapStart());
        hDescriptor.Offset(1 + i, m_cbvsrvuavDescriptorSize); // 1 + i   1 ǹ̴   constant buffer view  ̴.      

        device->CreateShaderResourceView(m_textureBuffer_defaults[i].Get(), &srvDesc, hDescriptor);
    }
}

UINT Scene::CalcConstantBufferByteSize(UINT byteSize)
{
    return (byteSize + 255) & ~255;
}

Framework* Scene::GetFramework()
{
    return m_parent;
}

UINT Scene::GetNumOfTexture()
{
    return static_cast<UINT>(m_DDSFileName.size());
}

void Scene::BuildProjMatrix()
{
    XMMATRIX proj = XMMatrixPerspectiveFovLH(XM_PI * 0.25f, m_viewport.Width / m_viewport.Height, 1.0f, 1000.0f);
    XMStoreFloat4x4(&m_proj, proj);
}

void Scene::LoadMeshAnimationTexture()
{
    NetworkManager::LogToFile("[Scene] LoadMeshAnimationTexture - Starting");
    
    m_resourceManager = make_unique<ResourceManager>();
    NetworkManager::LogToFile("[Scene] Creating Plane");
    m_resourceManager->CreatePlane("Plane", 500);
    NetworkManager::LogToFile("[Scene] Creating Terrain");
    m_resourceManager->CreateTerrain("HeightMap.raw", 200, 10, 80);
    
    NetworkManager::LogToFile("[Scene] Loading FBX files");
    m_resourceManager->LoadFbx("1P(boy-idle).fbx", false, false);
    m_resourceManager->LoadFbx("1P(boy-jump).fbx", true, false);
    m_resourceManager->LoadFbx("boy_run_fix.fbx", true, false);
    m_resourceManager->LoadFbx("boy_walk_fix.fbx", true, false);
    m_resourceManager->LoadFbx("boy_pickup_fix.fbx", true, false);
    m_resourceManager->LoadFbx("long_tree.fbx", false, true);
    m_resourceManager->LoadFbx("202411_walk_tiger_center.fbx", false, false);
    
    NetworkManager::LogToFile("[Scene] Loading DDS textures");
    int i = 0;
    m_DDSFileName.push_back(L"./Textures/boy.dds");
    m_subTextureData.insert({ L"boy", i++ });
    m_DDSFileName.push_back(L"./Textures/bricks3.dds");
    m_subTextureData.insert({ L"bricks3", i++ });
    m_DDSFileName.push_back(L"./Textures/checkboard.dds");
    m_subTextureData.insert({ L"checkboard", i++ });
    m_DDSFileName.push_back(L"./Textures/grass.dds");
    m_subTextureData.insert({ L"grass", i++ });
    m_DDSFileName.push_back(L"./Textures/tile.dds");
    m_subTextureData.insert({ L"tile", i++ });
    m_DDSFileName.push_back(L"./Textures/WireFence.dds");
    m_subTextureData.insert({ L"WireFence", i++ });
    m_DDSFileName.push_back(L"./Textures/god.dds");
    m_subTextureData.insert({ L"god", i++ });
    m_DDSFileName.push_back(L"./Textures/sister.dds");
    m_subTextureData.insert({ L"sister", i++ });
    m_DDSFileName.push_back(L"./Textures/water1.dds");
    m_subTextureData.insert({ L"water1", i++ });
    m_DDSFileName.push_back(L"./Textures/PP_Color_Palette.dds");
    m_subTextureData.insert({ L"PP_Color_Palette", i++ });
    m_DDSFileName.push_back(L"./Textures/tigercolor.dds");
    m_subTextureData.insert({ L"tigercolor", i++ });
    m_DDSFileName.push_back(L"./Textures/stone.dds");
    m_subTextureData.insert({ L"stone", i++ });
    m_DDSFileName.push_back(L"./Textures/normaltree_texture.dds");
    m_subTextureData.insert({ L"normalTree", i++ });
    m_DDSFileName.push_back(L"./Textures/longtree_texture.dds");
    m_subTextureData.insert({ L"longTree", i++ });
    m_DDSFileName.push_back(L"./Textures/rock(smooth).dds");
    m_subTextureData.insert({ L"rock", i++ });
    
    NetworkManager::LogToFile("[Scene] LoadMeshAnimationTexture - Complete");
}

void Scene::SetState(ID3D12GraphicsCommandList* commandList)
{
    // Set necessary state.
    commandList->SetPipelineState(m_PSOs["PSO_Opaque"].Get());
    commandList->SetGraphicsRootSignature(m_rootSignature.Get());
    commandList->RSSetViewports(1, &m_viewport);
    commandList->RSSetScissorRects(1, &m_scissorRect);
}

void Scene::SetDescriptorHeaps(ID3D12GraphicsCommandList* commandList)
{
    ID3D12DescriptorHeap* ppHeaps[] = { m_descriptorHeap.Get() };
    commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
}

// Update frame-based values.
void Scene::OnUpdate(GameTimer& gTimer)
{
    // 기존 업데이트 코드
    for (auto& [key, value] : m_objects) {
        visit([&gTimer](auto& arg) {arg.OnUpdate(gTimer); }, value);
    }
    
    // 호랑이 보간 업데이트
    float deltaTime = gTimer.DeltaTime();
    std::vector<int> completedTigers;
    
    for (auto& [tigerID, interpData] : m_tigerInterpolationData) {
        wstring objectName = L"NetworkTiger_" + std::to_wstring(tigerID);
        if (m_objects.find(objectName) != m_objects.end()) {
            auto& tiger = GetObj<TigerObject>(objectName);
            
            interpData.currentTime += deltaTime;
            float t = min(interpData.currentTime / interpData.interpolationTime, 1.0f);
            
            // 선형 보간 수행
            float newX = std::lerp(interpData.prevPosition.x, interpData.targetPosition.x, t);
            float newY = std::lerp(interpData.prevPosition.y, interpData.targetPosition.y, t);
            float newZ = std::lerp(interpData.prevPosition.z, interpData.targetPosition.z, t);
            float newRotY = std::lerp(interpData.prevRotY, interpData.targetRotY, t);
            
            // 실제 위치와 회전 업데이트
            tiger.GetComponent<Position>().SetXMVECTOR(XMVectorSet(newX, newY, newZ, 1.0f));
            tiger.GetComponent<Rotation>().SetXMVECTOR(XMVectorSet(0.0f, newRotY, 0.0f, 0.0f));
            
            // 보간이 완료되면 정리 대상에 추가
            if (t >= 1.0f) {
                completedTigers.push_back(tigerID);
            }
        } else {
            // 객체가 존재하지 않으면 보간 데이터도 정리
            completedTigers.push_back(tigerID);
        }
    }
    
    // 완료된 보간 데이터 정리
    for (int tigerID : completedTigers) {
        m_tigerInterpolationData.erase(tigerID);
    }
    
    // 메모리 사용량 모니터링 (30초마다, 로그 출력 최소화)
    static float memoryCheckTimer = 0.0f;
    memoryCheckTimer += deltaTime;
    if (memoryCheckTimer >= 30.0f) {
        memoryCheckTimer = 0.0f;
        
        // 메모리 사용량 확인 (80% 초과 시에만 경고)
        PROCESS_MEMORY_COUNTERS_EX pmc;
        if (GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc))) {
            float memoryUsageMB = static_cast<float>(pmc.WorkingSetSize) / (1024.0f * 1024.0f);
            float memoryUsagePercent = (memoryUsageMB / 16057.0f) * 100.0f; // 16GB 기준
            
            if (memoryUsagePercent > 80.0f) {
                char buffer[256];
                sprintf_s(buffer, "[Memory] High usage: %.1f%% (%.1f MB), Objects: %zu", 
                    memoryUsagePercent, memoryUsageMB, m_objects.size());
                NetworkManager::LogToFile(buffer);
            }
        }
    }
    
    m_shadow->UpdateShadow();
    memcpy(static_cast<UINT8*>(m_mappedData) + sizeof(XMMATRIX), &XMMatrixTranspose(XMLoadFloat4x4(&m_proj)), sizeof(XMMATRIX));
}

// Render the scene.
void Scene::OnRender(ID3D12Device* device, ID3D12GraphicsCommandList* commandList, ePass pass)
{
    switch (pass)
    {
    case ePass::Shadow:
    {
        commandList->SetGraphicsRootSignature(m_rootSignature.Get());
        ID3D12DescriptorHeap* ppHeaps[] = { m_descriptorHeap.Get() };
        commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
        commandList->SetGraphicsRootDescriptorTable(3, m_shadow->GetGpuDescHandleForNullShadow());
        CD3DX12_GPU_DESCRIPTOR_HANDLE hDescriptor(m_descriptorHeap->GetGPUDescriptorHandleForHeapStart());
        commandList->SetGraphicsRootDescriptorTable(0, hDescriptor);
        commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
        commandList->IASetIndexBuffer(&m_indexBufferView);
        m_shadow->DrawShadowMap();

        // 그림자 패스에서 객체들 렌더링
        for (auto& [key, value] : m_objects) {
            visit([device, commandList](auto& arg) {arg.OnRender(device, commandList); }, value);
        }
        break;
    }
    case ePass::Default:
    {
        commandList->RSSetViewports(1, &m_viewport);
        commandList->RSSetScissorRects(1, &m_scissorRect);
        commandList->SetPipelineState(m_PSOs["PSO_Opaque"].Get());
        commandList->SetGraphicsRootDescriptorTable(3, m_shadow->GetGpuDescHandleForShadow());
        RenderObjects(device, commandList);
        break;
    }
    default:
        break;
    }
}

void Scene::OnResize(UINT width, UINT height)
{
    m_viewport = { CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f) };
    m_scissorRect = { CD3DX12_RECT(0, 0, static_cast<LONG>(width), static_cast<LONG>(height)) };
    BuildProjMatrix();
}

void Scene::OnDestroy()
{
    CD3DX12_RANGE Range(0, CalcConstantBufferByteSize(sizeof(ObjectCB)));
    m_constantBuffer->Unmap(0, &Range);
}

void Scene::OnKeyDown(UINT8 key)
{

}

void Scene::OnKeyUp(UINT8 key)
{
}

void Scene::CheckCollision()
{
    for (auto it1 = m_objects.begin(); it1 != m_objects.end(); ++it1) {
        Object* object1 = visit([](auto& arg)->Object* { return &arg; }, it1->second);
        if (object1->FindComponent<Collider>() == false) continue;
        Collider& collider1 = object1->GetComponent<Collider>();
        for (auto it2 = std::next(it1); it2 != m_objects.end(); ++it2) {
            Object* object2 = visit([](auto& arg)->Object* { return &arg; }, it2->second);
            if (object2->FindComponent<Collider>() == false) continue;
            Collider& collider2 = object2->GetComponent<Collider>();
            if (collider1.mAABB.Intersects(collider2.mAABB)) { // obj1  obj2  浹ߴٸ?
                if (collider1.FindCollisionObj(object2)) { //   Ʈ 浹  ִٸ?
                    CollisionState state = collider1.mCollisionStates.at(object2);
                    if (state == CollisionState::ENTER || state == CollisionState::STAY) {
                        collider1.mCollisionStates[object2] = CollisionState::STAY;
                        OutputDebugStringW((it1->first + L" and " + it2->first + L" collision stay\n").c_str());
                        OutputDebugStringW((L"Collision count: " + to_wstring(collider1.mCollisionStates.size()) + L"\n").c_str());
                    }
                    else {
                        collider1.mCollisionStates[object2] = CollisionState::ENTER;
                    }
                }
                else {
                    collider1.mCollisionStates[object2] = CollisionState::ENTER;
                    OutputDebugStringW((it1->first + L" and " + it2->first + L" collision enter\n").c_str());
                    OutputDebugStringW((L"Collision count: " + to_wstring(collider1.mCollisionStates.size()) + L"\n").c_str());
                }

                if (collider2.FindCollisionObj(object1)) {
                    CollisionState state = collider2.mCollisionStates.at(object1);
                    if (state == CollisionState::ENTER || state == CollisionState::STAY) {
                        collider2.mCollisionStates[object1] = CollisionState::STAY;
                        OutputDebugStringW((it2->first + L" and " + it1->first + L" collision stay\n").c_str());
                        OutputDebugStringW((L"Collision count: " + to_wstring(collider2.mCollisionStates.size()) + L"\n").c_str());
                    }
                    else {
                        collider2.mCollisionStates[object1] = CollisionState::ENTER;
                    }
                }
                else {
                    collider2.mCollisionStates[object1] = CollisionState::ENTER;
                    OutputDebugStringW((it2->first + L" and " + it1->first + L" collision enter\n").c_str());
                    OutputDebugStringW((L"Collision count: " + to_wstring(collider2.mCollisionStates.size()) + L"\n").c_str());
                }
            }
            else { // obj1  obj2 浹 ʾҴٸ
                if (collider1.FindCollisionObj(object2)) {
                    CollisionState state = collider1.mCollisionStates.at(object2);
                    if (state == CollisionState::EXIT) {
                        collider1.mCollisionStates.erase(object2);
                        OutputDebugStringW((it1->first + L" and " + it2->first + L" collision exit\n").c_str());
                        OutputDebugStringW((L"Collision count: " + to_wstring(collider1.mCollisionStates.size()) + L"\n").c_str());
                    }
                    else {
                        collider1.mCollisionStates[object2] = CollisionState::EXIT;
                        OutputDebugStringW((it1->first + L" and " + it2->first + L" collision exit\n").c_str());
                        OutputDebugStringW((L"Collision count: " + to_wstring(collider1.mCollisionStates.size()) + L"\n").c_str());
                    }
                }

                if (collider2.FindCollisionObj(object1)) {
                    CollisionState state = collider2.mCollisionStates.at(object1);
                    if (state == CollisionState::EXIT) {
                        collider2.mCollisionStates.erase(object1);
                        OutputDebugStringW((it2->first + L" and " + it1->first + L" collision exit\n").c_str());
                        OutputDebugStringW((L"Collision count: " + to_wstring(collider2.mCollisionStates.size()) + L"\n").c_str());
                    }
                    else {
                        collider2.mCollisionStates[object1] = CollisionState::EXIT;
                        OutputDebugStringW((it2->first + L" and " + it1->first + L" collision exit\n").c_str());
                        OutputDebugStringW((L"Collision count: " + to_wstring(collider2.mCollisionStates.size()) + L"\n").c_str());
                    }
                }
            }
        }
    }
}

void Scene::LateUpdate(GameTimer& gTimer)
{
    for (auto& [key, value] : m_objects)
    {
        visit([&gTimer](auto& arg) {arg.LateUpdate(gTimer); }, value);
    }
}

std::wstring Scene::GetSceneName() const
{
    return m_name;
}

ResourceManager& Scene::GetResourceManager()
{
    return *(m_resourceManager.get());
}

void* Scene::GetConstantBufferMappedData()
{
    // TODO: ⿡ return  մϴ.
    return m_mappedData;
}

ID3D12DescriptorHeap* Scene::GetDescriptorHeap()
{
    return m_descriptorHeap.Get();
}

void Scene::CreateTigerObject(int tigerID, float x, float y, float z, ID3D12Device* device) {
    if (!device) {
        return;
    }

    wstring objectName = L"NetworkTiger_" + std::to_wstring(tigerID);
    
    try {
        // 기존 Tiger가 있다면 제거
        if (m_objects.find(objectName) != m_objects.end()) {
            m_objects.erase(objectName);
        }
        
        // 객체 수 제한 (메모리 보호)
        if (m_objects.size() > 400) {
            return;
        }
        
        AddObj(objectName, TigerObject(this));
        
        auto& tiger = GetObj<TigerObject>(objectName);
        
        // 지형 높이 계산
        float terrainHeight = CalculateTerrainHeight(x, z);
        
        // 컴포넌트 추가
        tiger.AddComponent<Position>(Position{ x, terrainHeight, z, 1.0f, &tiger });
        tiger.AddComponent<Rotation>(Rotation{ 0.0f, 0.0f, 0.0f, 0.0f, &tiger });
        tiger.AddComponent<Scale>(Scale{ 0.2f, &tiger });
        tiger.AddComponent<Velocity>(Velocity{ 0.0f, 0.0f, 0.0f, 0.0f, &tiger });
        tiger.AddComponent<Animation>(Animation{ m_resourceManager->GetAnimationData(), &tiger });
        tiger.AddComponent<Mesh>(Mesh{ m_resourceManager->GetSubMeshData().at("202411_walk_tiger_center.fbx"), &tiger });
        tiger.AddComponent<Texture>(Texture{ m_subTextureData.at(L"tigercolor"), &tiger });
        tiger.AddComponent<Collider>(Collider{ 0.0f, 0.0f, 0.0f, 2.0f, 50.0f, 10.0f, &tiger });
        
        // 상수 버퍼 생성
        tiger.BuildConstantBuffer(device);
        
    } catch (const std::bad_alloc& e) {
        // 메모리 부족 시 기존 객체 정리 시도
        if (m_objects.find(objectName) != m_objects.end()) {
            m_objects.erase(objectName);
        }
    } catch (const std::exception& e) {
        // 예외 발생 시 기존 객체 정리 시도
        if (m_objects.find(objectName) != m_objects.end()) {
            m_objects.erase(objectName);
        }
    } catch (...) {
        // 알 수 없는 예외 발생 시 기존 객체 정리 시도
        if (m_objects.find(objectName) != m_objects.end()) {
            m_objects.erase(objectName);
        }
    }
}

void Scene::UpdateTigerObject(int tigerID, float x, float y, float z, float rotY) {
    wstring objectName = L"NetworkTiger_" + std::to_wstring(tigerID);
    
    if (m_objects.find(objectName) != m_objects.end()) {
        auto& tiger = GetObj<TigerObject>(objectName);
        
        // 현재 위치를 이전 위치로 저장
        XMFLOAT3 currentPos;
        XMStoreFloat3(&currentPos, tiger.GetComponent<Position>().GetXMVECTOR());
        float currentRotY = XMVectorGetY(tiger.GetComponent<Rotation>().GetXMVECTOR());
        
        // 지형 높이 계산
        float terrainHeight = CalculateTerrainHeight(x, z);
        
        // 보간 데이터 업데이트
        TigerInterpolationData& interpData = m_tigerInterpolationData[tigerID];
        interpData.prevPosition = currentPos;
        interpData.targetPosition = XMFLOAT3(x, terrainHeight, z);  // y 대신 terrainHeight 사용
        interpData.prevRotY = currentRotY;
        interpData.targetRotY = rotY;
        interpData.interpolationTime = INTERPOLATION_DURATION;
        interpData.currentTime = 0.0f;
    }
}

float Scene::CalculateTerrainHeight(float x, float z) {
    ResourceManager& rm = GetResourceManager();
    int width = rm.GetTerrainData().terrainWidth;
    int height = rm.GetTerrainData().terrainHeight;
    int terrainScale = rm.GetTerrainData().terrainScale;
    
    if (x >= 0 && z >= 0 && x <= width * terrainScale && z <= height * terrainScale) {
        vector<Vertex>& vertexBuffer = rm.GetVertexBuffer();
        UINT startVertex = GetObj<TerrainObject>(L"TerrainObject").GetComponent<Mesh>().mSubMeshData.startVertexLocation;

        int indexX = (int)(x / terrainScale);
        int indexZ = (int)(z / terrainScale);

        float leftBottom = vertexBuffer[startVertex + indexZ * width + indexX].position.y;
        float rightBottom = vertexBuffer[startVertex + indexZ * width + indexX + 1].position.y;
        float leftTop = vertexBuffer[startVertex + (indexZ + 1) * width + indexX].position.y;
        float rightTop = vertexBuffer[startVertex + (indexZ + 1) * width + indexX + 1].position.y;

        float offsetX = x / terrainScale - indexX;
        float offsetZ = z / terrainScale - indexZ;

        float lerpXBottom = (1 - offsetX) * leftBottom + offsetX * rightBottom;
        float lerpXTop = (1 - offsetX) * leftTop + offsetX * rightTop;

        return (1 - offsetZ) * lerpXBottom + offsetZ * lerpXTop;
    }
    return 0.0f;
}

void Scene::Initialize() {
    NetworkManager::LogToFile("[Scene] Starting initialization");
    
    if (m_device) {
        NetworkManager::LogToFile("[Scene] Device initialized successfully");
        NetworkManager::LogToFile("[Scene] Starting to build objects...");
        try {
            // 리소스 매니저 초기화 확인
            if (!m_resourceManager) {
                NetworkManager::LogToFile("[Scene] Creating ResourceManager");
                m_resourceManager = make_unique<ResourceManager>();
            }
            NetworkManager::LogToFile("[Scene] ResourceManager initialized");
        }
        catch (const std::exception& e) {
            NetworkManager::LogToFile(std::string("[Scene] Error during initialization: ") + e.what());
        }
    } else {
        NetworkManager::LogToFile("[Scene] Device initialization failed");
    }
    
    NetworkManager::LogToFile("[Scene] Initialization complete");
}

void Scene::ProcessTigerSpawn(const PacketTigerSpawn* packet) {
    if (!m_device) {
        NetworkManager::LogToFile("[Scene] Queueing tiger spawn - Device not ready");
        // 대기 큐에 추가
        m_pendingTigerSpawns.push_back(*packet);
        return;
    }
    
    // 타이거 생성 시도
    CreateTigerObject(packet->tigerID, packet->x, packet->y, packet->z, m_device);
}

// Device 초기화 완료 시 호출
void Scene::OnDeviceReady() {
    NetworkManager::LogToFile("[Scene] Device is ready, processing pending spawns");
    
    // 대기 중인 타이거 스폰 처리
    for (const auto& spawnPacket : m_pendingTigerSpawns) {
        CreateTigerObject(spawnPacket.tigerID, 
            spawnPacket.x, spawnPacket.y, spawnPacket.z, m_device);
    }
    m_pendingTigerSpawns.clear();
}

void Scene::BuildShadow()
{
    m_shadow = make_unique<Shadow>(this, 2048, 2048);
}

void Scene::BuildShaders()
{
    m_shaders["VS_Opaque"] = CompileShader(L"Shaders/Opaque.hlsl", nullptr, "VS", "vs_5_1");
    m_shaders["PS_Opaque"] = CompileShader(L"Shaders/Opaque.hlsl", nullptr, "PS", "ps_5_1");
    m_shaders["VS_Shadow"] = CompileShader(L"Shaders/Shadow.hlsl", nullptr, "VS", "vs_5_1");
    m_shaders["PS_Shadow"] = CompileShader(L"Shaders/Shadow.hlsl", nullptr, "PS", "ps_5_1");
}

void Scene::BuildInputElement()
{
    m_inputElement =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "WEIGHT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "BONEINDEX", 0, DXGI_FORMAT_R32G32B32A32_SINT, 0, 48, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };
}

ComPtr<ID3DBlob> Scene::CompileShader(
    const std::wstring& fileName,
    const D3D_SHADER_MACRO* defines,
    const std::string& entryPoint,
    const std::string& target)
{
    UINT compileFlags = 0;
#if defined(_DEBUG) || defined(DBG)
    compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    HRESULT hr;
    ComPtr<ID3DBlob> byteCode = nullptr;
    ComPtr<ID3DBlob> errors;
    hr = D3DCompileFromFile(fileName.c_str(), defines, D3D_COMPILE_STANDARD_FILE_INCLUDE,
        entryPoint.c_str(), target.c_str(), compileFlags, 0, &byteCode, &errors);

    if (errors != nullptr)
    {
        OutputDebugStringA((char*)errors->GetBufferPointer());
    }
    ThrowIfFailed(hr);

    return byteCode;
}

void Scene::RenderObjects(ID3D12Device* device, ID3D12GraphicsCommandList* commandList)
{
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
    commandList->IASetIndexBuffer(&m_indexBufferView);

    CD3DX12_GPU_DESCRIPTOR_HANDLE hDescriptor(m_descriptorHeap->GetGPUDescriptorHandleForHeapStart());
    commandList->SetGraphicsRootDescriptorTable(0, hDescriptor);

    // 기존 오브젝트 렌더링
    for (auto& [key, value] : m_objects) {
        visit([device, commandList](auto& arg) {arg.OnRender(device, commandList); }, value);
    }
}






