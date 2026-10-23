#include "MessageWindow.h"
#include "ResourceManager.h"

void MessageWindow::Open(const std::string& speaker, const std::string& text)
{
    speaker_ = speaker;
    fullText_ = text;
    isOpen_ = true;
    isCompleted_ = false;
    textTimer_ = 0.0f;
    displayCharCount_ = 0;
}


void MessageWindow::Update(float deltaTime)
{
    if (!isOpen_) return;

    const bool advance =
        IsKeyPressed(KEY_SPACE) ||
        IsMouseButtonPressed(MOUSE_BUTTON_LEFT);

    if (!isCompleted_)
    {
        textTimer_ += deltaTime;

        while (textTimer_ >= textSpeed_ &&
            displayCharCount_ < static_cast<int>(fullText_.size()))
        {
            textTimer_ -= textSpeed_;

            unsigned char c = static_cast<unsigned char>(
                fullText_[displayCharCount_]);

            int byteCount = 1;

            if ((c & 0xE0) == 0xC0)
                byteCount = 2;
            else if ((c & 0xF0) == 0xE0)
                byteCount = 3;
            else if ((c & 0xF8) == 0xF0)
                byteCount = 4;

            displayCharCount_ += byteCount;

            if (displayCharCount_ >=
                static_cast<int>(fullText_.size()))
            {
                displayCharCount_ =
                    static_cast<int>(fullText_.size());

                isCompleted_ = true;
                break;
            }
        }

        if (advance)
        {
            displayCharCount_ =
                static_cast<int>(fullText_.size());

            isCompleted_ = true;
        }
    }
    else if (advance)
    {
        Close();
    }
}

void MessageWindow::Draw() const
{
    if (!isOpen_) return;

    int screenWidth = GetScreenWidth();
    int screenHeight = GetScreenHeight();

    // ウィンドウのサイズと位置
    int winX = 100;
    int winY = screenHeight - 260;
    int winWidth = screenWidth - 200;
    int winHeight = 200;

    // ウィンドウ背景
    DrawRectangle(winX, winY, winWidth, winHeight, Fade(BLACK, 0.75f));
    // ウィンドウ枠線
    DrawRectangleLines(winX, winY, winWidth, winHeight, RAYWHITE);

    const auto& jpFont = RM().GetJapaneseFont();

    // 話者名の描画
    if (!speaker_.empty() && jpFont.IsLoaded())
    {
        DrawRectangle(winX + 20, winY - 35, 200, 35, Fade(DARKGRAY, 0.9f));
        DrawRectangleLines(winX + 20, winY - 35, 200, 35, RAYWHITE);
        jpFont.DrawText(speaker_.c_str(), winX + 30, winY - 30, 20, RAYWHITE);
    }

    // 本文の描画
    if (jpFont.IsLoaded())
    {
        std::string displayText = fullText_.substr(0, displayCharCount_);
        jpFont.DrawText(displayText.c_str(), winX + 30, winY + 30, 24, RAYWHITE);
    }

    // 全文表示完了後のアイコン
    if (isCompleted_)
    {
        jpFont.DrawText(u8"Next...", winX + winWidth - 40, winY + winHeight - 40, 20, YELLOW);
    }
}

void MessageWindow::Close()
{
    isOpen_ = false;
}