#include "GoalScene.h"
#include "SceneManager.h"
#include "AudioManager.h"

void GoalScene::Init()
{
    finished = false;
    nextScene = nullptr;
    // クリア時のBGMや効果音があればここで再生できます
    // AM().PlayBgm(BgmId::Goal);

    // ゴール画面ではマウスカーソルを表示状態にする
    EnableCursor();
}

void GoalScene::Update(float deltaTime)
{
    // スペースキーまたはタイトルに戻る処理など
    if (IsKeyPressed(KEY_SPACE))
    {
        AM().PlaySe(SeId::Confirm);
        // タイトルへ戻る場合
        SetNextScene(SceneManager::GetInstance().GetScene(SceneID::Title));
    }
}

void GoalScene::Render() const
{
    ClearBackground({ 10, 20, 30, 255 });
    DrawText("CLEAR", 880, 400, 50, { 255, 220, 50, 255 });
    DrawText("PRESS SPACE TO RETURN TITLE", 760, 500, 20, GRAY);
}

void GoalScene::End()
{
    // AM().StopBgm();
}