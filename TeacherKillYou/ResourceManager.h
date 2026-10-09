#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include "raylib.h"
#include "ResourceKeys.h"

class ResourceManager {
public:
    static ResourceManager& GetInstance();

    // キーとパスを指定してモデルを登録する。FBXも変換せず直接読み込む。
    void LoadModel(const std::string& key, const std::string& path);

    // キーを指定してどこからでもモデルを取得する
    Model GetModel(const std::string& key) const;
    Model& GetModelRef(const std::string& key);

    // 読み込み時の寸法を保持し、描画時の頂点走査を省く。
    BoundingBox GetModelBounds(const std::string& key) const;
    // 先頭の有効なクリップを返す。未登録なら空。所有と解放はResourceManagerが担当する。
    ModelAnimation GetModelAnimation(const std::string& key) const;

    // アニメーション読み込み用関数
    void LoadModelAnimations(const std::string& key, const std::string& path);

    // FBXはシアーを含む行列で再生し、通常のモデルはraylibへ委譲する。
    void ApplyModelAnimation(const std::string& key, int frame, int animationIndex = 0);

    // 指定したキーのアニメーションを取得（存在しない場合は count = 0）
    ModelAnimation* GetModelAnimations(const std::string& key, int* count = nullptr) const;

    // モデルが所有するシェーダーを重複なしで返す。照明更新用の借用参照。
    std::vector<Shader> GetModelShaders() const;

    // 全モデルの一括ロード／一括解放
    void LoadAll();
    void UnloadAll();

private:
    ResourceManager() = default;
    ~ResourceManager() = default;

    ResourceManager(const ResourceManager&) = delete;
    ResourceManager& operator=(const ResourceManager&) = delete;

    Shader modelShader_{}; // GLBモデル間で共有。UnloadAllで一度だけ解放する。
    std::unordered_map<std::string, Model> models_;
    std::unordered_map<std::string, BoundingBox> modelBounds_;

    struct AnimationData {
        ModelAnimation* anims{ nullptr };
        int count{ 0 };
        std::vector<std::vector<Matrix>> bakedFrames;
    };
    std::unordered_map<std::string, AnimationData> animations_;
};

// ショートカット関数
inline ResourceManager& RM() {
    return ResourceManager::GetInstance();
}
