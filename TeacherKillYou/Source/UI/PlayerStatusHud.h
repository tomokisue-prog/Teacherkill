#pragma once

#include "raylib.h"
#include <cstddef>
#include <vector>

// HUD for the supplied HP_Bar.png. This class owns its texture; the Player
// initializes it once and calls End() when the gameplay scene ends.
class PlayerStatusHud
{
public:
    bool Init(const char* imagePath);

    // The supplied JPG has a white background. Remove it in memory so the
    // original bullet artwork can be drawn cleanly over the game.
    bool InitAmmoIcon(const char* imagePath);

    void End();

    bool IsReady() const { return texture_.id != 0; }

    Rectangle GetPanelRect() const;

    // A small, borderless row under the HP artwork: loaded bullets + spare
    // full magazines. A remainder is shown only for incomplete spare magazines.
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
