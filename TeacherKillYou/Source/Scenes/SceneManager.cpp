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
    // ウィンドウやImGuiの初期設定はここで行う
    // 表示領域より大きい固定サイズで起動すると、Windowsが高さだけを縮める。
    // raylibの画面サイズとマウス座標をそろえるため、収まるサイズで作成して最大化する。
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(1280, 720, u8"学校脱出 3D");
    if (runConfig.windowed) MaximizeWindow();
    SetTargetFPS(60);
    SetExitKey(KEY_NULL); // Esc belongs to the pause menu; the window close button still exits.
    rlImGuiSetup(true);

    // ImGuiとraylibのカーソル競合を防ぐ設定
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;

    // アセットの一括ロード
    AM().Init();
    GameSettings settings;
    settings.Load();
    settings.Apply(gameContext.GetCameraController().GetPlayerCamera());
    // タイトルでは日本語フォントだけを使い、3D素材はゲーム開始時に読む。
    RM().LoadJapaneseFont();
    
    // 各シーンにコンテキストを伝搬
    titleScene.SetGameContext(&gameContext);
    gameScene.SetGameContext(&gameContext);
    goalScene.SetGameContext(&gameContext);

    systemMode_.Init(&gameContext);

    // 最初はタイトルシーンから開始
    currentScene = &titleScene;
    currentScene->Init();

}

void SceneManager::Shutdown()
{
    gameContext.End();
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
        // raylibのGPU操作は同じスレッドに保ち、素材の間に描画と入力処理を挟む。
        // 進捗は読み込み件数の割合で、残り時間の予測値ではない。
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

        // 1. シーンおよびSystemModeの更新
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
                    break;
                }
            }
        }

        // 死亡メニューにデバッグUIの描画やマウス操作を重ねない。
        const bool showDebugUI = runConfig.enableDebugUI && currentScene == &gameScene && !gameScene.IsGameOver() && !gameScene.IsPaused();
        if (showDebugUI) systemMode_.Update(deltaTime);
        AM().Update();

        // 2. 描画処理
        BeginDrawing();
        ClearBackground({ 3, 5, 8, 255 });

        // 3D/2Dシーン描画
        if (currentScene)
        {
            currentScene->Render();
        }

        // プレイ中のデバッグ表示が有効な場合だけImGuiを描画する。
        if (showDebugUI) {
            rlImGuiBegin();
            systemMode_.Draw();
            rlImGuiEnd();
        }

        EndDrawing();
    }
}