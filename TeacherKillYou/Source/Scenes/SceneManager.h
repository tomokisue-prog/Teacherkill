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

    SceneManager(const SceneManager&) = delete;
    SceneManager& operator=(const SceneManager&) = delete;

    struct RunConfig
    {
        bool windowed = true;
        bool enableDebugUI = true;
    };
    void SetRunConfig(const RunConfig& cfg) { runConfig = cfg; }
    void ChangeScene(SceneID id);
    // イントロ再生前にゲーム素材を読み込む
    bool PrepareGameResources() { return LoadGameResources(); }
private:
    SceneManager() = default;
    ~SceneManager() = default;

    // 初回だけロード画面を出しながら3D素材を読む
    bool LoadGameResources();

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