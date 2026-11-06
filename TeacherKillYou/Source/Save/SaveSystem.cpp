#include "SaveSystem.h"
#include "GameContext.h"
#include "raymath.h"
#include "json.hpp"
#include "SaveData.h"
#include <optional>
#include <stdexcept>

static nlohmann::json Vector3ToJson(const Vector3& v)
{
    return nlohmann::json{ {"x", v.x}, {"y", v.y}, {"z", v.z} };
}

static Vector3 JsonToVector3(const nlohmann::json& j)
{
    return Vector3{
        j.value("x", 0.0f),
        j.value("y", 0.0f),
        j.value("z", 0.0f)
    };
}

namespace {
    using nlohmann::json;

    Vector3 ParsePlacementPosition(const json& position)
    {
        return {SaveData::Number(position.at("x"), -100000, 100000),
                SaveData::Number(position.at("y"), -100000, 100000),
                SaveData::Number(position.at("z"), -100000, 100000)};
    }

    std::vector<Item> ParseItemPlacements(const json& entries)
    {
        if (!entries.is_array() || entries.size() > MaxItemPlacements)
            throw std::runtime_error("invalid items array");
        std::vector<Item> items;
        items.reserve(entries.size());
        for (const auto& entry : entries) {
            const auto type = static_cast<ItemType>(SaveData::Integer(entry.at("type"), 1, int(ItemType::Count) - 1));
            items.emplace_back(type, ParsePlacementPosition(entry.at("position")));
            items.back().SetYaw(SaveData::Number(entry.at("yaw"), -PI, PI));
            items.back().SetScale(SaveData::Number(entry.at("scale"), .05f, 20));
        }
        return items;
    }

    std::vector<demo::EnemyPlacement> ParseEnemyPlacements(const json& entries)
    {
        if (!entries.is_array() || entries.size() > demo::MaxEnemyPlacements)
            throw std::runtime_error("invalid enemies array");
        std::vector<demo::EnemyPlacement> placements;
        placements.reserve(entries.size());
        for (const auto& entry : entries) {
            // Older scenes may still contain the retired enemy ID 0.
            if (SaveData::Integer(entry.at("config").at("type"), 0, 2) == 0) continue;
            placements.push_back({ParsePlacementPosition(entry.at("position")),
                SaveData::Number(entry.at("yaw"), -PI, PI),
                SaveData::ParseEnemyConfig(entry.at("config"))});
        }
        return placements;
    }
}

bool SaveSystem::SaveScene(const GameContext& gameContext, const std::string& filepath)
{
    nlohmann::json root;

    // 1. Player
    const auto& player = gameContext.GetPlayer();
    root["player"]["position"] = Vector3ToJson(player.GetPosition());
    root["player"]["rotation"] = Vector3ToJson(player.GetRotation());
    root["player"]["scale"] = Vector3ToJson(player.GetScale());

    // 2. Door
    const auto& door = gameContext.GetDoor();
    root["door"]["position"] = Vector3ToJson(door.GetPosition());
    root["door"]["rotation"] = Vector3ToJson(door.GetRotation());
    root["door"]["scale"] = Vector3ToJson(door.GetScale());
    root["door"]["slideSpeed"] = door.GetSlideSpeed();
    root["door"]["openDistance"] = door.GetOpenDistance();

    // 3. TeleportPoints
    nlohmann::json tpArray = nlohmann::json::array();
    for (const auto& tp : gameContext.GetTeleportPoints())
    {
        nlohmann::json tpJson;
        tpJson["position"] = Vector3ToJson(tp.GetPosition());
        tpJson["rotation"] = Vector3ToJson(tp.GetRotation());
        tpJson["scale"] = Vector3ToJson(tp.GetScale());
        tpJson["pointNumber"] = tp.GetPointNumber();
        tpJson["type"] = static_cast<int>(tp.GetTeleportType());
        tpJson["radius"] = tp.GetRadius();
        tpArray.push_back(tpJson);
    }
    root["teleportPoints"] = tpArray;

    const auto& goalPoint = gameContext.GetGoalPoint();
    root["goalPoint"]["position"] = Vector3ToJson(goalPoint.GetPosition());
    root["goalPoint"]["rotation"] = Vector3ToJson(goalPoint.GetRotation());
    root["goalPoint"]["scale"] = Vector3ToJson(goalPoint.GetScale());
    root["goalPoint"]["radius"] = goalPoint.GetRadius();

    try {
        auto& enemies = root["enemies"] = nlohmann::json::array();
        for (const auto& placement : gameContext.GetEnemyPlacements())
            enemies.push_back({{"position", Vector3ToJson(placement.position)}, {"yaw", placement.yaw},
                {"config", SaveData::PackEnemyConfig(placement.config)}});
        ParseEnemyPlacements(enemies);

        auto& items = root["items"] = nlohmann::json::array();
        for (const auto& item : gameContext.GetInventory().GetPlacements())
            items.push_back({{"type", int(item.GetType())}, {"position", Vector3ToJson(item.GetPosition())},
                {"yaw", item.GetYaw()}, {"scale", item.GetScale()}});
        ParseItemPlacements(items);
        return SaveData::Write(filepath, root);
    }
    catch (const std::exception& error) {
        TraceLog(LOG_ERROR, "SAVE_SYSTEM: Cannot save scene placements: %s", error.what());
        return false;
    }
}

bool SaveSystem::LoadScene(GameContext& gameContext, const std::string& filepath)
{
    nlohmann::json root;
    if (!SaveData::Read(filepath, root)) return false;

    std::optional<std::vector<demo::EnemyPlacement>> placements;
    std::optional<std::vector<Item>> items;
    try {
        if (root.contains("enemies"))
            placements = ParseEnemyPlacements(root.at("enemies"));
        if (root.contains("items"))
            items = ParseItemPlacements(root.at("items"));
    }
    catch (const std::exception& error) {
        TraceLog(LOG_ERROR, "SAVE_SYSTEM: Cannot load scene placements: %s", error.what());
        return false;
    }

    // 1. Player
    if (root.contains("player"))
    {
        const auto& p = root["player"];
        auto& player = gameContext.GetPlayer();
        if (p.contains("position")) player.SetPosition(JsonToVector3(p["position"]));
        if (p.contains("rotation")) player.SetRotation(JsonToVector3(p["rotation"]));
        if (p.contains("scale"))    player.SetScale(JsonToVector3(p["scale"]));
    }

    // 2. Door
    if (root.contains("door"))
    {
        const auto& d = root["door"];
        auto& door = gameContext.GetDoor();
        if (d.contains("position"))     door.SetPosition(JsonToVector3(d["position"]));
        if (d.contains("rotation"))     door.SetRotation(JsonToVector3(d["rotation"]));
        if (d.contains("scale"))        door.SetScale(JsonToVector3(d["scale"]));
        if (d.contains("slideSpeed"))   door.SetSlideSpeed(d["slideSpeed"].get<float>());
        if (d.contains("openDistance")) door.SetOpenDistance(d["openDistance"].get<float>());
    }

    // 3. TeleportPoints
    if (root.contains("teleportPoints") && root["teleportPoints"].is_array())
    {
        auto& tps = gameContext.GetTeleportPoints();
        tps.clear();
        for (const auto& tpJson : root["teleportPoints"])
        {
            TeleportPoint tp;
            if (tpJson.contains("position"))    tp.SetPosition(JsonToVector3(tpJson["position"]));
            if (tpJson.contains("rotation"))    tp.SetRotation(JsonToVector3(tpJson["rotation"]));
            if (tpJson.contains("scale"))       tp.SetScale(JsonToVector3(tpJson["scale"]));
            if (tpJson.contains("pointNumber")) tp.SetPointNumber(tpJson["pointNumber"].get<int>());
            if (tpJson.contains("type"))        tp.SetTeleportType(static_cast<TeleportType>(tpJson["type"].get<int>()));
            if (tpJson.contains("radius"))      tp.SetRadius(tpJson["radius"].get<float>());

            tps.push_back(tp);
        }
    }

    if (root.contains("goalPoint"))
    {
        const auto& g = root["goalPoint"];
        auto& goalPoint = gameContext.GetGoalPoint();
        if (g.contains("position")) goalPoint.SetPosition(JsonToVector3(g["position"]));
        if (g.contains("rotation")) goalPoint.SetRotation(JsonToVector3(g["rotation"]));
        if (g.contains("scale"))    goalPoint.SetScale(JsonToVector3(g["scale"]));
        if (g.contains("radius"))   goalPoint.SetRadius(g["radius"].get<float>());
    }

    // ínå`í≤êÆÇ»Ç«ÇÃå„èàóù
    gameContext.GetPlayer().SetPosition(
        gameContext.GetStage().FindGroundPosition(gameContext.GetPlayer().GetPosition())
    );

    if (placements) {
        gameContext.GetEnemyPlacements() = std::move(*placements);
        gameContext.ApplyEnemyPlacements();
    }

    if (items) {
        auto& inventory = gameContext.GetInventory();
        inventory.GetPlacements() = std::move(*items);
        inventory.ApplyPlacements();
    }

    TraceLog(LOG_INFO, "SAVE_SYSTEM: Loaded successfully from %s", filepath.c_str());
    return true;
}