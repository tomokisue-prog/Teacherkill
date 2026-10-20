#pragma once
#include "BakedAnimation.h"
#include <string>
#include <vector>

// Store the original mesh bind transforms for animation-only imports.
struct FbxSkinBinding {
    std::string nodePath;
    Matrix toBone;
    Matrix fromWorld;
    bool rigid;
};
struct FbxAnimationData {
    ModelAnimation* animations = nullptr;
    int animationCount = 0;
    BakedAnimationFrames bakedFrames;
};

// FBXを一度だけ解析し、raylibが所有・描画・解放できるデータへ取り込む。
struct FbxModelData {
    Model model{};
    ModelAnimation* animations = nullptr;
    int animationCount = 0;
    BakedAnimationFrames bakedFrames;
    std::vector<FbxSkinBinding> bindings;
};

FbxModelData LoadFbxModel(const std::string& path, bool inPlaceLocomotion = false);

// Reuse the existing mesh/rig; no textures, geometry or GPU buffers are reloaded.
FbxAnimationData LoadFbxAnimations(const std::string& path, const Model& model,
                                   const std::vector<FbxSkinBinding>& bindings);
