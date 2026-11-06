#include "ItemMaterials.h"
#include "ResourceManager.h"
#include "raymath.h"
#include "rlgl.h"
#include <algorithm>
#include <cmath>

namespace {
    struct Surface {
        const char* model;
        const char* material; 
        Color color;
        float metalness;
        float roughness;
        bool rust = false;
    };

    const Surface Surfaces[] = {
        {ResourceKeys::Model_Books, "book cover", {134, 79, 72, 255}, 0, .92f},
        {ResourceKeys::Model_Books, "stacked paper", {228, 218, 190, 255}, 0, .96f},
        {ResourceKeys::Model_Books, "Material.002", {130, 110, 83, 255}, 0, .90f},
        {ResourceKeys::Model_Ammo, "Bullet Brass", {213, 179, 112, 255}, .65f, .58f},
        {ResourceKeys::Model_Ammo, "Material.003", {125, 133, 104, 255}, .12f, .82f},
        {ResourceKeys::Model_Ammo, "Material", {141, 146, 138, 255}, .55f, .72f},
        {ResourceKeys::Model_Medkit, "Material.002", {208, 197, 169, 255}, 0, .87f},
        {ResourceKeys::Model_Medkit, "Material.001", {156, 62, 51, 255}, 0, .85f},
        {ResourceKeys::Model_KeyCard, "Material.002", {205, 195, 167, 255}, 0, .83f},
        {ResourceKeys::Model_KeyCard, "Material.003", {143, 69, 56, 255}, 0, .80f},
        {ResourceKeys::Model_KeyCard, "Material.001", {91, 104, 89, 255}, 0, .86f},
        {ResourceKeys::Model_Chair1, "Material.001", {146, 150, 137, 255}, .40f, .79f},
        {ResourceKeys::Model_Computer, "plastic black", {205, 201, 175, 255}, 0, .84f},
        {ResourceKeys::Model_Computer, "Material.006", {65, 85, 81, 255}, 0, .48f},
        {ResourceKeys::Model_Computer, "Material.008", {132, 144, 136, 255}, .40f, .78f},
        {ResourceKeys::Model_Computer, "Material", {142, 151, 140, 255}, .28f, .77f},
        {ResourceKeys::Model_Computer, "Material.005", {57, 72, 69, 255}, .10f, .43f},
        {ResourceKeys::Model_LostItem, "basic", {173, 177, 151, 255}, 0, .79f},
        {ResourceKeys::Model_LostItem, "black", {77, 84, 77, 255}, 0, .78f},
        {ResourceKeys::Model_LostItem, "metallic", {156, 164, 153, 255}, .48f, .62f},
        {ResourceKeys::Model_LostItem, "screen", {57, 77, 71, 255}, 0, .43f},
        {ResourceKeys::Model_LostItem, "glass", {132, 150, 141, 255}, 0, .38f},
        {ResourceKeys::Model_LostItem, "glossy", {122, 131, 117, 255}, .10f, .56f},
        {ResourceKeys::Model_LostItem, "mic", {67, 76, 70, 255}, .25f, .80f},
        {ResourceKeys::Model_LostItem, "red", {148, 62, 51, 255}, 0, .74f},
        {ResourceKeys::Model_Whiteboard, "Chalk", {221, 213, 184, 255}, 0, .96f},
        {ResourceKeys::Model_Whiteboard, "Plastik Corner", {121, 132, 112, 255}, 0, .83f},
        {ResourceKeys::Model_Whiteboard, "Aluminium", {175, 182, 165, 255}, .55f, .72f},
        {ResourceKeys::Model_Whiteboard, "Aluminium.001", {175, 182, 165, 255}, .55f, .72f},
        {ResourceKeys::Model_TrashBin, nullptr, {156, 167, 137, 255}, .12f, .89f},
        {ResourceKeys::Model_Table, "Material.001", {127, 139, 121, 255}, .35f, .82f},
        {ResourceKeys::Model_HorrorTable, nullptr, WHITE, .25f, .94f, true},
        {ResourceKeys::Model_SlideDoorProp, "Material.002", {136, 145, 128, 255}, .30f, .85f},
    };

    Texture2D SurfaceTexture(bool rust) {
        const char* key = rust ? "ItemSurface_RustedSteel" : "ItemSurface_WornPaint";
        RM().LoadTexture(key, rust ? "Data/Image/Items/Materials/rusted_steel.png" :
                                   "Data/Image/Items/Materials/worn_paint.png", true);
        const auto* texture = RM().GetTexture(key);
        if (!texture) return {};
        SetTextureWrap(*texture, TEXTURE_WRAP_REPEAT);
        return *texture;
    }

    void ProjectSurfaceUVs(Mesh& mesh, BoundingBox bounds) {
        if (!mesh.vertices || !mesh.normals || !mesh.texcoords) return;
        const Vector3 size = Vector3Subtract(bounds.max, bounds.min);
        const float longest = (std::max)({size.x, size.y, size.z});
        if (longest < .0001f) return;
        const float density = 2.0f / longest;
        for (int v = 0; v < mesh.vertexCount; ++v) {
            const auto* p = mesh.vertices + v * 3;
            const auto* n = mesh.normals + v * 3;
            const float x = p[0] - bounds.min.x, y = p[1] - bounds.min.y, z = p[2] - bounds.min.z;
            const float nx = std::fabs(n[0]), ny = std::fabs(n[1]), nz = std::fabs(n[2]);
            Vector2 uv;
            if (ny >= nx && ny >= nz) uv = {x, z};
            else if (nx >= nz) uv = {z, y};
            else uv = {x, y};
            mesh.texcoords[v * 2] = uv.x * density;
            mesh.texcoords[v * 2 + 1] = uv.y * density;
        }
        UpdateMeshBuffer(mesh, 1, mesh.texcoords, mesh.vertexCount * 2 * sizeof(float), 0);
    }
}

void ApplyItemMaterials(Model& model, const std::string& resourceKey,
                        const std::vector<std::string>& materialNames) {
    if (!model.materials || !model.meshMaterial || materialNames.size() != model.materialCount) return;
    std::vector<bool> changed(model.materialCount, false);
    for (int i = 1; i < model.materialCount; ++i) {
        auto& material = model.materials[i];
        const auto albedo = material.maps[MATERIAL_MAP_ALBEDO].texture;
        if (albedo.id && albedo.id != rlGetTextureIdDefault()) continue;
        for (const auto& surface : Surfaces) {
            if (resourceKey != surface.model ||
                (surface.material && materialNames[i] != surface.material)) continue;
            const auto texture = SurfaceTexture(surface.rust);
            if (!texture.id) break;
            material.maps[MATERIAL_MAP_ALBEDO].texture = texture;
            material.maps[MATERIAL_MAP_ALBEDO].color = surface.color;
            material.maps[MATERIAL_MAP_METALNESS].value = surface.metalness;
            material.maps[MATERIAL_MAP_ROUGHNESS].value = surface.roughness;
            material.maps[MATERIAL_MAP_EMISSION].value = 0;
            material.params[2] = 0;
            changed[i] = true;
            TraceLog(LOG_INFO, "ITEM: [%s] Added worn surface to '%s'", resourceKey.c_str(), materialNames[i].c_str());
            break;
        }
    }
    if (std::find(changed.begin(), changed.end(), true) == changed.end()) return;
    const auto bounds = GetModelBoundingBox(model);
    for (int m = 0; m < model.meshCount; ++m) {
        const int material = model.meshMaterial[m];
        if (material >= 0 && material < model.materialCount && changed[material])
            ProjectSurfaceUVs(model.meshes[m], bounds);
    }
}
