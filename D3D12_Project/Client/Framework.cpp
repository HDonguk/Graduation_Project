#include "stdafx.h"
#include "Framework.h"
#include "DXSampleHelper.h"
#include <DirectXColors.h>
#include "OtherPlayerManager.h"
#include "NetworkManager.h"
#include <thread>
#include <atlbase.h>
#include <atlconv.h>

Framework::Framework(HINSTANCE hInstance, int nCmdShow, UINT width, UINT height, std::wstring name) :
    m_frameIndex(0),
    m_rtvDescriptorSize(0),
    m_useWarpDevice(false),
    m_isInLoginScreen(true)
{
    //ThrowIfFailed(DXGIDeclareAdapterRemovalSupport());
    m_win32App = make_unique<Win32Application>(width, height, name);
}

int Framework::Run(HINSTANCE hInstance, int nCmdShow)
{
    // Initialize the framework.
    OnInit(hInstance, nCmdShow);

    ShowWindow(m_win32App->GetHwnd(), nCmdShow);
    ShowCursor(false);
    m_Timer.Reset();

    // Main loop.
    MSG msg{};
    while (msg.message != WM_QUIT)
    {
        // Process any messages in the queue.
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            try {
                m_Timer.Tick();
                CalculateFrame();
                OnUpdate(m_Timer);
                CheckCollision();
                LateUpdate(m_Timer);
                OnNetworkUpdate(); // 네트워크 업데이트 추가 (나무 생성 큐 처리)
                OnRender();

                // 로그인 화면일 때만 FPS 제한 (60FPS)
                if (m_isInLoginScreen) {
                    Sleep(16); // 1000ms / 60 ≈ 16ms
                }
            }
            catch (const std::exception& e) {
                NetworkManager::LogToFile("[Critical] Exception in main loop: " + std::string(e.what()));
                // 예외 발생 시 잠시 대기 후 계속
                Sleep(100);
            }
            catch (...) {
                NetworkManager::LogToFile("[Critical] Unknown exception in main loop");
                Sleep(100);
            }
        }
    }
    OnDestroy();

    // Return this part of the WM_QUIT message to Windows.
    return static_cast<char>(msg.wParam);
}

void Framework::OnInit(HINSTANCE hInstance, int nCmdShow)
{
    NetworkManager::LogToFile("[Framework] Starting initialization");

    InitWnd(hInstance);
    NetworkManager::LogToFile("[Framework] Window initialized");
    
    BuildFactoryAndDevice();
    NetworkManager::LogToFile("[Framework] D3D12 factory and device created");
    
    BuildCommandQueueAndSwapChain();
    NetworkManager::LogToFile("[Framework] Command queue and swap chain created");
    
    BuildCommandListAndAllocator();
    BuildRtvDescriptorHeap();
    BuildRtv();
    BuildDsvDescriptorHeap();
    BuildDepthStencilBuffer(m_win32App->GetWidth(), m_win32App->GetHeight());
    BuildDsv();
    BuildFence();
    NetworkManager::LogToFile("[Framework] D3D12 resources initialized");

    // Scene 생성
    NetworkManager::LogToFile("[Framework] Starting scene creation");
    BuildScenes(m_device.Get(), m_commandList.Get());
    
    // Command List 실행
    NetworkManager::LogToFile("[Framework] Executing initial command list");
    ThrowIfFailed(m_commandList->Close());
    ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
    
    // 초기 프레임 동기화
    WaitForPreviousFrame();
    
    // 로그인 화면 상태로 초기화
    m_isInLoginScreen = true;
    
    // 로그인 UI 초기화
    m_loginUI.Initialize(m_win32App->GetHwnd(), hInstance);
    SetFocus(m_loginUI.GetEditUsernameHwnd()); // username 입력란에 포커스
    m_loginUI.SetLoginCallback([this](const std::wstring& username, const std::wstring& nickname) {
        // 로그인 콜백 - 현재는 사용하지 않음
    });
    m_loginUI.SetConnectCallback([this](const std::wstring& serverIP, int port) {
        OnConnectToServer(serverIP, port);
    });
    
    // 네트워크 콜백 설정
    networkManager.SetLoginSuccessCallback([this](int clientID, const std::string& username) {
        OnLoginSuccess(clientID, username);
    });
    networkManager.SetLoginFailedCallback([this](const std::string& errorMessage) {
        OnLoginFailed(errorMessage);
    });
    
    NetworkManager::LogToFile("[Framework] Initialization complete");
}

void Framework::OnUpdate(GameTimer& gTimer)
{
    if (m_isInLoginScreen) {
        // 로그인 화면에서는 게임 업데이트 하지 않음
        return;
    }

    try {
        if (m_scenes.find(L"BaseScene") != m_scenes.end()) {
            m_scenes[L"BaseScene"].OnUpdate(gTimer);

            // 네트워크가 실행 중이고 로그인된 상태일 때만 업데이트 전송
            if (networkManager.IsRunning() && networkManager.IsLoggedIn()) {
                static float networkTimer = 0.0f;
                networkTimer += gTimer.DeltaTime();
                if (networkTimer >= 1.0f) {  // 1초마다 업데이트 (네트워크 부하 더욱 감소)
                    auto& player = m_scenes[L"BaseScene"].GetObj<PlayerObject>(L"PlayerObject");
                    auto& position = player.GetComponent<Position>();
                    auto& rotation = player.GetComponent<Rotation>();
                    
                    networkManager.SendPlayerUpdate(
                        position.mFloat4.x,
                        position.mFloat4.y,
                        position.mFloat4.z,
                        rotation.mFloat4.y
                    );
                    networkTimer = 0.0f;
                }
            }
        }
    }
    catch (const std::exception& e) {
        NetworkManager::LogToFile("[Critical] Exception in OnUpdate: " + std::string(e.what()));
    }
    catch (...) {
        NetworkManager::LogToFile("[Critical] Unknown exception in OnUpdate");
    }
}

void Framework::CheckCollision()
{
    if (m_isInLoginScreen) return;
    if (m_scenes.find(L"BaseScene") != m_scenes.end()) {
        m_scenes[L"BaseScene"].CheckCollision();
    }
}

void Framework::LateUpdate(GameTimer& gTimer)
{
    if (m_isInLoginScreen) return;
    if (m_scenes.find(L"BaseScene") != m_scenes.end()) {
        m_scenes[L"BaseScene"].LateUpdate(gTimer);
    }
}

// Render the scene.
void Framework::OnRender()
{
    if (m_isInLoginScreen) {
        // 로그인 화면에서는 간단한 배경만 렌더링
        // Windows 컨트롤은 자동으로 렌더링됨
        return;
    }
    
    // 게임 화면 렌더링 시작 로그
    static int renderCount = 0;
    if (++renderCount % 60 == 0) { // 1초마다 로그 (60FPS 기준)
        NetworkManager::LogToFile("[Framework] Rendering game screen - Scene: " + std::string(m_currentSceneName.begin(), m_currentSceneName.end()));
    }

    try {
        // 메모리 상태 체크 (렌더링 전)
        MEMORYSTATUSEX memInfo;
        memInfo.dwLength = sizeof(MEMORYSTATUSEX);
        if (GlobalMemoryStatusEx(&memInfo)) {
            DWORDLONG usedMemory = memInfo.ullTotalPhys - memInfo.ullAvailPhys;
            DWORDLONG totalMemory = memInfo.ullTotalPhys;
            double memoryUsagePercent = (double)usedMemory / totalMemory * 100.0;
            
            if (memoryUsagePercent > 95.0) {
                NetworkManager::LogToFile("[Critical] Memory usage too high (" + std::to_string(memoryUsagePercent) + "%), skipping render");
                Sleep(100);  // 잠시 대기
                return;
            }
        }

        // Record all the commands we need to render the scene into the command list.
        PopulateCommandList();

        // Execute the command list.
        ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
        m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

        // Present the frame.
        ThrowIfFailed(m_swapChain->Present(0, 0));

        WaitForPreviousFrame();
    }
    catch (const std::exception& e) {
        NetworkManager::LogToFile("[Critical] Exception in OnRender: " + std::string(e.what()));
        Sleep(100);  // 예외 발생 시 잠시 대기
    }
    catch (...) {
        NetworkManager::LogToFile("[Critical] Unknown exception in OnRender");
        Sleep(100);  // 예외 발생 시 잠시 대기
    }
}

void Framework::OnResize(UINT width, UINT height, bool minimized)
{
    // Determine if the swap buffers and other resources need to be resized or not.
    if ((width != m_win32App->GetWidth() || height != m_win32App->GetHeight()) && !minimized)
    {
        WaitForPreviousFrame();
        ThrowIfFailed(m_commandList->Reset(m_commandAllocator.Get(), nullptr));

        m_win32App->OnResize(width, height);

        for (UINT n = 0; n < FrameCount; n++)
        {
            m_renderTargets[n].Reset();
        }
        m_depthStencilBuffer.Reset();

        // Resize the swap chain to the desired dimensions.
        DXGI_SWAP_CHAIN_DESC desc = {};
        m_swapChain->GetDesc(&desc);
        ThrowIfFailed(m_swapChain->ResizeBuffers(FrameCount, width, height, desc.BufferDesc.Format, desc.Flags));
        m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
        BuildRtv();

        BuildDepthStencilBuffer(width, height);
        BuildDsv();

        if (m_scenes.find(m_currentSceneName) != m_scenes.end()) {
            m_scenes[m_currentSceneName].OnResize(width, height);
        }

        ThrowIfFailed(m_commandList->Close());
        ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
        m_commandQueue.Get()->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
        WaitForPreviousFrame();
    }
    m_win32App->SetWindowVisible(!minimized);
}

void Framework::OnDestroy()
{
    // 연결 해제 패킷 전송
    if (networkManager.IsLoggedIn()) {
        networkManager.SendPlayerDisconnect();
    }
    
    // Ensure that the GPU is no longer referencing resources that are about to be
    // cleaned up by the destructor.
    WaitForPreviousFrame();
    CloseHandle(m_fenceEvent);
}

void Framework::OnLoginSuccess(int clientID, const std::string& username) {
    NetworkManager::LogToFile("[Framework] Login successful: " + username + " (ID: " + std::to_string(clientID) + ")");
    
    // 로그인 성공 시 게임 화면으로 전환
    m_isInLoginScreen = false;
    m_loginUI.SetState(UIState::CONNECTED);
    
    // 게임 씬으로 전환
    if (m_scenes.find(L"BaseScene") != m_scenes.end()) {
        m_currentSceneName = L"BaseScene";
        NetworkManager::LogToFile("[Framework] Switched to BaseScene successfully");
    } else {
        NetworkManager::LogToFile("[Framework] ERROR: BaseScene not found!");
    }
    
    NetworkManager::LogToFile("[Framework] Game screen transition completed");
}

void Framework::OnLoginFailed(const std::string& errorMessage) {
    NetworkManager::LogToFile("[Framework] Login failed: " + errorMessage);
    
    // 로그인 실패 시 에러 메시지 표시
    std::wstring wErrorMessage(errorMessage.begin(), errorMessage.end());
    m_loginUI.SetErrorMessage(wErrorMessage);
    m_loginUI.SetState(UIState::ERROR_STATE);
}

void Framework::OnConnectToServer(const std::wstring& serverIP, int port) {
    NetworkManager::LogToFile("[Framework] Attempting to connect to server: " + std::string(serverIP.begin(), serverIP.end()) + ":" + std::to_string(port));
    
    // 서버 연결 시도
    std::string ipStr(serverIP.begin(), serverIP.end());
    if (m_scenes.find(L"BaseScene") != m_scenes.end() && networkManager.Initialize(ipStr.c_str(), port, &m_scenes[L"BaseScene"])) {
        // 연결 성공 시 로그인 요청 전송
        m_loginUI.SetState(UIState::CONNECTING);
        
            // 잠시 후 로그인 요청 전송 (서버 연결 완료 대기)
    std::thread([this]() {
        Sleep(500); // 500ms로 증가 (서버 연결 완료 대기)
        NetworkManager::LogToFile("[Framework] Attempting to send login request after connection");
        
        if (networkManager.IsRunning()) {
            // 사용자명 가져오기 (LoginUI에서)
            wchar_t username[256] = {0};
            HWND editUsername = GetDlgItem(m_loginUI.GetHwnd(), 1001);
            if (editUsername) {
                GetWindowText(editUsername, username, 256);
            }
            
            // WideCharToMultiByte를 사용하여 wchar_t를 char로 변환
            int size_needed = WideCharToMultiByte(CP_UTF8, 0, username, -1, NULL, 0, NULL, NULL);
            std::string usernameStr(size_needed, 0);
            WideCharToMultiByte(CP_UTF8, 0, username, -1, &usernameStr[0], size_needed, NULL, NULL);
            
            NetworkManager::LogToFile("[Framework] Username: " + usernameStr);
            
            if (!usernameStr.empty()) {
                NetworkManager::LogToFile("[Framework] Sending login request");
                networkManager.SendLoginRequest(usernameStr);
            } else {
                NetworkManager::LogToFile("[Framework] Username is empty, showing error");
                m_loginUI.SetErrorMessage(L"Please enter a username");
                m_loginUI.SetState(UIState::ERROR_STATE);
            }
        } else {
            NetworkManager::LogToFile("[Framework] Network manager is not running");
            m_loginUI.SetErrorMessage(L"Network connection failed");
            m_loginUI.SetState(UIState::ERROR_STATE);
        }
    }).detach();
    } else {
        // 연결 실패
        m_loginUI.SetErrorMessage(L"Failed to connect to server");
        m_loginUI.SetState(UIState::ERROR_STATE);
    }
}

void Framework::OnKeyDown(UINT8 key)
{
    if (m_scenes.find(m_currentSceneName) != m_scenes.end()) {
        m_scenes[m_currentSceneName].OnKeyDown(key);
    }
}

void Framework::OnKeyUp(UINT8 key)
{
    if (m_scenes.find(m_currentSceneName) != m_scenes.end()) {
        m_scenes[m_currentSceneName].OnKeyUp(key);
    }
}

void Framework::OnNetworkUpdate()
{
    // 네트워크 업데이트 처리
    if (networkManager.IsRunning() && m_scenes.find(L"BaseScene") != m_scenes.end()) {
        networkManager.Update(m_Timer, &m_scenes[L"BaseScene"]);
    }
}

// Helper function for acquiring the first available hardware adapter that supports Direct3D 12.
// If no such adapter can be found, *ppAdapter will be set to nullptr.
void Framework::GetHardwareAdapter(IDXGIFactory1* pFactory, IDXGIAdapter1** ppAdapter, bool requestHighPerformanceAdapter)
{
    *ppAdapter = nullptr;

    ComPtr<IDXGIAdapter1> adapter;

    ComPtr<IDXGIFactory6> factory6;
    if (SUCCEEDED(pFactory->QueryInterface(IID_PPV_ARGS(&factory6))))
    {
        for (
            UINT adapterIndex = 0;
            SUCCEEDED(factory6->EnumAdapterByGpuPreference(
                adapterIndex,
                requestHighPerformanceAdapter ? DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE : DXGI_GPU_PREFERENCE_UNSPECIFIED,
                IID_PPV_ARGS(&adapter)));
                ++adapterIndex)
        {
            DXGI_ADAPTER_DESC1 desc;
            adapter->GetDesc1(&desc);

            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
            {
                // Don't select the Basic Render Driver adapter.
                // If you want a software adapter, pass in "/warp" on the command line.
                continue;
            }

            // Check to see whether the adapter supports Direct3D 12, but don't create the
            // actual device yet.
            if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr)))
            {
                break;
            }
        }
    }

    //if (adapter.Get() == nullptr)
    //{
    //    for (UINT adapterIndex = 0; SUCCEEDED(pFactory->EnumAdapters1(adapterIndex, &adapter)); ++adapterIndex)
    //    {
    //        DXGI_ADAPTER_DESC1 desc;
    //        adapter->GetDesc1(&desc);

    //        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
    //        {
    //            // Don't select the Basic Render Driver adapter.
    //            // If you want a software adapter, pass in "/warp" on the command line.
    //            continue;
    //        }

    //        // Check to see whether the adapter supports Direct3D 12, but don't create the
    //        // actual device yet.
    //        if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr)))
    //        {
    //            break;
    //        }
    //    }
    //}

    *ppAdapter = adapter.Detach();
}

void Framework::InitWnd(HINSTANCE hInstance)
{
    m_win32App->CreateWnd(this, hInstance);
}

void Framework::BuildFactoryAndDevice()
{
    UINT dxgiFactoryFlags = 0;

#if defined(_DEBUG)
    // Enable the debug layer (requires the Graphics Tools "optional feature").
    // NOTE: Enabling the debug layer after device creation will invalidate the active device.
    {
        ComPtr<ID3D12Debug> debugController;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
        {
            debugController->EnableDebugLayer();

            // Enable additional debug layers.
            dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
}
    }
#endif

    ThrowIfFailed(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&m_factory)));

    if (m_useWarpDevice)
    {
        ComPtr<IDXGIAdapter> warpAdapter;
        ThrowIfFailed(m_factory->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter)));

        ThrowIfFailed(D3D12CreateDevice(
            warpAdapter.Get(),
            D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS(&m_device)
        ));
    }
    else
    {
        ComPtr<IDXGIAdapter1> hardwareAdapter;
        GetHardwareAdapter(m_factory.Get(), hardwareAdapter.GetAddressOf());

        if (hardwareAdapter.Get() == nullptr) throw;

        ThrowIfFailed(D3D12CreateDevice(
            hardwareAdapter.Get(),
            D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS(&m_device)
        ));
    }
    //ThrowIfFailed(m_factory->MakeWindowAssociation(m_win32App->GetHwnd(), DXGI_MWA_NO_ALT_ENTER));
}

void Framework::BuildCommandQueueAndSwapChain()
{
    // Describe and create the command queue.
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

    ThrowIfFailed(m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue)));

    // Describe and create the swap chain.
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.BufferCount = FrameCount;
    swapChainDesc.Width = m_win32App->GetWidth();
    swapChainDesc.Height = m_win32App->GetHeight();
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.SampleDesc.Count = 1;

    ComPtr<IDXGISwapChain1> swapChain;
    ThrowIfFailed(m_factory->CreateSwapChainForHwnd(
        m_commandQueue.Get(),        // Swap chain needs the queue so that it can force a flush on it.
        m_win32App->GetHwnd(),
        &swapChainDesc,
        nullptr,
        nullptr,
        &swapChain
    ));
    ThrowIfFailed(swapChain.As(&m_swapChain));
    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

}

void Framework::BuildCommandListAndAllocator()
{
    // Create the command allocator.
    ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocator)));

    // Create the command list.
    ThrowIfFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocator.Get(), nullptr, IID_PPV_ARGS(&m_commandList)));

}

void Framework::BuildRtvDescriptorHeap()
{
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.NumDescriptors = FrameCount;
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    ThrowIfFailed(m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap)));
    m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
}

void Framework::BuildRtv()
{
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());
    for (UINT n = 0; n < FrameCount; n++)
    {
        ThrowIfFailed(m_swapChain->GetBuffer(n, IID_PPV_ARGS(&m_renderTargets[n])));
        m_device->CreateRenderTargetView(m_renderTargets[n].Get(), nullptr, rtvHandle);
        rtvHandle.Offset(1, m_rtvDescriptorSize);
    }
}

void Framework::BuildDsvDescriptorHeap()
{
    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;
    dsvHeapDesc.NumDescriptors = 1 + 1; // 1 은 shdowmap용
    dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    dsvHeapDesc.NodeMask = 0;
    ThrowIfFailed(m_device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(m_dsvHeap.GetAddressOf())));
    m_dsvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
}

void Framework::BuildDepthStencilBuffer(UINT width, UINT height)
{
    D3D12_RESOURCE_DESC depthStencilDesc;
    depthStencilDesc = CD3DX12_RESOURCE_DESC::Tex2D(
        DXGI_FORMAT_D24_UNORM_S8_UINT,
        width, height,
        1, 0, 1, 0,
        D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL
    );

    D3D12_CLEAR_VALUE depthOptimizedClearValue;
    depthOptimizedClearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthOptimizedClearValue.DepthStencil.Depth = 1.0f;
    depthOptimizedClearValue.DepthStencil.Stencil = 0;

    m_device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &depthStencilDesc,
        D3D12_RESOURCE_STATE_DEPTH_WRITE,
        &depthOptimizedClearValue,
        IID_PPV_ARGS(&m_depthStencilBuffer)
    );
}

void Framework::BuildDsv()
{
    m_device->CreateDepthStencilView(m_depthStencilBuffer.Get(), nullptr, m_dsvHeap->GetCPUDescriptorHandleForHeapStart());
}

void Framework::BuildFence()
{
    ThrowIfFailed(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
    m_fenceValue = 1;

    // Create an event handle to use for frame synchronization.
    m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (m_fenceEvent == nullptr)
    {
        ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
    }
}

void Framework::PopulateCommandList()
{
    ThrowIfFailed(m_commandAllocator->Reset());
    ThrowIfFailed(m_commandList->Reset(m_commandAllocator.Get(), nullptr));

    m_scenes[L"BaseScene"].OnRender(m_device.Get(), m_commandList.Get(), ePass::Shadow);

    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = m_renderTargets[m_frameIndex].Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    m_commandList->ResourceBarrier(1, &barrier);

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), m_frameIndex, m_rtvDescriptorSize);
    CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(m_dsvHeap->GetCPUDescriptorHandleForHeapStart(), 0, m_dsvDescriptorSize);
    
    m_commandList->ClearRenderTargetView(rtvHandle, Colors::LightSteelBlue, 0, nullptr);
    m_commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1, 0, 0, nullptr);

    m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);
    
    m_scenes[L"BaseScene"].OnRender(m_device.Get(), m_commandList.Get(), ePass::Default);
    
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    m_commandList->ResourceBarrier(1, &barrier);
    ThrowIfFailed(m_commandList->Close());
}

void Framework::BuildScenes(ID3D12Device* device, ID3D12GraphicsCommandList* commandList)
{
    wstring name = L"BaseScene";
    m_scenes[name] = Scene{ this, m_win32App->GetWidth(), m_win32App->GetHeight(), name };
    m_scenes[name].OnInit(device, commandList);
    m_currentSceneName = name;
}

void Framework::WaitForPreviousFrame()
{
    // Signal and increment the fence value.
    const UINT64 fence = m_fenceValue;
    ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), fence));
    m_fenceValue++;

    // Wait until the previous frame is finished.
    if (m_fence->GetCompletedValue() < fence)
    {
        ThrowIfFailed(m_fence->SetEventOnCompletion(fence, m_fenceEvent));
        WaitForSingleObject(m_fenceEvent, INFINITE);
    }
    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
}

void Framework::CalculateFrame()
{
    static int frameCnt = 0;
    static float timeElapsed = 0.0f;

    frameCnt++;

    // Compute averages over one second period.
    if ((m_Timer.TotalTime() - timeElapsed) >= 1.0f)
    {
        float fps = (float)frameCnt; // fps = frameCnt / 1
        wstring windowText = L" FPS " + to_wstring(fps);
        m_win32App->SetCustomWindowText(windowText.c_str());
        // Reset for next average.
        frameCnt = 0;
        timeElapsed += 1.0f;
    }


}

GameTimer& Framework::GetTimer()
{
    return m_Timer;
}

Scene& Framework::GetScene(const wstring& name)
{
    return m_scenes.at(name);
}

const wstring& Framework::GetCurrentSceneName()
{
    return m_currentSceneName;
}

Win32Application& Framework::GetWin32App()
{
    return *m_win32App.get();
}

ID3D12Device* Framework::GetDevice()
{
    return m_device.Get();
}

ID3D12GraphicsCommandList* Framework::GetCommandList()
{
    return m_commandList.Get();
}

ID3D12DescriptorHeap* Framework::GetDsvDescHeap()
{
    return m_dsvHeap.Get();
}