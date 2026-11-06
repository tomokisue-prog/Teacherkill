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

    // 説明文のみ保持
    std::vector<std::string> slideDescriptions_;
};