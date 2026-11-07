#pragma once

#include "raylib.h"
#include <cstddef>
#include <vector>

// HUD for the supplied HP_Bar.png.
class PlayerStatusHud
{
public:
    PlayerStatusHud() = default;
    ~PlayerStatusHud() = default;

    bool Init(const char* imagePath);
    bool InitAmmoIcon(const char* imagePath);
    void End();

    bool IsReady() const { return texture_.id != 0; }

    Rectangle GetPanelRect() const;
    void DrawAmmoRow(int ammo, int reserve, Rectangle healthPanel, bool isMainArtwork) const;
    void Draw(int hp, int maxHp, float stamina, float maxStamina);

private:
    struct ArcPixel
    {
        std::size_t index;
        float progress;
    };

    void BuildArcMasks(int width, int height);
    static void DimPixel(Color& color);
    void RefreshArcs(int hp, int maxHp, float stamina, float maxStamina);

    Texture2D texture_{};
    Texture2D bulletTexture_{};
    Rectangle bulletSource_{};
    std::vector<Color> originalPixels_;
    std::vector<Color> originalPatch_;
    std::vector<Color> displayPatch_;
    std::vector<ArcPixel> healthArc_;
    std::vector<ArcPixel> staminaArc_;
    int patchX_ = 0;
    int patchY_ = 0;
    int patchWidth_ = 0;
    int patchHeight_ = 0;
    int lastHp_ = -1;
    int lastMaxHp_ = -1;
    int lastStaminaPercent_ = -1;
};