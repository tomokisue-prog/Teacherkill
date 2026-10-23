#pragma once
#include "CheckpointData.h"
#include <string>

class CheckpointSave {
public:
    static bool Save(const CheckpointData& data, const std::string& path = "Data/Save/checkpoint.json");
    static bool Load(CheckpointData& data, const std::string& path = "Data/Save/checkpoint.json");
};
