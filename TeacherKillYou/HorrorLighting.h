#pragma once
#include "raylib.h"
#include <vector>

// ResourceManager owns the returned model shader; FBX supplies its skinning vertex shader.
Shader LoadHorrorShader(const char* vertexCode = nullptr, bool normalMaps = false);

// Set material uniforms before every mesh draw; shared shaders must not retain another actor's flash.
void DrawHorrorMesh(Mesh mesh, Material material, Matrix transform, Color tint = WHITE, float damageFlash = 0.0f);
void DrawHorrorModelEx(Model model, Vector3 position, Vector3 axis, float degrees, Vector3 scale,
                       Color tint = WHITE, float damageFlash = 0.0f);
void DrawHorrorModel(Model model, Vector3 position, float scale, Color tint = WHITE);
void UnloadHorrorShader(Shader shader);

// Model shaders are borrowed. This class owns only the scene buffer and post effect.
class HorrorLighting {
public:
    HorrorLighting() = default;
    HorrorLighting(const HorrorLighting&) = delete;
    HorrorLighting& operator=(const HorrorLighting&) = delete;
    void Init(const std::vector<Shader>& shaders, const Model& building = {});
    void Reset() { flashlightOn_ = true; }
    void Update();
    void BeginFrame(const Camera3D& view, const Camera3D& flashlight, Vector3 stagePosition = {}, float stageScale = 1.0f);
    int GetBuildingLightCount() const { return static_cast<int>(buildingLights_.size()); }
    void EndFrame(bool drawControls) const;
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
    Shader post_{};
    int timeLocation_ = -1;
    int aspectLocation_ = -1;
    int texelLocation_ = -1;
    bool flashlightOn_ = true;
};
