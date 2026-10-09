#pragma once
#include "raylib.h"
#include <string>

// Adapt the material's base-color UV selection to raylib's default shader.
void ApplyGltfTextureCoordinates(Model& model, const std::string& path);
