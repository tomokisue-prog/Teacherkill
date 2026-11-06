#pragma once
#include "raylib.h"
#include <string>
#include <vector>

void ApplyItemMaterials(Model& model, const std::string& resourceKey,
                        const std::vector<std::string>& materialNames);
