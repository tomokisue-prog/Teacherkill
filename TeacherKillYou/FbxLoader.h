#pragma once
#include "BakedAnimation.h"
#include <string>

// FBXを一度だけ解析し、raylibが所有・描画・解放できるデータへ取り込む。
struct FbxModelData {
    Model model{};
    ModelAnimation* animations = nullptr;
    int animationCount = 0;
    BakedAnimationFrames bakedFrames;
};

FbxModelData LoadFbxModel(const std::string& path);
