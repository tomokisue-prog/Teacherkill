#pragma once
#include "raylib.h"
#include <string>

class JapaneseFont
{
public:
    JapaneseFont() = default;
    ~JapaneseFont();

    // フォントのロード
    bool Load(const std::string& fontPath, int fontSize);

    // 破棄
    void Unload();

    // 日本語文字列の描画
    void DrawText(const std::string& text, int posX, int posY, float fontSize, Color color) const;

    // 文字列の幅や高さを取得する場合
    Vector2 MeasureText(const std::string& text, float fontSize) const;

    bool IsLoaded() const { return font_.texture.id != 0; }

private:
    Font font_{};
};