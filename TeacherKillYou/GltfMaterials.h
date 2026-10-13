#pragma once
#include "raylib.h"
#include <string>
#include <vector>

// Preserve base-color UV selection and PBR/emission factors for the lighting shader.
// Asset-specific decorative materials can opt out of emission without changing their textures.
void ApplyGltfTextureCoordinates(Model& model, const std::string& path,
                                const std::vector<std::string>& nonEmissiveMaterials = {});
