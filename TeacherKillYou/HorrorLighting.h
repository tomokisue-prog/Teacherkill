#pragma once
#include "raylib.h"
#include <vector>

// ResourceManager owns the returned model shader; FBX supplies its skinning vertex shader.
Shader LoadHorrorShader(const char* vertexCode = nullptr, bool normalMaps = false);

// Model shaders are borrowed. This class owns only the scene buffer and post effect.
class HorrorLighting {
public:
    HorrorLighting() = default;
    HorrorLighting(const HorrorLighting&) = delete;
    HorrorLighting& operator=(const HorrorLighting&) = delete;
    void Init(const std::vector<Shader>& shaders);
    void Reset() { flashlightOn_ = true; }
    void Update();
    void BeginFrame(const Camera3D& view, const Camera3D& flashlight);
    void EndFrame(bool drawControls) const;
    void Shutdown();
    bool IsFlashlightOn() const { return flashlightOn_; }
    void SetFlashlightOn(bool enabled) { flashlightOn_ = enabled; }

private:
    struct ModelLight {
        Shader shader;
        int viewPosition, lightPosition, lightDirection, enabled;
    };
    std::vector<ModelLight> modelLights_;
    RenderTexture2D scene_{};
    Shader post_{};
    int timeLocation_ = -1;
    int aspectLocation_ = -1;
    bool flashlightOn_ = true;
};
