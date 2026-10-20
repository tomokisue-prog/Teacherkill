#pragma once
#include "raylib.h"
#include <vector>

// モデル用シェーダーはResourceManagerが所有する。FBXではスキニング用頂点シェーダーを渡す。
Shader LoadHorrorShader(const char* vertexCode = nullptr, bool normalMaps = false);

// 毎メッシュで材質と照明補正を設定する。viewModelはFPSの銃・腕だけtrueにし、補助光と白飛び防止を使う。
void DrawHorrorMesh(Mesh mesh, Material material, Matrix transform, Color tint = WHITE,
                    float damageFlash = 0.0f, bool viewModel = false);
void DrawHorrorModelEx(Model model, Vector3 position, Vector3 axis, float degrees, Vector3 scale,
                       Color tint = WHITE, float damageFlash = 0.0f, bool viewModel = false);
void DrawHorrorModel(Model model, Vector3 position, float scale, Color tint = WHITE);
void UnloadHorrorShader(Shader shader);

// モデル用シェーダーは借用する。描画バッファ・照射マスク・画面効果はこのクラスが所有する。
class HorrorLighting {
public:
    HorrorLighting() = default;
    HorrorLighting(const HorrorLighting&) = delete;
    HorrorLighting& operator=(const HorrorLighting&) = delete;
    void Init(const std::vector<Shader>& shaders, const Model& building = {}); // 光源抽出は初期化時だけ行う。
    void Reset() { flashlightOn_ = true; }
    void Update(); // 毎フレーム1回、懐中電灯の入力とSEを更新する。
    // BeginFrame→3D描画→EndFrameの順に呼ぶ。viewは表示用、flashlightはプレイヤーの視線。
    void BeginFrame(const Camera3D& view, const Camera3D& flashlight, Vector3 stagePosition = {}, float stageScale = 1.0f);
    int GetBuildingLightCount() const { return static_cast<int>(buildingLights_.size()); }
    void EndFrame(bool drawControls) const; // 画面効果を適用。HUDや被弾表示はこの後に描く。
    void Shutdown();
    bool IsFlashlightOn() const { return flashlightOn_; }
    void SetFlashlightOn(bool enabled) { flashlightOn_ = enabled; }

private:
    struct ModelLight {
        Shader shader;
        int viewPosition, lightPosition, lightDirection, enabled;
        int buildingCount, buildingPositions, buildingColors, buildingDirections;
    };
    struct BuildingLight {
        Vector3 position, direction;
        Vector4 color;
    };
    void CollectBuildingLights(const Model& model);
    std::vector<BuildingLight> buildingLights_;
    std::vector<ModelLight> modelLights_;
    RenderTexture2D scene_{};
    Texture2D focus_{}; // 同じ3D描画でライトの照射量を記録する1チャンネルのテクスチャ。
    Shader post_{};
    int timeLocation_ = -1;
    int texelLocation_ = -1; // ぼかし半径を描画バッファの解像度に合わせるuniform。
    int focusLocation_ = -1;
    int focusEnabledLocation_ = -1;
    bool flashlightOn_ = true;
};
