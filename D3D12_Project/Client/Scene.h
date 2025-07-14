#pragma once
#include "stdafx.h"
#include "Object.h"  // Objects.h 대신 Object.h를 포함
#include "ResourceManager.h"
#include "Packet.h"  // 패킷 정의를 위해 추가
#include <utility>
#include "Shadow.h"

class NetworkManager;  // 전방 선언 추가
class GameTimer;
class Framework;

class Scene
{
public:
    Scene() = default;
    Scene(Framework* parent, UINT width, UINT height, wstring name);
    
    // 복사 생성자와 복사 할당 연산자를 삭제
    Scene(const Scene& other) = delete;
    Scene& operator=(const Scene& other) = delete;
    
    // 이동 생성자와 이동 할당 연산자만 허용
    Scene(Scene&& other) noexcept;
    Scene& operator=(Scene&& other) noexcept;

    virtual void OnInit(ID3D12Device* device, ID3D12GraphicsCommandList* commandList);
    virtual void OnUpdate(GameTimer& gTimer);
    virtual void CheckCollision();
    virtual void LateUpdate(GameTimer& gTimer);
    virtual void OnRender(ID3D12Device* device, ID3D12GraphicsCommandList* commandList, ePass pass);
    virtual void OnResize(UINT width, UINT height);
    virtual void OnDestroy();

    virtual void OnKeyDown(UINT8 key);
    virtual void OnKeyUp(UINT8 key);
    virtual void OnDeviceReady();
    virtual void Initialize();

    void SetState(ID3D12GraphicsCommandList* commandList);
    void SetDescriptorHeaps(ID3D12GraphicsCommandList* commandList);

    wstring GetSceneName() const;
    ResourceManager& GetResourceManager();
    ID3D12Device* GetDevice() const { return m_device; }
    Framework* GetFramework();

    template<typename T>
    void AddObj(const wstring& name, T&& object) { m_objects.emplace(name, move(object)); }

    template<typename T>
    T& GetObj(const wstring& name) { return get<T>(m_objects.at(name)); }

    void* GetConstantBufferMappedData();
    ID3D12DescriptorHeap* GetDescriptorHeap();

    UINT CalcConstantBufferByteSize(UINT byteSize);

    const auto& GetSubTextureData() const { return m_subTextureData; }; //Hong other player 에서 사용

    void ProcessTigerSpawn(const PacketTigerSpawn* packet);
    void CreateTigerObject(int tigerID, float x, float y, float z, ID3D12Device* device);
    void UpdateTigerObject(int tigerID, float x, float y, float z, float rotY);

    float CalculateTerrainHeight(float x, float z);

    UINT GetNumOfTexture();

    std::unordered_map<std::string, ComPtr<ID3D12PipelineState>>& GetPSOs() { return m_PSOs; }
    void RenderObjects(ID3D12Device* device, ID3D12GraphicsCommandList* commandList);

private:
    void BuildRootSignature(ID3D12Device* device);
    void BuildPSO(ID3D12Device* device);
    void BuildVertexBuffer(ID3D12Device* device, ID3D12GraphicsCommandList* commandList);
    void BuildIndexBuffer(ID3D12Device* device, ID3D12GraphicsCommandList* commandList);
    void BuildVertexBufferView();
    void BuildIndexBufferView();
    void BuildConstantBuffer(ID3D12Device* device);
    void BuildConstantBufferView(ID3D12Device* device);
    void BuildTextureBuffer(ID3D12Device* device, ID3D12GraphicsCommandList* commandList);
    void BuildTextureBufferView(ID3D12Device* device);
    void BuildDescriptorHeap(ID3D12Device* device);
    void BuildProjMatrix();
    void BuildObjects(ID3D12Device* device);
    void LoadMeshAnimationTexture();
    void BuildShadow();
    void BuildShaders();
    void BuildInputElement();

    ComPtr<ID3DBlob> CompileShader(
        const std::wstring& fileName,
        const D3D_SHADER_MACRO* defines,
        const std::string& entryPoint,
        const std::string& target);

    struct TigerInterpolationData {
        XMFLOAT3 prevPosition;
        XMFLOAT3 targetPosition;
        float prevRotY;
        float targetRotY;
        float interpolationTime;
        float currentTime;
    };
    
    std::unordered_map<int, TigerInterpolationData> m_tigerInterpolationData;
    const float INTERPOLATION_DURATION = 0.1f; // 보간에 걸리는 시간 (100ms로 조정하여 더 부드러운 움직임)

    Framework* m_parent = nullptr;
    wstring m_name;
    unordered_map<wstring, ObjectVariant> m_objects;
    unique_ptr<ResourceManager> m_resourceManager;
    //
    CD3DX12_VIEWPORT m_viewport;
    CD3DX12_RECT m_scissorRect;
    ComPtr<ID3D12RootSignature> m_rootSignature;
    std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> m_PSOs;
    std::unordered_map<std::string, ComPtr<ID3DBlob>> m_shaders;
    // App resources.
    ComPtr<ID3D12DescriptorHeap> m_descriptorHeap;
    UINT m_cbvsrvuavDescriptorSize;
    //
    ComPtr<ID3D12Resource> m_vertexBuffer_default;
    ComPtr<ID3D12Resource> m_indexBuffer_default;
    ComPtr<ID3D12Resource> m_vertexBuffer_upload;
    ComPtr<ID3D12Resource> m_indexBuffer_upload;
    D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;
    D3D12_INDEX_BUFFER_VIEW m_indexBufferView;
    //
    unordered_map<wstring, int> m_subTextureData;
    vector<wstring> m_DDSFileName;
    vector<ComPtr<ID3D12Resource>> m_textureBuffer_defaults;
    vector<ComPtr<ID3D12Resource>> m_textureBuffer_uploads;
    //
    ComPtr<ID3D12Resource> m_constantBuffer;
    void* m_mappedData;
    //
    XMFLOAT4X4 m_proj;
    ID3D12Device* m_device{ nullptr };
    std::vector<PacketTigerSpawn> m_pendingTigerSpawns;  // 대기 중인 타이거 스폰 패킷들

    unique_ptr<Shadow> m_shadow = nullptr;
    std::vector<D3D12_INPUT_ELEMENT_DESC> m_inputElement;
};