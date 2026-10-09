#pragma once
#include "raylib.h"
#include <vector>

// クリップごとに、フレーム順×ボーン順の変形行列を保持する。
using BakedAnimationFrames = std::vector<std::vector<Matrix>>;

void ApplyBakedAnimation(Model model, const std::vector<Matrix>& matrices, int frame);
