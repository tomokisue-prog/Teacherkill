#pragma once
#include "raylib.h"
#include <vector>
#include <string>

class GunTutorialUI
{
public:

	GunTutorialUI() = default;
    void Init();
    void Reset();
    void Update();
    void Draw() const;
    void End();

    void Open();
    void Close();
    bool IsOpen() const { return isOpen_; }

private:
    bool isOpen_{ false };
    int currentSlide_{ 0 };
    static constexpr int MAX_SLIDES = 2;

    // スライド用の写真テクスチャと説明文
    std::vector<Texture2D> slideTextures_;
    std::vector<std::string> slideDescriptions_;
};