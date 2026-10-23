#include "GltfMaterials.h"
#include "nlohmann/json.hpp"
#include "rlgl.h"
#include "raymath.h"
#include <algorithm>
#include <array>

#include <cstdint>
#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>
#include <new>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace {

nlohmann::json ReadDocument(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error("Cannot read material metadata");
    if (IsFileExtension(path.c_str(), ".gltf")) return nlohmann::json::parse(file);

    // Read only the JSON chunk; geometry/images have already been loaded by raylib.
    std::uint32_t header[5] = {};
    file.read(reinterpret_cast<char*>(header), sizeof(header));
    if (!file || header[0] != 0x46546C67 || header[1] != 2 || header[2] < 20 ||
        header[4] != 0x4E4F534A || header[3] > header[2] - 20) {
        throw std::runtime_error("Invalid GLB JSON chunk");
    }
    file.seekg(0, std::ios::end);
    if (file.tellg() < static_cast<std::streamoff>(header[2])) throw std::runtime_error("Truncated GLB");
    file.seekg(20);
    std::string jsonText(header[3], '\0');
    file.read(&jsonText[0], jsonText.size());
    if (!file) throw std::runtime_error("Truncated GLB JSON chunk");
    return nlohmann::json::parse(jsonText);
}

void ApplyMaterialFactors(Material& target, const nlohmann::json& source) {
    const auto pbr = source.value("pbrMetallicRoughness", nlohmann::json::object());
    if (source.contains("pbrMetallicRoughness")) {
        target.maps[MATERIAL_MAP_ROUGHNESS].value = Clamp(pbr.value("roughnessFactor", 1.0f), 0.04f, 1.0f);
        target.maps[MATERIAL_MAP_METALNESS].value = Clamp(pbr.value("metallicFactor", 1.0f), 0.0f, 1.0f);
    }
    const auto base = pbr.value("baseColorTexture", nlohmann::json::object());
    const int uvSet = base.value("texCoord", 0);
    const auto normal = source.value("normalTexture", nlohmann::json::object());
    const auto packed = pbr.value("metallicRoughnessTexture", nlohmann::json::object());
    const auto emission = source.value("emissiveTexture", nlohmann::json::object());
    // Only sample maps that share the UV set used by this shader.
    target.params[0] = target.maps[MATERIAL_MAP_NORMAL].texture.id && normal.contains("index") && normal.value("texCoord", 0) == uvSet ? 1.0f : 0.0f;
    target.params[1] = target.maps[MATERIAL_MAP_ROUGHNESS].texture.id && packed.contains("index") && packed.value("texCoord", 0) == uvSet ? 1.0f : 0.0f;
    target.params[2] = target.maps[MATERIAL_MAP_EMISSION].texture.id && emission.contains("index") && emission.value("texCoord", 0) == uvSet ? 1.0f : 0.0f;
    const auto factor = source.value("emissiveFactor", std::array<float, 3>{0, 0, 0});
    const float peak = (std::max)({factor[0], factor[1], factor[2], 0.001f});
    target.maps[MATERIAL_MAP_EMISSION].color = {
        static_cast<unsigned char>(Clamp(factor[0] / peak, 0, 1) * 255),
        static_cast<unsigned char>(Clamp(factor[1] / peak, 0, 1) * 255),
        static_cast<unsigned char>(Clamp(factor[2] / peak, 0, 1) * 255), 255};
    const auto extensions = source.value("extensions", nlohmann::json::object());
    const float strength = extensions.value("KHR_materials_emissive_strength", nlohmann::json::object()).value("emissiveStrength", 1.0f);
    target.maps[MATERIAL_MAP_EMISSION].value = peak * (std::max)(0.0f, strength);
}

void UsePrimaryCoordinates(Mesh& mesh, const float* coordinates) {
    const int bytes = mesh.vertexCount * 2 * static_cast<int>(sizeof(float));
    if (!mesh.texcoords) mesh.texcoords = static_cast<float*>(MemAlloc(bytes));
    if (!mesh.texcoords) throw std::bad_alloc();
    std::memcpy(mesh.texcoords, coordinates, bytes);
    if (mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_TEXCOORD]) {
        UpdateMeshBuffer(mesh, RL_DEFAULT_SHADER_ATTRIB_LOCATION_TEXCOORD, mesh.texcoords, bytes, 0);
    } else {
        // A valid glTF may contain TEXCOORD_1 without TEXCOORD_0.
        rlEnableVertexArray(mesh.vaoId);
        mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_TEXCOORD] = rlLoadVertexBuffer(mesh.texcoords, bytes, false);
        rlSetVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_TEXCOORD, 2, RL_FLOAT, false, 0, 0);
        rlEnableVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_TEXCOORD);
        rlDisableVertexArray();
        rlDisableVertexBuffer();
    }
}

} // namespace

void ApplyGltfTextureCoordinates(Model& model, const std::string& path,
                                const std::vector<std::string>& nonEmissiveMaterials) {
    if (!IsFileExtension(path.c_str(), ".glb;.gltf") || !model.meshes || !model.meshMaterial) return;
    try {
        const auto document = ReadDocument(path);
        if (!document.contains("materials")) return;
        const auto& materials = document.at("materials");
        // raylib 5.5 reserves material 0; source material i is stored at i + 1.
        for (int i = 0; i < model.materialCount; ++i) {
            model.materials[i].maps[MATERIAL_MAP_ROUGHNESS].value = 0.8f;
            model.materials[i].maps[MATERIAL_MAP_METALNESS].value = 0.0f;
        }
        for (int i = 0; i < static_cast<int>(materials.size()) && i + 1 < model.materialCount; ++i) {
            const auto& source = materials.at(i);
            Material& target = model.materials[i + 1];
            ApplyMaterialFactors(target, source);
            // A decorative pattern may carry emission in the source asset; keep its albedo/UVs intact.
            if (!nonEmissiveMaterials.empty() &&
                std::find(nonEmissiveMaterials.begin(), nonEmissiveMaterials.end(), source.value("name", std::string{})) != nonEmissiveMaterials.end()) {
                target.maps[MATERIAL_MAP_EMISSION].value = 0.0f;
                target.params[2] = 0.0f;
            }
        }
        for (int m = 0; m < model.meshCount; ++m) {
            const int materialIndex = model.meshMaterial[m] - 1;
            if (materialIndex < 0 || materialIndex >= static_cast<int>(materials.size())) continue;
            const auto& material = materials.at(materialIndex);
            if (!material.contains("pbrMetallicRoughness")) continue;
            const auto& pbr = material.at("pbrMetallicRoughness");
            if (!pbr.contains("baseColorTexture")) continue;
            const int uvSet = pbr.at("baseColorTexture").value("texCoord", 0);
            if (uvSet == 0) continue;
            Mesh& mesh = model.meshes[m];
            if (uvSet != 1 || !mesh.texcoords2) {
                // Missing UVs cannot be reconstructed from material/image metadata.
                TraceLog(LOG_WARNING, "MODEL: [%s] Mesh %d lacks TEXCOORD_%d; keeping available UVs", path.c_str(), m, uvSet);
                continue;
            }
            if (!mesh.vboId || mesh.vertexCount <= 0 || mesh.vertexCount > (std::numeric_limits<int>::max)() / 8) continue;
            // Preserve TEXCOORD_1 and copy it into the CPU/GPU slot sampled by the default shader.
            UsePrimaryCoordinates(mesh, mesh.texcoords2);
        }
    } catch (const std::exception& error) {
        TraceLog(LOG_WARNING, "MODEL: [%s] Material UV metadata: %s", path.c_str(), error.what());
    }
}

std::vector<int> ApplyGlbMaterialCompanion(Model& model, const std::string& path,
                                         const std::vector<std::string>& materialNames) {
    std::vector<int> uvSets(model.materialCount, -1);
    if (!FileExists(path.c_str())) return uvSets;
    std::unordered_map<int, Texture2D> images;
    try {
        const auto document = ReadDocument(path);
        const auto& sources = document.at("materials");
        std::ifstream file(path, std::ios::binary);
        std::uint32_t header[5]{}, binHeader[2]{};
        file.read(reinterpret_cast<char*>(header), sizeof(header));
        file.seekg(20ull + header[3]);
        file.read(reinterpret_cast<char*>(binHeader), sizeof(binHeader));
        const auto binStart = file.tellg();
        if (!file || binHeader[1] != 0x004E4942 || binStart < 0 ||
            static_cast<std::uint64_t>(binStart) + binHeader[0] > header[2]) {
            throw std::runtime_error("Invalid GLB binary chunk");
        }
        auto readTexture = [&](const nlohmann::json& reference) -> Texture2D {
            if (!reference.contains("index")) return {};
            const int imageIndex = document.at("textures").at(reference.at("index").get<int>()).at("source");
            const auto cached = images.find(imageIndex);
            if (cached != images.end()) return cached->second;
            const auto& image = document.at("images").at(imageIndex);
            const auto& view = document.at("bufferViews").at(image.at("bufferView").get<int>());
            const auto offset = view.value("byteOffset", std::uint64_t(0));
            const auto length = view.at("byteLength").get<std::uint64_t>();
            if (view.value("buffer", 0) != 0 || length == 0 || length > (std::numeric_limits<int>::max)() ||
                offset > binHeader[0] || length > binHeader[0] - offset) {
                throw std::runtime_error("Invalid GLB image range");
            }
            // 180MBのGLBでも、メッシュのBIN領域は読み込まず画像の範囲だけを読む。
            std::vector<unsigned char> bytes(static_cast<std::size_t>(length));
            file.seekg(static_cast<std::streamoff>(binStart) + static_cast<std::streamoff>(offset));
            file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(length));
            if (!file) throw std::runtime_error("Truncated GLB image");
            const std::string mime = image.value("mimeType", std::string{});
            Image decoded = LoadImageFromMemory(mime == "image/png" ? ".png" : ".jpg", bytes.data(), static_cast<int>(length));
            if (!decoded.data) throw std::runtime_error("Cannot decode companion texture");
            Texture2D texture = LoadTextureFromImage(decoded);
            UnloadImage(decoded);
            if (!texture.id) throw std::runtime_error("Cannot upload companion texture");
            GenTextureMipmaps(&texture);
            SetTextureFilter(texture, TEXTURE_FILTER_TRILINEAR);
            images.emplace(imageIndex, texture);
            return texture;
        };
        for (int i = 1; i < model.materialCount && i < static_cast<int>(materialNames.size()); ++i) {
            const auto found = std::find_if(sources.begin(), sources.end(), [&](const auto& source) {
                return source.value("name", std::string{}) == materialNames[i];
            });
            if (found == sources.end()) continue;
            const auto& source = *found;
            const auto pbr = source.value("pbrMetallicRoughness", nlohmann::json::object());
            const auto base = pbr.value("baseColorTexture", nlohmann::json::object());
            const int uvSet = base.value("texCoord", 0);
            Material& target = model.materials[i];
            std::array<MaterialMap, 12> maps;
            std::copy_n(target.maps, maps.size(), maps.begin());
            Material candidate = target;
            candidate.maps = maps.data();
            const auto factor = pbr.value("baseColorFactor", std::array<float, 4>{1, 1, 1, 1});
            // glTFの係数は線形色。照明シェーダーがRGBを線形化するため、材質色は表示用RGBで渡す。
            candidate.maps[MATERIAL_MAP_ALBEDO].color = {
                static_cast<unsigned char>(std::pow(Clamp(factor[0], 0, 1), 1.0f / 2.2f) * 255),
                static_cast<unsigned char>(std::pow(Clamp(factor[1], 0, 1), 1.0f / 2.2f) * 255),
                static_cast<unsigned char>(std::pow(Clamp(factor[2], 0, 1), 1.0f / 2.2f) * 255),
                static_cast<unsigned char>(Clamp(factor[3], 0, 1) * 255)};
            auto assign = [&](int slot, const nlohmann::json& reference) {
                if (reference.value("texCoord", 0) != uvSet) return;
                const Texture2D texture = readTexture(reference);
                if (texture.id) candidate.maps[slot].texture = texture;
            };
            assign(MATERIAL_MAP_ALBEDO, base);
            assign(MATERIAL_MAP_NORMAL, source.value("normalTexture", nlohmann::json::object()));
            assign(MATERIAL_MAP_ROUGHNESS, pbr.value("metallicRoughnessTexture", nlohmann::json::object()));
            assign(MATERIAL_MAP_EMISSION, source.value("emissiveTexture", nlohmann::json::object()));
            ApplyMaterialFactors(candidate, source);
            std::copy(maps.begin(), maps.end(), target.maps);
            std::copy_n(candidate.params, 4, target.params);
            uvSets[i] = uvSet;
            TraceLog(LOG_INFO, "FBX: [%s] Companion material '%s', TEXCOORD_%d", path.c_str(), materialNames[i].c_str(), uvSet);
        }
    } catch (const std::exception& error) {
        TraceLog(LOG_WARNING, "FBX: [%s] Companion material: %s", path.c_str(), error.what());
    }
    // 読み込み途中で失敗した画像だけを解放する。材質へ渡した画像はResourceManagerが所有する。
    std::unordered_set<unsigned int> owned;
    for (int i = 0; i < model.materialCount; ++i)
        for (int slot = 0; slot < 12; ++slot) owned.insert(model.materials[i].maps[slot].texture.id);
    for (const auto& image : images) if (!owned.count(image.second.id)) UnloadTexture(image.second);
    return uvSets;
}
