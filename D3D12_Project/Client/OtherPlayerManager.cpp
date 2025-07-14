#include "OtherPlayerManager.h"
#include "GameTimer.h"

OtherPlayerManager* OtherPlayerManager::instance = nullptr;

void OtherPlayerManager::SpawnOtherPlayer(int clientID) {
    if (otherPlayers.find(clientID) != otherPlayers.end()) {
        m_networkManager->LogToFile("[OtherPlayerManager] Player already exists: " + std::to_string(clientID));
        return;
    }

    try {
        m_networkManager->LogToFile("[OtherPlayerManager] Getting OtherPlayer object from scene...");
        
        // Scene과 NetworkManager 상태 확인
        if (!m_currentScene) {
            m_networkManager->LogToFile("[OtherPlayerManager] Scene is null");
            return;
        }
        
        if (!m_networkManager) {
            m_networkManager->LogToFile("[OtherPlayerManager] NetworkManager is null");
            return;
        }
        
        auto& player = m_currentScene->GetObj<PlayerObject>(L"OtherPlayer");
        
        m_networkManager->LogToFile("[OtherPlayerManager] Starting component checks...");
        if (!player.FindComponent<Position>()) {
            m_networkManager->LogToFile("[OtherPlayerManager] Position component missing");
            throw std::runtime_error("Position component not found");
        }
        if (!player.FindComponent<Rotation>()) {
            m_networkManager->LogToFile("[OtherPlayerManager] Rotation component missing");
            throw std::runtime_error("Rotation component not found");
        }
        if (!player.FindComponent<Mesh>()) {
            m_networkManager->LogToFile("[OtherPlayerManager] Mesh component missing");
            throw std::runtime_error("Mesh component not found");
        }
        if (!player.FindComponent<Texture>()) {
            m_networkManager->LogToFile("[OtherPlayerManager] Texture component missing");
            throw std::runtime_error("Texture component not found");
        }
        
        m_networkManager->LogToFile("[OtherPlayerManager] All components found, setting player active");
        player.SetActive(true);
        otherPlayers[clientID] = &player;
        m_networkManager->LogToFile("[OtherPlayerManager] Successfully spawned player " + std::to_string(clientID));
    }
    catch (const std::bad_alloc& e) {
        m_networkManager->LogToFile("[OtherPlayerManager] Memory allocation failed: " + std::string(e.what()));
    }
    catch (const std::exception& e) {
        m_networkManager->LogToFile("[OtherPlayerManager] Exception during spawn: " + std::string(e.what()));
    }
    catch (...) {
        m_networkManager->LogToFile("[OtherPlayerManager] Unknown exception during spawn");
    }
}

void OtherPlayerManager::UpdateOtherPlayer(int clientID, float x, float y, float z, float rotY) {
   
    
    auto it = otherPlayers.find(clientID);
    if (it == otherPlayers.end()) {

        SpawnOtherPlayer(clientID);
        return;
    }

    auto* player = it->second;
    auto& position = player->GetComponent<Position>();
    auto& rotation = player->GetComponent<Rotation>();
    
    // 네트워크에서 받은 위치와 회전값만 업데이트
    position.mFloat4 = XMFLOAT4(x, y, z, 1.0f);
    rotation.mFloat4 = XMFLOAT4(0.0f, rotY, 0.0f, 0.0f);
}

// Scene의 Update에서 처리