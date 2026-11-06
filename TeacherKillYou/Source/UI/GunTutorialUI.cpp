#include "GunTutorialUI.h"
#include "ResourceManager.h"
#include "raymath.h"
#include <algorithm>

void GunTutorialUI::Init()
{
    Reset();

    slideDescriptions_.clear();
    slideDescriptions_.push_back(u8"【射撃の説明】\nマウス左クリックで銃を撃つことができます。\n弾が当たると敵にダメージを与えられます。");
    slideDescriptions_.push_back(u8"【リロードの説明】\n弾薬がなくなったら [R] キーを押すことで\n予備弾倉からリロードが行えます。");

    slideTextures_.resize(MAX_SLIDES);
    slideTextures_[0] = LoadTexture("Data/Image/Tutorial/ShootTutorial.png");
    slideTextures_[1] = LoadTexture("Data/Image/Tutorial/ReloadTutorial.png");
}

void GunTutorialUI::Reset()
{
    isOpen_ = false;
    currentSlide_ = 0;
}

void GunTutorialUI::Open()
{
    isOpen_ = true;
    currentSlide_ = 0;
}

void GunTutorialUI::Close()
{
    isOpen_ = false;
}

void GunTutorialUI::Update()
{
    if (!isOpen_) return;

    // 次のスライドへ（右矢印 または Dキー）
    if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D))
    {
        if (currentSlide_ < MAX_SLIDES - 1)
        {
            currentSlide_++;
        }
    }

    // 前のスライドへ（左矢印 または Aキー）
    if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A))
    {
        if (currentSlide_ > 0)
        {
            currentSlide_--;
        }
    }

    // [B] キーで閉じる
    if (IsKeyPressed(KEY_B))
    {
        Close();
    }
}

void GunTutorialUI::Draw() const
{
    if (!isOpen_) return;

    const int screenWidth = GetScreenWidth();
    const int screenHeight = GetScreenHeight();

    // 1. 背景を暗くする半透明オーバーレイ
    DrawRectangle(0, 0, screenWidth, screenHeight, Fade(BLACK, 0.75f));

    // 2. ウィンドウ全体のサイズを少し大きく確保（画面の80%幅・85%高さ）
    const float panelWidth = static_cast<float>(screenWidth) * 0.80f;
    const float panelHeight = static_cast<float>(screenHeight) * 0.85f;
    const float panelX = (screenWidth - panelWidth) * 0.5f;
    const float panelY = (screenHeight - panelHeight) * 0.5f;

    // 背景枠
    DrawRectangleRec({ panelX, panelY, panelWidth, panelHeight }, Color{ 30, 35, 40, 240 });
    DrawRectangleLinesEx({ panelX, panelY, panelWidth, panelHeight }, 3.0f, DARKGRAY);

    // 3. 写真（画像）表示枠のレイアウト
    const float marginX = panelWidth * 0.05f;
    const float imgWidth = panelWidth - (marginX * 2.0f);
    const float imgHeight = panelHeight * 0.38f; // 大きな文字の分、画像高さを少し調整
    const float imgX = panelX + marginX;
    const float imgY = panelY + panelHeight * 0.07f;

    DrawRectangle(static_cast<int>(imgX), static_cast<int>(imgY), static_cast<int>(imgWidth), static_cast<int>(imgHeight), BLACK);
    DrawRectangleLines(static_cast<int>(imgX), static_cast<int>(imgY), static_cast<int>(imgWidth), static_cast<int>(imgHeight), LIGHTGRAY);

    if (currentSlide_ < static_cast<int>(slideTextures_.size()) && slideTextures_[currentSlide_].id != 0)
    {
        const Texture2D& tex = slideTextures_[currentSlide_];
        DrawTexturePro(
            tex,
            { 0.0f, 0.0f, static_cast<float>(tex.width), static_cast<float>(tex.height) },
            { imgX, imgY, imgWidth, imgHeight },
            { 0.0f, 0.0f }, 0.0f, WHITE
        );
    }
    else
    {
        // 画像がない場合のフォールバックテキスト
        const float fontSizeNoImg = 36.0f;
        std::string noImgText = "No Image Available";
        float textW = JPFont().MeasureText(noImgText, fontSizeNoImg).x;
        JPFont().DrawText(noImgText, static_cast<int>(imgX + (imgWidth - textW) * 0.5f), static_cast<int>(imgY + imgHeight * 0.5f - 18.0f), fontSizeNoImg, LIGHTGRAY);
    }

    // 4. 説明枠のレイアウト（48pxの文字が3行収まるように高さを大きく確保）
    const float descWidth = imgWidth;
    const float descHeight = panelHeight * 0.35f;
    const float descX = imgX;
    const float descY = imgY + imgHeight + panelHeight * 0.03f;

    DrawRectangle(static_cast<int>(descX), static_cast<int>(descY), static_cast<int>(descWidth), static_cast<int>(descHeight), Color{ 20, 22, 25, 255 });
    DrawRectangleLines(static_cast<int>(descX), static_cast<int>(descY), static_cast<int>(descWidth), static_cast<int>(descHeight), LIGHTGRAY);

    if (currentSlide_ < static_cast<int>(slideDescriptions_.size()))
    {
        // ★ メイン説明文のフォントサイズを 48.0f に設定
        const float descFontSize = 48.0f;
        JPFont().DrawText(slideDescriptions_[currentSlide_], static_cast<int>(descX + 25), static_cast<int>(descY + 20), descFontSize, RAYWHITE);
    }

    // 5. ガイド・ボタン・インジケーターの描画
    const float bottomY = panelY + panelHeight - 50.0f;

    // [B]キーで閉じる案内（右上）
    JPFont().DrawText(u8"[B] キーで閉じる", static_cast<int>(panelX + panelWidth - 280), static_cast<int>(panelY + 18), 32, YELLOW);

    // ページインジケーター（中央下）
    const std::string pageText = TextFormat("%d / %d", currentSlide_ + 1, MAX_SLIDES);
    float pageTextWidth = JPFont().MeasureText(pageText, 36).x;
    JPFont().DrawText(pageText, static_cast<int>(panelX + panelWidth * 0.5f - pageTextWidth * 0.5f), static_cast<int>(bottomY), 36, LIGHTGRAY);

    // 左右の切り替え矢印ガイド
    if (currentSlide_ > 0)
    {
        JPFont().DrawText(u8"< [A] 前の説明", static_cast<int>(panelX + marginX), static_cast<int>(bottomY), 32, SKYBLUE);
    }
    if (currentSlide_ < MAX_SLIDES - 1)
    {
        JPFont().DrawText(u8"次の説明 [D] >", static_cast<int>(panelX + panelWidth - marginX - 240), static_cast<int>(bottomY), 32, SKYBLUE);
    }
}

void GunTutorialUI::End()
{
    for (auto& tex : slideTextures_)
    {
        if (tex.id != 0)
        {
            UnloadTexture(tex);
        }
    }
    slideTextures_.clear();
    slideDescriptions_.clear();
}