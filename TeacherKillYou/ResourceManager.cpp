#include "ResourceManager.h"
#include "raylib.h"

ResourceManager& ResourceManager::GetInstance() {
    static ResourceManager instance;
    return instance;
}

// ゲーム全体で使うモデルをキーと一緒に登録しておく
void ResourceManager::LoadAll() {
    LoadModel(ResourceKeys::Model_Player, "Data/Image/greenman.glb");

	LoadModel(ResourceKeys::Model_Stage1, "Data/Image/free_loft_18_mini_office_v.optimization.glb");

    LoadModel(ResourceKeys::Model_Paladin, "Data/Image/BrainStem.glb");

    // アニメーションのロードも行う
    LoadModelAnimations(ResourceKeys::Model_Paladin, "Data/Image/BrainStem.glb");

    // 他のモデルが増えたらここに追加
    // LoadModel(ResourceKeys::Model_Stage, "Data/Image/stage.glb");
}
 
void ResourceManager::LoadModel(const std::string& key, const std::string& path) {
    // すでに登録済みならロードしない（二重ロード防止）
    if (models_.find(key) != models_.end()) {
        return;
    }

    Model model = ::LoadModel(path.c_str());
    models_[key] = model;
}

// キーでモデルを取得（どのクラスからでも呼べる）
Model ResourceManager::GetModel(const std::string& key) const {
    auto it = models_.find(key);
    if (it != models_.end()) {
        return it->second;
    }
    return Model{}; // 見つからない場合は空のモデル
}

Model& ResourceManager::GetModelRef(const std::string& key)
{
    return models_[key];
}

void ResourceManager::LoadModelAnimations(const std::string& key, const std::string& path)
{
    if (animations_.find(key) != animations_.end()) return;

    int animCount = 0;
    ModelAnimation* anims = ::LoadModelAnimations(path.c_str(), &animCount);

    if (anims != nullptr && animCount > 0) {
        animations_[key] = { anims, animCount };
    }
}

ModelAnimation* ResourceManager::GetModelAnimations(const std::string& key, int* count) const
{
    auto it = animations_.find(key);
    if (it != animations_.end()) {
        if (count) *count = it->second.count;
        return it->second.anims;
    }
    if (count) *count = 0;
    return nullptr;
}

void ResourceManager::UnloadAll() {
    for (auto& pair : models_) {
        ::UnloadModel(pair.second);
    }
    models_.clear();

    for (auto& pair : animations_) {
        if (pair.second.anims != nullptr) {
            ::UnloadModelAnimations(pair.second.anims, pair.second.count);
        }
    }
    animations_.clear();
}