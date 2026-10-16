#pragma once
#include "Scene.h"
#include "GameContext.h"
#include "TitleScene.h"
#include "GameScene.h"
#include "GoalScene.h"
#include "SystemMode.h"

enum class SceneID { Title, Game , Goal };

class SceneManager
{
public:
    static SceneManager& GetInstance()
    {
		static SceneManager instance;
        return instance;
    }

    void Init();
    void Shutdown();
    void Run();

    Scene* GetScene(SceneID id);

    GameContext& GetGameContext() { return gameContext; }

    // コピー禁止
    SceneManager(const SceneManager&) = delete;
    SceneManager& operator=(const SceneManager&) = delete;

    // 実行コンフィグ構造体とセッター
    struct RunConfig
    {
        bool windowed = true;
        bool enableDebugUI = true;
    };
    void SetRunConfig(const RunConfig& cfg) { runConfig = cfg; }
    void ChangeScene(SceneID id);
private:
    SceneManager() = default;
    ~SceneManager() = default;

    GameContext gameContext;
    TitleScene  titleScene{ &gameContext };
    GameScene   gameScene{ &gameContext };
    GoalScene   goalScene{ &gameContext };

    SystemMode systemMode_;
    Scene* currentScene = nullptr;

    RunConfig runConfig{};

    bool hasLoadedScene_{ false };
};

inline SceneManager& SM() { return SceneManager::GetInstance(); }