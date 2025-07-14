#include "stdafx.h"
#include "NetworkManager.h"
#include "OtherPlayerManager.h"
#include <iostream>
#include "ResourceManager.h"
#include <ctime>
#include <cstdio>
#include <mutex>

std::ofstream NetworkManager::m_logFile;
std::mutex NetworkManager::m_logMutex;

NetworkManager::NetworkManager() : sock(INVALID_SOCKET), m_networkThread(NULL), m_isRunning(false), m_myClientID(0) {
    
    // 에러 정보 초기화
    m_errorCount = 0;
    m_lastErrorTime = 0;
    m_shouldReconnect = false;
    m_reconnectAttempts = 0;
    m_lastReconnectTime = 0;
    
    try {
        CreateDirectory(L"logs", NULL);
        
        time_t now = time(0);
        tm ltm;
        localtime_s(&ltm, &now);
        char filename[100];
        sprintf_s(filename, "logs/network_log_%d%02d%02d_%02d%02d%02d.txt",
            ltm.tm_year + 1900, ltm.tm_mon + 1, ltm.tm_mday,
            ltm.tm_hour, ltm.tm_min, ltm.tm_sec);
        
        m_logFile.open(filename);
        if (m_logFile.is_open()) {
            LogToFile("NetworkManager initialized");
        }
        else {
            std::cerr << "Failed to open log file: " << filename << std::endl;
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Failed to initialize NetworkManager: " << e.what() << std::endl;
    }
    catch (...) {
        std::cerr << "Unknown error in NetworkManager initialization" << std::endl;
    }
    
}

NetworkManager::~NetworkManager() {
    Shutdown();
}

bool NetworkManager::Initialize(const char* serverIP, int port, Scene* scene) {
    if (!scene) {
        LogToFile("[Error] Scene is null");
        return false;
    }
    m_scene = scene;
    
    try {
        auto& player = m_scene->GetObj<PlayerObject>(L"PlayerObject");
        LogToFile("[Info] Found PlayerObject in scene");
    }
    catch (const std::exception& e) {
        LogToFile("[Error] Failed to find PlayerObject: " + std::string(e.what()));
        return false;
    }
    
    // OtherPlayerManager 초기화
    OtherPlayerManager::GetInstance()->SetScene(scene);
    OtherPlayerManager::GetInstance()->SetNetworkManager(this);
    LogToFile("[Info] OtherPlayerManager initialized with scene and network manager");
    
    std::string logMsg = "[Client] Connecting to server " + std::string(serverIP) + ":" + std::to_string(port);
    LogToFile(logMsg);
    
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        LogToFile("[Error] WSAStartup failed");
        return false;
    }

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        LogToFile("[Error] Socket creation failed");
        return false;
    }

    SOCKADDR_IN serverAddr = { 0 };
    serverAddr.sin_family = AF_INET;
    inet_pton(AF_INET, serverIP, &serverAddr.sin_addr);
    serverAddr.sin_port = htons(port);

    if (connect(sock, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        LogToFile("[Error] Connection failed");
        return false;
    }

    // recv 버퍼 초기화
    memset(m_recvBuffer, 0, sizeof(m_recvBuffer));
    memset(m_packetBuffer, 0, sizeof(m_packetBuffer));
    m_packetBufferSize = 0;

    m_isRunning = true;
    m_networkThread = CreateThread(NULL, 0, NetworkThread, this, 0, NULL);
    if (m_networkThread == NULL) {
        std::cout << "[Error] Failed to create network thread" << std::endl;
        return false;
    }

    LogToFile("[Client] Successfully connected to server");

    return true;
}

void NetworkManager::SendLoginRequest(const std::string& username) {
    if (!m_isRunning) return;

    m_username = username;
    
    try {
        PacketLoginRequest pkt;
        pkt.header.size = sizeof(PacketLoginRequest);
        pkt.header.type = PACKET_LOGIN_REQUEST;
        strncpy_s(pkt.username, username.c_str(), sizeof(pkt.username) - 1);

        int sendResult = send(sock, (char*)&pkt, sizeof(pkt), 0);
        if (sendResult == SOCKET_ERROR) {
            int error = WSAGetLastError();
            HandleError("Login request failed: " + std::to_string(error));
        } else {
            LogToFile("[Login] Sent login request for user: " + username);
        }
    }
    catch (const std::exception& e) {
        HandleError("Exception in SendLoginRequest: " + std::string(e.what()));
    }
}

void NetworkManager::SendPlayerDisconnect() {
    if (!m_isRunning || !m_isLoggedIn) return;

    try {
        PacketPlayerDisconnect pkt;
        pkt.header.size = sizeof(PacketPlayerDisconnect);
        pkt.header.type = PACKET_PLAYER_DISCONNECT;
        pkt.playerID = m_myClientID;
        strncpy_s(pkt.username, m_username.c_str(), sizeof(pkt.username) - 1);

        send(sock, (char*)&pkt, sizeof(pkt), 0);
        LogToFile("[Disconnect] Sent disconnect packet for user: " + m_username);
    }
    catch (const std::exception& e) {
        LogToFile("[Error] Failed to send disconnect packet: " + std::string(e.what()));
    }
}

void NetworkManager::SetLoginSuccessCallback(std::function<void(int, const std::string&)> callback) {
    m_loginSuccessCallback = callback;
}

void NetworkManager::SetLoginFailedCallback(std::function<void(const std::string&)> callback) {
    m_loginFailedCallback = callback;
}

DWORD WINAPI NetworkManager::NetworkThread(LPVOID arg) {
    NetworkManager* network = (NetworkManager*)arg;
    network->LogToFile("[Thread] Network thread started");
    
    int errorCount = 0;
    const int MAX_ERRORS = 10;  // 최대 연속 에러 허용 횟수 증가 (더 관대하게)
    const int ERROR_RESET_TIME = 10000;  // 에러 카운트 리셋 시간 (ms) - 더 길게
    DWORD lastErrorTime = GetTickCount();
    
    while (network->m_isRunning) {
        try {
            fd_set readSet;
            FD_ZERO(&readSet);
            FD_SET(network->sock, &readSet);
            
            timeval timeout;
            timeout.tv_sec = 0;
            timeout.tv_usec = 50000;  // 50ms (더 짧은 타임아웃)
            
            int selectResult = select(0, &readSet, nullptr, nullptr, &timeout);
            if (selectResult == SOCKET_ERROR) {
                int error = WSAGetLastError();
                if (error == WSAEWOULDBLOCK) {
                    continue;  // 일시적 에러, 무시
                }
                network->LogToFile("[Error] Select failed with error: " + std::to_string(error));
                            if (++errorCount >= MAX_ERRORS) {
                network->LogToFile("[Warning] Many select errors, but continuing...");
                // 에러가 많아도 연결 유지
                continue;
            }
                continue;
            }
            
            if (selectResult == 0) continue;  // 타임아웃

            int recvBytes = recv(network->sock, network->m_recvBuffer, sizeof(network->m_recvBuffer), 0);
            if (recvBytes <= 0) {
                int error = WSAGetLastError();
                if (error == WSAEWOULDBLOCK) {
                    continue;  // 일시적 에러, 무시
                }
                if (error == WSAECONNRESET || error == WSAECONNABORTED || error == WSAENOTSOCK) {
                    network->LogToFile("[Error] Connection issue: " + std::to_string(error) + ", but continuing...");
                    // 연결 문제가 있어도 계속 시도
                    continue;
                }
                network->LogToFile("[Error] Receive failed: " + std::to_string(error));
                if (++errorCount >= MAX_ERRORS) {
                    network->LogToFile("[Warning] Many receive errors, but continuing...");
                    // 에러가 많아도 연결 유지
                    continue;
                }
                continue;
            }

            if (recvBytes > sizeof(network->m_recvBuffer)) {
                network->LogToFile("[Error] Receive buffer overflow: " + std::to_string(recvBytes) + " bytes");
                return 1;
            }

            // 수신된 데이터를 패킷 버퍼에 추가
            if (network->m_packetBufferSize + recvBytes > sizeof(network->m_packetBuffer)) {
                network->LogToFile("[Error] Packet buffer overflow, clearing buffer");
                network->m_packetBufferSize = 0;
                memset(network->m_packetBuffer, 0, sizeof(network->m_packetBuffer));
                continue;
            }

            memcpy(network->m_packetBuffer + network->m_packetBufferSize, network->m_recvBuffer, recvBytes);
            network->m_packetBufferSize += recvBytes;

            // 패킷 버퍼에서 완전한 패킷들을 처리
            int processedBytes = 0;
            while (network->m_packetBufferSize - processedBytes >= sizeof(PacketHeader)) {
                PacketHeader* header = (PacketHeader*)(network->m_packetBuffer + processedBytes);
                
                // 패킷 크기 검증
                if (header->size < sizeof(PacketHeader) || header->size > sizeof(network->m_packetBuffer)) {
                    network->LogToFile("[Error] Invalid packet size: " + std::to_string(header->size));
                    network->m_packetBufferSize = 0;
                    memset(network->m_packetBuffer, 0, sizeof(network->m_packetBuffer));
                    if (++errorCount >= MAX_ERRORS) {
                        network->LogToFile("[Warning] Many invalid packets, but continuing...");
                        // 에러가 많아도 연결 유지
                        continue;
                    }
                    break;
                }

                // 완전한 패킷이 있는지 확인
                if (network->m_packetBufferSize - processedBytes < header->size) {
                    break;  // 완전한 패킷이 없음
                }

                // 패킷 처리 (안전한 접근)
                try {
                    network->ProcessPacket(network->m_packetBuffer + processedBytes);
                } catch (const std::exception& e) {
                    // 패킷 처리 중 예외 발생 시 무시하고 계속 진행
                    network->LogToFile("[Error] Exception during packet processing: " + std::string(e.what()));
                } catch (...) {
                    // 패킷 처리 중 예외 발생 시 무시하고 계속 진행
                    network->LogToFile("[Error] Unknown exception during packet processing");
                }
                processedBytes += header->size;
            }

            // 처리된 데이터를 버퍼에서 제거
            if (processedBytes > 0) {
                if (processedBytes < network->m_packetBufferSize) {
                    memmove(network->m_packetBuffer, network->m_packetBuffer + processedBytes, 
                           network->m_packetBufferSize - processedBytes);
                }
                network->m_packetBufferSize -= processedBytes;
            }

            // 성공적인 패킷 수신 시 에러 카운트 리셋
            errorCount = 0;
            lastErrorTime = GetTickCount();
        }
        catch (const std::exception& e) {
            network->LogToFile("[Error] Exception in network thread: " + std::string(e.what()));
            if (++errorCount >= MAX_ERRORS) {
                network->LogToFile("[Warning] Many exceptions, but continuing...");
                // 에러가 많아도 연결 유지
                continue;
            }
            Sleep(1000);
        }
    }
    
    network->LogToFile("[Thread] Network thread ended normally");
    return 0;
}

void NetworkManager::HandleError(const std::string& description) {
    DWORD currentTime = GetTickCount();
    
    m_errorCount++;
    m_lastErrorTime = currentTime;
    
    std::string errorMsg = "[Error] Count: " + std::to_string(m_errorCount) + 
                          ", Description: " + description;
    LogToFile(errorMsg);
    
    // 5번 연속 에러 시 재연결 시도
    if (m_errorCount >= 5) {
        LogToFile("[Warning] Too many errors, considering reconnection");
        m_shouldReconnect = true;
    }
}

bool NetworkManager::ShouldReconnect() const {
    return m_shouldReconnect && m_reconnectAttempts < 5;
}

bool NetworkManager::AttemptReconnect() {
    DWORD currentTime = GetTickCount();
    
    if (currentTime - m_lastReconnectTime < 5000) {
        return false;
    }
    
    m_lastReconnectTime = currentTime;
    m_reconnectAttempts++;
    
    LogToFile("[Reconnect] Attempting reconnection #" + std::to_string(m_reconnectAttempts));
    
    if (sock != INVALID_SOCKET) {
        closesocket(sock);
        sock = INVALID_SOCKET;
    }
    
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        LogToFile("[Error] WSAStartup failed during reconnection");
        return false;
    }

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        LogToFile("[Error] Socket creation failed during reconnection");
        return false;
    }

    SOCKADDR_IN serverAddr = { 0 };
    serverAddr.sin_family = AF_INET;
    inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr);
    serverAddr.sin_port = htons(5000);

    if (connect(sock, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        LogToFile("[Error] Reconnection failed");
        return false;
    }

    LogToFile("[Reconnect] Successfully reconnected to server");
    
    m_shouldReconnect = false;
    m_reconnectAttempts = 0;
    ResetErrorInfo();
    
    return true;
}

void NetworkManager::ResetErrorInfo() {
    m_errorCount = 0;
    m_packetBufferSize = 0;
    memset(m_packetBuffer, 0, sizeof(m_packetBuffer));
    LogToFile("[Info] Error info and packet buffer reset");
}

void NetworkManager::SendPlayerUpdate(float x, float y, float z, float rotY) {
    if (!m_isRunning || !m_isLoggedIn) return;

    try {
        PacketPlayerUpdate pkt;
        pkt.header.size = sizeof(PacketPlayerUpdate);
        pkt.header.type = PACKET_PLAYER_UPDATE;
        pkt.clientID = m_myClientID;
        pkt.x = x;
        pkt.y = y;
        pkt.z = z;
        pkt.rotY = rotY;

        int sendResult = send(sock, (char*)&pkt, sizeof(pkt), 0);
        if (sendResult == SOCKET_ERROR) {
            int error = WSAGetLastError();
            if (error == WSAECONNRESET || error == WSAECONNABORTED) {
                HandleError("Connection lost during send: " + std::to_string(error));
            } else if (error == WSAEWOULDBLOCK) {
                // 일시적 에러, 무시
            } else {
                HandleError("Send failed: " + std::to_string(error));
            }
        }
    }
    catch (const std::exception& e) {
        HandleError("Exception in SendPlayerUpdate: " + std::string(e.what()));
    }
}

void NetworkManager::ProcessPacket(char* buffer) {
    PacketHeader* header = (PacketHeader*)buffer;
    // 패킷 처리 시작 로그 제거 (로그 출력 최소화)

    // 메모리 사용량 모니터링 (최적화)
    static int packetCount = 0;
    packetCount++;
    if (packetCount % 50 == 0) {  // 50개 패킷마다 메모리 상태 체크 (빈도 감소)
        try {
            MEMORYSTATUSEX memInfo;
            memInfo.dwLength = sizeof(MEMORYSTATUSEX);
            if (GlobalMemoryStatusEx(&memInfo)) {
                DWORDLONG usedMemory = memInfo.ullTotalPhys - memInfo.ullAvailPhys;
                DWORDLONG totalMemory = memInfo.ullTotalPhys;
                if (totalMemory > 0) {
                    double memoryUsagePercent = (double)usedMemory / totalMemory * 100.0;
                    
                    // 85% 초과 시에만 경고
                    if (memoryUsagePercent > 85.0) {
                        char memBuffer[256];
                        sprintf_s(memBuffer, "[Memory] High usage: %.1f%% (%llu MB)", 
                            memoryUsagePercent, usedMemory / (1024*1024));
                        LogToFile(memBuffer);
                    }
                }
            }
        } catch (...) {
            // 메모리 상태 확인 실패 시 무시
        }
    }

    try {
        switch (header->type) {
            case PACKET_LOGIN_RESPONSE: {
                PacketLoginResponse* loginRespPkt = (PacketLoginResponse*)buffer;
                LogToFile("[Login] Received login response - Success: " + std::to_string(loginRespPkt->success));
                
                if (loginRespPkt->success) {
                    m_myClientID = loginRespPkt->clientID;
                    m_isLoggedIn = true;
                    LogToFile("[Login] Login successful - Client ID: " + std::to_string(m_myClientID));
                    
                    // 로그인 성공 후 준비 완료 신호 전송
                    PacketClientReady readyPacket;
                    readyPacket.header.type = PACKET_CLIENT_READY;
                    readyPacket.header.size = sizeof(PacketClientReady);
                    readyPacket.clientID = m_myClientID;
                    
                    int sendResult = send(sock, (char*)&readyPacket, sizeof(readyPacket), 0);
                    if (sendResult == SOCKET_ERROR) {
                        int error = WSAGetLastError();
                        LogToFile("[Error] Failed to send ready packet: " + std::to_string(error));
                    } else {
                        LogToFile("[Login] Sent client ready packet");
                    }
                    
                    if (m_loginSuccessCallback) {
                        m_loginSuccessCallback(m_myClientID, m_username);
                    }
                } else {
                    m_isLoggedIn = false;
                    std::string errorMsg = loginRespPkt->message;
                    LogToFile("[Login] Login failed: " + errorMsg);
                    
                    if (m_loginFailedCallback) {
                        m_loginFailedCallback(errorMsg);
                    }
                }
                break;
            }
            
            case PACKET_PLAYER_SPAWN: {
                PacketPlayerSpawn* spawnPkt = (PacketPlayerSpawn*)buffer;
                LogToFile("[Spawn] Processing spawn packet for ID: " + std::to_string(spawnPkt->playerID));

                // 로그인 상태 확인
                if (!m_isLoggedIn) {
                    LogToFile("[Spawn] Ignoring player spawn packet - not logged in yet");
                    break;
                }

                if (m_myClientID == 0) {
                    m_myClientID = spawnPkt->playerID;
                    LogToFile("[Spawn] Set my client ID to: " + std::to_string(m_myClientID));
                }
                else if (spawnPkt->playerID != m_myClientID) {
                    try {
                        OtherPlayerManager::GetInstance()->SpawnOtherPlayer(spawnPkt->playerID);
                        LogToFile("[Spawn] Successfully spawned other player: " + std::to_string(spawnPkt->playerID) + " (" + spawnPkt->username + ")");
                    }
                    catch (const std::exception& e) {
                        LogToFile("[Error] Failed to spawn other player: " + std::string(e.what()));
                    }
                }
                break;
            }
            
            case PACKET_PLAYER_DISCONNECT: {
                PacketPlayerDisconnect* disconnectPkt = (PacketPlayerDisconnect*)buffer;
                LogToFile("[Disconnect] Player disconnected: " + std::to_string(disconnectPkt->playerID) + " (" + disconnectPkt->username + ")");
                
                if (disconnectPkt->playerID != m_myClientID) {
                    try {
                        OtherPlayerManager::GetInstance()->RemoveOtherPlayer(disconnectPkt->playerID);
                        LogToFile("[Disconnect] Removed other player: " + std::to_string(disconnectPkt->playerID));
                    }
                    catch (const std::exception& e) {
                        LogToFile("[Error] Failed to remove other player: " + std::string(e.what()));
                    }
                }
                break;
            }

            case PACKET_PLAYER_UPDATE: {
                PacketPlayerUpdate* updatePkt = (PacketPlayerUpdate*)buffer;
                
                // 로그인 상태 확인
                if (!m_isLoggedIn) {
                    LogToFile("[Update] Ignoring player update packet - not logged in yet");
                    break;
                }
                
                if (updatePkt->clientID == m_myClientID) {
                    LogToFile("[Update] Ignoring own update packet");
                    return;
                }

                OtherPlayerManager::GetInstance()->UpdateOtherPlayer(
                    updatePkt->clientID, updatePkt->x, updatePkt->y, updatePkt->z, updatePkt->rotY);
                LogToFile("[Update] Successfully updated player: " + std::to_string(updatePkt->clientID));
                break;
            }

            case PACKET_TIGER_SPAWN: {
                PacketTigerSpawn* tigerSpawnPkt = (PacketTigerSpawn*)buffer;
                LogToFile("[Tiger] Received spawn packet for tiger ID: " + std::to_string(tigerSpawnPkt->tigerID));
                
                // 로그인 상태 확인 - 로그인 전에 받은 호랑이 스폰 패킷은 무시
                if (!m_isLoggedIn) {
                    LogToFile("[Tiger] Ignoring tiger spawn packet - not logged in yet");
                    break;
                }
                
                // 이미 스폰된 호랑이인지 확인
                if (m_tigers.find(tigerSpawnPkt->tigerID) != m_tigers.end()) {
                    LogToFile("[Tiger] Tiger ID " + std::to_string(tigerSpawnPkt->tigerID) + " already spawned, ignoring duplicate spawn");
                    break;
                }
                
                // Tiger 정보 저장
                TigerInfo tigerInfo;
                tigerInfo.tigerID = tigerSpawnPkt->tigerID;
                tigerInfo.x = tigerSpawnPkt->x;
                tigerInfo.y = tigerSpawnPkt->y;
                tigerInfo.z = tigerSpawnPkt->z;
                tigerInfo.rotY = 0.0f;
                m_tigers[tigerSpawnPkt->tigerID] = tigerInfo;
                
                LogToFile("[Tiger] Successfully stored tiger info for ID: " + std::to_string(tigerSpawnPkt->tigerID));
                
                // Scene에 Tiger 생성 요청
                if (m_scene) {
                    char logBuffer[256];
                    sprintf_s(logBuffer, "[Tiger] Attempting to create tiger in scene - ID: %d, Position: (%.2f, %.2f, %.2f)", 
                        tigerSpawnPkt->tigerID, tigerSpawnPkt->x, tigerSpawnPkt->y, tigerSpawnPkt->z);
                    LogToFile(logBuffer);
                    
                    if (m_scene->GetDevice() == nullptr) {
                        LogToFile("[Error] Device is null before CreateTigerObject");
                        break;
                    }
                    
                    try {
                        m_scene->CreateTigerObject(tigerSpawnPkt->tigerID, tigerSpawnPkt->x, tigerSpawnPkt->y, tigerSpawnPkt->z, m_scene->GetDevice());
                        LogToFile("[Tiger] CreateTigerObject call completed successfully");
                    }
                    catch (const std::exception& e) {
                        LogToFile("[Error] Exception in CreateTigerObject: " + std::string(e.what()));
                        // 예외가 발생해도 클라이언트는 계속 실행
                    }
                    catch (...) {
                        LogToFile("[Error] Unknown exception in CreateTigerObject");
                        // 예외가 발생해도 클라이언트는 계속 실행
                    }
                } else {
                    LogToFile("[Error] Scene is null, cannot create tiger object");
                }
                break;
            }
            
            case PACKET_TIGER_UPDATE: {
                PacketTigerUpdate* tigerUpdatePkt = (PacketTigerUpdate*)buffer;
                LogToFile("[Tiger] Received update for tiger ID: " + std::to_string(tigerUpdatePkt->tigerID));
                
                // 로그인 상태 확인
                if (!m_isLoggedIn) {
                    LogToFile("[Tiger] Ignoring tiger update packet - not logged in yet");
                    break;
                }
                
                if (m_tigers.find(tigerUpdatePkt->tigerID) != m_tigers.end()) {
                    // Tiger 정보 업데이트
                    m_tigers[tigerUpdatePkt->tigerID].x = tigerUpdatePkt->x;
                    m_tigers[tigerUpdatePkt->tigerID].y = tigerUpdatePkt->y;
                    m_tigers[tigerUpdatePkt->tigerID].z = tigerUpdatePkt->z;
                    m_tigers[tigerUpdatePkt->tigerID].rotY = tigerUpdatePkt->rotY;
                    
                    // Scene의 Tiger 오브젝트 업데이트
                    if (m_scene) {
                        try {
                            m_scene->UpdateTigerObject(tigerUpdatePkt->tigerID, tigerUpdatePkt->x, tigerUpdatePkt->y, tigerUpdatePkt->z, tigerUpdatePkt->rotY);
                            LogToFile("[Tiger] Updated tiger object with ID: " + std::to_string(tigerUpdatePkt->tigerID));
                        }
                        catch (const std::exception& e) {
                            LogToFile("[Error] Exception in UpdateTigerObject: " + std::string(e.what()));
                            // 예외가 발생해도 클라이언트는 계속 실행
                        }
                        catch (...) {
                            LogToFile("[Error] Unknown exception in UpdateTigerObject");
                            // 예외가 발생해도 클라이언트는 계속 실행
                        }
                    }
                } else {
                    LogToFile("[Tiger] Received update for non-spawned tiger ID: " + std::to_string(tigerUpdatePkt->tigerID) + ", ignoring");
                }
                break;
            }

            default:
                LogToFile("[Warning] Unknown packet type: " + std::to_string(header->type));
                break;
        }
    }
    catch (const std::exception& e) {
        LogToFile("[Error] Failed to process packet: " + std::string(e.what()));
        // 예외를 상위로 전파하지 않고 여기서 처리
    }

    LogToFile("[ProcessPacket] Finished processing packet type: " + std::to_string(header->type));
}

void NetworkManager::Shutdown() {
    m_isRunning = false;
    if (m_networkThread) {
        WaitForSingleObject(m_networkThread, INFINITE);
        CloseHandle(m_networkThread);
        m_networkThread = NULL;
    }
    if (sock != INVALID_SOCKET) {
        closesocket(sock);
        sock = INVALID_SOCKET;
    }
    WSACleanup();
}

void NetworkManager::LogToFile(const std::string& message) {
    // 로그 기록 완전 비활성화 (클라이언트 안정성 향상)
    return;
}

void NetworkManager::Update(GameTimer& gTimer, Scene* scene) {
    if (!m_scene) return;

    // 재연결 시도가 필요한 경우
    if (ShouldReconnect()) {
        if (AttemptReconnect()) {
            LogToFile("[Update] Successfully reconnected");
        } else {
            LogToFile("[Update] Reconnection attempt failed");
        }
        return;
    }

    if (!m_isRunning) return;

    auto& player = scene->GetObj<PlayerObject>(L"PlayerObject");
    auto& position = player.GetComponent<Position>();
    auto& rotation = player.GetComponent<Rotation>();
    
    m_updateTimer += gTimer.DeltaTime();
    const float UPDATE_INTERVAL = 0.5f;  // 500ms마다 업데이트 (네트워크 부하 감소)

    if (m_updateTimer >= UPDATE_INTERVAL) {
        SendPlayerUpdate(
            position.mFloat4.x, 
            position.mFloat4.y, 
            position.mFloat4.z,
            rotation.mFloat4.y
        );
        m_updateTimer = 0.0f;
    }
}

