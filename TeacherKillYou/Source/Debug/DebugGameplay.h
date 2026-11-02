#pragma once
#ifndef NDEBUG
#include "raylib.h"
#include "nlohmann/json_fwd.hpp"
#include <array>
#include <cstdint>
#include <deque>
class GameContext;

enum class DebugShotTarget { Miss, Wall, Enemy };
struct DebugShot {
    std::uint64_t number = 0;
    double time = 0;
    Ray ray{};
    Vector3 end{}, normal{};
    DebugShotTarget target = DebugShotTarget::Miss;
    std::uint64_t enemyId = 0;
    int damage = 0, hpAfter = 0;
    bool killed = false;
};
struct DebugPosition {
    bool valid = false;
    Vector3 position{}, stagePosition{};
    float yaw = 0, pitch = 0, stageScale = 1;
};

// 開発用の履歴と位置登録を所有する。通常のセーブデータには含めない。
class DebugGameplay {
public:
    static constexpr int PositionCount = 5;
    void RecordShot(DebugShot shot);
    void ClearShots();
    const std::deque<DebugShot>& GetShots() const;
    bool StorePosition(const GameContext& context, int slot);
    bool RecallPosition(GameContext& context, int slot, bool snapToFloor) const;
    void ClearPosition(int slot);
    const std::array<DebugPosition, PositionCount>& GetPositions() const;
    nlohmann::json SavePositions() const;
    void LoadPositions(const nlohmann::json& data);
private:
    std::deque<DebugShot> shots_;
    std::uint64_t nextShot_ = 1;
    std::array<DebugPosition, PositionCount> positions_{};
};
#endif
