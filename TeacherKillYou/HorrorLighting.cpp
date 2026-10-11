#include "HorrorLighting.h"
#include "raymath.h"
#include "rlgl.h"
#include <algorithm>
#include <cmath>
#include <array>
#include <unordered_map>

namespace {
constexpr Color NightBackground{ 2, 4, 4, 255 };
// Cold, slightly green fluorescent lamps; a warmer torch keeps the player's focus readable.
constexpr Vector3 FluorescentTint{ 0.46f, 0.72f, 0.60f };
constexpr Vector3 FlashlightTint{ 1.0f, 0.95f, 0.82f };
constexpr float FixtureIntensity = 0.55f;

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

// Material roughness/metalness are evaluated per fragment. Light highlights move with the view.
const char* ModelFragment = R"(#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 fragPosition;
in vec3 fragNormal;
uniform sampler2D texture0;
uniform sampler2D texture2;
uniform sampler2D texture3;
uniform sampler2D texture5;
uniform vec4 colDiffuse;
uniform vec3 viewPosition;
uniform vec3 lightPosition;
uniform vec3 lightDirection;
uniform float flashlightEnabled;
uniform vec3 fluorescentTint;
uniform vec3 flashlightTint;
uniform vec4 surface; // roughness, metalness, normal map mode, packed MR enabled
uniform vec4 emission;
uniform float emissionTextureEnabled;
uniform float damageFlash;
const int MAX_LIGHTS = 8;
uniform int buildingLightCount;
uniform vec3 buildingPositions[MAX_LIGHTS];
uniform vec3 buildingDirections[MAX_LIGHTS];
uniform vec4 buildingColors[MAX_LIGHTS];
out vec4 finalColor;
const float PI = 3.14159265;
vec3 reflectedLight(vec3 base, vec3 n, vec3 v, vec3 l, vec3 radiance, float roughness, float metalness) {
    float nl = max(dot(n, l), 0.0), nv = max(dot(n, v), 0.001);
    if (nl <= 0.0) return vec3(0.0);
    vec3 h = normalize(l + v);
    float nh = max(dot(n, h), 0.0), vh = max(dot(v, h), 0.0);
    vec3 f0 = mix(vec3(0.04), base, metalness);
    vec3 fresnel = f0 + (1.0 - f0) * pow(1.0 - vh, 5.0);
    float a = roughness * roughness, a2 = a * a;
    float d = nh * nh * (a2 - 1.0) + 1.0;
    float distribution = a2 / max(PI * d * d, 0.00001);
    float k = (roughness + 1.0) * (roughness + 1.0) / 8.0;
    float geometry = (nv / (nv * (1.0 - k) + k)) * (nl / (nl * (1.0 - k) + k));
    vec3 specular = distribution * geometry * fresnel / max(4.0 * nv * nl, 0.001);
    vec3 diffuse = (1.0 - fresnel) * (1.0 - metalness) * base / PI;
    return (diffuse + specular) * radiance * nl;
}
void main() {
    vec4 albedo = texture(texture0, fragTexCoord) * colDiffuse * fragColor;
    if (albedo.a < 0.01) discard;
    vec3 normal = normalize(fragNormal);
    if (!gl_FrontFacing) normal = -normal;
    if (surface.z > 0.5) {
        vec3 dp1 = dFdx(fragPosition), dp2 = dFdy(fragPosition);
        vec2 uv1 = dFdx(fragTexCoord), uv2 = dFdy(fragTexCoord);
        vec3 tangent = cross(dp2, normal) * uv1.x + cross(normal, dp1) * uv2.x;
        vec3 bitangent = cross(dp2, normal) * uv1.y + cross(normal, dp1) * uv2.y;
        float length2 = max(dot(tangent, tangent), dot(bitangent, bitangent));
        vec3 mapped = texture(texture2, fragTexCoord).rgb * 2.0 - 1.0;
        if (surface.z > 1.5) mapped.y = -mapped.y;
        if (length2 > 1e-12) normal = normalize(mat3(tangent, bitangent, normal * sqrt(length2)) * mapped);
    }
    float roughness = surface.x, metalness = surface.y;
    if (surface.w > 0.5) {
        vec3 packedMR = texture(texture3, fragTexCoord).rgb;
        roughness *= packedMR.g;
        metalness *= packedMR.b;
    }
    roughness = clamp(roughness, 0.08, 1.0);
    metalness = clamp(metalness, 0.0, 1.0);
    vec3 base = pow(max(albedo.rgb, vec3(0.0)), vec3(2.2));
    vec3 view = normalize(viewPosition - fragPosition);
    vec3 toLight = lightPosition - fragPosition;
    float distanceToLight = max(length(toLight), 0.001);
    vec3 lightVector = toLight / distanceToLight;
    float alignment = dot(-lightVector, lightDirection);
    float cone = smoothstep(cos(radians(22.0)), cos(radians(12.0)), alignment);
    float spill = 0.015 * smoothstep(cos(radians(34.0)), cos(radians(22.0)), alignment);
    float attenuation = 1.0 / (1.0 + 0.12 * distanceToLight + 0.06 * distanceToLight * distanceToLight);
    float beam = (cone + spill) * attenuation * (1.0 - smoothstep(12.0, 20.0, distanceToLight)) * flashlightEnabled;
    float distanceToView = length(viewPosition - fragPosition);
    float nearFill = 0.06 * (1.0 - smoothstep(0.15, 0.9, distanceToView)) * flashlightEnabled;
    vec3 color = base * (vec3(0.003, 0.006, 0.005) + flashlightTint * (0.16 * beam + nearFill));
    color += reflectedLight(base, normal, view, lightVector, flashlightTint * 7.5 * beam, roughness, metalness);
    for (int i = 0; i < buildingLightCount; ++i) {
        vec3 delta = buildingPositions[i] - fragPosition;
        float distance = max(length(delta), 0.001);
        vec3 l = delta / distance;
        // Fixtures emit into their room, with a smooth six metre falloff.
        float facing = max(dot(-l, buildingDirections[i]), 0.0);
        float falloff = (1.0 - smoothstep(3.0, 6.0, distance)) / (1.0 + 0.3 * distance + 0.8 * distance * distance);
        vec3 radiance = buildingColors[i].rgb * buildingColors[i].a * facing * falloff;
        color += reflectedLight(base, normal, view, l, radiance, roughness, metalness);
    }
    // White ceiling panels follow the fluorescent palette; coloured signs retain their hue.
    float neutralEmission = min(emission.r, min(emission.g, emission.b)) / max(max(emission.r, max(emission.g, emission.b)), 0.001);
    vec3 emitted = emission.rgb * emission.a * mix(vec3(1.0), fluorescentTint * 0.65, neutralEmission);
    if (emissionTextureEnabled > 0.5) emitted *= pow(texture(texture5, fragTexCoord).rgb, vec3(2.2));
    color += emitted;
    float fog = 1.0 - exp(-0.006 * distanceToView * distanceToView);
    color = mix(color, vec3(0.0015, 0.0035, 0.0030), fog);
    color = color / (vec3(1.0) + color);
    color = pow(color, vec3(1.0 / 2.2));
    // A hit remains visible even where the flashlight does not reach.
    color = mix(color, vec3(0.95, 0.025, 0.04), clamp(damageFlash, 0.0, 1.0) * 0.85);
    finalColor = vec4(color, albedo.a);
})";

struct MaterialLocations { int surface, emission, emissionTexture, flash; };
std::unordered_map<unsigned int, MaterialLocations> materialLocations;
constexpr int MaxBuildingLights = 8;

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
    float vignette = 1.0 - 0.68 * smoothstep(0.25, 1.5, dot(edge, edge));
    float luma = dot(color, vec3(0.2126, 0.7152, 0.0722));
    // Grade only the scene; HUD and damage overlay are drawn afterwards.
    vec3 shadowTint = mix(vec3(0.72, 0.91, 0.82), vec3(1.0), smoothstep(0.04, 0.65, luma));
    color = mix(vec3(luma), color, 0.74) * shadowTint;
    color = max((color * 0.90 - vec3(0.035)) * 1.06, vec3(0.0)) * vignette;
    float grain = fract(sin(dot(gl_FragCoord.xy + floor(time * 24.0), vec2(12.9898, 78.233))) * 43758.5453) - 0.5;
    finalColor = vec4(max(color + grain * 0.012, vec3(0.0)), 1.0) * colDiffuse * fragColor;
})";
}

Shader LoadHorrorShader(const char* vertexCode, bool normalMaps) {
    Shader shader = LoadShaderFromMemory(vertexCode ? vertexCode : ModelVertex, ModelFragment);
    if (!shader.id || !shader.locs || shader.id == rlGetShaderIdDefault()) return {};
    shader.locs[SHADER_LOC_MATRIX_MODEL] = GetShaderLocation(shader, "matModel");
    shader.locs[SHADER_LOC_MATRIX_NORMAL] = GetShaderLocation(shader, "matNormal");
    shader.locs[SHADER_LOC_MAP_NORMAL] = GetShaderLocation(shader, "texture2");
    shader.locs[SHADER_LOC_MAP_ROUGHNESS] = GetShaderLocation(shader, "texture3");
    shader.locs[SHADER_LOC_MAP_EMISSION] = GetShaderLocation(shader, "texture5");
    const MaterialLocations locations = {GetShaderLocation(shader, "surface"), GetShaderLocation(shader, "emission"),
        GetShaderLocation(shader, "emissionTextureEnabled"), GetShaderLocation(shader, "damageFlash")};
    materialLocations[shader.id] = locations;
    SetShaderValue(shader, GetShaderLocation(shader, "fluorescentTint"), &FluorescentTint, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader, GetShaderLocation(shader, "flashlightTint"), &FlashlightTint, SHADER_UNIFORM_VEC3);
    const Vector4 defaults = {0.8f, 0.0f, normalMaps ? 2.0f : 0.0f, 0.0f};
    SetShaderValue(shader, locations.surface, &defaults, SHADER_UNIFORM_VEC4);
    return shader;
}

void UnloadHorrorShader(Shader shader) {
    materialLocations.erase(shader.id);
    UnloadShader(shader);
}

void DrawHorrorMesh(Mesh mesh, Material material, Matrix transform, Color tint, float damageFlash) {
    if (!material.maps) return;
    // Borrow textures while copying map values, so an actor's tint cannot alter a shared material.
    std::array<MaterialMap, 12> maps; // raylib 5.5 MAX_MATERIAL_MAPS includes one reserved slot.
    std::copy_n(material.maps, maps.size(), maps.begin());
    material.maps = maps.data();
    Color& color = material.maps[MATERIAL_MAP_ALBEDO].color;
    color = {static_cast<unsigned char>(color.r * tint.r / 255), static_cast<unsigned char>(color.g * tint.g / 255),
             static_cast<unsigned char>(color.b * tint.b / 255), static_cast<unsigned char>(color.a * tint.a / 255)};
    const auto found = materialLocations.find(material.shader.id);
    if (found != materialLocations.end()) {
        const auto& locations = found->second;
        const Vector4 surface = {Clamp(material.maps[MATERIAL_MAP_ROUGHNESS].value, 0.04f, 1.0f),
            Clamp(material.maps[MATERIAL_MAP_METALNESS].value, 0.0f, 1.0f), material.params[0], material.params[1]};
        const auto& emission = material.maps[MATERIAL_MAP_EMISSION];
        const Vector4 emitted = {emission.color.r / 255.0f, emission.color.g / 255.0f,
                                emission.color.b / 255.0f, emission.value};
        SetShaderValue(material.shader, locations.surface, &surface, SHADER_UNIFORM_VEC4);
        SetShaderValue(material.shader, locations.emission, &emitted, SHADER_UNIFORM_VEC4);
        SetShaderValue(material.shader, locations.emissionTexture, &material.params[2], SHADER_UNIFORM_FLOAT);
        SetShaderValue(material.shader, locations.flash, &damageFlash, SHADER_UNIFORM_FLOAT);
    }
    DrawMesh(mesh, material, transform);
}

void DrawHorrorModelEx(Model model, Vector3 position, Vector3 axis, float degrees, Vector3 scale, Color tint, float flash) {
    const Matrix transform = MatrixMultiply(model.transform, MatrixMultiply(
        MatrixMultiply(MatrixScale(scale.x, scale.y, scale.z), MatrixRotate(axis, degrees * DEG2RAD)),
        MatrixTranslate(position.x, position.y, position.z)));
    for (int i = 0; i < model.meshCount; ++i)
        DrawHorrorMesh(model.meshes[i], model.materials[model.meshMaterial[i]], transform, tint, flash);
}

void DrawHorrorModel(Model model, Vector3 position, float scale, Color tint) {
    DrawHorrorModelEx(model, position, {0, 1, 0}, 0, {scale, scale, scale}, tint);
}

void HorrorLighting::CollectBuildingLights(const Model& model) {
    buildingLights_.clear();
    std::vector<float> weights;
    const Matrix normals = MatrixTranspose(MatrixInvert(model.transform));
    for (int m = 0; m < model.meshCount; ++m) {
        const Material& material = model.materials[model.meshMaterial[m]];
        const auto& emission = material.maps[MATERIAL_MAP_EMISSION];
        if (emission.value <= 0.01f) continue;
        const Mesh& mesh = model.meshes[m];
        if (!mesh.vertices || !mesh.normals) continue;
        Image image{};
        Color* pixels = nullptr;
        if (material.params[2] > 0.5f && emission.texture.id && mesh.texcoords) {
            image = LoadImageFromTexture(emission.texture);
            if (image.data) pixels = LoadImageColors(image);
        }
        for (int t = 0; t < mesh.triangleCount; ++t) {
            Vector3 positions[3], normal{};
            Vector2 uv[3]{};
            for (int c = 0; c < 3; ++c) {
                const int v = mesh.indices ? mesh.indices[t * 3 + c] : t * 3 + c;
                positions[c] = {mesh.vertices[v * 3], mesh.vertices[v * 3 + 1], mesh.vertices[v * 3 + 2]};
                normal = Vector3Add(normal, {mesh.normals[v * 3], mesh.normals[v * 3 + 1], mesh.normals[v * 3 + 2]});
                if (mesh.texcoords) uv[c] = {mesh.texcoords[v * 2], mesh.texcoords[v * 2 + 1]};
            }
            normal = Vector3Normalize({normals.m0 * normal.x + normals.m4 * normal.y + normals.m8 * normal.z,
                normals.m1 * normal.x + normals.m5 * normal.y + normals.m9 * normal.z,
                normals.m2 * normal.x + normals.m6 * normal.y + normals.m10 * normal.z});
            if (Vector3LengthSqr(normal) < 0.5f) continue;
            if (normal.y > 0.5f) normal = Vector3Negate(normal);
            const float area = Vector3Length(Vector3CrossProduct(Vector3Subtract(positions[1], positions[0]),
                Vector3Subtract(positions[2], positions[0]))) * 0.5f;
            if (area < 0.00001f) continue;
            // Sample inside triangles: the centre alone misses lamps embedded in a dark ceiling atlas.
            const int steps = pixels ? 16 : 1;
            for (int a = 0; a < steps; ++a) for (int b = 0; b < steps - a; ++b) {
                const float w0 = (a + 1.0f / 3.0f) / steps, w1 = (b + 1.0f / 3.0f) / steps;
                const float w2 = 1.0f - w0 - w1;
                float brightness = 1.0f;
                if (pixels) {
                    const Vector2 point = Vector2Add(Vector2Add(Vector2Scale(uv[0], w0), Vector2Scale(uv[1], w1)), Vector2Scale(uv[2], w2));
                    const int x = (std::min)(image.width - 1, static_cast<int>((point.x - std::floor(point.x)) * image.width));
                    const int y = (std::min)(image.height - 1, static_cast<int>((point.y - std::floor(point.y)) * image.height));
                    const Color sample = pixels[y * image.width + x];
                    brightness = (sample.r + sample.g + sample.b) / 765.0f;
                }
                if (brightness < 0.3f) continue;
                Vector3 center = Vector3Add(Vector3Add(Vector3Scale(positions[0], w0), Vector3Scale(positions[1], w1)), Vector3Scale(positions[2], w2));
                center = Vector3Add(Vector3Transform(center, model.transform), Vector3Scale(normal, 0.04f));
                const float weight = area * brightness / (steps * steps);
                const auto existing = std::find_if(buildingLights_.begin(), buildingLights_.end(), [&](const BuildingLight& light) {
                    return Vector3DotProduct(light.direction, normal) > 0.8f && Vector3Distance(light.position, center) < 1.2f;
                });
                if (existing != buildingLights_.end()) {
                    const auto index = static_cast<std::size_t>(existing - buildingLights_.begin());
                    existing->position = Vector3Lerp(existing->position, center, weight / (weights[index] + weight));
                    weights[index] += weight;
                } else if (buildingLights_.size() < 64) {
                    buildingLights_.push_back({center, normal, {emission.color.r / 255.0f * FluorescentTint.x,
                        emission.color.g / 255.0f * FluorescentTint.y, emission.color.b / 255.0f * FluorescentTint.z,
                        Clamp(emission.value * brightness * 0.6f, 0.3f, 5.0f) * FixtureIntensity}});
                    weights.push_back(weight);
                }
            }
        }
        if (pixels) UnloadImageColors(pixels);
        if (image.data) UnloadImage(image);
    }
    TraceLog(LOG_INFO, "LIGHTING: %i fixture lights extracted from building emission materials", GetBuildingLightCount());
}

void HorrorLighting::Init(const std::vector<Shader>& shaders, const Model& building) {
    CollectBuildingLights(building);
    modelLights_.clear();
    for (Shader shader : shaders) {
        const int enabled = GetShaderLocation(shader, "flashlightEnabled");
        if (enabled >= 0) modelLights_.push_back({shader,
            GetShaderLocation(shader, "viewPosition"), GetShaderLocation(shader, "lightPosition"),
            GetShaderLocation(shader, "lightDirection"), enabled,
            GetShaderLocation(shader, "buildingLightCount"), GetShaderLocation(shader, "buildingPositions[0]"),
            GetShaderLocation(shader, "buildingColors[0]"), GetShaderLocation(shader, "buildingDirections[0]")});
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

void HorrorLighting::BeginFrame(const Camera3D& view, const Camera3D& flashlight, Vector3 stagePosition, float stageScale) {
    const Vector3 direction = Vector3Normalize(Vector3Subtract(flashlight.target, flashlight.position));
    const Vector3 right = Vector3Normalize(Vector3CrossProduct(direction, flashlight.up));
    Vector3 position = Vector3Add(flashlight.position, Vector3Scale(direction, 0.12f));
    position = Vector3Add(position, Vector3Scale(right, 0.10f));
    position = Vector3Subtract(position, Vector3Scale(flashlight.up, 0.09f));
    const float enabled = flashlightOn_ ? 1.0f : 0.0f;
    std::vector<BuildingLight> nearest = buildingLights_;
    for (auto& light : nearest) light.position = Vector3Add(stagePosition, Vector3Scale(light.position, stageScale));
    std::sort(nearest.begin(), nearest.end(), [&](const BuildingLight& a, const BuildingLight& b) {
        return Vector3DistanceSqr(a.position, view.position) < Vector3DistanceSqr(b.position, view.position);
    });
    const int count = (std::min)(static_cast<int>(nearest.size()), MaxBuildingLights);
    std::array<Vector3, MaxBuildingLights> positions{}, directions{};
    std::array<Vector4, MaxBuildingLights> colors{};
    for (int i = 0; i < count; ++i) {
        positions[i] = nearest[i].position;
        directions[i] = stageScale < 0.0f ? Vector3Negate(nearest[i].direction) : nearest[i].direction;
        colors[i] = nearest[i].color;
    }
    for (const ModelLight& light : modelLights_) {
        SetShaderValue(light.shader, light.viewPosition, &view.position, SHADER_UNIFORM_VEC3);
        SetShaderValue(light.shader, light.lightPosition, &position, SHADER_UNIFORM_VEC3);
        SetShaderValue(light.shader, light.lightDirection, &direction, SHADER_UNIFORM_VEC3);
        SetShaderValue(light.shader, light.enabled, &enabled, SHADER_UNIFORM_FLOAT);
        SetShaderValue(light.shader, light.buildingCount, &count, SHADER_UNIFORM_INT);
        if (count > 0) {
            SetShaderValueV(light.shader, light.buildingPositions, positions.data(), SHADER_UNIFORM_VEC3, count);
            SetShaderValueV(light.shader, light.buildingColors, colors.data(), SHADER_UNIFORM_VEC4, count);
            SetShaderValueV(light.shader, light.buildingDirections, directions.data(), SHADER_UNIFORM_VEC3, count);
        }
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
    buildingLights_.clear();
}
