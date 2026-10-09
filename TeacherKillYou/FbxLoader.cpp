#include "FbxLoader.h"
#include "HorrorLighting.h"
#include "third_party/ufbx/ufbx.h"
#include "raymath.h"
#include "rlgl.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <memory>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace {
constexpr double FrameDuration = 0.017;
constexpr std::size_t ChunkVertices = 65535; // raylibの16bitインデックスに収める。
constexpr int GpuBones = 128;
using ScenePtr = std::unique_ptr<ufbx_scene, decltype(&ufbx_free_scene)>;

template<class T> T* Allocate(std::size_t count) {
    if (count == 0) return nullptr;
    if (count > (std::numeric_limits<unsigned int>::max)() / sizeof(T)) throw std::bad_alloc();
    auto* result = static_cast<T*>(MemAlloc(static_cast<unsigned int>(count * sizeof(T))));
    if (!result) throw std::bad_alloc();
    std::memset(result, 0, count * sizeof(T));
    return result;
}

std::string Text(ufbx_string value) { return std::string(value.data, value.length); }
std::string Lower(std::string value) {
    for (char& ch : value) if (ch >= 'A' && ch <= 'Z') ch += 'a' - 'A';
    return value;
}
std::string PathText(ufbx_string value) {
    auto result = Text(value);
    std::replace(result.begin(), result.end(), '\\', '/');
    return result;
}
Vector3 Vector(ufbx_vec3 v) { return { static_cast<float>(v.x), static_cast<float>(v.y), static_cast<float>(v.z) }; }
Matrix MatrixFrom(const ufbx_matrix& m) {
    return { static_cast<float>(m.m00), static_cast<float>(m.m01), static_cast<float>(m.m02), static_cast<float>(m.m03),
             static_cast<float>(m.m10), static_cast<float>(m.m11), static_cast<float>(m.m12), static_cast<float>(m.m13),
             static_cast<float>(m.m20), static_cast<float>(m.m21), static_cast<float>(m.m22), static_cast<float>(m.m23), 0, 0, 0, 1 };
}
void CopyName(char (&destination)[32], ufbx_string name) {
    std::memcpy(destination, name.data, (std::min)(name.length, sizeof(destination) - 1));
}
std::string ErrorText(const ufbx_error& error) {
    char message[1024]{};
    ufbx_format_error(message, sizeof(message), &error);
    return message;
}

// クラスタの逆バインド行列はメッシュごとに異なる。骨ノードだけで共有しない。
struct Binding {
    uint32_t node;
    ufbx_matrix toBone;
    ufbx_matrix fromWorld;
    bool rigid;
};
struct MeshSource {
    const ufbx_node* node;
    const ufbx_skin_deformer* skin;
    int firstBone;
    int rigidBone;
    ufbx_matrix normalMatrix;
};
struct MeshPart {
    const MeshSource* source;
    int material;
    std::vector<uint32_t> corners;
};
struct Vertex {
    Vector3 position, normal;
    Vector2 uv;
    unsigned char bones[4];
    float weights[4];
};

Texture2D ReadTexture(const ufbx_texture* texture, const std::string& directory) {
    if (!texture) return {};
    if (texture->type != UFBX_TEXTURE_FILE) {
        if (texture->file_textures.count == 0) return {};
        texture = texture->file_textures[0];
    }
    if (texture->content.size > 0 && texture->content.size <= (std::numeric_limits<int>::max)()) {
        const auto filename = PathText(texture->filename);
        const char* extension = GetFileExtension(filename.c_str());
        const auto* bytes = static_cast<const unsigned char*>(texture->content.data);
        // 拡張子のない埋め込み画像にも対応する。
        if (!extension || extension[0] == '\0') extension = bytes[0] == 0x89 ? ".png" : ".jpg";
        Image image = LoadImageFromMemory(extension, bytes, static_cast<int>(texture->content.size));
        Texture2D result{};
        if (image.data) { result = LoadTextureFromImage(image); UnloadImage(image); }
        if (result.id) { GenTextureMipmaps(&result); SetTextureFilter(result, TEXTURE_FILTER_TRILINEAR); }
        return result;
    }
    const auto filename = PathText(texture->filename);
    const auto relative = PathText(texture->relative_filename);
    const std::string basename = GetFileName(filename.c_str());
    const char* fileExtension = GetFileExtension(basename.c_str());
    const bool extensionMissing = !fileExtension || fileExtension[0] == '\0';
    // 制作PCの絶対パスが残っていても、FBXの横やtextures内に置いた画像を探す。
    for (const auto& candidate : { filename, directory + "/" + relative, directory + "/" + basename,
                                   directory + "/textures/" + basename, directory + "/textures/packed/" + basename }) {
        // MONSTERのように画像名だけの参照でも、同名のPNG/JPEGなどを見つける。
        for (const char* extension : { "", ".png", ".jpg", ".jpeg", ".tga", ".bmp" }) {
            if (extension[0] != '\0' && !extensionMissing) break;
            const std::string imagePath = candidate + extension;
            if (!candidate.empty() && FileExists(imagePath.c_str())) {
                Texture2D result = LoadTexture(imagePath.c_str());
                if (!result.id) continue;
                GenTextureMipmaps(&result);
                SetTextureFilter(result, TEXTURE_FILTER_TRILINEAR);
                return result;
            }
        }
    }
    TraceLog(LOG_WARNING, "FBX: Texture missing: %s", filename.c_str());
    return {};
}

// CPU・GPUの両方で同じ陰影を使う。法線は面の内側で補間してから照明を計算する。
Shader LoadFbxShader(bool gpu) {
    const std::string vertex = std::string("#version 330\n") + (gpu ? "#define GPU_SKINNING\n" : "") + R"(
layout(location=0) in vec3 vertexPosition;
layout(location=1) in vec2 vertexTexCoord;
layout(location=2) in vec3 vertexNormal;
layout(location=3) in vec4 vertexColor;
uniform mat4 mvp;
uniform mat4 matModel;
uniform mat4 matNormal;
#ifdef GPU_SKINNING
// raylib自身の属性配置に合わせる。配布ライブラリで番号が異なるため固定しない。
in vec4 vertexBoneIds;
in vec4 vertexBoneWeights;
uniform mat4 boneMatrices[128];
#endif
out vec2 fragTexCoord;
out vec4 fragColor;
out vec3 fragPosition;
out vec3 fragNormal;
void main() {
    mat4 skin = mat4(1.0);
#ifdef GPU_SKINNING
    skin = boneMatrices[int(vertexBoneIds.x)] * vertexBoneWeights.x
              + boneMatrices[int(vertexBoneIds.y)] * vertexBoneWeights.y
              + boneMatrices[int(vertexBoneIds.z)] * vertexBoneWeights.z
              + boneMatrices[int(vertexBoneIds.w)] * vertexBoneWeights.w;
#endif
    gl_Position = mvp * skin * vec4(vertexPosition, 1.0);
    fragTexCoord = vertexTexCoord;
    fragColor = vertexColor;
    fragPosition = (matModel * skin * vec4(vertexPosition, 1.0)).xyz;
    fragNormal = mat3(matNormal) * transpose(inverse(mat3(skin))) * vertexNormal;
})";
    Shader shader = LoadHorrorShader(vertex.c_str(), true);
    if (shader.id == rlGetShaderIdDefault()) return {};
    shader.locs[SHADER_LOC_MATRIX_MODEL] = GetShaderLocation(shader, "matModel");
    shader.locs[SHADER_LOC_MATRIX_NORMAL] = GetShaderLocation(shader, "matNormal");
    shader.locs[SHADER_LOC_MAP_NORMAL] = GetShaderLocation(shader, "texture2");
    if (gpu) {
        shader.locs[SHADER_LOC_BONE_MATRICES] = GetShaderLocation(shader, "boneMatrices[0]");
        shader.locs[SHADER_LOC_VERTEX_BONEIDS] = GetShaderLocationAttrib(shader, "vertexBoneIds");
        shader.locs[SHADER_LOC_VERTEX_BONEWEIGHTS] = GetShaderLocationAttrib(shader, "vertexBoneWeights");
    }
    return shader;
}

void Release(FbxModelData& data) {
    std::unordered_set<unsigned int> textures, shaders;
    for (int m = 0; m < data.model.materialCount; ++m) {
        const Material& material = data.model.materials[m];
        if (material.maps) for (int map = MATERIAL_MAP_ALBEDO; map <= MATERIAL_MAP_BRDF; ++map) {
            const auto texture = material.maps[map].texture;
            if (texture.id && texture.id != rlGetTextureIdDefault() && textures.insert(texture.id).second) UnloadTexture(texture);
        }
        if (material.shader.id && material.shader.id != rlGetShaderIdDefault() && shaders.insert(material.shader.id).second) UnloadShader(material.shader);
    }
    UnloadModel(data.model);
    if (data.animations) UnloadModelAnimations(data.animations, data.animationCount);
    data = {};
}

Vertex ReadVertex(const MeshSource& source, uint32_t corner) {
    const auto* mesh = source.node->mesh;
    Vertex result{};
    result.position = Vector(ufbx_transform_position(&source.node->geometry_to_world, mesh->vertex_position[corner]));
    result.normal = Vector3Normalize(Vector(ufbx_transform_direction(&source.normalMatrix, mesh->vertex_normal[corner])));
    if (mesh->vertex_uv.exists) {
        const ufbx_vec2 uv = mesh->vertex_uv[corner];
        result.uv = { static_cast<float>(uv.x), 1.0f - static_cast<float>(uv.y) };
    }
    float weightSum = 0;
    if (source.skin) {
        const auto vertex = source.skin->vertices[mesh->vertex_indices[corner]];
        for (std::size_t i = 0; i < (std::min)(vertex.num_weights, uint32_t(4)); ++i) {
            const auto weight = source.skin->weights[vertex.weight_begin + i];
            result.bones[i] = static_cast<unsigned char>(source.firstBone + weight.cluster_index);
            result.weights[i] = static_cast<float>(weight.weight);
            weightSum += result.weights[i];
        }
    }
    if (weightSum > 0) for (auto& weight : result.weights) weight /= weightSum;
    else { result.bones[0] = static_cast<unsigned char>(source.rigidBone); result.weights[0] = 1; }
    return result;
}

void BuildMesh(Mesh& mesh, const MeshPart& part, int bones, bool gpu) {
    std::vector<Vertex> vertices;
    vertices.reserve(part.corners.size());
    for (uint32_t corner : part.corners) vertices.push_back(ReadVertex(*part.source, corner));
    std::vector<uint32_t> indices(vertices.size());
    const ufbx_vertex_stream stream{ vertices.data(), vertices.size(), sizeof(Vertex) };
    ufbx_error error{};
    const std::size_t unique = ufbx_generate_indices(&stream, 1, indices.data(), indices.size(), nullptr, &error);
    if (!unique) throw std::runtime_error(ErrorText(error));
    mesh.vertexCount = static_cast<int>(unique);
    mesh.triangleCount = static_cast<int>(indices.size() / 3);
    mesh.vertices = Allocate<float>(unique * 3);
    mesh.normals = Allocate<float>(unique * 3);
    mesh.texcoords = Allocate<float>(unique * 2);
    mesh.indices = Allocate<unsigned short>(indices.size());
    mesh.boneCount = bones;
    mesh.boneIds = Allocate<unsigned char>(unique * 4);
    mesh.boneWeights = Allocate<float>(unique * 4);
    mesh.boneMatrices = Allocate<Matrix>(bones);
    if (!gpu) { mesh.animVertices = Allocate<float>(unique * 3); mesh.animNormals = Allocate<float>(unique * 3); }
    for (std::size_t v = 0; v < unique; ++v) {
        std::memcpy(mesh.vertices + v * 3, &vertices[v].position, sizeof(Vector3));
        std::memcpy(mesh.normals + v * 3, &vertices[v].normal, sizeof(Vector3));
        std::memcpy(mesh.texcoords + v * 2, &vertices[v].uv, sizeof(Vector2));
        std::memcpy(mesh.boneIds + v * 4, vertices[v].bones, sizeof(vertices[v].bones));
        std::memcpy(mesh.boneWeights + v * 4, vertices[v].weights, sizeof(vertices[v].weights));
    }
    for (std::size_t i = 0; i < indices.size(); ++i) mesh.indices[i] = static_cast<unsigned short>(indices[i]);
    for (int bone = 0; bone < bones; ++bone) mesh.boneMatrices[bone] = MatrixIdentity();
    if (!gpu) {
        std::memcpy(mesh.animVertices, mesh.vertices, unique * 3 * sizeof(float));
        std::memcpy(mesh.animNormals, mesh.normals, unique * 3 * sizeof(float));
    }
    UploadMesh(&mesh, !gpu);
}
}

FbxModelData LoadFbxModel(const std::string& path) {
    FbxModelData data;
    try {
        ufbx_load_opts options{};
        options.target_axes = ufbx_axes_right_handed_y_up;
        options.target_unit_meters = 1.0;
        options.generate_missing_normals = true;
        ufbx_error error{};
        ScenePtr scene(ufbx_load_file(path.c_str(), &options, &error), ufbx_free_scene);
        if (!scene && error.type == UFBX_ERROR_FILE_NOT_FOUND) {
            // ufbxはUTF-8のパスを使う。既存のCP932ソースから渡されたパスも開けるようにする。
            std::FILE* nativeFile = nullptr;
#ifdef _MSC_VER
            fopen_s(&nativeFile, path.c_str(), "rb");
#else
            nativeFile = std::fopen(path.c_str(), "rb");
#endif
            std::unique_ptr<std::FILE, decltype(&std::fclose)> file(nativeFile, std::fclose);
            if (file) {
                options.filename = { path.c_str(), path.size() };
                scene.reset(ufbx_load_stdio(file.get(), &options, &error));
            }
        }
        if (!scene) throw std::runtime_error(ErrorText(error));
        std::vector<MeshSource> sources;
        std::vector<Binding> bindings;
        sources.reserve(scene->nodes.count);
        for (const auto* node : scene->nodes) {
            if (!node->mesh || node->mesh->num_triangles == 0) continue;
            const auto* skin = node->mesh->skin_deformers.count ? node->mesh->skin_deformers[0] : nullptr;
            const ufbx_matrix inverseGeometry = ufbx_matrix_invert(&node->geometry_to_world);
            const int firstBone = static_cast<int>(bindings.size());
            if (skin) for (const auto* cluster : skin->clusters) {
                bindings.push_back({ cluster->bone_node->typed_id, cluster->geometry_to_bone, inverseGeometry, false });
            }
            const int rigidBone = static_cast<int>(bindings.size());
            bindings.push_back({ node->typed_id, ufbx_identity_matrix, inverseGeometry, true });
            sources.push_back({ node, skin, firstBone, rigidBone, ufbx_matrix_for_normals(&node->geometry_to_world) });
        }
        if (sources.empty()) throw std::runtime_error("FBX has no triangle mesh");
        if (bindings.size() > 256) throw std::runtime_error("FBX exceeds raylib's 256 skin binding limit");
        std::vector<MeshPart> parts;
        for (const auto& source : sources) {
            const auto* mesh = source.node->mesh;
            std::vector<uint32_t> triangle(mesh->max_face_triangles * 3);
            for (std::size_t material = 0; material < mesh->material_parts.count; ++material) {
                const auto& group = mesh->material_parts[material];
                if (group.num_triangles == 0) continue;
                const int materialId = material < source.node->materials.count
                    ? static_cast<int>(source.node->materials[material]->typed_id) + 1 : 0;
                MeshPart part{ &source, materialId, {} };
                for (uint32_t faceIndex : group.face_indices) {
                    const auto face = mesh->faces[faceIndex];
                    const auto count = ufbx_triangulate_face(triangle.data(), triangle.size(), mesh, face) * 3;
                    for (std::size_t i = 0; i < count; i += 3) {
                        if (part.corners.size() + 3 > ChunkVertices) {
                            parts.push_back(std::move(part)); part = { &source, materialId, {} };
                        }
                        part.corners.insert(part.corners.end(), triangle.begin() + i, triangle.begin() + i + 3);
                    }
                }
                if (!part.corners.empty()) parts.push_back(std::move(part));
            }
        }
        data.model.transform = MatrixIdentity();
        data.model.meshes = Allocate<Mesh>(parts.size());
        data.model.meshMaterial = Allocate<int>(parts.size());
        data.model.meshCount = static_cast<int>(parts.size());
        data.model.materials = Allocate<Material>(scene->materials.count + 1);
        data.model.materialCount = static_cast<int>(scene->materials.count + 1);
        data.model.boneCount = static_cast<int>(bindings.size());
        data.model.bones = Allocate<BoneInfo>(bindings.size());
        data.model.bindPose = Allocate<Transform>(bindings.size());
        for (std::size_t bone = 0; bone < bindings.size(); ++bone) {
            CopyName(data.model.bones[bone].name, scene->nodes[bindings[bone].node]->name);
            data.model.bones[bone].parent = -1;
            data.model.bindPose[bone] = { {}, {0,0,0,1}, {1,1,1} };
        }
        std::unordered_map<const ufbx_texture*, Texture2D> textures;
        const std::string directory = GetDirectoryPath(path.c_str());
        bool gpu = data.model.boneCount <= GpuBones && scene->anim_stacks.count;
        Shader shader = LoadFbxShader(gpu);
        if (!shader.id && gpu) { gpu = false; shader = LoadFbxShader(false); }
        gpu = gpu && shader.id != 0;
        // 法線マップがない材質には平坦な法線を使い、シェーダーを材質間で共有する。
        Image flatImage = GenImageColor(1, 1, {128, 128, 255, 255});
        const Texture2D flatNormal = LoadTextureFromImage(flatImage);
        UnloadImage(flatImage);
        for (int m = 0; m < data.model.materialCount; ++m) {
            Material& material = data.model.materials[m];
            material = LoadMaterialDefault();
            if (shader.id) material.shader = shader;
            material.maps[MATERIAL_MAP_NORMAL].texture = flatNormal;
            if (m == 0) continue;
            const auto* source = scene->materials[m - 1];
            const auto& base = source->pbr.base_color;
            const auto& color = base.has_value ? base : source->fbx.diffuse_color;
            if (color.has_value) material.maps[MATERIAL_MAP_ALBEDO].color = {
                static_cast<unsigned char>(Clamp(static_cast<float>(color.value_vec3.x), 0, 1) * 255),
                static_cast<unsigned char>(Clamp(static_cast<float>(color.value_vec3.y), 0, 1) * 255),
                static_cast<unsigned char>(Clamp(static_cast<float>(color.value_vec3.z), 0, 1) * 255), 255 };
            // FBXの透明な毛・葉などを不透明な板として描かない。
            const double opacity = source->pbr.opacity.has_value ? source->pbr.opacity.value_real
                : ufbx_find_real(&source->props, "Opacity", 1.0);
            material.maps[MATERIAL_MAP_ALBEDO].color.a = static_cast<unsigned char>(Clamp(static_cast<float>(opacity), 0, 1) * 255);
            const auto* texture = base.texture ? base.texture : source->fbx.diffuse_color.texture;
            if (texture) {
                auto loaded = textures.find(texture);
                if (loaded == textures.end()) loaded = textures.emplace(texture, ReadTexture(texture, directory)).first;
                if (loaded->second.id) material.maps[MATERIAL_MAP_ALBEDO].texture = loaded->second;
            }
            const auto* normal = source->pbr.normal_map.texture ? source->pbr.normal_map.texture : source->fbx.normal_map.texture;
            if (normal) {
                auto loaded = textures.find(normal);
                if (loaded == textures.end()) loaded = textures.emplace(normal, ReadTexture(normal, directory)).first;
                if (loaded->second.id) material.maps[MATERIAL_MAP_NORMAL].texture = loaded->second;
            }
        }
        for (std::size_t m = 0; m < parts.size(); ++m) {
            data.model.meshMaterial[m] = parts[m].material;
            BuildMesh(data.model.meshes[m], parts[m], data.model.boneCount, gpu);
        }
        std::vector<const ufbx_anim_stack*> clips(scene->anim_stacks.begin(), scene->anim_stacks.end());
        // 複数のアクションがあるとき、Enemy用の歩行を先頭にする。他のクリップも保持する。
        const auto walk = std::find_if(clips.begin(), clips.end(), [](const auto* clip) { return Lower(Text(clip->name)).find("walk") != std::string::npos; });
        if (walk != clips.end()) std::rotate(clips.begin(), walk, walk + 1);
        data.animations = Allocate<ModelAnimation>(clips.size());
        data.animationCount = static_cast<int>(clips.size());
        data.bakedFrames.resize(clips.size());
        for (std::size_t clip = 0; clip < clips.size(); ++clip) {
            const auto* source = clips[clip];
            const double duration = source->time_end - source->time_begin;
            if (!std::isfinite(duration) || duration < 0 || duration / FrameDuration > (std::numeric_limits<int>::max)() - 1) throw std::runtime_error("Invalid FBX animation duration");
            ModelAnimation& animation = data.animations[clip];
            CopyName(animation.name, source->name);
            animation.boneCount = data.model.boneCount;
            const int frameCount = static_cast<int>(std::ceil(duration / FrameDuration)) + 1;
            animation.bones = Allocate<BoneInfo>(bindings.size());
            std::memcpy(animation.bones, data.model.bones, bindings.size() * sizeof(BoneInfo));
            animation.framePoses = Allocate<Transform*>(frameCount);
            animation.frameCount = frameCount;
            auto& frames = data.bakedFrames[clip];
            frames.reserve(static_cast<std::size_t>(animation.frameCount) * bindings.size());
            for (int frame = 0; frame < animation.frameCount; ++frame) {
                const double time = source->time_begin + (std::min)(duration, frame * FrameDuration);
                ScenePtr evaluated(ufbx_evaluate_scene(scene.get(), source->anim, time, nullptr, &error), ufbx_free_scene);
                if (!evaluated) throw std::runtime_error(ErrorText(error));
                animation.framePoses[frame] = Allocate<Transform>(bindings.size());
                for (std::size_t bone = 0; bone < bindings.size(); ++bone) {
                    const auto& binding = bindings[bone];
                    const auto* node = evaluated->nodes[binding.node];
                    const ufbx_matrix toWorld = binding.rigid ? node->geometry_to_world : ufbx_matrix_mul(&node->node_to_world, &binding.toBone);
                    const Matrix matrix = MatrixFrom(ufbx_matrix_mul(&toWorld, &binding.fromWorld));
                    frames.push_back(matrix);
                    Transform& pose = animation.framePoses[frame][bone];
                    MatrixDecompose(matrix, &pose.translation, &pose.rotation, &pose.scale);
                }
            }
        }
        if (!data.bakedFrames.empty()) ApplyBakedAnimation(data.model, data.bakedFrames[0], 0);
        TraceLog(LOG_INFO, "FBX: [%s] Loaded directly (%i meshes, %i bindings, %i clips, %s skinning)",
                 path.c_str(), data.model.meshCount, data.model.boneCount, data.animationCount, gpu ? "GPU" : "CPU");
    } catch (const std::exception& error) {
        TraceLog(LOG_WARNING, "FBX: [%s] %s", path.c_str(), error.what());
        Release(data);
    }
    return data;
}
