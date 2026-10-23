#pragma once
#include "raylib.h"
#include <string>
#include <vector>

// Preserve base-color UV selection and PBR/emission factors for the lighting shader.
// Asset-specific decorative materials can opt out of emission without changing their textures.
void ApplyGltfTextureCoordinates(Model& model, const std::string& path,
                                const std::vector<std::string>& nonEmissiveMaterials = {});

// 補助GLBから材質名を照合し、埋め込みPBR画像とUV番号を読み込む。メッシュ・骨格は変更しない。
// 戻り値は材質ごとのUV番号。対応する材質がない場合は-1。
std::vector<int> ApplyGlbMaterialCompanion(Model& model, const std::string& path,
                                         const std::vector<std::string>& materialNames);
