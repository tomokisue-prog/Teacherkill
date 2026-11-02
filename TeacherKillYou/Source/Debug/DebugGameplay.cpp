#include "DebugGameplay.h"
#ifndef NDEBUG
#include "GameContext.h"
#include "raymath.h"
#include "nlohmann/json.hpp"
#include <cmath>

namespace {
    bool Finite(Vector3 p) { return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z); }
    bool ReadNumber(const nlohmann::json& object, const char* name, float& value, double limit = 100000) {
        auto it = object.find(name);
        if (it == object.end() || !it->is_number()) return false;
        const double number = it->get<double>();
        if (!std::isfinite(number) || std::abs(number) > limit) return false;
        value = float(number);
        return true;
    }
    bool ReadVector(const nlohmann::json& value, Vector3& v) {
        return value.is_object() && ReadNumber(value,"x",v.x) && ReadNumber(value,"y",v.y) && ReadNumber(value,"z",v.z);
    }
    nlohmann::json VectorJson(Vector3 v) { return {{"x",v.x},{"y",v.y},{"z",v.z}}; }
}

void DebugGameplay::RecordShot(DebugShot shot) {
    shot.number = nextShot_++;
    shot.time = GetTime();
    if (shots_.size() >= 32) shots_.pop_front();
    shots_.push_back(shot);
}
void DebugGameplay::ClearShots() { shots_.clear(); }
const std::deque<DebugShot>& DebugGameplay::GetShots() const { return shots_; }
const std::array<DebugPosition, DebugGameplay::PositionCount>& DebugGameplay::GetPositions() const { return positions_; }

bool DebugGameplay::StorePosition(const GameContext& context, int slot) {
    if (slot < 0 || slot >= PositionCount) return false;
    const auto& camera = context.GetCameraController().GetPlayerCamera();
    DebugPosition entry{true,context.GetPlayer().GetPosition(),context.GetStage().GetPosition(),camera.GetYaw(),camera.GetPitch(),context.GetStage().GetScale()};
    if (!Finite(entry.position) || !Finite(entry.stagePosition) || !std::isfinite(entry.stageScale) || entry.stageScale == 0) return false;
    positions_[slot] = entry;
    return true;
}

bool DebugGameplay::RecallPosition(GameContext& context, int slot, bool snapToFloor) const {
    if (slot < 0 || slot >= PositionCount || !positions_[slot].valid) return false;
    const auto& entry = positions_[slot];
    // ステージの移動・拡縮後に、古い座標へ誤って移動しない。
    if (Vector3Distance(entry.stagePosition,context.GetStage().GetPosition()) > .001f ||
        std::abs(entry.stageScale-context.GetStage().GetScale()) > .001f) return false;
    const auto p = snapToFloor ? context.GetStage().FindGroundPosition(entry.position) : entry.position;
    return context.DebugTeleportPlayer(p,entry.yaw,entry.pitch);
}
void DebugGameplay::ClearPosition(int slot) {
    if (slot >= 0 && slot < PositionCount) positions_[slot] = {};
}

nlohmann::json DebugGameplay::SavePositions() const {
    auto data = nlohmann::json::array();
    for (const auto& entry : positions_) {
        if (!entry.valid) { data.push_back(nullptr); continue; }
        data.push_back({{"position",VectorJson(entry.position)},{"stagePosition",VectorJson(entry.stagePosition)},
            {"yaw",entry.yaw},{"pitch",entry.pitch},{"stageScale",entry.stageScale}});
    }
    return data;
}
void DebugGameplay::LoadPositions(const nlohmann::json& data) {
    if (!data.is_array() || data.size() > PositionCount) return;
    std::array<DebugPosition, PositionCount> next{};
    for (std::size_t i=0;i<data.size();++i) {
        const auto& item=data[i];if(!item.is_object() || !item.contains("position") || !item.contains("stagePosition")) continue;
        DebugPosition entry;
        if (!ReadVector(item["position"],entry.position) || !ReadVector(item["stagePosition"],entry.stagePosition) ||
            !ReadNumber(item,"yaw",entry.yaw) || !ReadNumber(item,"pitch",entry.pitch,1.4) ||
            !ReadNumber(item,"stageScale",entry.stageScale,10000) || entry.stageScale==0) continue;
        entry.valid=true;next[i]=entry;
    }
    positions_=next;
}
#endif
