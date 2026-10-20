#include "BloodEffect.h"
#include "raymath.h"
#include "rlgl.h"
#include <cmath>

namespace {
    // 血は加算合成せず、通常のアルファ合成で描く。第2描画先は懐中電灯の照射マスク。
    // その出力のアルファを0にして、血の描画で既存の照射範囲を書き換えない。
    const char* BloodFragment = R"(#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
uniform sampler2D texture0;
uniform vec4 colDiffuse;
layout(location = 0) out vec4 finalColor;
layout(location = 1) out vec4 focusColor;
void main() {
    finalColor = texture(texture0, fragTexCoord) * colDiffuse * fragColor;
    focusColor = vec4(0.0);
})";

    float Random(float low, float high) {
        return Lerp(low, high, GetRandomValue(0, 10000) / 10000.0f);
    }
}

void BloodEffect::Init() {
    Reset();
    if (texture_.id && shader_.id) return;
    Shutdown();
    // 輪郭だけをぼかした小さな粒を生成する。外部のエフェクト画像は不要。
    Image image = GenImageColor(32, 32, BLANK);
    for (int y = 0; y < image.height; ++y) {
        for (int x = 0; x < image.width; ++x) {
            const float dx = (x + 0.5f) / 16.0f - 1.0f;
            const float dy = (y + 0.5f) / 16.0f - 1.0f;
            const float alpha = Clamp((1.0f - std::sqrt(dx * dx + dy * dy)) * 4.0f, 0.0f, 1.0f);
            ImageDrawPixel(&image, x, y, {255, 255, 255, static_cast<unsigned char>(255 * alpha)});
        }
    }
    texture_ = LoadTextureFromImage(image);
    UnloadImage(image);
    SetTextureFilter(texture_, TEXTURE_FILTER_BILINEAR);
    shader_ = LoadShaderFromMemory(nullptr, BloodFragment);
    if (!texture_.id || !shader_.id || shader_.id == rlGetShaderIdDefault()) {
        TraceLog(LOG_WARNING, "BLOOD: failed to initialize impact renderer");
        Shutdown();
    }
}

void BloodEffect::Reset() {
    droplets_ = {};
    next_ = 0;
}

void BloodEffect::Spawn(Vector3 point, Vector3 normal) {
    if (!texture_.id || !shader_.id) return;
    if (Vector3LengthSqr(normal) < 0.001f) normal = {0, 0, -1};
    normal = Vector3Normalize(normal);
    const Vector3 tangent = Vector3Normalize(Vector3CrossProduct(
        normal, std::fabs(normal.y) > 0.9f ? Vector3{1, 0, 0} : Vector3{0, 1, 0}));
    const Vector3 bitangent = Vector3CrossProduct(normal, tangent);
    const Vector3 origin = Vector3Add(point, Vector3Scale(normal, 0.015f));
    for (int i = 0; i < 17; ++i) {
        auto& drop = droplets_[next_];
        next_ = (next_ + 1) % droplets_.size();
        drop = {};
        drop.position = origin;
        drop.velocity = Vector3Add(Vector3Scale(normal, Random(0.45f, 1.5f)),
            Vector3Add(Vector3Scale(tangent, Random(-0.85f, 0.85f)),
                       Vector3Scale(bitangent, Random(-0.85f, 0.85f))));
        drop.velocity.y += 0.35f;
        drop.lifetime = Random(0.3f, 0.6f);
        drop.size = Random(0.025f, 0.055f);
        drop.color = {static_cast<unsigned char>(GetRandomValue(145, 195)), 8, 15, 235};
        // 最初の1粒は命中位置に短く残し、そこから飛び散ったことを見せる。
        if (i == 0) {
            drop.velocity = Vector3Scale(normal, 0.08f);
            drop.lifetime = 0.16f;
            drop.size = 0.1f;
        }
    }
}

void BloodEffect::Update(float deltaTime) {
    if (!std::isfinite(deltaTime) || deltaTime <= 0.0f) return;
    for (auto& drop : droplets_) {
        if (drop.age >= drop.lifetime) continue;
        drop.age += deltaTime;
        if (drop.age >= drop.lifetime) continue;
        // 重力による移動量を時間で積分し、フレームレートによる軌道の差を抑える。
        drop.position = Vector3Add(drop.position, Vector3Add(Vector3Scale(drop.velocity, deltaTime),
            {0, -2.4f * deltaTime * deltaTime, 0}));
        drop.velocity.y -= 4.8f * deltaTime;
    }
}

void BloodEffect::Draw(const Camera3D& camera) const {
    if (!texture_.id || !shader_.id) return;
    bool active = false;
    for (const auto& drop : droplets_) active |= drop.age < drop.lifetime;
    if (!active) return;

    // 壁や敵に隠れる粒は描かない。半透明の粒自身は深度を書き込まない。
    rlDrawRenderBatchActive();
    rlDisableDepthMask();
    BeginBlendMode(BLEND_ALPHA);
    BeginShaderMode(shader_);
    for (const auto& drop : droplets_) {
        if (drop.age >= drop.lifetime) continue;
        const float life = drop.age / drop.lifetime;
        const float fadeTime = Clamp((life - 0.4f) / 0.6f, 0.0f, 1.0f);
        const float fade = 1.0f - fadeTime * fadeTime * (3.0f - 2.0f * fadeTime);
        Color color = drop.color;
        color.a = static_cast<unsigned char>(color.a * fade);
        const float size = drop.size * Lerp(1.0f, 0.65f, life);
        DrawBillboardRec(camera, texture_, {0, 0, float(texture_.width), float(texture_.height)},
            drop.position, {size, size * 1.4f}, color);
    }
    EndShaderMode();
    EndBlendMode();
    rlDrawRenderBatchActive();
    rlEnableDepthMask();
}

void BloodEffect::Shutdown() {
    Reset();
    if (texture_.id) UnloadTexture(texture_);
    if (shader_.id && shader_.id != rlGetShaderIdDefault()) UnloadShader(shader_);
    texture_ = {};
    shader_ = {};
}
