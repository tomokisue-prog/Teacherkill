#include "JapaneseFont.h"
#include <vector>

JapaneseFont::~JapaneseFont()
{
    Unload();
}

bool JapaneseFont::Load(const std::string& fontPath, int fontSize)
{
    // すでにロード済みの場合は一度アンロード
    Unload();

    std::vector<int> codepoints;

    for (int i = 32; i <= 126; ++i) codepoints.push_back(i);
   
    for (int i = 0x3000; i <= 0x9FFF; ++i) {
        codepoints.push_back(i);
    }

    font_ = LoadFontEx(fontPath.c_str(), fontSize, codepoints.data(), static_cast<int>(codepoints.size()));

    if (font_.texture.id == 0)
    {
        TraceLog(LOG_ERROR, "JAPANESE_FONT: Failed to load font from %s", fontPath.c_str());
        return false;
    }

    // テクスチャーのフィルタリング設定（文字をクッキリさせる）
    SetTextureFilter(font_.texture, TEXTURE_FILTER_BILINEAR);

    TraceLog(LOG_INFO, "JAPANESE_FONT: Loaded successfully from %s", fontPath.c_str());
    return true;
}

void JapaneseFont::Unload()
{
    if (font_.texture.id != 0)
    {
        UnloadFont(font_);
        font_ = {};
    }
}

void JapaneseFont::DrawText(const std::string& text, int posX, int posY, float fontSize, Color color) const
{
    if (!IsLoaded()) return;

    Vector2 position = { static_cast<float>(posX), static_cast<float>(posY) };
    float spacing = 1.0f; // 文字間隔

    DrawTextEx(font_, text.c_str(), position, fontSize, spacing, color);
}

Vector2 JapaneseFont::MeasureText(const std::string& text, float fontSize) const
{
    if (!IsLoaded()) return { 0.0f, 0.0f };
    return MeasureTextEx(font_, text.c_str(), fontSize, 1.0f);
}