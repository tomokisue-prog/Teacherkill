#include "TitleScene.h"
#include "SceneManager.h"
#include "AudioManager.h"
#include "ResourceManager.h"

void TitleScene::Init()
{
    finished = false;
    nextScene = nullptr;
    AM().PlayBgm(BgmId::Title);
    // タイトル時はマウスカーソルを表示状態にする
    EnableCursor();
}

void TitleScene::Update(float deltaTime)
{
    // スペースキーまたは画面クリックでゲームシーンへ遷移
    if (IsKeyPressed(KEY_SPACE))
    {
        AM().PlaySe(SeId::Confirm);
        SetNextScene(SceneManager::GetInstance().GetScene(SceneID::Game));
    }
}

void TitleScene::Render() const
{
    ClearBackground({ 3, 5, 8, 255 });
    DrawText("ESCAPE SCHOOL 3D", 750, 400, 40, { 170, 180, 180, 255 });
    DrawText("PRESS SPACE TO START", 780, 500, 20, GRAY);

    const auto& jpFont = JPFont();
    if (jpFont.IsLoaded())
    {
        jpFont.DrawText(u8"学校脱出 3D", 780, 320, 58, { 220, 220, 220, 255 });
        jpFont.DrawText(u8"スペースキーを押してスタート", 730, 550, 40, { 150, 150, 150, 255 });
    }
}

void TitleScene::End()
{
    AM().StopBgm();
}