#include "GltfMaterials.h"
#include "nlohmann/json.hpp"
#include "rlgl.h"

#include <cstdint>
#include <cstring>
#include <fstream>
#include <limits>
#include <new>
#include <stdexcept>

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

void ApplyGltfTextureCoordinates(Model& model, const std::string& path) {
    if (!IsFileExtension(path.c_str(), ".glb;.gltf") || !model.meshes || !model.meshMaterial) return;
    try {
        const auto document = ReadDocument(path);
        if (!document.contains("materials")) return;
        const auto& materials = document.at("materials");
        // raylib 5.5 reserves material 0; source material i is stored at i + 1.
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
