#include "ResourceManager.h"
#include "BakedAnimation.h"
#include "FbxLoader.h"
#include "GltfMaterials.h"
#include "HorrorLighting.h"
#include "raylib.h"
#include "rlgl.h"

#include <unordered_set>

ResourceManager& ResourceManager::GetInstance() {
    static ResourceManager instance;
    return instance;
}

// ゲーム全体で使うモデルをキーと一緒に登録しておく
void ResourceManager::LoadAll() {
    LoadModel(ResourceKeys::Model_HandGunView, "Data/Image/HandPov/handgun.glb");

	LoadModel(ResourceKeys::Model_Stage1, "Data/Image/floor 4.glb");

    LoadModel(ResourceKeys::Model_Paladin, "Data/Image/BrainStem.glb");

    // アニメーションのロードも行う
    LoadModelAnimations(ResourceKeys::Model_Paladin, "Data/Image/BrainStem.glb");

    LoadModel(ResourceKeys::Model_Enemy, "Data/Image/MONSTER   run.fbx");

	LoadModel(ResourceKeys::Model_Door, "Data/Image/slide door.glb");

}
 
void ResourceManager::LoadModel(const std::string& key, const std::string& path) {
    // すでに登録済みならロードしない（二重ロード防止）
    if (models_.find(key) != models_.end()) {
        return;
    }

    if (IsFileExtension(path.c_str(), ".fbx")) {
        FbxModelData data = LoadFbxModel(path);
        if (data.model.meshCount == 0) return;
        models_[key] = data.model;
        modelBounds_[key] = GetModelBoundingBox(data.model);
        if (data.animationCount > 0) {
            animations_[key] = { data.animations, data.animationCount, std::move(data.bakedFrames) };
        }
        return;
    }
    Model model = ::LoadModel(path.c_str());
    ApplyGltfTextureCoordinates(model, path);
    if (model.meshCount > 0) {
        if (!modelShader_.id) modelShader_ = LoadHorrorShader();
        if (modelShader_.id) {
            for (int i = 0; i < model.materialCount; ++i) model.materials[i].shader = modelShader_;
        }
    }
    models_[key] = model;
    if (model.meshCount > 0) modelBounds_[key] = GetModelBoundingBox(model);
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

BoundingBox ResourceManager::GetModelBounds(const std::string& key) const
{
    const auto it = modelBounds_.find(key);
    return it != modelBounds_.end() ? it->second : BoundingBox{};
}

ModelAnimation ResourceManager::GetModelAnimation(const std::string& key) const
{
    int count = 0;
    ModelAnimation* animations = GetModelAnimations(key, &count);
    if (!animations || count == 0 || animations[0].frameCount == 0) return {};
    const Model model = GetModel(key);
    return IsModelAnimationValid(model, animations[0]) ? animations[0] : ModelAnimation{};
}

void ResourceManager::LoadModelAnimations(const std::string& key, const std::string& path)
{
    if (animations_.find(key) != animations_.end()) return;

    if (IsFileExtension(path.c_str(), ".fbx")) {
        // LoadModelと同じ一回の解析で骨格・全クリップも取り込む。
        LoadModel(key, path);
        return;
    }
    int animCount = 0;
    ModelAnimation* anims = ::LoadModelAnimations(path.c_str(), &animCount);

    if (anims != nullptr && animCount > 0) {
        animations_[key] = { anims, animCount, {} };
    }
}

void ResourceManager::ApplyModelAnimation(const std::string& key, int frame, int animationIndex) {
    const auto model = models_.find(key);
    const auto animation = animations_.find(key);
    if (model == models_.end() || animation == animations_.end()) return;
    const AnimationData& data = animation->second;
    if (animationIndex < 0 || animationIndex >= data.count ||
        !IsModelAnimationValid(model->second, data.anims[animationIndex])) return;
    if (animationIndex < static_cast<int>(data.bakedFrames.size()) && !data.bakedFrames[animationIndex].empty()) {
        ApplyBakedAnimation(model->second, data.bakedFrames[animationIndex], frame);
    } else {
        ::UpdateModelAnimation(model->second, data.anims[animationIndex], frame);
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

std::vector<Shader> ResourceManager::GetModelShaders() const {
    std::vector<Shader> result;
    std::unordered_set<unsigned int> ids;
    for (const auto& pair : models_) {
        for (int i = 0; i < pair.second.materialCount; ++i) {
            const Shader shader = pair.second.materials[i].shader;
            if (shader.id && shader.id != rlGetShaderIdDefault() && ids.insert(shader.id).second) result.push_back(shader);
        }
    }
    return result;
}

void ResourceManager::UnloadAll() {
    // UnloadModelは画像を解放しない。共有IDの重複解放と既定テクスチャの解放を避ける。
    std::unordered_set<unsigned int> textures, shaders;
    for (auto& pair : models_) {
        const Model& model = pair.second;
        for (int material = 0; material < model.materialCount; ++material) {
            if (model.materials[material].shader.id != 0 && model.materials[material].shader.id != rlGetShaderIdDefault() &&
                shaders.insert(model.materials[material].shader.id).second) {
                UnloadShader(model.materials[material].shader);
            }
            for (int map = MATERIAL_MAP_ALBEDO; map <= MATERIAL_MAP_BRDF; ++map) {
                const Texture2D texture = model.materials[material].maps[map].texture;
                if (texture.id != 0 && texture.id != rlGetTextureIdDefault() && textures.insert(texture.id).second) {
                    ::UnloadTexture(texture);
                }
            }
        }
        ::UnloadModel(pair.second);
    }
    models_.clear();
    modelShader_ = {};
    modelBounds_.clear();

    for (auto& pair : animations_) {
        if (pair.second.anims != nullptr) {
            ::UnloadModelAnimations(pair.second.anims, pair.second.count);
        }
    }
    animations_.clear();
}
