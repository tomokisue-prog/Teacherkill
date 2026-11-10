#include "ItemCatalog.h"
#include "ResourceKeys.h"

namespace {
    // Array order follows the persisted IDs in ItemType. Keep IDs fixed when adding items.
    constexpr ItemDefinitionList Definitions{ {
        {ItemType::None, "EMPTY", u8"空", nullptr, nullptr, 1.0f, false, false},
        {ItemType::Medkit, "MEDKIT", u8"回復キット", ResourceKeys::Model_Medkit,
            "Data/Image/Items/Placeables/medkit.fbx", .65f, true, true},
        {ItemType::Battery, "BATTERY", u8"電池", nullptr, nullptr, 1.0f, true, true},
        {ItemType::Key, "KEY", u8"鍵", nullptr, nullptr, 1.0f, true, true},
        {ItemType::KeyCard, "KEY CARD", u8"キーカード", ResourceKeys::Model_KeyCard,
            "Data/Image/Items/Placeables/key_card.fbx", .47f, true, true},
        {ItemType::Ammo, "AMMO", u8"弾薬", ResourceKeys::Model_Ammo,
            "Data/Image/Items/Placeables/ammo.fbx", .55f, true, false},
        {ItemType::LostItem, "LOST ITEM", u8"携帯電話（忘れ物）", ResourceKeys::Model_LostItem,
            "Data/Image/Items/Placeables/phone.fbx", .40f, true, true},
        {ItemType::Gun, "GUN", u8"銃", ResourceKeys::Model_Gun,
            "Data/Image/Items/Model_Gun.glb", .52f, true, false},
        {ItemType::AuthenticationKey, "AUTHENTICATION KEY", u8"認証キー", ResourceKeys::Model_Authentication_Key,
            "Data/Image/Items/Key_Card.glb", .47f, true, false},
        {ItemType::EmergencyStairsKey, "SERVICE ELEVATOR KEY", u8"業務用エレベーターの鍵", ResourceKeys::Model_EmergencyStairsKey,
            "Data/Image/Items/Key_Card.glb", .47f, true, false}, // 番号順を一致させる
        {ItemType::Books, "BOOKS", u8"本", ResourceKeys::Model_Books,
            "Data/Image/Items/Placeables/books.fbx", .58f, false, false},
        {ItemType::Chair1, "CHAIR 1", u8"椅子 1", ResourceKeys::Model_Chair1,
            "Data/Image/Items/Placeables/chair_1.fbx", 1.55f, false, false},
        {ItemType::Chair2, "CHAIR 2", u8"椅子 2", ResourceKeys::Model_Chair2,
            "Data/Image/Items/Placeables/chair_2.fbx", 1.4f, false, false},
        {ItemType::Computer, "COMPUTER", u8"パソコン", ResourceKeys::Model_Computer,
            "Data/Image/Items/Placeables/computer.fbx", .90f, false, false},
        {ItemType::Locker, "LOCKER", u8"ロッカー", ResourceKeys::Model_Locker,
            "Data/Image/Items/Placeables/locker.fbx", 3.0f, false, false},
        {ItemType::Whiteboard, "WHITEBOARD", u8"ホワイトボード", ResourceKeys::Model_Whiteboard,
            "Data/Image/Items/Placeables/whiteboard.fbx", 3.1f, false, false},
        {ItemType::TrashBin, "TRASH BIN", u8"ごみ箱", ResourceKeys::Model_TrashBin,
            "Data/Image/Items/Placeables/trash_bin.fbx", .85f, false, false},
        {ItemType::Table, "TABLE", u8"机", ResourceKeys::Model_Table,
            "Data/Image/Items/Placeables/table.fbx", 1.8f, false, false},
        {ItemType::HorrorTable, "HORROR TABLE", u8"ホラー調の机", ResourceKeys::Model_HorrorTable,
            "Data/Image/Items/Placeables/table_horror.fbx", 2.6f, false, false},
        {ItemType::SlideDoorProp, "SLIDE DOOR (PROP)", u8"スライドドア（装飾）", ResourceKeys::Model_SlideDoorProp,
            "Data/Image/Items/Placeables/slide_door.fbx", 3.6f, false, false},
        {ItemType::Crowbar, "CROWBAR", u8"バール", ResourceKeys::Model_Crowbar,
            "Data/Image/Items/Placeables/crowbar.glb", 1.0f, true, false},
    } };
    static_assert([] {
        for (std::size_t i = 0; i < Definitions.size(); ++i)
            if (static_cast<std::size_t>(Definitions[i].type) != i) return false;
        return true;
        }(), "The item catalog must cover every persisted item ID in order");
}

const ItemDefinitionList& GetItemDefinitions() {
    return Definitions;
}

const ItemDefinition& GetItemDefinition(ItemType type) {
    const auto index = static_cast<std::size_t>(type);
    return index < Definitions.size() ? Definitions[index] : Definitions[0];
}
