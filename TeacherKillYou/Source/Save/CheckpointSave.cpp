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
    float Number(const Json& data, float low, float high) {
        Require(data.is_number());
        const float value = data.get<float>();
        Require(std::isfinite(value) && value >= low && value <= high);
        return value;
    }
    int Integer(const Json& data, int low, int high) {
        Require(data.is_number_integer());
        const auto value = data.get<std::int64_t>();
        Require(value >= low && value <= high);
        return static_cast<int>(value);
    }
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
    Json Pack(const demo::EnemyConfig& c) {
        return {{"type", int(c.type)}, {"maxHp", c.maxHp}, {"moveSpeed", c.moveSpeed}, {"turnSpeed", c.turnSpeed},
            {"bodyRadius", c.bodyRadius}, {"bodyHeight", c.bodyHeight}, {"detectionRange", c.detectionRange},
            {"loseRange", c.loseRange}, {"stopDistance", c.stopDistance}, {"resumeDistance", c.resumeDistance},
            {"corpseDuration", c.corpseDuration}, {"attackDamage", c.attackDamage}, {"attackRange", c.attackRange},
            {"attackHitFraction", c.attackHitFraction}, {"attackRecovery", c.attackRecovery}, {"animationSpeed", c.animationSpeed}};
    }
    demo::EnemyConfig Config(const Json& j) {
        demo::EnemyConfig c;
        c.type = static_cast<demo::EnemyType>(Integer(j.at("type"), 0, 1));
        c.maxHp = Integer(j.at("maxHp"), 1, 100000);
        c.moveSpeed = Number(j.at("moveSpeed"), 0, 100);
        c.turnSpeed = Number(j.at("turnSpeed"), 0, 100);
        c.bodyRadius = Number(j.at("bodyRadius"), .01f, 100);
        c.bodyHeight = Number(j.at("bodyHeight"), .01f, 100);
        c.detectionRange = Number(j.at("detectionRange"), 0, 10000);
        c.loseRange = Number(j.at("loseRange"), 0, 10000);
        c.stopDistance = Number(j.at("stopDistance"), 0, 100);
        c.resumeDistance = Number(j.at("resumeDistance"), 0, 100);
        c.corpseDuration = Number(j.at("corpseDuration"), 0, 10000);
        c.attackDamage = Integer(j.at("attackDamage"), 0, 100000);
        c.attackRange = Number(j.at("attackRange"), 0, 100);
        c.attackHitFraction = Number(j.at("attackHitFraction"), 0, 1);
        c.attackRecovery = Number(j.at("attackRecovery"), 0, 10000);
        c.animationSpeed = Number(j.at("animationSpeed"), .01f, 100);
        c.Validate();
        return c;
    }
    Json Pack(const CheckpointData& data) {
        Json j{{"version", 2}, {"player", {{"position", Pack(data.player.position)}, {"hp", data.player.hp},
            {"ammo", data.player.ammo}, {"reserve", data.player.reserveAmmo}, {"stamina", data.player.stamina}}},
            {"camera", {{"yaw", data.yaw}, {"pitch", data.pitch}}}, {"flashlight", data.flashlight},
            {"stage", {{"position", Pack(data.stagePosition)}, {"scale", data.stageScale}}},
            {"goal", {{"transform", Pack(data.goal)}, {"radius", data.goalRadius}}},
            {"door", {{"transform", Pack(data.door.transform)}, {"speed", data.door.speed}, {"distance", data.door.distance},
                {"radius", data.door.radius}, {"progress", data.door.progress}, {"open", data.door.open}}}};
        j["inventory"]["slots"] = Json::array();
        for (auto slot : data.inventory.slots) j["inventory"]["slots"].push_back(int(slot));
        j["inventory"]["selected"] = data.inventory.selectedSlot;
        j["inventory"]["items"] = Json::array();
        for (const auto& item : data.inventory.items)
            j["inventory"]["items"].push_back({{"type", int(item.type)}, {"position", Pack(item.position)}, {"collected", item.collected}});
        j["enemies"] = Json::array();
        for (const auto& e : data.enemies)
            j["enemies"].push_back({{"config", Pack(e.config)}, {"position", Pack(e.position)}, {"yaw", e.yaw}, {"hp", e.hp}});
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
        const auto& door = j.at("door");
        d.door = {Transform(door.at("transform")), Number(door.at("speed"), 0, 100), Number(door.at("distance"), 0, 10000),
            Number(door.at("radius"), .001f, 10000), Number(door.at("progress"), 0, 1), door.at("open").get<bool>()};
        const auto& inv = j.at("inventory");
        Require(inv.at("slots").is_array() && inv.at("slots").size() == d.inventory.slots.size());
        for (int i = 0; i < 4; ++i) {
            const int slot = Integer(inv["slots"][i], 0, int(ItemType::Ammo) - 1);
            d.inventory.slots[i] = static_cast<ItemType>(slot);
        }
        d.inventory.selectedSlot = Integer(inv.at("selected"), 0, 3);
        Require(inv.at("items").is_array() && inv.at("items").size() <= 1024);
        for (const auto& item : inv["items"])
            d.inventory.items.push_back({static_cast<ItemType>(Integer(item.at("type"), 1, int(ItemType::Ammo))),
                Vector(item.at("position")), item.at("collected").get<bool>()});
        Require(j.at("enemies").is_array() && j.at("enemies").size() <= 512);
        for (const auto& e : j["enemies"]) {
            auto c = Config(e.at("config"));
            if (version == 1) {
                // 以前のチェックポイントでも、HPや所持品を保って現行マップの体格へ移行する。
                const auto size = c.type == demo::EnemyType::FemaleZombie ? WorldScale::Zombie : WorldScale::Monster;
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
        Parse(json); // Apply the same constraints on both sides; never replace a good save with bad state.
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
        auto parsed = Parse(json); // Do not touch the live world until every field has been validated.
        data = std::move(parsed);
        return true;
    }
    catch (const std::exception& error) {
        TraceLog(LOG_WARNING, "CHECKPOINT: Cannot load: %s", error.what());
        return false;
    }
}
