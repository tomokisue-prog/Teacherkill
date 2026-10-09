#include "BakedAnimation.h"
#include "raymath.h"
#include "rlgl.h"

#include <cmath>

void ApplyBakedAnimation(Model model, const std::vector<Matrix>& matrices, int frame) {
    if (model.boneCount <= 0 || matrices.empty()) return;
    const int frames = static_cast<int>(matrices.size() / model.boneCount);
    if (frames == 0) return;
    frame = frame < 0 ? 0 : frame % frames;
    const Matrix* pose = matrices.data() + static_cast<std::size_t>(frame) * model.boneCount;
    std::vector<Matrix> normalMatrices;
    unsigned int lastGpuShader = 0;
    for (int m = 0; m < model.meshCount; ++m) {
        Mesh& mesh = model.meshes[m];
        if (!mesh.boneIds || !mesh.boneWeights) continue;
        if (mesh.boneMatrices) {
            for (int bone = 0; bone < mesh.boneCount && bone < model.boneCount; ++bone) mesh.boneMatrices[bone] = pose[bone];
        }
        const Shader shader = model.materials[model.meshMaterial[m]].shader;
        if (shader.locs && shader.locs[SHADER_LOC_BONE_MATRICES] >= 0 && mesh.boneMatrices) {
            // 大きいFBXでは頂点をCPUで毎フレーム走査せず、GPUへ変形行列だけ送る。
            if (shader.id != lastGpuShader) {
                rlEnableShader(shader.id);
                rlSetUniformMatrices(shader.locs[SHADER_LOC_BONE_MATRICES], mesh.boneMatrices, mesh.boneCount);
                rlDisableShader();
                lastGpuShader = shader.id;
            }
            continue;
        }
        if (!mesh.animVertices) continue;
        if (normalMatrices.empty()) {
            normalMatrices.resize(model.boneCount);
            for (int bone = 0; bone < model.boneCount; ++bone) {
                // CPUで描画するメッシュだけ、法線用の逆転置行列を一度計算する。
                normalMatrices[bone] = std::fabs(MatrixDeterminant(pose[bone])) > 0.00000001f
                    ? MatrixTranspose(MatrixInvert(pose[bone])) : MatrixIdentity();
                normalMatrices[bone].m12 = normalMatrices[bone].m13 = normalMatrices[bone].m14 = 0;
            }
        }
        for (int vertex = 0; vertex < mesh.vertexCount; ++vertex) {
            const int offset = vertex * 3;
            const Vector3 original = { mesh.vertices[offset], mesh.vertices[offset + 1], mesh.vertices[offset + 2] };
            const Vector3 originalNormal = mesh.normals
                ? Vector3{ mesh.normals[offset], mesh.normals[offset + 1], mesh.normals[offset + 2] } : Vector3{};
            Vector3 position = {}, normal = {};
            float totalWeight = 0;
            for (int influence = 0; influence < 4; ++influence) {
                const int index = vertex * 4 + influence;
                const int bone = mesh.boneIds[index];
                const float weight = mesh.boneWeights[index];
                if (weight <= 0 || bone >= model.boneCount) continue;
                position = Vector3Add(position, Vector3Scale(Vector3Transform(original, pose[bone]), weight));
                normal = Vector3Add(normal, Vector3Scale(Vector3Transform(originalNormal, normalMatrices[bone]), weight));
                totalWeight += weight;
            }
            if (totalWeight == 0) { position = original; normal = originalNormal; }
            else normal = Vector3Normalize(normal);
            mesh.animVertices[offset] = position.x;
            mesh.animVertices[offset + 1] = position.y;
            mesh.animVertices[offset + 2] = position.z;
            if (mesh.animNormals) {
                mesh.animNormals[offset] = normal.x;
                mesh.animNormals[offset + 1] = normal.y;
                mesh.animNormals[offset + 2] = normal.z;
            }
        }
        const int bytes = mesh.vertexCount * 3 * static_cast<int>(sizeof(float));
        UpdateMeshBuffer(mesh, 0, mesh.animVertices, bytes, 0);
        if (mesh.normals && mesh.animNormals) UpdateMeshBuffer(mesh, 2, mesh.animNormals, bytes, 0);
    }
}
