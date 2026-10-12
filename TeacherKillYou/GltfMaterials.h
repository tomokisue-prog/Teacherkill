#pragma once
#include "raylib.h"
#include <string>

// Preserve base-color UV selection and PBR/emission factors for the lighting shader.
void ApplyGltfTextureCoordinates(Model& model, const std::string& path);
