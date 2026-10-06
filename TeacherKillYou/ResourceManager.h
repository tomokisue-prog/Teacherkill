#pragma once
#include <string>
#include <unordered_map>
#include "raylib.h"
#include "ResourceKeys.h"

class ResourceManager {
public:
    static ResourceManager& GetInstance();

    // キーとパスを指定してモデルをロード・登録する
    void LoadModel(const std::string& key, const std::string& path, bool animated = false);

    // キーを指定してどこからでもモデルを取得する
    Model GetModel(const std::string& key) const;

    // 静止姿勢の寸法を保持し、描画時の頂点走査を省く。
    BoundingBox GetModelBounds(const std::string& key) const;
    // 先頭のクリップを返す。未登録なら空。所有と解放はResourceManagerが担当する。
    ModelAnimation GetModelAnimation(const std::string& key) const;

    // 全モデルの一括ロード／一括解放
    void LoadAll();
    void UnloadAll();

private:
    ResourceManager() = default;
    ~ResourceManager() = default;

    ResourceManager(const ResourceManager&) = delete;
    ResourceManager& operator=(const ResourceManager&) = delete;

    struct ModelResource {
        Model model = {};
        BoundingBox bounds = {};
        ModelAnimation* animations = nullptr;
        int animationCount = 0;
    };
    std::unordered_map<std::string, ModelResource> models_;
};

// ショートカット関数
inline ResourceManager& RM() {
    return ResourceManager::GetInstance();
}
