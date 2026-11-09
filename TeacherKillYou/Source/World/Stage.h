#pragma once
#include "raylib.h"
#include "ResourceKeys.h"
#include <vector>
#include <string>
#include <unordered_map>

struct StageSection {
    std::string name = "Stage 1";
    std::string modelKey = ResourceKeys::Model_Stage1;
    Vector3 position = { 0.0f, 0.0f, 0.0f };
    Vector3 rotation = { 0.0f, 0.0f, 0.0f };
    float scale = 1.0f;
    bool active = true;
};

class Stage
{
public:
    Stage() = default;
    ~Stage() = default;

    void Init();
    void Reset();
    void Draw() const;
    void End();

    // 複数ステージセクション（Stage 1, Stage 2, Stage 3 ...）の取得・管理
    std::vector<StageSection>& GetSections() { return sections_; }
    const std::vector<StageSection>& GetSections() const { return sections_; }
    StageSection* GetSection(int index) {
        if (index >= 0 && index < static_cast<int>(sections_.size())) return &sections_[index];
        return nullptr;
    }
    const StageSection* GetSection(int index) const {
        if (index >= 0 && index < static_cast<int>(sections_.size())) return &sections_[index];
        return nullptr;
    }
    int GetSectionCount() const { return static_cast<int>(sections_.size()); }

    // 既存コードとの互換用アクセサ（メインステージ / セクション0）
    Vector3 GetPosition() const { return sections_.empty() ? Vector3{} : sections_[0].position; }
    void SetPosition(Vector3 pos) { if (!sections_.empty()) sections_[0].position = pos; }

    float GetScale() const { return sections_.empty() ? 1.0f : sections_[0].scale; }
    void SetScale(float scale) { if (!sections_.empty()) sections_[0].scale = scale; }

    const std::string& GetStageKey() const {
        static const std::string fallback = ResourceKeys::Model_Stage1;
        return sections_.empty() ? fallback : sections_[0].modelKey;
    }
    void SetStageKey(const std::string& key) { if (!sections_.empty()) sections_[0].modelKey = key; }

    // レイキャストを実行し、衝突情報（ヒットした座標や距離）を返す（全ステージ走査）
    RayCollision Raycast(Ray ray) const;
    RayCollision Raycast(Ray ray, float maxDistance) const;

    // 上向きの面だけを床として判定し、同じメッシュ内の天井を除外する（全ステージ走査）
    RayCollision RaycastGround(Ray ray, float maxDistance = 999999.0f) const;
    // 希望するX/Zを保持し、最も近い床の高さへ初期配置する。床がなければ元の位置を返す
    Vector3 FindGroundPosition(Vector3 preferred) const;

private:
    RayCollision RaycastSurfaces(Ray ray, float maxDistance, bool groundOnly) const;
    void CacheCollisionBounds(const std::string& key, const Model& model) const;

    struct ModelBoundsCache {
        const Mesh* cachedMeshes = nullptr;
        std::vector<BoundingBox> bounds;
    };
    mutable std::unordered_map<std::string, ModelBoundsCache> modelBoundsCache_;

    std::vector<StageSection> sections_;
};