#include "HorrorLighting.h"
#include "AudioManager.h"
#include "raymath.h"
#include "rlgl.h"
#include <algorithm>
#include <cmath>
#include <array>
#include <unordered_map>

namespace {
// 未描画の背景にも緑を残し、照射範囲の外を真っ黒なマスクにしない。
constexpr Color NightBackground{ 15, 26, 17, 255 };
// 懐中電灯は緑を強める。赤・青の成分も残し、単色の暗視映像にならないようにする。
// RGBは0～1の光量係数。FixtureIntensityは建物の光量をまとめて調整する。
constexpr Vector3 FluorescentTint{ 0.46f, 0.72f, 0.60f };
constexpr Vector3 FlashlightTint{ 0.32f, 0.82f, 0.38f };
constexpr float FixtureIntensity = 0.55f;
// 照射円の各断面に足す半径（ワールド単位）。0.0で追加分なし、正の値で照射範囲を広げる。
constexpr float FlashlightRadiusPadding = 0.0f;

// 色付きの光を同じ輝度の中性色へ寄せ、緑の調整で光量が大きく変わらないようにする。
Vector3 AdjustGreenTint(Vector3 tint, float strength) {
    const float luma = Vector3DotProduct(tint, {0.2126f, 0.7152f, 0.0722f});
    return Vector3Lerp({luma, luma, luma}, tint, strength);
}


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

// 材質の粗さ・金属度から反射を計算する。線形色で照明を足し、最後に表示用の色へ戻す。
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
uniform float flashlightRadiusPadding;
uniform float viewModelLighting;
uniform float editorLighting;
uniform float greenStrength;
uniform vec3 fluorescentTint;
uniform vec3 flashlightTint;
uniform vec4 surface; // 粗さ、金属度、法線マップの種類、MRテクスチャの使用フラグ
uniform vec4 emission;
uniform float emissionTextureEnabled;
uniform float damageFlash;
const int MAX_LIGHTS = 8;
uniform int buildingLightCount;
uniform vec3 buildingPositions[MAX_LIGHTS];
uniform vec3 buildingDirections[MAX_LIGHTS];
uniform vec4 buildingColors[MAX_LIGHTS];
layout(location = 0) out vec4 finalColor;
layout(location = 1) out vec4 focusColor;
const float PI = 3.14159265;
vec3 adjustGreenTint(vec3 tint) {
    return mix(vec3(dot(tint, vec3(0.2126, 0.7152, 0.0722))), tint, greenStrength);
}
// 面の法線・視線・光の向きから拡散反射と鏡面反射を求める。粗い面ほど反射が広がる。
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
    // 位置とUVの微分から接空間を作り、法線マップの凹凸を照明に反映する。
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
    // Neutral, even lighting for the editor; keep textures and skinned geometry.
    if (editorLighting > 0.5) {
        float diffuse = max(dot(normal, normalize(vec3(-0.35, 0.8, -0.45))), 0.0);
        vec3 base = pow(max(albedo.rgb, vec3(0.0)), vec3(2.2));
        finalColor = vec4(pow(base * (0.55 + 0.30 * diffuse), vec3(1.0 / 2.2)), albedo.a);
        focusColor = vec4(1.0, 0.0, 0.0, albedo.a);
        return;
    }
    float roughness = surface.x, metalness = surface.y;
    if (surface.w > 0.5) {
        vec3 packedMR = texture(texture3, fragTexCoord).rgb;
        roughness *= packedMR.g;
        metalness *= packedMR.b;
    }
    // 手元の銃は反射を少し広げ、反動で向きが変わっても鋭い白飛びを起こさないようにする。
    roughness = clamp(roughness, viewModelLighting > 0.5 ? 0.32 : 0.08, 1.0);
    metalness = clamp(metalness, 0.0, 1.0);
    vec3 base = pow(max(albedo.rgb, vec3(0.0)), vec3(2.2));
    vec3 view = normalize(viewPosition - fragPosition);
    vec3 toLight = lightPosition - fragPosition;
    float distanceToLight = max(length(toLight), 0.001);
    vec3 lightVector = toLight / distanceToLight;
    // 光源からの奥行きと横方向の距離で照射円を求める。背面には照射しない。
    vec3 fromLight = -toLight;
    float forwardDistance = dot(fromLight, lightDirection);
    float radialDistance = length(fromLight - lightDirection * forwardDistance);
    float padding = max(flashlightRadiusPadding, 0.0);
    float coneDistance = max(forwardDistance, 0.01);
    // 内側12度は明るく、外側22度まで滑らかに減衰。34度まで弱い漏れ光を加える。
    float innerRadius = coneDistance * tan(radians(12.0)) + padding;
    float outerRadius = coneDistance * tan(radians(22.0)) + padding;
    float spillRadius = coneDistance * tan(radians(34.0)) + padding;
    float cone = forwardDistance > 0.001 ? 1.0 - smoothstep(innerRadius, outerRadius, radialDistance) : 0.0;
    float spill = forwardDistance > 0.001 ? 0.015 * (1.0 - smoothstep(outerRadius, spillRadius, radialDistance)) : 0.0;
    // 距離減衰に加え、12～20ワールド単位で照射を消す。消灯時はbeamと手元の近距離光を0にする。
    float attenuation = 1.0 / (1.0 + 0.12 * distanceToLight + 0.06 * distanceToLight * distanceToLight);
    float beam = (cone + spill) * attenuation * (1.0 - smoothstep(12.0, 20.0, distanceToLight)) * flashlightEnabled;
    float distanceToView = length(viewPosition - fragPosition);
    float nearFill = 0.06 * (1.0 - smoothstep(0.15, 0.9, distanceToView)) * flashlightEnabled;
    // 暗所にも弱い間接光を残す。模様や陰影を見せながらライトの中心との明暗差を保つ。
    vec3 color = base * (adjustGreenTint(vec3(0.64, 0.94, 0.69)) * 0.045 + flashlightTint * (0.16 * beam + nearFill));
    color += adjustGreenTint(vec3(0.0009, 0.0018, 0.0011));
    // FPSの銃と腕だけ直接光を弱める。通常のモデルの反射強度は変えない。
    float directStrength = viewModelLighting > 0.5 ? 0.4 : 1.0;
    color += reflectedLight(base, normal, view, lightVector, flashlightTint * 9.0 * beam * directStrength, roughness, metalness);
    for (int i = 0; i < buildingLightCount; ++i) {
        vec3 delta = buildingPositions[i] - fragPosition;
        float distance = max(length(delta), 0.001);
        vec3 l = delta / distance;
        // 発光面の向きに従って照らし、6ワールド単位で滑らかに減衰を終える。
        float facing = max(dot(-l, buildingDirections[i]), 0.0);
        float falloff = (1.0 - smoothstep(3.0, 6.0, distance)) / (1.0 + 0.3 * distance + 0.8 * distance * distance);
        vec3 radiance = buildingColors[i].rgb * buildingColors[i].a * facing * falloff;
        color += reflectedLight(base, normal, view, l, radiance * directStrength, roughness, metalness);
    }
    if (viewModelLighting > 0.5) {
        // 銃と腕に柔らかい間接光を足す。テクスチャの色と面の陰影を残し、消灯時は弱める。
        // この補助光は手元のモデルだけに適用し、ステージや敵の照明には影響させない。
        float facing = 0.45 + 0.55 * max(dot(normal, view), 0.0);
        float fill = mix(0.04, 0.20, flashlightEnabled);
        color += base * vec3(0.94, 0.96, 0.93) * facing * fill;
    }
    // 無彩色の発光面は照明の色に寄せる。天井模様などの発光例外は読み込み側で無効にする。
    float neutralEmission = min(emission.r, min(emission.g, emission.b)) / max(max(emission.r, max(emission.g, emission.b)), 0.001);
    vec3 emitted = emission.rgb * emission.a * mix(vec3(1.0), fluorescentTint * 0.65, neutralEmission);
    if (emissionTextureEnabled > 0.5) emitted *= pow(texture(texture5, fragTexCoord).rgb, vec3(2.2));
    color += emitted;
    // 銃の明部だけを滑らかに圧縮する。RGBを同じ比率で下げ、色と質感を保ちながら白飛びを防ぐ。
    if (viewModelLighting > 0.5) {
        float peak = max(color.r, max(color.g, color.b));
        float excess = max(peak - 0.16, 0.0);
        float compressed = min(peak, 0.16) + excess / (1.0 + excess / 0.40);
        color *= compressed / max(peak, 0.00001);
    }
    float fog = 1.0 - exp(-0.006 * distanceToView * distanceToView);
    // 遠方は緑の霞へ滑らかに寄せる。黒へ切り落とさず、トーンマッピングで階調を保つ。
    color = mix(color, adjustGreenTint(vec3(0.0036, 0.0070, 0.0042)), fog);
    color = color / (vec3(1.0) + color);
    color = pow(color, vec3(1.0 / 2.2));
    // 被弾時の赤色は照明計算の後に重ね、暗所でも見えるようにする。
    color = mix(color, vec3(0.95, 0.025, 0.04), clamp(damageFlash, 0.0, 1.0) * 0.85);
    finalColor = vec4(color, albedo.a);
    // 色・透明度を保ったまま、実際の照射量を別の描画先へ記録する。再度モデルを描く必要はない。
    float focus = cone * (1.0 - smoothstep(12.0, 20.0, distanceToLight)) * flashlightEnabled;
    focusColor = vec4(focus, 0.0, 0.0, albedo.a);
})";

struct MaterialLocations { int surface, emission, emissionTexture, flash, viewModel; };
std::unordered_map<unsigned int, MaterialLocations> materialLocations;
constexpr int MaxBuildingLights = 8;

const char* PostFragment = R"(#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
uniform sampler2D texture0;
uniform sampler2D focusTexture;
uniform float focusEnabled;
uniform vec4 colDiffuse;
uniform float time;
uniform vec2 texelSize;
uniform float displayBrightness;
uniform float greenStrength;
out vec4 finalColor;
// 周辺ぼかしは0～1の比率、半径は描画バッファ上のピクセル数。緑の色味はこのRGBで調整する。
const float PeripheralBlurRatio = 0.75; // 照射範囲の外でぼかした画像を75%混ぜる。
const float PeripheralBlurRadius = 6.0;
const float PeripheralTintRatio = 0.60;
const vec3 PeripheralTint = vec3(0.42, 0.98, 0.52);
vec3 adjustGreenTint(vec3 tint) {
    return mix(vec3(dot(tint, vec3(0.2126, 0.7152, 0.0722))), tint, greenStrength);
}
vec3 brightPart(vec3 value) {
    float luma = dot(value, vec3(0.2126, 0.7152, 0.0722));
    return value * max(luma - 0.62, 0.0) / max(luma, 0.001);
}
void addSceneSample(vec2 uv, float weight, inout vec3 blurred, inout vec3 glow) {
    vec3 value = texture(texture0, clamp(uv, vec2(0.0), vec2(1.0))).rgb;
    blurred += value * weight;
    glow += brightPart(value);
}
vec3 blurredScene(vec2 uv, vec3 center, out vec3 glow) {
    // ブルームとぼかしで同じ9点の読み込みを共有する。追加の全画面バッファは作らない。
    vec2 radius = texelSize * PeripheralBlurRadius;
    vec3 result = center * 4.0;
    glow = brightPart(center) * 2.0;
    addSceneSample(uv + vec2(radius.x, 0.0), 2.0, result, glow);
    addSceneSample(uv - vec2(radius.x, 0.0), 2.0, result, glow);
    addSceneSample(uv + vec2(0.0, radius.y), 2.0, result, glow);
    addSceneSample(uv - vec2(0.0, radius.y), 2.0, result, glow);
    addSceneSample(uv + radius, 1.0, result, glow);
    addSceneSample(uv - radius, 1.0, result, glow);
    addSceneSample(uv + vec2(radius.x, -radius.y), 1.0, result, glow);
    addSceneSample(uv + vec2(-radius.x, radius.y), 1.0, result, glow);
    return result / 16.0;
}
void main() {
    vec3 color = texture(texture0, fragTexCoord).rgb;
    vec3 glow;
    vec3 blurred = blurredScene(fragTexCoord, color, glow);
    color += glow * 0.022;
    blurred += glow * 0.022;
    // 画面中央やテクスチャの明るさで判断せず、実際にライトが当たった場所を鮮明に保つ。
    // 同じ滑らかな照射マスクでぼかしと緑を混ぜ、切り替えの境界線を作らない。
    float coverage = focusEnabled > 0.5 ? texture(focusTexture, fragTexCoord).r : 0.0;
    float peripheral = 1.0 - smoothstep(0.12, 0.90, coverage);
    color = mix(color, blurred, peripheral * PeripheralBlurRatio);
    float luma = dot(color, vec3(0.2126, 0.7152, 0.0722));
    // 色調補正はシーンだけに適用する。HUDや被弾表示はEndFrameの後に描いて色と文字を保つ。
    vec3 shadowTint = mix(vec3(0.88), vec3(1.0), smoothstep(0.04, 0.65, luma));
    color = mix(vec3(luma), color, 0.74) * shadowTint;
    color *= adjustGreenTint(mix(vec3(1.0), PeripheralTint, peripheral * PeripheralTintRatio));
    // 黒いビネットと暗部の切り捨てを使わず、暗い部分の階調を残す。
    color *= 0.90 * 1.06;
    // わずかな粒状感を加える。timeで更新するが、文字やHUDにはかからない。
    float grain = fract(sin(dot(gl_FragCoord.xy + floor(time * 24.0), vec2(12.9898, 78.233))) * 43758.5453) - 0.5;
    // 表示用のガンマ補正で暗部を持ち上げる。100%では従来と同じで、HUDには適用しない。
    color = pow(clamp(color + grain * 0.012, vec3(0.0), vec3(1.0)), vec3(1.0 / displayBrightness));
    finalColor = vec4(color, 1.0) * colDiffuse * fragColor;
})";
}

Shader LoadHorrorShader(const char* vertexCode, bool normalMaps) {
    // モデル用シェーダーの生成とuniform位置の取得は読み込み時だけ行う。
    Shader shader = LoadShaderFromMemory(vertexCode ? vertexCode : ModelVertex, ModelFragment);
    if (!shader.id || !shader.locs || shader.id == rlGetShaderIdDefault()) return {};
    shader.locs[SHADER_LOC_MATRIX_MODEL] = GetShaderLocation(shader, "matModel");
    shader.locs[SHADER_LOC_MATRIX_NORMAL] = GetShaderLocation(shader, "matNormal");
    shader.locs[SHADER_LOC_MAP_NORMAL] = GetShaderLocation(shader, "texture2");
    shader.locs[SHADER_LOC_MAP_ROUGHNESS] = GetShaderLocation(shader, "texture3");
    shader.locs[SHADER_LOC_MAP_EMISSION] = GetShaderLocation(shader, "texture5");
    const MaterialLocations locations = {GetShaderLocation(shader, "surface"), GetShaderLocation(shader, "emission"),
        GetShaderLocation(shader, "emissionTextureEnabled"), GetShaderLocation(shader, "damageFlash"),
        GetShaderLocation(shader, "viewModelLighting")};
    materialLocations[shader.id] = locations;
    SetShaderValue(shader, GetShaderLocation(shader, "fluorescentTint"), &FluorescentTint, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader, GetShaderLocation(shader, "flashlightTint"), &FlashlightTint, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader, GetShaderLocation(shader, "flashlightRadiusPadding"), &FlashlightRadiusPadding, SHADER_UNIFORM_FLOAT);
    const Vector4 defaults = {0.8f, 0.0f, normalMaps ? 2.0f : 0.0f, 0.0f};
    SetShaderValue(shader, locations.surface, &defaults, SHADER_UNIFORM_VEC4);
    const float greenStrength = 1.0f;
    SetShaderValue(shader, GetShaderLocation(shader, "greenStrength"), &greenStrength, SHADER_UNIFORM_FLOAT);
    return shader;
}

void UnloadHorrorShader(Shader shader) {
    materialLocations.erase(shader.id);
    UnloadShader(shader);
}

void DrawHorrorMesh(Mesh mesh, Material material, Matrix transform, Color tint, float damageFlash, bool viewModel) {
    if (!material.maps) return;
    // テクスチャは借用し、材質の値だけコピーする。被弾色や銃の補正で共有材質を変更しない。
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
        // 共有シェーダーに前のモデルの補正が残らないよう、毎メッシュで設定する。
        const float weapon = viewModel ? 1.0f : 0.0f;
        SetShaderValue(material.shader, locations.viewModel, &weapon, SHADER_UNIFORM_FLOAT);
    }
    DrawMesh(mesh, material, transform);
}

void DrawHorrorModelEx(Model model, Vector3 position, Vector3 axis, float degrees, Vector3 scale, Color tint, float flash, bool viewModel) {
    const Matrix transform = MatrixMultiply(model.transform, MatrixMultiply(
        MatrixMultiply(MatrixScale(scale.x, scale.y, scale.z), MatrixRotate(axis, degrees * DEG2RAD)),
        MatrixTranslate(position.x, position.y, position.z)));
    for (int i = 0; i < model.meshCount; ++i)
        DrawHorrorMesh(model.meshes[i], model.materials[model.meshMaterial[i]], transform, tint, flash, viewModel);
}

void DrawHorrorModel(Model model, Vector3 position, float scale, Color tint) {
    DrawHorrorModelEx(model, position, {0, 1, 0}, 0, {scale, scale, scale}, tint);
}

void HorrorLighting::CollectBuildingLights(const Model& model) {
    // 初期化時だけ発光が有効な素材から光源を抽出する。天井模様はResourceManagerで除外する。
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
            // 三角形の内部も調べ、暗いテクスチャ内の小さな発光面を取りこぼさないようにする。
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
                    buildingLights_.push_back({center, normal, {emission.color.r / 255.0f,
                        emission.color.g / 255.0f, emission.color.b / 255.0f,
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
    // モデル用シェーダーは借用する。描画バッファ・照射マスク・画面効果はこのクラスが所有する。
    CollectBuildingLights(building);
    modelLights_.clear();
    for (Shader shader : shaders) {
        const int enabled = GetShaderLocation(shader, "flashlightEnabled");
        if (enabled >= 0) modelLights_.push_back({shader,
            GetShaderLocation(shader, "viewPosition"), GetShaderLocation(shader, "lightPosition"),
            GetShaderLocation(shader, "lightDirection"), enabled, GetShaderLocation(shader, "editorLighting"),
            GetShaderLocation(shader, "buildingLightCount"), GetShaderLocation(shader, "buildingPositions[0]"),
            GetShaderLocation(shader, "buildingColors[0]"), GetShaderLocation(shader, "buildingDirections[0]"),
            GetShaderLocation(shader, "fluorescentTint"), GetShaderLocation(shader, "flashlightTint"),
            GetShaderLocation(shader, "greenStrength")});
    }
    if (!post_.id) {
        post_ = LoadShaderFromMemory(nullptr, PostFragment);
        if (post_.id == rlGetShaderIdDefault()) post_ = {};
        if (post_.id) {
            timeLocation_ = GetShaderLocation(post_, "time");
            texelLocation_ = GetShaderLocation(post_, "texelSize");
            focusLocation_ = GetShaderLocation(post_, "focusTexture");
            focusEnabledLocation_ = GetShaderLocation(post_, "focusEnabled");
            brightnessLocation_ = GetShaderLocation(post_, "displayBrightness");
            greenStrengthLocation_ = GetShaderLocation(post_, "greenStrength");
        }
    }
    Reset();
}

void HorrorLighting::SetDisplaySettings(float brightness, float greenStrength) {
    brightness_ = std::isfinite(brightness) ? Clamp(brightness, 0.5f, 2.0f) : 1.0f;
    greenStrength_ = std::isfinite(greenStrength) ? Clamp(greenStrength, 0.0f, 1.0f) : 1.0f;
}

void HorrorLighting::Update() {
    // 入力は毎フレーム1回処理する。Fで懐中電灯と切り替え音を更新する。
    if (IsKeyPressed(KEY_F)) {
        flashlightOn_ = !flashlightOn_;
        AM().PlaySe(SeId::Flashlight);
    }
}

void HorrorLighting::BeginFrame(const Camera3D& view, const Camera3D& flashlight, Vector3 stagePosition, float stageScale, bool editorView) {
    editorView_ = editorView;
    // 表示用カメラとは別にプレイヤーの視線から光源を置き、手で持つ位置へ少しずらす。
    const Vector3 direction = Vector3Normalize(Vector3Subtract(flashlight.target, flashlight.position));
    const Vector3 right = Vector3Normalize(Vector3CrossProduct(direction, flashlight.up));
    Vector3 position = Vector3Add(flashlight.position, Vector3Scale(direction, 0.12f));
    position = Vector3Add(position, Vector3Scale(right, 0.10f));
    position = Vector3Subtract(position, Vector3Scale(flashlight.up, 0.09f));
    const float enabled = flashlightOn_ ? 1.0f : 0.0f;
    const float editor = editorView_ ? 1.0f : 0.0f;
    const Vector3 fixtureTint = AdjustGreenTint(FluorescentTint, greenStrength_);
    const Vector3 flashlightTint = AdjustGreenTint(FlashlightTint, greenStrength_);
    // ステージの移動・拡縮に追従し、表示カメラに近い8灯だけをGPUへ送る。
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
        colors[i].x *= fixtureTint.x;
        colors[i].y *= fixtureTint.y;
        colors[i].z *= fixtureTint.z;
    }
    for (const ModelLight& light : modelLights_) {
        SetShaderValue(light.shader, light.viewPosition, &view.position, SHADER_UNIFORM_VEC3);
        SetShaderValue(light.shader, light.lightPosition, &position, SHADER_UNIFORM_VEC3);
        SetShaderValue(light.shader, light.lightDirection, &direction, SHADER_UNIFORM_VEC3);
        SetShaderValue(light.shader, light.enabled, &enabled, SHADER_UNIFORM_FLOAT);
        SetShaderValue(light.shader, light.editorView, &editor, SHADER_UNIFORM_FLOAT);
        SetShaderValue(light.shader, light.fluorescentTint, &fixtureTint, SHADER_UNIFORM_VEC3);
        SetShaderValue(light.shader, light.flashlightTint, &flashlightTint, SHADER_UNIFORM_VEC3);
        SetShaderValue(light.shader, light.greenStrength, &greenStrength_, SHADER_UNIFORM_FLOAT);
        SetShaderValue(light.shader, light.buildingCount, &count, SHADER_UNIFORM_INT);
        if (count > 0) {
            SetShaderValueV(light.shader, light.buildingPositions, positions.data(), SHADER_UNIFORM_VEC3, count);
            SetShaderValueV(light.shader, light.buildingColors, colors.data(), SHADER_UNIFORM_VEC4, count);
            SetShaderValueV(light.shader, light.buildingDirections, directions.data(), SHADER_UNIFORM_VEC3, count);
        }
    }
    // リサイズ時だけバッファを再確保する。画面効果の描画解像度は最大1920×1080に抑える。
    if (editorView_) {
        // 編集中は中立色の背景へ直接描画し、暗い霧・緑の色調補正・周辺ぼかしを通さない。
        ClearBackground({48, 54, 62, 255});
        return;
    }
    const int screenWidth = (std::max)(GetScreenWidth(), 1);
    const int screenHeight = (std::max)(GetScreenHeight(), 1);























    const float scale = (std::min)(1.0f, (std::min)(1920.0f / screenWidth, 1080.0f / screenHeight));
    const int width = (std::max)(1, static_cast<int>(screenWidth * scale));
    const int height = (std::max)(1, static_cast<int>(screenHeight * scale));
    if (!scene_.id || scene_.texture.width != width || scene_.texture.height != height) {
        if (focus_.id) UnloadTexture(focus_);
        focus_ = {};
        if (scene_.id) UnloadRenderTexture(scene_);
        scene_ = LoadRenderTexture(width, height);
        if (scene_.id) {
            SetTextureFilter(scene_.texture, TEXTURE_FILTER_BILINEAR);
            SetTextureWrap(scene_.texture, TEXTURE_WRAP_CLAMP);
            // 色と照射量を1回の3D描画で記録する。1チャンネルのため追加メモリは1080pで約2MB。
            focus_ = {rlLoadTexture(nullptr, width, height, PIXELFORMAT_UNCOMPRESSED_GRAYSCALE, 1),
                      width, height, 1, PIXELFORMAT_UNCOMPRESSED_GRAYSCALE};
            if (focus_.id) {
                SetTextureFilter(focus_, TEXTURE_FILTER_BILINEAR);
                SetTextureWrap(focus_, TEXTURE_WRAP_CLAMP);
                rlFramebufferAttach(scene_.id, focus_.id, RL_ATTACHMENT_COLOR_CHANNEL1, RL_ATTACHMENT_TEXTURE2D, 0);
                if (!rlFramebufferComplete(scene_.id)) {
                    rlFramebufferAttach(scene_.id, 0, RL_ATTACHMENT_COLOR_CHANNEL1, RL_ATTACHMENT_TEXTURE2D, 0);
                    UnloadTexture(focus_);
                    focus_ = {};
                }
            }
        }
    }
    if (scene_.id) {
        BeginTextureMode(scene_);
        if (focus_.id) rlActiveDrawBuffers(2);
    }
    const Vector3 background = AdjustGreenTint({float(NightBackground.r), float(NightBackground.g), float(NightBackground.b)}, greenStrength_);
    ClearBackground({static_cast<unsigned char>(std::round(background.x)), static_cast<unsigned char>(std::round(background.y)),
                     static_cast<unsigned char>(std::round(background.z)), NightBackground.a});
}

void HorrorLighting::EndFrame(bool drawControls) const {
    if (editorView_) return;
    // 3D描画を終了し、照射範囲の外だけをぼかして緑のグラデーションを重ねる。HUDはこの後。
    if (scene_.id) {
        rlDrawRenderBatchActive();
        if (focus_.id) rlActiveDrawBuffers(1);
        EndTextureMode();
        if (post_.id) {
            const float time = static_cast<float>(GetTime());
            SetShaderValue(post_, timeLocation_, &time, SHADER_UNIFORM_FLOAT);
            const Vector2 texel = {1.0f / scene_.texture.width, 1.0f / scene_.texture.height};
            SetShaderValue(post_, texelLocation_, &texel, SHADER_UNIFORM_VEC2);
            const float enabled = focus_.id ? 1.0f : 0.0f;
            SetShaderValue(post_, focusEnabledLocation_, &enabled, SHADER_UNIFORM_FLOAT);
            SetShaderValue(post_, brightnessLocation_, &brightness_, SHADER_UNIFORM_FLOAT);
            SetShaderValue(post_, greenStrengthLocation_, &greenStrength_, SHADER_UNIFORM_FLOAT);
            BeginShaderMode(post_);
            // シェーダー切り替えはバッチをフラッシュするため、その後に追加テクスチャを登録する。
            if (focus_.id) SetShaderValueTexture(post_, focusLocation_, focus_);
        }
        DrawTexturePro(scene_.texture, {0, 0, static_cast<float>(scene_.texture.width), -static_cast<float>(scene_.texture.height)},
            {0, 0, static_cast<float>(GetScreenWidth()), static_cast<float>(GetScreenHeight())}, {0, 0}, 0, WHITE);
        if (post_.id) EndShaderMode();
    }
    if (drawControls) DrawText(flashlightOn_ ? "[F] FLASHLIGHT ON" : "[F] FLASHLIGHT OFF",
        24, GetScreenHeight() - 36, 16, {135, 145, 150, 255});
}

void HorrorLighting::Shutdown() {
    // 所有するGPUリソースだけを解放する。共有モデル用シェーダーはResourceManagerが解放する。
    if (focus_.id) UnloadTexture(focus_);
    if (scene_.id) UnloadRenderTexture(scene_);
    if (post_.id) UnloadShader(post_);
    scene_ = {};
    focus_ = {};
    post_ = {};
    modelLights_.clear();
    buildingLights_.clear();
    editorView_ = false;
}
