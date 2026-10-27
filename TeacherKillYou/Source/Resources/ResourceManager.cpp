#include "ResourceManager.h"
#include "BakedAnimation.h"
#include "FbxLoader.h"
#include "GltfMaterials.h"
#include "HorrorLighting.h"
#include "raylib.h"
#include "rlgl.h"

#include <unordered_set>
#include <chrono>

ResourceManager& ResourceManager::GetInstance() {
    static ResourceManager instance;
    return instance;
}

namespace {
    enum class LoadKind { Model, Animation, FbxAnimation, Texture };
    struct LoadJob {
        LoadKind kind;
        const char* key;
        const char* path;
        const char* modelKey = nullptr;
    };

    // Keep model imports before the clips that reuse their mesh and rig.
    const LoadJob GameResources[] = {
        {LoadKind::Model, ResourceKeys::Model_HandGunView, "Data/Image/HandPov/idle_static (2).glb"},
        {LoadKind::Model, "PlayerShoot", "Data/Image/HandPov/shoot_raylib.glb"},
        {LoadKind::Animation, "PlayerShoot", "Data/Image/HandPov/shoot_raylib.glb"},
        {LoadKind::Model, "PlayerReload", "Data/Image/HandPov/reload_raylib.glb"},
        {LoadKind::Animation, "PlayerReload", "Data/Image/HandPov/reload_raylib.glb"},
        {LoadKind::Model, ResourceKeys::Model_Stage1, "Data/Image/floor 4 uv warp all.glb"},
        {LoadKind::Model, ResourceKeys::Model_Enemy, "Data/Image/Monster_final_animations/MONSTER_run_final.fbx"},
        {LoadKind::FbxAnimation, ResourceKeys::Animation_EnemyAttack, "Data/Image/Monster_final_animations/MONSTER _attack_final.fbx", ResourceKeys::Model_Enemy},
        {LoadKind::FbxAnimation, ResourceKeys::Animation_EnemyDeath, "Data/Image/Monster_final_animations/MONSTER_dead_final.fbx", ResourceKeys::Model_Enemy},
        {LoadKind::Model, ResourceKeys::Model_FemaleZombie, "Data/Image/ZOMBIE_FEMALE_ANIMATIONS/zombie 1 running.fbx"},
        {LoadKind::FbxAnimation, ResourceKeys::Animation_FemaleZombieAttack, "Data/Image/ZOMBIE_FEMALE_ANIMATIONS/zombie 1 attack.fbx", ResourceKeys::Model_FemaleZombie},
        {LoadKind::FbxAnimation, ResourceKeys::Animation_FemaleZombieDeath, "Data/Image/ZOMBIE_FEMALE_ANIMATIONS/zombie 1 death.fbx", ResourceKeys::Model_FemaleZombie},
        {LoadKind::FbxAnimation, ResourceKeys::Animation_FemaleZombieHit, "Data/Image/ZOMBIE_FEMALE_ANIMATIONS/zombie 1 hit.fbx", ResourceKeys::Model_FemaleZombie},
        {LoadKind::Model, ResourceKeys::Model_Door, "Data/Image/slide door.glb"},
		{LoadKind::Texture, ResourceKeys::WASD_UI, "Data/Image/WASDUI.png"},

        // アイテムモデルのロード
        {LoadKind::Model, ResourceKeys::Model_KeyCard, "Data/Image/Items/Key_Card.glb"},
        {LoadKind::Model, ResourceKeys::Model_Medkit,  "Data/Image/Items/Med_Kit.glb"},
        {LoadKind::Model, ResourceKeys::Model_Ammo,    "Data/Image/Items/Ammo.glb"},
        {LoadKind::Model, ResourceKeys::Model_LostItem,    "Data/Image/Items/Key_Card.glb"},

        // アイテムアイコン・UIのロード
        {LoadKind::Texture, ResourceKeys::Icon_KeyCard,    "Data/Image/Items/Key_Card.png"},
        {LoadKind::Texture, ResourceKeys::Icon_Medkit,     "Data/Image/Items/Med_Kit.png"},
        {LoadKind::Texture, ResourceKeys::Icon_HealingBar, "Data/Image/Items/Healing_bar.png"},
        {LoadKind::Texture, ResourceKeys::Icon_LostItem, "Data/Image/Items/Key_Card.png"},
    };
    constexpr std::size_t GameResourceCount = sizeof(GameResources) / sizeof(GameResources[0]);
}

// Compatibility entry point for tools that need every resource immediately.
void ResourceManager::LoadAll() {
    LoadJapaneseFont();
    while (!LoadNextGameResource()) {}
}

bool ResourceManager::LoadNextGameResource() {
    if (nextGameResource_ >= GameResourceCount) return true;
    const auto& job = GameResources[nextGameResource_];
    const auto begin = std::chrono::steady_clock::now();
    switch (job.kind) {
    case LoadKind::Model: LoadModel(job.key, job.path); break;
    case LoadKind::Animation: LoadModelAnimations(job.key, job.path); break;
    case LoadKind::FbxAnimation: LoadFbxAnimation(job.key, job.modelKey, job.path); break;
	case LoadKind::Texture: LoadTexture(job.key, job.path); break;
    }
    ++nextGameResource_;
    const double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - begin).count();
    TraceLog(LOG_INFO, "LOADING: [%s] %.3f seconds", job.path, elapsed);
    return nextGameResource_ == GameResourceCount;
}

float ResourceManager::GetGameLoadProgress() const {
    return static_cast<float>(nextGameResource_) / GameResourceCount;
}

void ResourceManager::LoadJapaneseFont()
{
    if (japaneseFont_.IsLoaded()) return;
    japaneseFont_.Load("Data/Font/NotoSansJP-Regular.ttf", 32);

    if (japaneseFont_.IsLoaded()) {
        TraceLog(LOG_INFO, u8"SUCCESS: 日本語フォントがロードされました");
    }
    else {
        TraceLog(LOG_ERROR, u8"FAILED: 日本語フォントのロードに失敗しました");
    }
}
 
void ResourceManager::LoadModel(const std::string& key, const std::string& path) {
    // すでに登録済みならロードしない（二重ロード防止）
    if (models_.find(key) != models_.end()) {
        return;
    }

    if (IsFileExtension(path.c_str(), ".fbx")) {
        const bool enemyModel = key == ResourceKeys::Model_Enemy || key == ResourceKeys::Model_FemaleZombie;
        FbxModelData data = LoadFbxModel(path, enemyModel);
        if (data.model.meshCount == 0) return;
        if (enemyModel) {
            // 敵は自己発光させず、ステージ照明と懐中電灯で照らす。
            for (int i = 0; i < data.model.materialCount; ++i) {
                auto& material = data.model.materials[i];
                material.maps[MATERIAL_MAP_EMISSION].value = 0.0f;
                material.params[2] = 0.0f;
            }
        }
        models_[key] = data.model;
        modelBounds_[key] = !data.bakedFrames.empty() ?
            GetBakedModelBounds(data.model, data.bakedFrames[0]) : GetModelBoundingBox(data.model);
        fbxBindings_[key] = std::move(data.bindings);
        if (data.animationCount > 0) {
            animations_[key] = { data.animations, data.animationCount, std::move(data.bakedFrames) };
        }
        return;
    }
    Model model = ::LoadModel(path.c_str());
    if (key == ResourceKeys::Model_Stage1) {
        // この素材は天井の模様。名前や発光データがあっても照明として扱わない。
        ApplyGltfTextureCoordinates(model, path, { "floor 1 light" });
    } else {
        ApplyGltfTextureCoordinates(model, path);
    }
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
    const auto& owner = animations_.at(key).modelKey;
    const Model model = GetModel(owner.empty() ? key : owner);
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

void ResourceManager::LoadFbxAnimation(const std::string& key, const std::string& modelKey, const std::string& path) {
    if (animations_.find(key) != animations_.end()) return;
    const auto model = models_.find(modelKey);
    const auto bindings = fbxBindings_.find(modelKey);
    if (model == models_.end() || bindings == fbxBindings_.end()) return;
    FbxAnimationData data = LoadFbxAnimations(path, model->second, bindings->second);
    if (data.animationCount > 0)
        animations_[key] = { data.animations, data.animationCount, std::move(data.bakedFrames), modelKey };
}

void ResourceManager::ApplyModelAnimation(const std::string& key, int frame, int animationIndex) {
    const auto animation = animations_.find(key);
    if (animation == animations_.end()) return;
    const AnimationData& data = animation->second;
    const auto model = models_.find(data.modelKey.empty() ? key : data.modelKey);
    if (model == models_.end()) return;
    if (animationIndex < 0 || animationIndex >= data.count ||
        !IsModelAnimationValid(model->second, data.anims[animationIndex])) return;
    if (animationIndex < static_cast<int>(data.bakedFrames.size()) && !data.bakedFrames[animationIndex].empty()) {
        ApplyBakedAnimation(model->second, data.bakedFrames[animationIndex], frame);
    } else {
        ::UpdateModelAnimation(model->second, data.anims[animationIndex], frame);
    }
}

void ResourceManager::LoadTexture(const std::string& key, const std::string& path)
{
    if (textures_.find(key) != textures_.end()) {
        return;
	}

    Texture2D texture = ::LoadTexture(path.c_str());
    if (texture.id != 0) {
        textures_[key] = texture;
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
                UnloadHorrorShader(model.materials[material].shader);
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
    fbxBindings_.clear();
    japaneseFont_.Unload();
    nextGameResource_ = 0;
}
