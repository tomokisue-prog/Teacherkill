#pragma once

struct CharacterSize {
    float height;
    float radius;
};

// 現行マップのワールド単位。描画・移動・被弾判定が同じ寸法を参照する。
namespace WorldScale {
    extern const float StageScale;
    extern const CharacterSize Player;
    extern const CharacterSize Monster;
    extern const CharacterSize Zombie;
    extern const float EyeHeightRatio;
}
