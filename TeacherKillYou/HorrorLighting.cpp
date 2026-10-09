#include "HorrorLighting.h"
#include "raymath.h"
#include "rlgl.h"
#include <algorithm>
#include <cmath>

namespace {
constexpr Color NightBackground{ 3, 5, 8, 255 };

const char* ModelVertex = R"(#version 330
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec3 vertexNormal;
in vec4 vertexColor;
uniform mat4 mvp;
uniform mat4 matModel;
uniform mat4 matNormal;
out vec2 fragTexCoord;
out vec4 fragColor;
out vec3 fragPosition;
out vec3 fragNormal;
void main() {
    fragTexCoord = vertexTexCoord;
    fragColor = vertexColor;
    fragPosition = (matModel * vec4(vertexPosition, 1.0)).xyz;
    fragNormal = mat3(matNormal) * vertexNormal;
    gl_Position = mvp * vec4(vertexPosition, 1.0);
})";

const char* ModelFragment = R"(#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 fragPosition;
in vec3 fragNormal;
uniform sampler2D texture0;
uniform sampler2D texture2;
uniform vec4 colDiffuse;
uniform vec3 viewPosition;
uniform vec3 lightPosition;
uniform vec3 lightDirection;
uniform float flashlightEnabled;
uniform float normalMapEnabled;
out vec4 finalColor;
void main() {
    vec4 albedo = texture(texture0, fragTexCoord) * colDiffuse * fragColor;
    if (albedo.a < 0.01) discard;
    vec3 normal = normalize(fragNormal);
    if (!gl_FrontFacing) normal = -normal;
    if (normalMapEnabled > 0.5) {
        vec3 dp1 = dFdx(fragPosition), dp2 = dFdy(fragPosition);
        vec2 uv1 = dFdx(fragTexCoord), uv2 = dFdy(fragTexCoord);
        vec3 tangent = cross(dp2, normal) * uv1.x + cross(normal, dp1) * uv2.x;
        vec3 bitangent = cross(dp2, normal) * uv1.y + cross(normal, dp1) * uv2.y;
        float length2 = max(dot(tangent, tangent), dot(bitangent, bitangent));
        vec3 mapped = texture(texture2, fragTexCoord).rgb * 2.0 - 1.0;
        mapped.y = -mapped.y;
        if (length2 > 1e-12) normal = normalize(mat3(tangent, bitangent, normal * sqrt(length2)) * mapped);
    }
    vec3 toLight = lightPosition - fragPosition;
    float distanceToLight = max(length(toLight), 0.001);
    vec3 lightVector = toLight / distanceToLight;
    float alignment = dot(-lightVector, lightDirection);
    // Soft 14/25 degree cone, plus faint spill around its edge.
    float cone = smoothstep(cos(radians(25.0)), cos(radians(14.0)), alignment);
    float spill = 0.035 * smoothstep(cos(radians(38.0)), cos(radians(25.0)), alignment);
    float attenuation = 1.0 / (1.0 + 0.12 * distanceToLight + 0.06 * distanceToLight * distanceToLight);
    float rangeFade = 1.0 - smoothstep(12.0, 20.0, distanceToLight);
    float beam = (cone + spill) * attenuation * rangeFade * flashlightEnabled;
    float diffuse = max(dot(normal, lightVector), 0.0);
    float distanceToView = length(viewPosition - fragPosition);
    float nearFill = 0.07 * (1.0 - smoothstep(0.15, 0.9, distanceToView)) * flashlightEnabled;
    vec3 illumination = vec3(0.007, 0.010, 0.016)
                      + vec3(1.0, 0.91, 0.74) * (2.8 * beam * (0.08 + 0.92 * diffuse) + nearFill);
    vec3 color = pow(max(albedo.rgb, vec3(0.0)), vec3(2.2)) * illumination;
    vec3 halfVector = normalize(lightVector + normalize(viewPosition - fragPosition));
    color += vec3(0.08, 0.071, 0.055) * pow(max(dot(normal, halfVector), 0.0), 36.0) * beam;
    // Dark distance haze keeps unlit corridors from looking like a bright skybox.
    float fog = 1.0 - exp(-0.0045 * distanceToView * distanceToView);
    color = mix(color, vec3(0.003, 0.005, 0.008), fog);
    color = color / (vec3(1.0) + color);
    finalColor = vec4(pow(color, vec3(1.0 / 2.2)), albedo.a);
})";

const char* PostFragment = R"(#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform float time;
uniform float aspect;
out vec4 finalColor;
void main() {
    vec3 color = texture(texture0, fragTexCoord).rgb;
    vec2 edge = fragTexCoord * 2.0 - 1.0;
    edge.x *= min(aspect, 1.8) / 1.8;
    float vignette = 1.0 - 0.60 * smoothstep(0.25, 1.5, dot(edge, edge));
    float luma = dot(color, vec3(0.2126, 0.7152, 0.0722));
    color = mix(vec3(luma), color, 0.86) * vignette;
    float grain = fract(sin(dot(gl_FragCoord.xy + floor(time * 24.0), vec2(12.9898, 78.233))) * 43758.5453) - 0.5;
    finalColor = vec4(max(color + grain * 0.009, vec3(0.0)), 1.0) * colDiffuse * fragColor;
})";
}

Shader LoadHorrorShader(const char* vertexCode, bool normalMaps) {
    Shader shader = LoadShaderFromMemory(vertexCode ? vertexCode : ModelVertex, ModelFragment);
    if (shader.id == rlGetShaderIdDefault()) return {};
    shader.locs[SHADER_LOC_MATRIX_MODEL] = GetShaderLocation(shader, "matModel");
    shader.locs[SHADER_LOC_MATRIX_NORMAL] = GetShaderLocation(shader, "matNormal");
    shader.locs[SHADER_LOC_MAP_NORMAL] = GetShaderLocation(shader, "texture2");
    const float normalEnabled = normalMaps ? 1.0f : 0.0f;
    SetShaderValue(shader, GetShaderLocation(shader, "normalMapEnabled"), &normalEnabled, SHADER_UNIFORM_FLOAT);
    return shader;
}

void HorrorLighting::Init(const std::vector<Shader>& shaders) {
    modelLights_.clear();
    for (Shader shader : shaders) {
        const int enabled = GetShaderLocation(shader, "flashlightEnabled");
        if (enabled >= 0) modelLights_.push_back({shader,
            GetShaderLocation(shader, "viewPosition"), GetShaderLocation(shader, "lightPosition"),
            GetShaderLocation(shader, "lightDirection"), enabled});
    }
    if (!post_.id) {
        post_ = LoadShaderFromMemory(nullptr, PostFragment);
        if (post_.id == rlGetShaderIdDefault()) post_ = {};
        if (post_.id) {
            timeLocation_ = GetShaderLocation(post_, "time");
            aspectLocation_ = GetShaderLocation(post_, "aspect");
        }
    }
    Reset();
}

void HorrorLighting::Update() {
    if (IsKeyPressed(KEY_F)) flashlightOn_ = !flashlightOn_;
}

void HorrorLighting::BeginFrame(const Camera3D& view, const Camera3D& flashlight) {
    const Vector3 direction = Vector3Normalize(Vector3Subtract(flashlight.target, flashlight.position));
    const Vector3 right = Vector3Normalize(Vector3CrossProduct(direction, flashlight.up));
    Vector3 position = Vector3Add(flashlight.position, Vector3Scale(direction, 0.12f));
    position = Vector3Add(position, Vector3Scale(right, 0.10f));
    position = Vector3Subtract(position, Vector3Scale(flashlight.up, 0.09f));
    const float enabled = flashlightOn_ ? 1.0f : 0.0f;
    for (const ModelLight& light : modelLights_) {
        SetShaderValue(light.shader, light.viewPosition, &view.position, SHADER_UNIFORM_VEC3);
        SetShaderValue(light.shader, light.lightPosition, &position, SHADER_UNIFORM_VEC3);
        SetShaderValue(light.shader, light.lightDirection, &direction, SHADER_UNIFORM_VEC3);
        SetShaderValue(light.shader, light.enabled, &enabled, SHADER_UNIFORM_FLOAT);
    }
    // Reallocate only on resize. Cap the buffer at 1080p to avoid adding 4K fill cost.
    const int screenWidth = (std::max)(GetScreenWidth(), 1);
    const int screenHeight = (std::max)(GetScreenHeight(), 1);
    const float scale = (std::min)(1.0f, (std::min)(1920.0f / screenWidth, 1080.0f / screenHeight));
    const int width = (std::max)(1, static_cast<int>(screenWidth * scale));
    const int height = (std::max)(1, static_cast<int>(screenHeight * scale));
    if (!scene_.id || scene_.texture.width != width || scene_.texture.height != height) {
        if (scene_.id) UnloadRenderTexture(scene_);
        scene_ = LoadRenderTexture(width, height);
        if (scene_.id) SetTextureFilter(scene_.texture, TEXTURE_FILTER_BILINEAR);
    }
    if (scene_.id) BeginTextureMode(scene_);
    ClearBackground(NightBackground);
}

void HorrorLighting::EndFrame(bool drawControls) const {
    if (scene_.id) {
        EndTextureMode();
        if (post_.id) {
            const float time = static_cast<float>(GetTime());
            const float aspect = static_cast<float>(scene_.texture.width) / scene_.texture.height;
            SetShaderValue(post_, timeLocation_, &time, SHADER_UNIFORM_FLOAT);
            SetShaderValue(post_, aspectLocation_, &aspect, SHADER_UNIFORM_FLOAT);
            BeginShaderMode(post_);
        }
        DrawTexturePro(scene_.texture, {0, 0, static_cast<float>(scene_.texture.width), -static_cast<float>(scene_.texture.height)},
            {0, 0, static_cast<float>(GetScreenWidth()), static_cast<float>(GetScreenHeight())}, {0, 0}, 0, WHITE);
        if (post_.id) EndShaderMode();
    }
    if (drawControls) DrawText(flashlightOn_ ? "[F] FLASHLIGHT ON" : "[F] FLASHLIGHT OFF",
        24, GetScreenHeight() - 36, 16, {135, 145, 150, 255});
}

void HorrorLighting::Shutdown() {
    if (scene_.id) UnloadRenderTexture(scene_);
    if (post_.id) UnloadShader(post_);
    scene_ = {};
    post_ = {};
    modelLights_.clear();
}
