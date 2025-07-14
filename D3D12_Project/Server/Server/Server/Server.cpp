#include "Server.h"
#include <iostream>
#include <format>
#include <random>
#include <algorithm>
#define NOMINMAX
#include <windows.h>

GameServer::GameServer()
    : m_nextClientID(1)
    , m_nextTigerID(1)
    , m_isRunning(false)
    , m_hIOCP(NULL)
    , m_listenSocket(INVALID_SOCKET)
    , m_port(5000)
    , m_tigerUpdateTimer(0.0f)
    , m_randomEngine(std::random_device{}())
{
}

GameServer::~GameServer() {
    Cleanup();
}

bool GameServer::Initialize(int port) {
    m_port = port;
    std::cout << "[Server] Starting server on port " << m_port << std::endl;

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cout << "[Error] WSAStartup failed" << std::endl;
        return false;
    }

    m_listenSocket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
    if (m_listenSocket == INVALID_SOCKET) {
        std::cout << "[Error] Failed to create listen socket" << std::endl;
        return false;
    }

    SOCKADDR_IN serverAddr = { 0 };
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serverAddr.sin_port = htons(m_port);

    if (bind(m_listenSocket, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cout << "[Error] Bind failed" << std::endl;
        return false;
    }

    if (listen(m_listenSocket, SOMAXCONN) == SOCKET_ERROR) {
        std::cout << "[Error] Listen failed" << std::endl;
        return false;
    }

    m_hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
    if (m_hIOCP == NULL) {
        std::cout << "[Error] CreateIoCompletionPort failed" << std::endl;
        return false;
    }

    InitializeTigers();
    return true;
}

void GameServer::Start() {
    m_isRunning = true;
    
    // Create worker threads
    for (int i = 0; i < 2; ++i) {
        HANDLE hThread = CreateThread(NULL, 0, WorkerThreadProc, this, 0, NULL);
        if (hThread) {
            m_workerThreads.push_back(hThread);
        }
    }

    // Main accept loop
    while (m_isRunning) {
        SOCKET clientSocket = accept(m_listenSocket, NULL, NULL);
        if (clientSocket != INVALID_SOCKET) {
            ProcessNewClient(clientSocket);
        }
    }
}

void GameServer::Stop() {
    m_isRunning = false;
    Cleanup();
}

DWORD WINAPI GameServer::WorkerThreadProc(LPVOID lpParam) {
    GameServer* server = static_cast<GameServer*>(lpParam);
    return server->WorkerThread();
}

DWORD GameServer::WorkerThread() {
    DWORD lastTime = GetTickCount();
    
    while (m_isRunning) {
        DWORD currentTime = GetTickCount();
        float deltaTime = (currentTime - lastTime) / 1000.0f;
        lastTime = currentTime;
        
        // 호랑이 업데이트
        UpdateTigers(deltaTime);
        
        // 기존 IOCP 처리 코드
        DWORD bytesTransferred;
        ULONG_PTR completionKey;
        OVERLAPPED* pOverlapped;
        
        BOOL result = GetQueuedCompletionStatus(m_hIOCP, &bytesTransferred, 
            &completionKey, &pOverlapped, 100); // 타임아웃을 100ms로 설정
        
        if (!m_isRunning) break;
        
        // 타임아웃 발생 시 (pOverlapped가 NULL)
        if (!pOverlapped) {
            continue;  // 타임아웃은 정상적인 상황이므로 계속 진행
        }
        
        IOContext* ioContext = CONTAINING_RECORD(pOverlapped, IOContext, overlapped);
        int clientID = static_cast<int>(completionKey);
        
        if (m_clients.find(clientID) == m_clients.end()) {
            delete ioContext;
            continue;
        }
        
        // 연결 해제 감지: result가 FALSE이고 bytesTransferred가 0인 경우만
        if (!result && bytesTransferred == 0) {
            int error = WSAGetLastError();
            
            // 연결 관련 에러인 경우에도 연결을 유지 (최대한 관대하게)
            if (error == WSAENOTSOCK || error == WSAECONNRESET || error == WSAECONNABORTED || error == WSAENETDOWN) {
                std::cout << "[Warning] Connection issue detected for client " << clientID << " (Error: " << error << "), but keeping connection..." << std::endl;
                
                // 클라이언트가 존재하는지 확인
                auto clientIt = m_clients.find(clientID);
                if (clientIt != m_clients.end()) {
                    // 연결 에러 카운트만 증가하고 연결은 유지
                    clientIt->second.connectionErrorCount++;
                    std::cout << "[Info] Client " << clientID << " connection error count: " << clientIt->second.connectionErrorCount << std::endl;
                } else {
                    std::cout << "  -> Client " << clientID << " not found in client list" << std::endl;
                }
            } else {
                // 일시적인 에러는 무시하고 계속 시도
                std::cout << "[Warning] GetQueuedCompletionStatus failed with temporary error: " << error << " for client " << clientID << std::endl;
            }
            
            delete ioContext;
            continue;
        }
        
        // 정상적인 데이터 수신이지만 bytesTransferred가 0인 경우는 무시
        if (bytesTransferred == 0) {
            delete ioContext;
            continue;
        }

        HandlePacket(ioContext, clientID, bytesTransferred);

        // 다음 수신 준비 - 클라이언트 존재 여부 확인
        auto clientIt = m_clients.find(clientID);
        if (clientIt == m_clients.end()) {
            std::cout << "[Error] Client " << clientID << " not found for next receive" << std::endl;
            delete ioContext;
            continue;
        }
        
        if (clientIt->second.socket == INVALID_SOCKET) {
            std::cout << "[Error] Invalid socket for client " << clientID << " for next receive" << std::endl;
            delete ioContext;
            continue;
        }
        
        // 다음 수신 준비
        if (!StartReceive(clientIt->second.socket, clientID, ioContext)) {
            std::cout << "[Error] Failed to start next receive for client " << clientID << std::endl;
            delete ioContext;
            continue;
        }
    }
    return 0;
}

void GameServer::HandlePacket(IOContext* ioContext, int clientID, DWORD bytesTransferred) {
    // 클라이언트 정보 가져오기
    auto clientIt = m_clients.find(clientID);
    if (clientIt == m_clients.end()) {
        std::cout << "[Error] Client " << clientID << " not found in HandlePacket" << std::endl;
        return;
    }
    
    ClientInfo& client = clientIt->second;
    
    // 수신된 데이터를 패킷 버퍼에 추가
    if (client.packetBufferSize + bytesTransferred > sizeof(client.packetBuffer)) {
        std::cout << "[Error] Packet buffer overflow for client " << clientID << std::endl;
        client.packetBufferSize = 0;  // 버퍼 초기화
        return;
    }
    
    memcpy(client.packetBuffer + client.packetBufferSize, ioContext->buffer, bytesTransferred);
    client.packetBufferSize += bytesTransferred;
    
    // 패킷 버퍼에서 완전한 패킷들을 처리
    int processedBytes = 0;
    while (client.packetBufferSize - processedBytes >= sizeof(PacketHeader)) {
        PacketHeader* header = (PacketHeader*)(client.packetBuffer + processedBytes);
        
        // 패킷 헤더 유효성 검사
        if (header->size < sizeof(PacketHeader) || header->size > MAX_PACKET_SIZE || 
            header->type <= 0 || header->type > 10) {
            std::cout << "[Error] Invalid packet header - Size: " << header->size 
                      << ", Type: " << header->type << ", Client: " << clientID << std::endl;
            
            // 잘못된 패킷의 첫 몇 바이트를 출력하여 디버깅
            std::cout << "[Debug] First 16 bytes: ";
            for (int i = 0; i < std::min(16, static_cast<int>(client.packetBufferSize - processedBytes)); ++i) {
                printf("%02X ", static_cast<unsigned char>(client.packetBuffer[processedBytes + i]));
            }
            std::cout << std::endl;
            
            // 버퍼 초기화
            client.packetBufferSize = 0;
            return;
        }
        
        // 완전한 패킷이 있는지 확인
        if (client.packetBufferSize - processedBytes < header->size) {
            break;  // 완전한 패킷이 없음
        }
        
        // 패킷 처리
        ProcessSinglePacket(client.packetBuffer + processedBytes, clientID, header->size);
        processedBytes += header->size;
        
        // 성공적인 패킷 수신 시 연결 에러 카운트 리셋
        client.connectionErrorCount = 0;
    }
    
    // 처리된 데이터를 버퍼에서 제거
    if (processedBytes > 0) {
        if (processedBytes < client.packetBufferSize) {
            memmove(client.packetBuffer, client.packetBuffer + processedBytes, 
                   client.packetBufferSize - processedBytes);
        }
        client.packetBufferSize -= processedBytes;
    }
}

void GameServer::ProcessSinglePacket(char* buffer, int clientID, int packetSize) {
    PacketHeader* header = (PacketHeader*)buffer;
    
    std::cout << "\n[Receive] From client " << clientID << std::endl;
    std::cout << "  -> Packet type: " << header->type << std::endl;
    std::cout << "  -> Packet size: " << header->size << std::endl;

    switch (header->type) {
        case PACKET_LOGIN_REQUEST: {
            if (header->size != sizeof(PacketLoginRequest)) {
                std::cout << "[Error] Invalid LOGIN_REQUEST packet size" << std::endl;
                break;
            }
            PacketLoginRequest* pkt = (PacketLoginRequest*)buffer;
            
            // 사용자명 중복 체크
            bool usernameExists = false;
            for (const auto& [id, client] : m_clients) {
                if (client.isLoggedIn && client.username == pkt->username) {
                    usernameExists = true;
                    break;
                }
            }
            
            PacketLoginResponse response;
            response.header.type = PACKET_LOGIN_RESPONSE;
            response.header.size = sizeof(PacketLoginResponse);
            response.clientID = clientID;
            
            if (usernameExists) {
                response.success = false;
                strncpy_s(response.message, "Username already exists", sizeof(response.message) - 1);
                std::cout << "[Login] Failed for client " << clientID << " - Username already exists: " << pkt->username << std::endl;
            } else {
                response.success = true;
                strncpy_s(response.message, "Login successful", sizeof(response.message) - 1);
                
                // 클라이언트 정보 업데이트
                m_clients[clientID].username = pkt->username;
                m_clients[clientID].isLoggedIn = true;
                
                std::cout << "[Login] Success for client " << clientID << " - Username: " << pkt->username << std::endl;
                
                // 로그인 성공 후 플레이어 스폰 패킷 전송
                PacketPlayerSpawn spawnPacket;
                spawnPacket.header.type = PACKET_PLAYER_SPAWN;
                spawnPacket.header.size = sizeof(PacketPlayerSpawn);
                spawnPacket.playerID = clientID;
                strncpy_s(spawnPacket.username, pkt->username, sizeof(spawnPacket.username) - 1);
                
                BroadcastPacket(&spawnPacket, sizeof(spawnPacket));
                
                // 로그인 성공 후 클라이언트 준비 완료 신호를 기다림
                std::cout << "[Login] Waiting for client " << clientID << " to send ready signal" << std::endl;
                
                // BroadcastNewPlayer 호출 제거 - 클라이언트가 ready 신호를 보낸 후 처리하도록 변경
            }
            
            SendPacket(m_clients[clientID].socket, &response, sizeof(response));
            break;
        }
        
        case PACKET_PLAYER_DISCONNECT: {
            if (header->size != sizeof(PacketPlayerDisconnect)) {
                std::cout << "[Error] Invalid PLAYER_DISCONNECT packet size" << std::endl;
                break;
            }
            PacketPlayerDisconnect* pkt = (PacketPlayerDisconnect*)buffer;
            
            std::cout << "[Disconnect] Player " << pkt->username << " (ID: " << pkt->playerID << ") disconnected" << std::endl;
            
            // 다른 클라이언트들에게 연결 해제 알림
            BroadcastPacket(pkt, sizeof(PacketPlayerDisconnect), clientID);
            
            // 클라이언트 제거
            auto clientIt = m_clients.find(clientID);
            if (clientIt != m_clients.end()) {
                if (clientIt->second.socket != INVALID_SOCKET) {
                    closesocket(clientIt->second.socket);
                    clientIt->second.socket = INVALID_SOCKET;
                }
                m_clients.erase(clientIt);
                std::cout << "[Disconnect] Client " << clientID << " removed. Remaining clients: " << m_clients.size() << std::endl;
            } else {
                std::cout << "[Disconnect] Client " << clientID << " not found in client list" << std::endl;
            }
            break;
        }
        
        case PACKET_PLAYER_UPDATE: {
            if (header->size != sizeof(PacketPlayerUpdate)) {
                std::cout << "[Error] Invalid PLAYER_UPDATE packet size" << std::endl;
                break;
            }
            
            // 클라이언트 존재 여부 확인
            auto clientIt = m_clients.find(clientID);
            if (clientIt == m_clients.end()) {
                std::cout << "[Error] Client " << clientID << " not found for PLAYER_UPDATE" << std::endl;
                break;
            }
            
            // 소켓 유효성 확인
            if (clientIt->second.socket == INVALID_SOCKET) {
                std::cout << "[Error] Invalid socket for client " << clientID << " in PLAYER_UPDATE" << std::endl;
                break;
            }
            
            PacketPlayerUpdate* pkt = (PacketPlayerUpdate*)buffer;
            pkt->clientID = clientID;
            clientIt->second.lastUpdate = *pkt;
            BroadcastPacket(pkt, sizeof(PacketPlayerUpdate), clientID);
            break;
        }
        case PACKET_PLAYER_SPAWN: {
            if (header->size != sizeof(PacketPlayerSpawn)) {
                std::cout << "[Error] Invalid PLAYER_SPAWN packet size" << std::endl;
                break;
            }
            PacketPlayerSpawn* pkt = (PacketPlayerSpawn*)buffer;
            BroadcastPacket(pkt, sizeof(PacketPlayerSpawn), clientID);
            break;
        }
        case PACKET_TIGER_SPAWN: {
            if (header->size != sizeof(PacketTigerSpawn)) {
                std::cout << "[Error] Invalid TIGER_SPAWN packet size" << std::endl;
                break;
            }
            PacketTigerSpawn* pkt = (PacketTigerSpawn*)buffer;
            BroadcastPacket(pkt, sizeof(PacketTigerSpawn));
            break;
        }
        case PACKET_TIGER_UPDATE: {
            if (header->size != sizeof(PacketTigerUpdate)) {
                std::cout << "[Error] Invalid TIGER_UPDATE packet size" << std::endl;
                break;
            }
            PacketTigerUpdate* pkt = (PacketTigerUpdate*)buffer;
            BroadcastPacket(pkt, sizeof(PacketTigerUpdate));
            break;
        }
        case PACKET_CLIENT_READY: {
            if (header->size != sizeof(PacketClientReady)) {
                std::cout << "[Error] Invalid CLIENT_READY packet size" << std::endl;
                break;
            }
            PacketClientReady* pkt = (PacketClientReady*)buffer;
            std::cout << "[ClientReady] Client " << clientID << " is ready to receive game data" << std::endl;
            
            // 클라이언트가 준비되었으므로 호랑이 스폰 패킷들을 순차적으로 전송
            std::cout << "[ClientReady] Sending tiger spawn packets to client " << clientID << std::endl;
            std::cout << "[ClientReady] Total tigers to spawn: " << m_tigers.size() << std::endl;
            
            // 클라이언트 소켓 상태 재확인
            if (m_clients[clientID].socket == INVALID_SOCKET) {
                std::cout << "[Error] Client " << clientID << " socket is invalid, cannot send tiger spawn packets" << std::endl;
                break;
            }
            
            for (const auto& [tigerID, tiger] : m_tigers) {
                // 각 패킷 전송 전 클라이언트 상태 재확인
                if (m_clients.find(clientID) == m_clients.end() || 
                    m_clients[clientID].socket == INVALID_SOCKET) {
                    std::cout << "[Error] Client " << clientID << " disconnected during tiger spawn, stopping" << std::endl;
                    break;
                }
                
                PacketTigerSpawn tigerPacket;
                tigerPacket.header.type = PACKET_TIGER_SPAWN;
                tigerPacket.header.size = sizeof(PacketTigerSpawn);
                tigerPacket.tigerID = tiger.tigerID;
                tigerPacket.x = tiger.x;
                tigerPacket.y = tiger.y;
                tigerPacket.z = tiger.z;
                
                std::cout << "[ClientReady] Attempting to send tiger spawn packet for ID: " << tiger.tigerID 
                          << " at position (" << tiger.x << ", " << tiger.y << ", " << tiger.z << ")" << std::endl;
                
                if (!SendPacket(m_clients[clientID].socket, &tigerPacket, sizeof(PacketTigerSpawn))) {
                    std::cout << "[Error] Failed to send tiger spawn packet for ID: " << tiger.tigerID << std::endl;
                    // 에러가 발생하면 더 이상 전송하지 않음
                    break;
                }
                std::cout << "[Success] Sent tiger spawn packet for ID: " << tiger.tigerID << std::endl;
                
                // 각 호랑이 스폰 사이에 짧은 지연 추가 (클라이언트 처리 시간 확보)
                Sleep(50);  // 50ms로 줄임
            }
            
            std::cout << "[ClientReady] Completed sending all tiger spawn packets to client " << clientID << std::endl;
            
            // 모든 호랑이 스폰 패킷 전송 완료 후 짧은 지연 (클라이언트 처리 시간 확보)
            Sleep(100);  // 100ms로 줄임
            
            // 클라이언트 상태 최종 확인
            if (m_clients.find(clientID) != m_clients.end() && 
                m_clients[clientID].socket != INVALID_SOCKET) {
                std::cout << "[ClientReady] Client " << clientID << " successfully received all tiger spawn packets" << std::endl;
                
                // 호랑이 스폰 완료 후 기존 플레이어 정보 전송
                BroadcastNewPlayer(clientID);
            } else {
                std::cout << "[Error] Client " << clientID << " disconnected after tiger spawn" << std::endl;
            }
            break;
        }
        default:
            std::cout << "  -> Unknown packet type" << std::endl;
            break;
    }
}

void GameServer::Cleanup() {
    m_isRunning = false;

    for (auto& [id, client] : m_clients) {
        closesocket(client.socket);
    }
    m_clients.clear();

    if (m_listenSocket != INVALID_SOCKET) {
        closesocket(m_listenSocket);
        m_listenSocket = INVALID_SOCKET;
    }

    for (HANDLE hThread : m_workerThreads) {
        WaitForSingleObject(hThread, INFINITE);
        CloseHandle(hThread);
    }
    m_workerThreads.clear();

    if (m_hIOCP) {
        CloseHandle(m_hIOCP);
        m_hIOCP = NULL;
    }

    WSACleanup();
}

// [Broadcast] 관련 반복 로그 주석 처리
void GameServer::BroadcastPacket(const void* packet, int size, int excludeID) {
    for (auto& [id, client] : m_clients) {
        if (!client.isLoggedIn || client.socket == INVALID_SOCKET || id == excludeID)
            continue;
            
        // 모든 클라이언트에게 패킷 전송 시도 (연결 에러 무시)
        if (!SendPacket(client.socket, packet, size)) {
            client.sendFailCount++;
            std::cout << "[Broadcast] send failed for client " << id << " (count: " << client.sendFailCount << "), but continuing..." << std::endl;
            // 실패해도 클라이언트 제거하지 않고 계속 시도
            continue;
        } else {
            client.sendFailCount = 0;  // 성공 시 카운트 리셋
        }
    }
}

void GameServer::ProcessNewClient(SOCKET clientSocket) {
    int clientID = m_nextClientID++;
    std::cout << "[Info] ProcessNewClient" << std::endl;
    
    ClientInfo newClient;
    newClient.socket = clientSocket;
    newClient.clientID = clientID;
    newClient.username = "";
    newClient.isLoggedIn = false;
    newClient.lastUpdate = { 0 };
    newClient.packetBufferSize = 0;  // 패킷 버퍼 초기화
    memset(newClient.packetBuffer, 0, sizeof(newClient.packetBuffer));
    
    // 1. IOCP 설정
    if (CreateIoCompletionPort((HANDLE)clientSocket, m_hIOCP, clientID, 0) == NULL) {
        std::cout << "[Error] Failed to associate with IOCP" << std::endl;
        closesocket(clientSocket);
        return;
    }
    
    // 2. 수신 시작 - 먼저 시작
    IOContext* ioContext = new IOContext();
    if (!StartReceive(clientSocket, clientID, ioContext)) {
        std::cout << "[Error] Failed to start receive" << std::endl;
        delete ioContext;
        closesocket(clientSocket);
        return;
    }
    
    // 3. 클라이언트 맵에 추가
    m_clients[clientID] = newClient;
    std::cout << "[ProcessNewClient] " << clientID << " added to map. Total clients: " << m_clients.size() << std::endl;
    
    // 4. 로그인 대기 상태로 설정 (호랑이 스폰 패킷은 로그인 성공 후에 전송)
    std::cout << "[ProcessNewClient] Client " << clientID << " waiting for login..." << std::endl;
}

bool GameServer::SendPacket(SOCKET socket, const void* packet, int size) {
    if (socket == INVALID_SOCKET) return false;
    
    // IOCP에서는 select 없이 직접 send 시도 (IOCP와 호환되는 방식)
    int ret = send(socket, (const char*)packet, size, 0);
    if (ret == SOCKET_ERROR) {
        int err = WSAGetLastError();
        
        // 일시적인 에러는 무시
        if (err == WSAEWOULDBLOCK || err == WSAENOBUFS) {
            std::cout << "[SendPacket] Temporary send error (socket: " << socket << ", Error: " << err << ")" << std::endl;
            return false;
        }
        
        // 연결 관련 에러도 일정 횟수까지는 무시
        if (err == WSAECONNRESET || err == WSAECONNABORTED || err == WSAENOTSOCK) {
            std::cout << "[SendPacket] Connection error but continuing (socket: " << socket << ", Error: " << err << ")" << std::endl;
            return false;  // 즉시 소켓을 닫지 않고 계속 시도
        }
        
        // 기타 에러는 로그만 남기고 계속 시도
        std::cout << "[SendPacket] Send failed but continuing (socket: " << socket << ", Error: " << err << ")" << std::endl;
        return false;
    }
    
    return true;
}

bool GameServer::StartReceive(SOCKET clientSocket, int clientID, IOContext* ioContext) {
    memset(&ioContext->overlapped, 0, sizeof(OVERLAPPED));
    ioContext->wsaBuf.buf = ioContext->buffer;
    ioContext->wsaBuf.len = sizeof(ioContext->buffer);
    ioContext->flags = 0;

    DWORD recvBytes;
    if (WSARecv(clientSocket, &ioContext->wsaBuf, 1, &recvBytes,
        &ioContext->flags, &ioContext->overlapped, NULL) == SOCKET_ERROR) {
        int error = WSAGetLastError();
        if (error != ERROR_IO_PENDING && error != WSAEWOULDBLOCK) {
            std::cout << "[Error] Initial WSARecv failed with error: " << error << std::endl;
            return false;
        }
    }
    return true;
}

void GameServer::BroadcastNewPlayer(int newClientID) {
    std::cout << "\n[BroadcastNewPlayer] New client ID: " << newClientID << std::endl;
    
    // 새 클라이언트가 로그인되었는지 확인
    if (m_clients.find(newClientID) == m_clients.end() || !m_clients[newClientID].isLoggedIn) {
        std::cout << "[BroadcastNewPlayer] Client " << newClientID << " is not logged in yet, skipping" << std::endl;
        return;
    }
    
    // 새 클라이언트 소켓 상태 확인
    if (m_clients[newClientID].socket == INVALID_SOCKET) {
        std::cout << "[BroadcastNewPlayer] Client " << newClientID << " socket is invalid, skipping" << std::endl;
        return;
    }
    
    try {
        // 기존 플레이어 정보 전송 (로그인된 클라이언트만) - 지연 시간 증가
        for (const auto& [id, client] : m_clients) {
            if (id != newClientID && client.isLoggedIn && client.socket != INVALID_SOCKET) {
                PacketPlayerSpawn existingClientPacket;
                existingClientPacket.header.type = PACKET_PLAYER_SPAWN;
                existingClientPacket.header.size = sizeof(PacketPlayerSpawn);
                existingClientPacket.playerID = id;
                strncpy_s(existingClientPacket.username, client.username.c_str(), sizeof(existingClientPacket.username) - 1);
                
                // 새 클라이언트 상태 재확인
                if (m_clients.find(newClientID) == m_clients.end() || 
                    m_clients[newClientID].socket == INVALID_SOCKET) {
                    std::cout << "[BroadcastNewPlayer] Client " << newClientID << " disconnected during broadcast, stopping" << std::endl;
                    return;
                }
                
                if (!SendPacket(m_clients[newClientID].socket, &existingClientPacket, sizeof(existingClientPacket))) {
                    std::cout << "[Error] Failed to send existing player info for ID: " << id << std::endl;
                    continue;
                }
                Sleep(50);  // 50ms로 증가 (클라이언트 처리 시간 확보)
            }
        }
        
        // 잠시 대기 후 새 클라이언트 정보 전송
        Sleep(100);
        
        // 새 클라이언트 정보를 다른 로그인된 클라이언트들에게 전송
        PacketPlayerSpawn newClientPacket;
        newClientPacket.header.type = PACKET_PLAYER_SPAWN;
        newClientPacket.header.size = sizeof(PacketPlayerSpawn);
        newClientPacket.playerID = newClientID;
        strncpy_s(newClientPacket.username, m_clients[newClientID].username.c_str(), sizeof(newClientPacket.username) - 1);
        
        BroadcastPacket(&newClientPacket, sizeof(newClientPacket), newClientID);
        
        std::cout << "[BroadcastNewPlayer] Completed sending info to new client" << std::endl;
    }
    catch (const std::exception& e) {
        std::cout << "[Error] Exception in BroadcastNewPlayer: " << e.what() << std::endl;
    }
    catch (...) {
        std::cout << "[Error] Unknown exception in BroadcastNewPlayer" << std::endl;
    }
}

void GameServer::InitializeTigers() {
    std::cout << "\n[InitializeTigers] Starting tiger initialization..." << std::endl;
    
    // 초기 호랑이 생성 (스폰 패킷은 전송하지 않음)
    for (int i = 0; i < MAX_TIGERS; ++i) {
        TigerInfo tiger;
        tiger.tigerID = m_nextTigerID++;
        tiger.x = GetRandomFloat(0.0f, 1000.0f);
        tiger.y = 0.0f;
        tiger.z = GetRandomFloat(0.0f, 1000.0f);
        tiger.rotY = GetRandomFloat(0.0f, 360.0f);
        tiger.moveTimer = GetRandomFloat(0.0f, 5.0f);
        tiger.isChasing = false;
        
        m_tigers[tiger.tigerID] = tiger;

        std::cout << "[Tiger] Created tiger ID: " << tiger.tigerID 
                  << " at position (" << tiger.x << ", " << tiger.y << ", " << tiger.z << ")" << std::endl;
    }
    
    std::cout << "[InitializeTigers] Completed. Total tigers created: " << m_tigers.size() << std::endl;
    std::cout << "[InitializeTigers] Note: Tiger spawn packets will be sent when clients log in" << std::endl;
}

void GameServer::UpdateTigers(float deltaTime) {
    m_tigerUpdateTimer += deltaTime;
    if (m_tigerUpdateTimer < 0.5f) return; // 500ms마다 업데이트 (적절한 빈도)

    for (auto& [id, tiger] : m_tigers) {
        UpdateTigerBehavior(tiger, m_tigerUpdateTimer);
    }

    BroadcastTigerUpdates();
    m_tigerUpdateTimer = 0.0f;
}

void GameServer::UpdateTigerBehavior(TigerInfo& tiger, float deltaTime) {
    const float CHASE_RADIUS = 200.0f;
    const float MOVE_SPEED = 25.0f;  // 속도 증가로 더 자연스러운 움직임
    
    tiger.moveTimer -= deltaTime;
    
    if (IsPlayerNearby(tiger, CHASE_RADIUS)) {
        tiger.isChasing = true;
        float targetX, targetZ;
        GetNearestPlayerPosition(tiger, targetX, targetZ);
        
        // 플레이어 방향으로 이동 (더 부드럽게)
        float dx = targetX - tiger.x;
        float dz = targetZ - tiger.z;
        float dist = sqrt(dx * dx + dz * dz);
        if (dist > 0.1f) {
            tiger.x += (dx / dist) * MOVE_SPEED * deltaTime;
            tiger.z += (dz / dist) * MOVE_SPEED * deltaTime;
            tiger.rotY = atan2(dx, dz) * (180.0f / 3.141592f);
        }
    }
    else {
        tiger.isChasing = false;
        if (tiger.moveTimer <= 0.0f) {
            // 새로운 랜덤 이동 (더 자연스러운 패턴)
            tiger.moveTimer = GetRandomFloat(2.0f, 5.0f);  // 더 짧은 간격
            float angle = GetRandomFloat(0.0f, 360.0f) * (3.141592f / 180.0f);
            tiger.targetX = tiger.x + cos(angle) * GetRandomFloat(50.0f, 150.0f);  // 다양한 거리
            tiger.targetZ = tiger.z + sin(angle) * GetRandomFloat(50.0f, 150.0f);
        }
        
        // 목표 지점으로 이동 (더 자연스럽게)
        float dx = tiger.targetX - tiger.x;
        float dz = tiger.targetZ - tiger.z;
        float dist = sqrt(dx * dx + dz * dz);
        if (dist > 0.1f) {
            tiger.x += (dx / dist) * MOVE_SPEED * 0.7f * deltaTime;  // 속도 증가
            tiger.z += (dz / dist) * MOVE_SPEED * 0.7f * deltaTime;
            tiger.rotY = atan2(dx, dz) * (180.0f / 3.141592f);
        }
    }
}

void GameServer::BroadcastTigerUpdates() {
    // 로그인된 클라이언트가 없으면 업데이트 전송하지 않음
    int loggedInCount = 0;
    for (const auto& [id, client] : m_clients) {
        if (client.isLoggedIn && client.socket != INVALID_SOCKET) {
            loggedInCount++;
        }
    }
    
    if (loggedInCount == 0) {
        return;  // 로그인된 클라이언트가 없으면 업데이트 전송하지 않음
    }
    
    // 호랑이 업데이트 전송 (적절한 빈도)
    static int updateCounter = 0;
    updateCounter++;
    
    // 3번 중 1번만 업데이트 전송 (적절한 빈도)
    if (updateCounter % 3 != 0) {
        return;
    }
    
    for (const auto& [id, tiger] : m_tigers) {
        PacketTigerUpdate updatePacket;
        updatePacket.header.type = PACKET_TIGER_UPDATE;
        updatePacket.header.size = sizeof(PacketTigerUpdate);
        updatePacket.tigerID = tiger.tigerID;
        updatePacket.x = tiger.x;
        updatePacket.y = tiger.y;
        updatePacket.z = tiger.z;
        updatePacket.rotY = tiger.rotY;
        
        BroadcastPacket(&updatePacket, sizeof(updatePacket));
    }
}

float GameServer::GetRandomFloat(float min, float max) {
    std::uniform_real_distribution<float> dist(min, max);
    return dist(m_randomEngine);
}

bool GameServer::IsPlayerNearby(const TigerInfo& tiger, float radius) {
    for (const auto& [id, client] : m_clients) {
        float dx = client.lastUpdate.x - tiger.x;
        float dz = client.lastUpdate.z - tiger.z;
        float distSq = dx * dx + dz * dz;
        if (distSq < radius * radius) {
            return true;
        }
    }
    return false;
}

void GameServer::GetNearestPlayerPosition(const TigerInfo& tiger, float& outX, float& outZ) {
    float minDistSq = FLT_MAX;
    outX = tiger.x;
    outZ = tiger.z;
    
    for (const auto& [id, client] : m_clients) {
        float dx = client.lastUpdate.x - tiger.x;
        float dz = client.lastUpdate.z - tiger.z;
        float distSq = dx * dx + dz * dz;
        if (distSq < minDistSq) {
            minDistSq = distSq;
            outX = client.lastUpdate.x;
            outZ = client.lastUpdate.z;
        }
    }
}

int main() {
    GameServer server;
    
    if (!server.Initialize(5000)) {  // 포트 번호 지정 가능
        std::cout << "[Error] Server initialization failed" << std::endl;
        return 1;
    }

    server.Start();
    return 0;
}