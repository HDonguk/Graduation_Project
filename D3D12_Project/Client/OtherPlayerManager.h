#pragma once
#include <unordered_map>
#include "Object.h"
#include "Scene.h"
#include "ResourceManager.h"
#include "GameTimer.h"
#include "NetworkManager.h"
#include <mutex>

class OtherPlayerManager {
private:
    static OtherPlayerManager* instance;
    Scene* m_currentScene{nullptr};
    NetworkManager* m_networkManager{nullptr};
    std::unordered_map<int, PlayerObject*> otherPlayers;
    std::mutex m_mutex;  // 스레드 안전성을 위한 뮤텍스 추가


    OtherPlayerManager() {}

public:
    static OtherPlayerManager* GetInstance() {
        if (instance == nullptr) {
            instance = new OtherPlayerManager();
        }
        return instance;
    }

    void SetScene(Scene* scene) { m_currentScene = scene; }

    void SetNetworkManager(NetworkManager* networkManager) { m_networkManager = networkManager; }

    void SpawnOtherPlayer(int clientID);

    void UpdateOtherPlayer(int clientID, float x, float y, float z, float rotY);

    void RemoveOtherPlayer(int clientID) {
        if (otherPlayers.find(clientID) != otherPlayers.end()) {
            delete otherPlayers[clientID];
            otherPlayers.erase(clientID);
        }
    }

    std::unordered_map<int, PlayerObject*>& GetPlayers() {
        return otherPlayers;
    }
   
}; 