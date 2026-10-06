#include "ResourceManager.h"
#include "raylib.h"
#include "rlgl.h"

#include <unordered_set>

ResourceManager& ResourceManager::GetInstance() {
    static ResourceManager instance;
    return instance;
}

// ゲーム全体で使うモデルをキーと一緒に登録しておく
void ResourceManager::LoadAll() {
    LoadModel(ResourceKeys::Model_Player, "Data/Image/greenman.glb");

	LoadModel(ResourceKeys::Model_Stage1, "Data/Image/free_loft_18_mini_office_v.optimization.glb");
    LoadModel(ResourceKeys::Model_Enemy, "Data/Image/monster test.glb", true);
    // 他のモデルが増えたらここに追加
    // LoadModel(ResourceKeys::Model_Stage, "Data/Image/stage.glb");
}

void ResourceManager::LoadModel(const std::string& key, const std::string& path, bool animated) {
    // すでに登録済みならロードしない（二重ロード防止）
    if (models_.find(key) != models_.end()) {
        return;
    }

    ModelResource resource;
    resource.model = ::LoadModel(path.c_str());
    if (!IsModelValid(resource.model)) {
        ::UnloadModel(resource.model);
        return;
    }
    resource.bounds = GetModelBoundingBox(resource.model);
    if (animated) {
        resource.animations = ::LoadModelAnimations(path.c_str(), &resource.animationCount);
        if (resource.animations && (resource.animationCount == 0 || resource.animations[0].frameCount == 0 ||
            !IsModelAnimationValid(resource.model, resource.animations[0]))) {
            ::UnloadModelAnimations(resource.animations, resource.animationCount);
            resource.animations = nullptr;
            resource.animationCount = 0;
        }
    }
    models_[key] = resource;
}

// キーでモデルを取得（どのクラスからでも呼べる）
Model ResourceManager::GetModel(const std::string& key) const {
    auto it = models_.find(key);
    if (it != models_.end()) {
        return it->second.model;
    }
    return Model{}; // 見つからない場合は空のモデル
}

BoundingBox ResourceManager::GetModelBounds(const std::string& key) const {
    const auto it = models_.find(key);
    return it != models_.end() ? it->second.bounds : BoundingBox{};
}

ModelAnimation ResourceManager::GetModelAnimation(const std::string& key) const {
    const auto it = models_.find(key);
    if (it == models_.end()) return {};
    const ModelResource& resource = it->second;
    return resource.animations && resource.animationCount > 0 ? resource.animations[0] : ModelAnimation{};
}

void ResourceManager::UnloadAll() {
    // UnloadModelは画像を解放しない。共有IDの重複解放と既定テクスチャの解放を避ける。
    std::unordered_set<unsigned int> textures;
    for (auto& pair : models_) {
        ModelResource& resource = pair.second;
        for (int material = 0; material < resource.model.materialCount; ++material) {
            for (int map = MATERIAL_MAP_ALBEDO; map <= MATERIAL_MAP_BRDF; ++map) {
                const Texture2D texture = resource.model.materials[material].maps[map].texture;
                if (texture.id != 0 && texture.id != rlGetTextureIdDefault() && textures.insert(texture.id).second) {
                    ::UnloadTexture(texture);
                }
            }
        }
        if (resource.animations) ::UnloadModelAnimations(resource.animations, resource.animationCount);
        ::UnloadModel(resource.model);
    }
    models_.clear();
}
