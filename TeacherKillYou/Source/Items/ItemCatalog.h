#pragma once
#include "Item.h"
#include <array>

// Model imports, drawing and the placement editor share the same item definition.
// A missing modelKey means that the item is currently inventory-only.
struct ItemDefinition {
    ItemType type;
    const char* name;
    const char* displayName;
    const char* modelKey;
    const char* modelPath;
    float worldSize;
    bool pickup;
    bool storedInInventory;
};

using ItemDefinitionList = std::array<ItemDefinition, static_cast<std::size_t>(ItemType::Count)>;
const ItemDefinitionList& GetItemDefinitions();
const ItemDefinition& GetItemDefinition(ItemType type);
