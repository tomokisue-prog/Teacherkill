#pragma once
#include "EnemyConfig.h"
#include "Item.h"
#include <array>
#include <vector>

// Only durable gameplay state belongs here; textures, pointers and pending hits do not.
struct TransformCheckpoint {
    Vector3 position{};
    Vector3 rotation{};
    Vector3 scale{1, 1, 1};
};
struct PlayerCheckpoint {
    Vector3 position{};
    int hp = 100;
    int ammo = 15;
    int reserveAmmo = 90;
    float stamina = 100;
};
struct WorldItemCheckpoint {
    ItemType type = ItemType::None;
    Vector3 position{};
    bool collected = false;
};
struct InventoryCheckpoint {
    std::array<ItemType, 4> slots{};
    int selectedSlot = 0;
    std::vector<WorldItemCheckpoint> items;
};
struct DoorCheckpoint {
    TransformCheckpoint transform;
    float speed = 2;
    float distance = 2;
    float radius = 1.5f;
    float progress = 0;
    bool open = false;
};
struct EnemyCheckpoint {
    demo::EnemyConfig config;
    Vector3 position{};
    float yaw = 0;
    int hp = 100;
};
struct TeleportCheckpoint {
    TransformCheckpoint transform;
    int number = 0;
    int type = 0;
    float radius = 1.5f;
};
struct CheckpointData {
    PlayerCheckpoint player;
    InventoryCheckpoint inventory;
    DoorCheckpoint door;
    std::vector<EnemyCheckpoint> enemies;
    std::vector<TeleportCheckpoint> teleports;
    Vector3 stagePosition{};
    float stageScale = 1;
    TransformCheckpoint goal;
    float goalRadius = 1.5f;
    float yaw = 0;
    float pitch = 0;
    bool flashlight = true;
};
