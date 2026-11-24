#include "SceneManager.h"
#include "raylib.h"
#include "rlImGui.h"
#include "ResourceManager.h"
#include "AudioManager.h"
#include "imgui.h"
#include <algorithm>
#include <chrono>

void SceneManager::Init()
{
    // 画面に収まるサイズで起動してから最大化
    // Windowsの自動縮小で表示とマウス座標がずれないようにする
    SetConfigFlags(runConfig.windowed ? FLAG_WINDOW_RESIZABLE : FLAG_FULLSCREEN_MODE);
    InitWindow(runConfig.windowed ? 1280 : 0, runConfig.windowed ? 720 : 0, u8"学校脱出 3D");
    if (runConfig.windowed) MaximizeWindow();
    SetTargetFPS(60);
    SetExitKey(KEY_NULL); // Escはポーズ用に使う
#ifndef NDEBUG
    rlImGuiSetLoadFontsCallback(runConfig.enableDebugUI ? DebugMenu::LoadFonts : nullptr);
#endif
    rlImGuiSetup(true);

    // ImGuiとraylibのカーソル表示が競合しないようにする
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;

    AM().Init();
    GameSettings settings;
    settings.Load();
    settings.Apply(gameContext);
    // タイトルでは日本語フォントだけ読み、3D素材は開始時にロード
    RM().LoadJapaneseFont();
    
    titleScene.SetGameContext(&gameContext);
    gameScene.SetGameContext(&gameContext);
    gameScene.SetDebugToolsEnabled(runConfig.enableDebugUI);
    goalScene.SetGameContext(&gameContext);

    systemMode_.Init(&gameContext);

    currentScene = &titleScene;
    currentScene->Init();

}

void SceneManager::Shutdown()
{
    // リソースを解放する前にシーンを終了
    if (currentScene) {
        currentScene->End();
        currentScene = nullptr;
    }
    RM().UnloadAll();
    AM().Shutdown();
    rlImGuiShutdown();
    CloseWindow();
}

Scene* SceneManager::GetScene(SceneID id)
{
    switch (id)
    {
    case SceneID::Title: return &titleScene;
    case SceneID::Game:  return &gameScene;
    case SceneID::Goal:  return &goalScene;
    }
    return &titleScene;
}

bool SceneManager::LoadGameResources()
{
    if (RM().GetGameLoadProgress() >= 1.0f) return true;
    const auto begin = std::chrono::steady_clock::now();
    EnableCursor();
    for (;;) {
        // 素材を1件読むごとに画面と入力を更新。GPU操作は同じスレッドで行う
        // 進捗は読み込み件数の割合
        if (WindowShouldClose()) return false;
        const float progress = RM().GetGameLoadProgress();
        const float scale = (std::max)(.5f, (std::min)(GetScreenWidth() / 1920.0f, GetScreenHeight() / 1080.0f));
        const char* label = u8"読み込み中...";
        const auto& font = JPFont();
        const float size = 38 * scale;
        const float width = 560 * scale;
        const float x = (GetScreenWidth() - width) * .5f;
        const float y = GetScreenHeight() * .53f;
        BeginDrawing();
        ClearBackground({3, 5, 8, 255});
        if (font.IsLoaded()) font.DrawText(label, int((GetScreenWidth() - font.MeasureText(label, size).x) * .5f), int(y - 70 * scale), size, {220, 230, 222, 255});
        else DrawText("LOADING...", int(x), int(y - 70 * scale), int(size), RAYWHITE);
        DrawRectangleRec({x, y, width, 8 * scale}, {35, 44, 40, 255});
        DrawRectangleRec({x, y, width * progress, 8 * scale}, {127, 174, 145, 255});
        const char* percent = TextFormat("%d%%", int(progress * 100));
        DrawText(percent, (GetScreenWidth() - MeasureText(percent, int(24 * scale))) / 2, int(y + 28 * scale), int(24 * scale), {175, 191, 180, 255});
        EndDrawing();
        AM().Update();
        if (progress >= 1.0f) break;
        RM().LoadNextGameResource();
    }
    TraceLog(LOG_INFO, "LOADING: Game resources ready in %.3f seconds", std::chrono::duration<double>(std::chrono::steady_clock::now() - begin).count());
    return true;
}

void SceneManager::ChangeScene(SceneID id)
{
    if (id == SceneID::Game && !LoadGameResources()) return;
    if (currentScene)
    {
        currentScene->End();
    }
    currentScene = GetScene(id);
    if (currentScene)
    {
        currentScene->Init();
    }
}

void SceneManager::Run()
{

    while (!WindowShouldClose())
    {
        float deltaTime = GetFrameTime();

        if (currentScene)
        {
            currentScene->Update(deltaTime);

            if (currentScene->IsFinished())
            {
                Scene* next = currentScene->GetNextScene();
                if (next == &gameScene && !LoadGameResources()) break;
                currentScene->End();
                if (next)
                {
                    currentScene = next;
                    currentScene->Init();
                }
                else
                {
                    // ここで終了済みなので、ShutdownでEndを重ねない
                    currentScene = nullptr;
                    break;
                }
            }
        }

        // 死亡メニュー中はデバッグUIを重ねない
        const bool showDebugUI = runConfig.enableDebugUI && currentScene == &gameScene && !gameScene.IsGameOver() && !gameScene.IsPaused();
        const bool showDebugMenu = currentScene == &gameScene && gameScene.IsDebugMenuOpen();
        if (showDebugUI) systemMode_.Update(deltaTime);
        AM().Update();

        BeginDrawing();
        ClearBackground({ 3, 5, 8, 255 });

        if (currentScene)
        {
            currentScene->Render();
        }

        // デバッグUIはプレイ中で表示が有効なときだけ描く
        if (showDebugUI || showDebugMenu) {
            rlImGuiBegin();
            if (showDebugMenu) gameScene.DrawDebugMenu();
            else systemMode_.Draw();
            rlImGuiEnd();
        }

        EndDrawing();
    }
}