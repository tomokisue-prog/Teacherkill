#include "CheckpointSave.h"
#include "WorldScale.h"
#include "PlayerCamera.h"
#include "SaveData.h"
#include "nlohmann/json.hpp"
#include <cmath>
#include <stdexcept>

namespace {
    using Json = nlohmann::json;
    void Require(bool valid) { if (!valid) throw std::runtime_error("invalid checkpoint field"); }
    using SaveData::Number;
    using SaveData::Integer;
    Json Pack(Vector3 value) { return Json::array({value.x, value.y, value.z}); }
    Vector3 Vector(const Json& data) {
        Require(data.is_array() && data.size() == 3);
        return {Number(data[0], -100000, 100000), Number(data[1], -100000, 100000), Number(data[2], -100000, 100000)};
    }
    Json Pack(const TransformCheckpoint& value) {
        return {{"position", Pack(value.position)}, {"rotation", Pack(value.rotation)}, {"scale", Pack(value.scale)}};
    }
    TransformCheckpoint Transform(const Json& data) {
        TransformCheckpoint result{Vector(data.at("position")), Vector(data.at("rotation")), Vector(data.at("scale"))};
        Require(result.scale.x > 0 && result.scale.y > 0 && result.scale.z > 0);
        return result;
    }
    Json PackDoor(const DoorCheckpoint& door) {
        return {{"transform", Pack(door.transform)}, {"speed", door.speed},
                {"distance", door.distance}, {"radius", door.radius},
                {"progress", door.progress}, {"open", door.open},
                {"lockType", int(door.lockType)}};
    }
    DoorCheckpoint ParseDoor(const Json& door) {
        DoorCheckpoint result{Transform(door.at("transform")), Number(door.at("speed"), 0, 100),
            Number(door.at("distance"), 0, 10000), Number(door.at("radius"), .001f, 10000),
            Number(door.at("progress"), 0, 1), door.at("open").get<bool>()};
        if (door.contains("lockType"))
            result.lockType = static_cast<DoorLockType>(Integer(door.at("lockType"), 0, int(DoorLockType::CustomEventFlag)));
        return result;
    }
    Json Pack(const CheckpointData& data) {
        Json j{{"version", 2}, {"player", {{"position", Pack(data.player.position)}, {"hp", data.player.hp},
            {"ammo", data.player.ammo}, {"reserve", data.player.reserveAmmo}, {"stamina", data.player.stamina}}},
            {"camera", {{"yaw", data.yaw}, {"pitch", data.pitch}}}, {"flashlight", data.flashlight},
            {"stage", {{"position", Pack(data.stagePosition)}, {"scale", data.stageScale}}},
            {"goal", {{"transform", Pack(data.goal)}, {"radius", data.goalRadius}}},
            {"door", PackDoor(data.door)}};
        j["doors"] = Json::array();
        for (const auto& door : data.doors) j["doors"].push_back(PackDoor(door));
        j["inventory"]["slots"] = Json::array();
        for (auto slot : data.inventory.slots) j["inventory"]["slots"].push_back(int(slot));
        j["inventory"]["selected"] = data.inventory.selectedSlot;
        j["inventory"]["items"] = Json::array();
        for (const auto& item : data.inventory.items)
            j["inventory"]["items"].push_back({{"type", int(item.type)}, {"position", Pack(item.position)}, {"collected", item.collected},
                {"yaw", item.yaw}, {"scale", item.scale}});
        j["enemies"] = Json::array();
        for (const auto& e : data.enemies)
            j["enemies"].push_back({{"config", SaveData::PackEnemyConfig(e.config)}, {"position", Pack(e.position)}, {"yaw", e.yaw}, {"hp", e.hp}});
        j["teleports"] = Json::array();
        for (const auto& tp : data.teleports)
            j["teleports"].push_back({{"transform", Pack(tp.transform)}, {"number", tp.number}, {"type", tp.type}, {"radius", tp.radius}});
        return j;
    }
    CheckpointData Parse(const Json& j) {
        const int version = Integer(j.at("version"), 1, 2);
        CheckpointData d;
        const auto& p = j.at("player");
        d.player = {Vector(p.at("position")), Integer(p.at("hp"), 1, 100), Integer(p.at("ammo"), 0, 15),
            Integer(p.at("reserve"), 0, 1000000), Number(p.at("stamina"), 0, 100)};
        d.yaw = Number(j.at("camera").at("yaw"), -1000000, 1000000);
        d.pitch = Number(j.at("camera").at("pitch"), PlayerCamera::MinPitch, PlayerCamera::MaxPitch);
        d.flashlight = j.at("flashlight").get<bool>();
        d.stagePosition = Vector(j.at("stage").at("position"));
        d.stageScale = Number(j.at("stage").at("scale"), .0001f, 10000);
        d.goal = Transform(j.at("goal").at("transform"));
        d.goalRadius = Number(j.at("goal").at("radius"), .001f, 10000);
        d.door = ParseDoor(j.at("door"));
        if (j.contains("doors")) {
            Require(j.at("doors").is_array() && j.at("doors").size() <= 512);
            for (const auto& door : j.at("doors")) d.doors.push_back(ParseDoor(door));
        }
        const auto& inv = j.at("inventory");
        Require(inv.at("slots").is_array() && inv.at("slots").size() == d.inventory.slots.size());
        for (int i = 0; i < 4; ++i) {
            const int slot = Integer(inv["slots"][i], 0, int(ItemType::LostItem));
            Require(slot != int(ItemType::Ammo));
            d.inventory.slots[i] = static_cast<ItemType>(slot);
        }
        d.inventory.selectedSlot = Integer(inv.at("selected"), 0, 3);
        Require(inv.at("items").is_array() && inv.at("items").size() <= MaxItemPlacements);
        for (const auto& item : inv["items"])
            d.inventory.items.push_back({static_cast<ItemType>(Integer(item.at("type"), 1, int(ItemType::Count) - 1)),
                Vector(item.at("position")), item.at("collected").get<bool>(),
                item.contains("yaw") ? Number(item.at("yaw"), -PI, PI) : 0.0f,
                item.contains("scale") ? Number(item.at("scale"), .05f, 20) : 1.0f});
        Require(j.at("enemies").is_array() && j.at("enemies").size() <= demo::MaxEnemyPlacements);
        for (const auto& e : j["enemies"]) {
            // Skip the retired type without shifting IDs of the remaining enemies.
            if (Integer(e.at("config").at("type"), 0, 2) == 0) continue;
            auto c = SaveData::ParseEnemyConfig(e.at("config"));
            if (version == 1) {
                // 以前のチェックポイントでも、HPや所持品を保って現行マップの体格へ移行する。
                const auto size = WorldScale::Zombie;
                c.bodyHeight = size.height;
                c.bodyRadius = size.radius;
            }
            d.enemies.push_back({c, Vector(e.at("position")), Number(e.at("yaw"), -PI, PI), Integer(e.at("hp"), 1, c.maxHp)});
        }
        Require(j.at("teleports").is_array() && j.at("teleports").size() <= 512);
        for (const auto& tp : j["teleports"])
            d.teleports.push_back({Transform(tp.at("transform")), Integer(tp.at("number"), 0, 100000),
                Integer(tp.at("type"), 0, 1), Number(tp.at("radius"), .001f, 10000)});
        return d;
    }
}

bool CheckpointSave::Save(const CheckpointData& data, const std::string& path)
{
    try {
        auto json = Pack(data);
        Parse(json); 
        return SaveData::Write(path, json);
    }
    catch (const std::exception& error) {
        TraceLog(LOG_WARNING, "CHECKPOINT: Cannot save: %s", error.what());
        return false;
    }
}

bool CheckpointSave::Load(CheckpointData& data, const std::string& path)
{
    try {
        nlohmann::json json;
        if (!SaveData::Read(path, json)) return false;
        auto parsed = Parse(json); 
        data = std::move(parsed);
        return true;
    }
    catch (const std::exception& error) {
        TraceLog(LOG_WARNING, "CHECKPOINT: Cannot load: %s", error.what());
        return false;
    }
}
