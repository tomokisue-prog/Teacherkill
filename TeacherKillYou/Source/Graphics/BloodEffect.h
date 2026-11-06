#pragma once
#include "raylib.h"
#include <array>
#include <cstddef>

// 命中位置から飛び散る血。GL初期化後にInit、終了前にShutdownを呼ぶ。
class BloodEffect {
public:
    BloodEffect() = default;
    BloodEffect(const BloodEffect&) = delete;
    BloodEffect& operator=(const BloodEffect&) = delete;

    void Init();
    void Reset();
    void Spawn(Vector3 point, Vector3 normal);
    void Update(float deltaTime);
    void Draw(const Camera3D& camera) const;
    void Shutdown();

private:
    struct Droplet {
        Vector3 position{}, velocity{};
        float age = 0.0f, lifetime = 0.0f, size = 0.0f;
        Color color{};
    };

    // 固定長プールを再利用し、連射時も粒子数とメモリ確保の負荷を抑える。
    std::array<Droplet, 128> droplets_{};
    std::size_t next_ = 0;
    Texture2D texture_{};
    Shader shader_{};
};
