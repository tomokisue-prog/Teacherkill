#include "GameScene.h"
#include "SceneManager.h"
#include "AudioManager.h"
#include "SaveSystem.h"

void GameScene::Init()
{
    finished = false;
    nextScene = nullptr;
    AM().PlayBgm(BgmId::Game);

    // ゲームコンテキスト側の初期化（カメラコントローラーやステージの準備）
    if (gameContext)
    {
        gameContext->Init();
    }

    SaveSystem::LoadScene(*gameContext, "scene.json");
}

void GameScene::Update(float deltaTime)
{
    if (gameContext)
    {
        gameContext->Update(deltaTime);
    }

    // Bキーでタイトルに戻る（デバッグ用など）
    if (IsKeyPressed(KEY_B))
    {
        SetNextScene(SceneManager::GetInstance().GetScene(SceneID::Title));
    }
}

void GameScene::Render() const
{
    if (gameContext)
    {
        gameContext->Draw();
    }
}

void GameScene::End()
{
    AM().StopBgm();
    if (gameContext)
    {
        gameContext->End();
    }
}