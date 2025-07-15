#pragma once
#include "stdafx.h"
#include "Packet.h"
#include "Scene.h"
#include "GameTimer.h"
#include <fstream>
#include <ctime>
#include <mutex>
#include <unordered_map>
#include <queue>

// 에러 타입 정의
enum class ErrorType {
    NONE,
    CONNECTION_FAILED,
    SEND_FAILED,
    RECEIVE_FAILED,
    TIMEOUT
};

// 나무 생성 요청 구조체
struct TreeSpawnRequest {
    int treeID;
    float x, y, z;
    float rotY;
    int treeType;
};

class OtherPlayerManager;
class Scene;

class NetworkManager {
public:
    NetworkManager();
    ~NetworkManager();
    bool Initialize(const char* serverIP, int port, Scene* scene);
    void SetScene(Scene* scene) { m_scene = scene; }
    void SendPlayerUpdate(float x, float y, float z, float rotY);
    void SendLoginRequest(const std::string& username);
    void SendPlayerDisconnect();
    void Shutdown();
    bool IsRunning() const { return m_isRunning; }
    bool IsLoggedIn() const { return m_isLoggedIn; }
    static void LogToFile(const std::string& message);
    void Update(GameTimer& gTimer, Scene* scene);
    void HandleError(const std::string& description);
    bool ShouldReconnect() const;
    bool AttemptReconnect();
    void ResetErrorInfo();
    
    // 콜백 설정
    void SetLoginSuccessCallback(std::function<void(int, const std::string&)> callback);
    void SetLoginFailedCallback(std::function<void(const std::string&)> callback);

private:
    static DWORD WINAPI NetworkThread(LPVOID arg);
    void ProcessPacket(char* buffer);
    void ProcessTreeSpawnQueue(); // 나무 생성 큐 처리

    struct TigerInfo {
        int tigerID;
        float x, y, z;
        float rotY;
    };
    std::unordered_map<int, TigerInfo> m_tigers;  // 타이거 정보 저장

    // 나무 생성 요청 큐 (스레드 안전)
    std::queue<TreeSpawnRequest> m_treeSpawnQueue;
    std::mutex m_treeSpawnMutex;

    Scene* m_scene{nullptr};
    SOCKET sock;
    HANDLE m_networkThread;
    bool m_isRunning;
    char m_recvBuffer[4096];  // 버퍼 크기를 4KB로 증가
    char m_packetBuffer[8192];  // 패킷 큐잉을 위한 추가 버퍼
    int m_packetBufferSize{0};  // 현재 패킷 버퍼에 저장된 데이터 크기
    static std::ofstream m_logFile;
    static std::mutex m_logMutex;
    int m_myClientID{0};  // 자신의 클라이언트 ID 저장
    std::string m_username;  // 사용자명
    bool m_isLoggedIn{false};  // 로그인 상태
    float m_updateTimer{0.0f};  // 업데이트 간격 타이머
    
    // 에러 처리 관련 (간단한 버전)
    int m_errorCount{0};
    DWORD m_lastErrorTime{0};
    bool m_shouldReconnect{false};
    int m_reconnectAttempts{0};
    DWORD m_lastReconnectTime{0};
    
    // 콜백 함수
    std::function<void(int, const std::string&)> m_loginSuccessCallback;
    std::function<void(const std::string&)> m_loginFailedCallback;
};