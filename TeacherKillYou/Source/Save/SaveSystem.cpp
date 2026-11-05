#include "SaveSystem.h"
#include "GameContext.h"
#include "raymath.h"
#include "json.hpp"
#include <fstream>

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

bool SaveSystem::SaveScene(const GameContext& gameContext, const std::string& filepath)
{
    nlohmann::json root;

    // 1. Player
    const auto& player = gameContext.GetPlayer();
    root["player"]["position"] = Vector3ToJson(player.GetPosition());
    root["player"]["rotation"] = Vector3ToJson(player.GetRotation());
    root["player"]["scale"] = Vector3ToJson(player.GetScale());

    // Enemy (保存処理)
    nlohmann::json enemyArray = nlohmann::json::array();
    for (const auto& sp : gameContext.GetEnemyManager().GetSpawnPoints())
    {
        nlohmann::json spJson;
        spJson["position"] = Vector3ToJson(sp.GetPosition());
        spJson["type"] = static_cast<int>(sp.config.type);
        spJson["condition"] = static_cast<int>(sp.condition);
        spJson["despawnCondition"] = static_cast<int>(sp.despawnCondition);
        spJson["spawned"] = sp.spawned;

        spJson["bodyHeight"] = sp.config.bodyHeight;
        spJson["bodyRadius"] = sp.config.bodyRadius;
        spJson["spawnedEnemyId"] = sp.spawnedEnemyId;
        enemyArray.push_back(spJson);
    }
    root["enemies"] = enemyArray;

    // 2. Doors (複数ドアの配列保存)
    nlohmann::json doorArray = nlohmann::json::array();
    for (const auto& door : gameContext.GetDoors())
    {
        nlohmann::json dJson;
        dJson["position"] = Vector3ToJson(door.GetPosition());
        dJson["rotation"] = Vector3ToJson(door.GetRotation());
        dJson["scale"] = Vector3ToJson(door.GetScale());
        dJson["slideSpeed"] = door.GetSlideSpeed();
        dJson["openDistance"] = door.GetOpenDistance();
        dJson["lockType"] = static_cast<int>(door.GetLockType());
        dJson["openConditionType"] = static_cast<int>(door.GetOpenConditionType());
        doorArray.push_back(dJson);
    }
    root["doors"] = doorArray;

    const auto& securityPC = gameContext.GetSecurityPC();
    root["securityPC"]["position"] = Vector3ToJson(securityPC.GetPosition());
    root["securityPC"]["rotation"] = Vector3ToJson(securityPC.GetRotation());
    root["securityPC"]["scale"] = Vector3ToJson(securityPC.GetScale());
    root["securityPC"]["isActivated"] = securityPC.IsActivated();

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

    // EventDialoguePoints
    nlohmann::json epArray = nlohmann::json::array();
    for (const auto& ep : gameContext.GetEventDialoguePoints())
    {
        nlohmann::json epJson;
        epJson["position"] = Vector3ToJson(ep.GetPosition());
        epJson["rotation"] = Vector3ToJson(ep.GetRotation());
        epJson["scale"] = Vector3ToJson(ep.GetScale());
        epJson["radius"] = ep.GetRadius();
        epJson["dialogueId"] = ep.GetDialogueId();
        epJson["triggered"] = ep.IsTriggered();
        epJson["condition"] = static_cast<int>(ep.GetCondition());
        epArray.push_back(epJson);
    }
    root["eventDialoguePoints"] = epArray;

    nlohmann::json secTpArray = nlohmann::json::array();
    for (const auto& stp : gameContext.GetSecurityTeleportPoints())
    {
        nlohmann::json stpJson;
        stpJson["position"] = Vector3ToJson(stp.GetPosition());
        stpJson["rotation"] = Vector3ToJson(stp.GetRotation());
        stpJson["scale"] = Vector3ToJson(stp.GetScale());
        stpJson["pointNumber"] = stp.GetPointNumber();
        stpJson["type"] = static_cast<int>(stp.GetTeleportType());
        stpJson["radius"] = stp.GetRadius();
        stpJson["condition"] = static_cast<int>(stp.GetCondition());
        stpJson["triggered"] = stp.IsTriggered();
        secTpArray.push_back(stpJson);
    }
    root["securityTeleportPoints"] = secTpArray;

    std::ofstream file(filepath);
    if (file.is_open())
    {
        file << root.dump(4);
        TraceLog(LOG_INFO, "SAVE_SYSTEM: Saved successfully to %s", filepath.c_str());
        return true;
    }

    TraceLog(LOG_ERROR, "SAVE_SYSTEM: Failed to open file for saving: %s", filepath.c_str());
    return false;
}

bool SaveSystem::LoadScene(GameContext& gameContext, const std::string& filepath)
{
    std::ifstream file(filepath);
    if (!file.is_open())
    {
        TraceLog(LOG_WARNING, "SAVE_SYSTEM: Save file not found (%s). Skipping load.", filepath.c_str());
        return false;
    }

    nlohmann::json root;
    try
    {
        file >> root;
    }
    catch (const std::exception& e)
    {
        TraceLog(LOG_ERROR, "SAVE_SYSTEM: Failed to parse JSON: %s", e.what());
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

    // Enemy (ロード処理)
    if (root.contains("enemies") && root["enemies"].is_array())
    {
        auto& spawnPoints = gameContext.GetEnemyManager().GetSpawnPoints();
        spawnPoints.clear();
        gameContext.GetEnemyManager().Clear();

        for (const auto& spJson : root["enemies"])
        {
            demo::EnemySpawnData sp;
            if (spJson.contains("position")) sp.SetPosition(JsonToVector3(spJson["position"]));
            if (spJson.contains("type"))     sp.config.type = static_cast<demo::EnemyType>(spJson["type"].get<int>());
            if (spJson.contains("condition")) sp.condition = static_cast<demo::EnemySpawnCondition>(spJson["condition"].get<int>());
            if (spJson.contains("despawnCondition")) sp.despawnCondition = static_cast<demo::EnemyDespawnCondition>(spJson["despawnCondition"].get<int>());

            // ロード時は必ず未スポーン状態から再開できるようにする
            sp.spawned = false;

            if (spJson.contains("bodyHeight")) sp.config.bodyHeight = spJson["bodyHeight"].get<float>();
            if (spJson.contains("bodyRadius")) sp.config.bodyRadius = spJson["bodyRadius"].get<float>();

            if (spJson.contains("spawnedEnemyId"))   sp.spawnedEnemyId = static_cast<demo::EnemyId>(spJson["spawnedEnemyId"].get<int>());

            spawnPoints.push_back(sp);
        }
    }

    // 2. Doors (複数ドアの配列復元)
    if (root.contains("doors") && root["doors"].is_array())
    {
        auto& doors = gameContext.GetDoors();
        doors.clear();
        for (const auto& dJson : root["doors"])
        {
            DoorMove door;
            if (dJson.contains("position"))     door.SetPosition(JsonToVector3(dJson["position"]));
            if (dJson.contains("rotation"))     door.SetRotation(JsonToVector3(dJson["rotation"]));
            if (dJson.contains("scale"))        door.SetScale(JsonToVector3(dJson["scale"]));
            if (dJson.contains("slideSpeed"))   door.SetSlideSpeed(dJson["slideSpeed"].get<float>());
            if (dJson.contains("openDistance")) door.SetOpenDistance(dJson["openDistance"].get<float>());
            if (dJson.contains("lockType"))     door.SetLockType(static_cast<DoorLockType>(dJson["lockType"].get<int>()));
            if (dJson.contains("openConditionType")) door.SetOpenConditionType(static_cast<DoorOpenConditionType>(dJson["openConditionType"].get<int>()));
            doors.push_back(door);
        }
    }

    if (root.contains("securityPC"))
    {
        const auto& s = root["securityPC"];
        auto& securityPC = gameContext.GetSecurityPC();
        if (s.contains("position"))    securityPC.SetPosition(JsonToVector3(s["position"]));
        if (s.contains("rotation"))    securityPC.SetRotation(JsonToVector3(s["rotation"]));
        if (s.contains("scale"))       securityPC.SetScale(JsonToVector3(s["scale"]));
        if (s.contains("isActivated")) securityPC.SetActivated(s["isActivated"].get<bool>());
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

    // EventDialoguePoints
    if (root.contains("eventDialoguePoints") && root["eventDialoguePoints"].is_array())
    {
        auto& eventPoints = gameContext.GetEventDialoguePoints();
        eventPoints.clear();
        for (const auto& epJson : root["eventDialoguePoints"])
        {
            EventDialoguePoint ep;
            if (epJson.contains("position"))   ep.SetPosition(JsonToVector3(epJson["position"]));
            if (epJson.contains("rotation"))   ep.SetRotation(JsonToVector3(epJson["rotation"]));
            if (epJson.contains("scale"))      ep.SetScale(JsonToVector3(epJson["scale"]));
            if (epJson.contains("radius"))     ep.SetRadius(epJson["radius"].get<float>());
            if (epJson.contains("dialogueId")) ep.SetDialogueId(epJson["dialogueId"].get<std::string>());
            if (epJson.contains("triggered"))  ep.SetTriggered(epJson["triggered"].get<bool>());
            if (epJson.contains("condition"))  ep.SetCondition(static_cast<EventDialogueCondition>(epJson["condition"].get<int>()));

            eventPoints.push_back(ep);
        }
    }

    if (root.contains("securityTeleportPoints") && root["securityTeleportPoints"].is_array())
    {
        auto& stps = gameContext.GetSecurityTeleportPoints();
        stps.clear();
        for (const auto& stpJson : root["securityTeleportPoints"])
        {
            SecurityTeleportPoint stp;
            if (stpJson.contains("position"))    stp.SetPosition(JsonToVector3(stpJson["position"]));
            if (stpJson.contains("rotation"))    stp.SetRotation(JsonToVector3(stpJson["rotation"]));
            if (stpJson.contains("scale"))       stp.SetScale(JsonToVector3(stpJson["scale"]));
            if (stpJson.contains("pointNumber")) stp.SetPointNumber(stpJson["pointNumber"].get<int>());
            if (stpJson.contains("type"))        stp.SetTeleportType(static_cast<SecurityTeleportType>(stpJson["type"].get<int>()));
            if (stpJson.contains("condition"))   stp.SetCondition(static_cast<SecurityTeleportCondition>(stpJson["condition"].get<int>()));
            if (stpJson.contains("triggered"))   stp.SetTriggered(stpJson["triggered"].get<bool>());
            if (stpJson.contains("radius"))      stp.SetRadius(stpJson["radius"].get<float>());
            stps.push_back(stp);
        }
    }

    gameContext.GetPlayer().SetPosition(
        gameContext.GetStage().FindGroundPosition(gameContext.GetPlayer().GetPosition())
    );

    TraceLog(LOG_INFO, "SAVE_SYSTEM: Loaded successfully from %s", filepath.c_str());
    return true;
}