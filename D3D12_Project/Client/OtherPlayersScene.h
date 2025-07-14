#pragma once
#include "Scene.h"
#include "OtherPlayerManager.h"

class OtherPlayersScene : public Scene {
public:
    OtherPlayersScene(UINT width, UINT height, wstring name) 
        : Scene(width, height, name) {}

    void OnInit(ID3D12Device* device, ID3D12GraphicsCommandList* commandList) override {
        Scene::OnInit(device, commandList);
    }

    void OnUpdate(GameTimer& gTimer) override {
        Scene::OnUpdate(gTimer);
    }

    void OnRender(ID3D12Device* device, ID3D12GraphicsCommandList* commandList) override {
        Scene::OnRender(device, commandList);
        
        auto& otherPlayers = OtherPlayerManager::GetInstance()->GetPlayers();
        for (auto& [id, player] : otherPlayers) {
            if (player) {
                player->OnRender(device, commandList);
            }
        }
    }
}; 