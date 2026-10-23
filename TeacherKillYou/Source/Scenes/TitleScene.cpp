#include "TitleScene.h"
#include "SceneManager.h"
#include "AudioManager.h"
#include "ResourceManager.h"
#include "CheckpointSave.h"
#include <algorithm>

namespace {
    void TitleText(const char* jp, const char* en, float y, float size, Color color) {
        const auto& font = JPFont();
        if (font.IsLoaded()) font.DrawText(jp, int((GetScreenWidth() - font.MeasureText(jp, size).x) / 2), int(y), size, color);
        else DrawText(en, (GetScreenWidth() - MeasureText(en, int(size))) / 2, int(y), int(size), color);
    }
}

void TitleScene::Init()
{
    finished = false;
    nextScene = nullptr;
    CheckpointData checkpoint;
    checkpointAvailable_ = CheckpointSave::Load(checkpoint);
    AM().PlayBgm(BgmId::Title);
    EnableCursor();
}

void TitleScene::Update(float deltaTime)
{
    if (IsKeyPressed(KEY_SPACE)) {
        AM().PlaySe(SeId::Confirm);
        SetNextScene(SM().GetScene(SceneID::Game));
    }
    else if (checkpointAvailable_ && IsKeyPressed(KEY_C)) {
        static_cast<GameScene*>(SM().GetScene(SceneID::Game))->RequestCheckpointContinue();
        SetNextScene(SM().GetScene(SceneID::Game));
    }
    else if (IsKeyPressed(KEY_ESCAPE)) { finished = true; nextScene = nullptr; }
}

void TitleScene::Render() const
{
    ClearBackground({3, 5, 8, 255});
    const float s = (std::max)(.5f, (std::min)(GetScreenWidth() / 1920.0f, GetScreenHeight() / 1080.0f));
    TitleText(u8"学校脱出 3D", "ESCAPE SCHOOL 3D", GetScreenHeight() * .30f, 64 * s, {220, 230, 222, 255});
    TitleText(u8"スペースキー：最初から開始", "SPACE: NEW GAME", GetScreenHeight() * .50f, 34 * s, {175, 191, 180, 255});
    TitleText(checkpointAvailable_ ? u8"Cキー：チェックポイントから再開" : u8"チェックポイントはまだありません",
        checkpointAvailable_ ? "C: CONTINUE CHECKPOINT" : "NO CHECKPOINT SAVED", GetScreenHeight() * .59f, 30 * s,
        checkpointAvailable_ ? Color{175, 210, 183, 255} : Color{91, 103, 96, 255});
    TitleText(u8"Esc：終了", "Esc: EXIT", GetScreenHeight() * .75f, 22 * s, {128, 140, 132, 255});
}

void TitleScene::End() { AM().StopBgm(); }
