#pragma once

#pragma comment(lib, "ws2_32.lib")

#define NOMINMAX
#include <winsock2.h>
#include <windows.h>
#include <mswsock.h>
#include <unordered_map>
#include <vector>
#include <random>
#include "Packet.h"

class GameServer {
public:
    GameServer();
    ~GameServer();

    bool Initialize(int port = 5000);
    void Start();
    void Stop();

private:
    static constexpr int MAX_CLIENTS = 2;
    static constexpr int MAX_PACKET_SIZE = 1024;
    static constexpr int MAX_TIGERS = 5;  // 최대 호랑이 수
    static constexpr int MAX_TREES = 289;  // 17x17 나무

    struct ClientInfo {
        SOCKET socket;
        int clientID;
        std::string username;
        bool isLoggedIn;
        PacketPlayerUpdate lastUpdate;
        int sendFailCount = 0; // 송신 실패 횟수
        int connectionErrorCount = 0; // 연결 에러 횟수
        
        // 패킷 버퍼링을 위한 추가 필드
        char packetBuffer[MAX_PACKET_SIZE * 4];  // 여러 패킷을 저장할 수 있는 버퍼
        int packetBufferSize = 0;  // 현재 버퍼에 저장된 데이터 크기
    };

    struct TigerInfo {
        int tigerID;
        float x, y, z;
        float rotY;
        float targetX, targetZ;  // 목표 위치
        float moveTimer;         // 이동 타이머
        bool isChasing;         // 플레이어 추적 여부
    };

    struct TreeInfo {
        int treeID;
        float x, y, z;
        float rotY;
        int treeType;  // 0: long_tree, 1: normal_tree
    };

    struct IOContext {
        OVERLAPPED overlapped;
        WSABUF wsaBuf;
        char buffer[MAX_PACKET_SIZE];
        DWORD flags;
    };

private:
    // 멤버 변수
    HANDLE m_hIOCP;
    int m_nextClientID;
    int m_nextTigerID;
    int m_nextTreeID;
    std::unordered_map<int, ClientInfo> m_clients;
    std::unordered_map<int, TigerInfo> m_tigers;
    std::unordered_map<int, TreeInfo> m_trees;
    SOCKET m_listenSocket;
    std::vector<HANDLE> m_workerThreads;
    bool m_isRunning;
    int m_port;
    float m_tigerUpdateTimer;
    std::mt19937 m_randomEngine;

    // 내부 메서드
    static DWORD WINAPI WorkerThreadProc(LPVOID lpParam);
    DWORD WorkerThread();
    void Cleanup();
    void BroadcastPacket(const void* packet, int size, int excludeID = -1);
    void ProcessNewClient(SOCKET clientSocket);
    bool SendPacket(SOCKET socket, const void* packet, int size);
    bool StartReceive(SOCKET clientSocket, int clientID, IOContext* ioContext);
    void BroadcastNewPlayer(int newClientID);
    void HandlePacket(IOContext* ioContext, int clientID, DWORD bytesTransferred);
    void ProcessSinglePacket(char* buffer, int clientID, int packetSize);
    
    // 호랑이 관련 메서드
    void InitializeTigers();
    void UpdateTigers(float deltaTime);
    void BroadcastTigerUpdates();
    void UpdateTigerBehavior(TigerInfo& tiger, float deltaTime);
    float GetRandomFloat(float min, float max);
    bool IsPlayerNearby(const TigerInfo& tiger, float radius);
    void GetNearestPlayerPosition(const TigerInfo& tiger, float& outX, float& outZ);
    
    // 나무 관련 메서드
    void InitializeTrees();
    void SendTreePositions(int clientID);
}; 