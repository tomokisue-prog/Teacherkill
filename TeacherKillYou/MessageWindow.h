#pragma once
#include <string>
#include "raylib.h"

class MessageWindow
{
public:
    MessageWindow() = default;
    ~MessageWindow() = default;

    // ウィンドウを開いて話者名と本文をセットする
    void Open(const std::string& speaker, const std::string& text);

    // 毎フレームの更新（タイピングアニメーション、キー入力による送り）
    void Update(float deltaTime);

    // 描画処理（背景ウィンドウ、名前欄、日本語テキスト）
    void Draw() const;

    // ウィンドウが開いているかどうか
    bool IsOpen() const { return isOpen_; }

    // 全文字の表示が完了しているか
    bool IsCompleted() const { return isCompleted_; }

    // ウィンドウを閉じる
    void Close();

private:
    bool isOpen_{ false };       // ウィンドウが開いているか
    bool isCompleted_{ false };   // 全文字の表示が完了したか

    std::string speaker_;         // 話者名
    std::string fullText_;        // 表示する全テキスト

    float textSpeed_{ 0.03f };    // 1文字あたりの表示にかかる時間（秒）
    float textTimer_{ 0.0f };     // タイピング用のタイマー
    int displayCharCount_{ 0 };   // 現在何文字目まで表示しているか
};