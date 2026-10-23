#pragma once
#include <string>

class GameContext;

class SaveSystem
{
public:
    // GameContext の状態を JSON ファイルへ保存
    static bool SaveScene(const GameContext& gameContext, const std::string& filepath = "scene.json");

    // JSON ファイルから GameContext へ状態を復元
    static bool LoadScene(GameContext& gameContext, const std::string& filepath = "scene.json");
};