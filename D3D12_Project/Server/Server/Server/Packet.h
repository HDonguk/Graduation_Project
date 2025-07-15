#pragma once
#include <winsock2.h>

#pragma pack(push, 1)
struct PacketHeader {
    unsigned short size;
    unsigned short type;
};

enum PacketType {
    PACKET_PLAYER_UPDATE = 1,
    PACKET_PLAYER_SPAWN = 2,
    PACKET_TIGER_SPAWN = 3,    // 호랑이 스폰 패킷
    PACKET_TIGER_UPDATE = 4,   // 호랑이 업데이트 패킷
    PACKET_TREE_SPAWN = 5,     // 나무 스폰 패킷
    PACKET_LOGIN_REQUEST = 6,  // 로그인 요청
    PACKET_LOGIN_RESPONSE = 7, // 로그인 응답
    PACKET_PLAYER_DISCONNECT = 8, // 플레이어 연결 해제
    PACKET_CLIENT_READY = 9    // 클라이언트 준비 완료 신호
};

struct PacketPlayerUpdate {
    PacketHeader header;
    int clientID;
    float x, y, z;    // Position
    float rotY;       // Rotation
};

struct PacketPlayerSpawn {
    PacketHeader header;
    int playerID;     // 클라이언트 ID
    char username[32]; // 사용자명 (최대 31자 + null)
};

struct PacketTigerSpawn {
    PacketHeader header;
    int tigerID;
    float x, y, z;
};

struct PacketTigerUpdate {
    PacketHeader header;
    int tigerID;
    float x, y, z;
    float rotY;
};

struct TreePosition {
    float x, y, z;
    float rotY;
    int treeType;  // 0: long_tree, 1: normal_tree
};

struct PacketTreeSpawn {
    PacketHeader header;
    int treeCount;
    TreePosition trees[20];  // 최대 20개 나무 위치 정보
};

struct PacketLoginRequest {
    PacketHeader header;
    char username[32]; // 사용자명 (최대 31자 + null)
};

struct PacketLoginResponse {
    PacketHeader header;
    int clientID;
    bool success;
    char message[128]; // 응답 메시지
};

struct PacketPlayerDisconnect {
    PacketHeader header;
    int playerID;
    char username[32];
};

struct PacketClientReady {
    PacketHeader header;
    int clientID;
};
#pragma pack(pop)