#include "GameContext.h"
#include "ResourceManager.h"
#include "../Events/EventManager.h"
#include "raymath.h"
#include "rlgl.h"
#include <cmath>

static void DrawCrosshair()
{
    const int x = GetScreenWidth() / 2;
    const int y = GetScreenHeight() / 2;

    const int gap = 5;
    const int length = 10;
    const int thickness = 2;

    DrawRectangle(x - thickness / 2, y - gap - length,
        thickness, length, WHITE);

    DrawRectangle(x - thickness / 2, y + gap,
        thickness, length, WHITE);

    DrawRectangle(x - gap - length, y - thickness / 2,
        length, thickness, WHITE);

    DrawRectangle(x + gap, y - thickness / 2,
        length, thickness, WHITE);

    DrawCircle(x, y, 2.0f, WHITE);
}
static void DrawHitMarker(bool isKill)
{
    const float x = GetScreenWidth() * 0.5f;
    const float y = GetScreenHeight() * 0.5f;

    const float inner = 7.0f;
    const float outer = 14.0f;
    const float thickness = 2.0f;

    const Color color = isKill ? RED : WHITE;

    DrawLineEx(
        { x - outer, y - outer },
        { x - inner, y - inner },
        thickness, color);

    DrawLineEx(
        { x + inner, y - inner },
        { x + outer, y - outer },
        thickness, color);

    DrawLineEx(
        { x - outer, y + outer },
        { x - inner, y + inner },
        thickness, color);

    DrawLineEx(
        { x + inner, y + inner },
        { x + outer, y + outer },
        thickness, color);
}

void GameContext::Draw() const
{
    const bool playerView =
        cameraController_.GetActiveType() == CameraType::Player;

    lighting_.BeginFrame(
        cameraController_.GetActiveRaylibCamera(),
        cameraController_.GetPlayerCamera().GetRaylibCamera(),
        stage_.GetPosition(),
        stage_.GetScale(),
        !playerView
    );

    BeginMode3D(cameraController_.GetActiveRaylibCamera());

    if (!playerView)
    {
        DrawGrid(20, 1.0f);
    }

    stage_.Draw();

    if (!playerView)
    {
        for (const auto& tp : teleportPoints_)
        {
            tp.Draw();
        }

        for (const auto& stp : securityTeleportPoints_)
        {
            stp.Draw();
        }

        goalPoint_.Draw();

        for (const auto& ep : eventDialoguePoints_)
        {
            ep.Draw();
        }
    }

    const PlayerCamera* activePlayerCam = nullptr;

    if (playerView)
    {
        activePlayerCam =
            &cameraController_.GetPlayerCamera();
    }

    for (const auto& d : doors_)
    {
        d.Draw();
    }

    player_.Draw(activePlayerCam);
    if (playerView) {
        enemyManager_.Draw();
    }
    else {
        // エディタ（Hierarchy / Inspector）で編集している本物のスポーン設定位置に敵を描画
        for (const auto& spawnPoint : enemyManager_.GetSpawnPoints())
            demo::Enemy(0, spawnPoint.config, spawnPoint.GetPosition(), spawnPoint.yaw).Draw();
    }
    DrawBulletMarks();
    DrawShotEffects();
    inventory_.DrawWorld(!playerView);
    bloodEffect_.Draw(cameraController_.GetActiveRaylibCamera());

    securityPC_.Draw();

    EndMode3D();
    lighting_.EndFrame();

    // ==================================
    // 2D HUD
    // ==================================
    if (playerView && player_.IsAlive())
    {
        lowHpBlood_.Draw();
        player_.DrawDamageOverlay();
        DrawCrosshair();

        if (hitMarkerTimer_ > 0.0f)
        {
            DrawHitMarker(hitMarkerKill_);
        }

        player_.DrawHealthHUD();
        player_.DrawStaminaHUD();
        player_.DrawAmmoHUD();
        inventory_.DrawHUD();

        const DoorMove* nearestDoor = nullptr;
        float minDoorDist = 999.0f;
        for (const auto& d : doors_)
        {
            if (d.IsPlayerInInteractionRange(player_))
            {
                float dist = Vector3Distance(player_.GetPosition(), d.GetPosition());
                if (dist < minDoorDist)
                {
                    minDoorDist = dist;
                    nearestDoor = &d;
                }
            }
        }

        if (nearestDoor)
        {
            std::string prompt = "";
            bool needsUnlock = false;

            if (nearestDoor->GetLockType() == DoorLockType::None)
            {
                needsUnlock = (nearestDoor->GetOpenConditionType() != DoorOpenConditionType::None && !nearestDoor->IsOpenConditionSatisfied());
            }
            else
            {
                needsUnlock = (nearestDoor->IsLocked(&inventory_) && nearestDoor->GetOpenConditionType() != DoorOpenConditionType::None && !nearestDoor->IsOpenConditionSatisfied());
            }

            if (needsUnlock)
            {
                if (nearestDoor->GetOpenConditionType() == DoorOpenConditionType::SecurityTeleportEntered)
                {
                    prompt = "Locked (Requires Security Teleport)";
                }
                else if (nearestDoor->GetOpenConditionType() == DoorOpenConditionType::SecurityPCActivated)
                {
                    prompt = "Locked (Requires Security PC)";
                }
                else
                {
                    prompt = "[M] Unlock";
                }
            }
            else
            {
                prompt = nearestDoor->IsOpen() ? "[E] Close" : "[E] Open";
            }

            if (!prompt.empty())
            {
                const float fontSize = 36.0f;
                Vector2 textSize = JPFont().MeasureText(prompt, fontSize);
                const int x = static_cast<int>((GetScreenWidth() - textSize.x) / 2.0f);
                const int y = static_cast<int>((GetScreenHeight() / 2.0f) + 30.0f);

                JPFont().DrawText(prompt, x + 2, y + 2, fontSize, BLACK);
                JPFont().DrawText(prompt, x, y, fontSize, RAYWHITE);
            }
        }

        if (securityTeleportUnlocked_)
        {
            for (const auto& stp : securityTeleportPoints_)
            {
                if (stp.GetTeleportType() == SecurityTeleportType::Source &&
                    !stp.IsTriggered())
                {
                    Vector3 worldPos = stp.GetPosition();
                    worldPos.y += stp.GetRadius() * 0.5f;

                    const Camera3D& currentCam = cameraController_.GetPlayerCamera().GetRaylibCamera();
                    Vector2 screenPos = GetWorldToScreen(worldPos, currentCam);

                    Vector3 toPoint = Vector3Subtract(worldPos, currentCam.position);
                    float forwardDot = Vector3DotProduct(
                        Vector3Normalize(Vector3Subtract(currentCam.target, currentCam.position)),
                        Vector3Normalize(toPoint)
                    );

                    if (forwardDot > 0.0f &&
                        screenPos.x >= 0 && screenPos.x <= GetScreenWidth() &&
                        screenPos.y >= 0 && screenPos.y <= GetScreenHeight())
                    {
                        float distance = Vector3Length(toPoint);
                        float baseSize = 40.0f;
                        float scaleFactor = Clamp(6.0f / (distance + 1.0f), 0.5f, 1.5f);
                        float size = baseSize * scaleFactor;

                        float floatOffset = sinf(static_cast<float>(GetTime()) * 4.0f) * 4.0f * scaleFactor;
                        float centerY = screenPos.y + floatOffset;

                        Vector2 tip = { screenPos.x, centerY + size * 0.5f };
                        Vector2 topLeft = { screenPos.x - size * 0.45f, centerY - size * 0.5f };
                        Vector2 topRight = { screenPos.x + size * 0.45f, centerY - size * 0.5f };

                        DrawTriangle(topLeft, tip, topRight, GREEN);
                        DrawTriangleLines(topLeft, tip, topRight, BLUE);
                    }
                }
            }
        }

        if (showGunPin_)
        {
            Vector3 worldPos = inventory_.GetGunPosition();
            worldPos.y += 0.5f;

            const Camera3D& currentCam = cameraController_.GetPlayerCamera().GetRaylibCamera();
            Vector2 screenPos = GetWorldToScreen(worldPos, currentCam);

            Vector3 toPoint = Vector3Subtract(worldPos, currentCam.position);
            float forwardDot = Vector3DotProduct(
                Vector3Normalize(Vector3Subtract(currentCam.target, currentCam.position)),
                Vector3Normalize(toPoint)
            );

            if (forwardDot > 0.0f &&
                screenPos.x >= 0 && screenPos.x <= GetScreenWidth() &&
                screenPos.y >= 0 && screenPos.y <= GetScreenHeight())
            {
                float distance = Vector3Length(toPoint);
                float baseSize = 40.0f;
                float scaleFactor = Clamp(6.0f / (distance + 1.0f), 0.5f, 1.5f);
                float size = baseSize * scaleFactor;

                float floatOffset = sinf(static_cast<float>(GetTime()) * 4.0f) * 4.0f * scaleFactor;
                float centerY = screenPos.y + floatOffset;

                Vector2 tip = { screenPos.x, centerY + size * 0.5f };
                Vector2 topLeft = { screenPos.x - size * 0.45f, centerY - size * 0.5f };
                Vector2 topRight = { screenPos.x + size * 0.45f, centerY - size * 0.5f };

                DrawTriangle(topLeft, tip, topRight, YELLOW);
                DrawTriangleLines(topLeft, tip, topRight, ORANGE);
            }
        }

        if (showPCPin_)
        {
            Vector3 worldPos = securityPC_.GetPosition();
            worldPos.y += 0.8f;

            const Camera3D& currentCam = cameraController_.GetPlayerCamera().GetRaylibCamera();
            Vector2 screenPos = GetWorldToScreen(worldPos, currentCam);

            Vector3 toPoint = Vector3Subtract(worldPos, currentCam.position);
            float forwardDot = Vector3DotProduct(
                Vector3Normalize(Vector3Subtract(currentCam.target, currentCam.position)),
                Vector3Normalize(toPoint)
            );

            if (forwardDot > 0.0f &&
                screenPos.x >= 0 && screenPos.x <= GetScreenWidth() &&
                screenPos.y >= 0 && screenPos.y <= GetScreenHeight())
            {
                float distance = Vector3Length(toPoint);
                float baseSize = 40.0f;
                float scaleFactor = Clamp(6.0f / (distance + 1.0f), 0.5f, 1.5f);
                float size = baseSize * scaleFactor;

                float floatOffset = sinf(static_cast<float>(GetTime()) * 4.0f) * 4.0f * scaleFactor;
                float centerY = screenPos.y + floatOffset;

                Vector2 tip = { screenPos.x, centerY + size * 0.5f };
                Vector2 topLeft = { screenPos.x - size * 0.45f, centerY - size * 0.5f };
                Vector2 topRight = { screenPos.x + size * 0.45f, centerY - size * 0.5f };

                DrawTriangle(topLeft, tip, topRight, SKYBLUE);
                DrawTriangleLines(topLeft, tip, topRight, BLUE);
            }
        }

        if (showAuthKeyPin_)
        {
            Vector3 worldPos = inventory_.GetAuthenticationKeyPosition();
            worldPos.y += 0.5f;

            const Camera3D& currentCam = cameraController_.GetPlayerCamera().GetRaylibCamera();
            Vector2 screenPos = GetWorldToScreen(worldPos, currentCam);

            Vector3 toPoint = Vector3Subtract(worldPos, currentCam.position);
            float forwardDot = Vector3DotProduct(
                Vector3Normalize(Vector3Subtract(currentCam.target, currentCam.position)),
                Vector3Normalize(toPoint)
            );

            if (forwardDot > 0.0f &&
                screenPos.x >= 0 && screenPos.x <= GetScreenWidth() &&
                screenPos.y >= 0 && screenPos.y <= GetScreenHeight())
            {
                float distance = Vector3Length(toPoint);
                float baseSize = 40.0f;
                float scaleFactor = Clamp(6.0f / (distance + 1.0f), 0.5f, 1.5f);
                float size = baseSize * scaleFactor;

                float floatOffset = sinf(static_cast<float>(GetTime()) * 4.0f) * 4.0f * scaleFactor;
                float centerY = screenPos.y + floatOffset;

                Vector2 tip = { screenPos.x, centerY + size * 0.5f };
                Vector2 topLeft = { screenPos.x - size * 0.45f, centerY - size * 0.5f };
                Vector2 topRight = { screenPos.x + size * 0.45f, centerY - size * 0.5f };

                DrawTriangle(topLeft, tip, topRight, GREEN);
                DrawTriangleLines(topLeft, tip, topRight, DARKGREEN);
            }
        }

        if (showSecurityDoorPin_)
        {
            Vector3 worldPos{};
            bool foundDoor = false;
            for (const auto& d : doors_)
            {
                if (d.GetOpenConditionType() == DoorOpenConditionType::SecurityPCActivated)
                {
                    worldPos = d.GetPosition();
                    worldPos.y += 1.0f;
                    foundDoor = true;
                    break;
                }
            }

            if (foundDoor)
            {
                const Camera3D& currentCam = cameraController_.GetPlayerCamera().GetRaylibCamera();
                Vector2 screenPos = GetWorldToScreen(worldPos, currentCam);

                Vector3 toPoint = Vector3Subtract(worldPos, currentCam.position);
                float forwardDot = Vector3DotProduct(
                    Vector3Normalize(Vector3Subtract(currentCam.target, currentCam.position)),
                    Vector3Normalize(toPoint)
                );

                if (forwardDot > 0.0f &&
                    screenPos.x >= 0 && screenPos.x <= GetScreenWidth() &&
                    screenPos.y >= 0 && screenPos.y <= GetScreenHeight())
                {
                    float distance = Vector3Length(toPoint);
                    float baseSize = 40.0f;
                    float scaleFactor = Clamp(6.0f / (distance + 1.0f), 0.5f, 1.5f);
                    float size = baseSize * scaleFactor;

                    float floatOffset = sinf(static_cast<float>(GetTime()) * 4.0f) * 4.0f * scaleFactor;
                    float centerY = screenPos.y + floatOffset;

                    Vector2 tip = { screenPos.x, centerY + size * 0.5f };
                    Vector2 topLeft = { screenPos.x - size * 0.45f, centerY - size * 0.5f };
                    Vector2 topRight = { screenPos.x + size * 0.45f, centerY - size * 0.5f };

                    DrawTriangle(topLeft, tip, topRight, MAGENTA);
                    DrawTriangleLines(topLeft, tip, topRight, PURPLE);
                }
            }
        }
    }

    if (playerView) {
        gameFade_.Draw();
        EventMgr().Draw();
        if (player_.IsAlive() && IsMessageActive())
            messageWindow_.Draw();
    }
    gunTutorialUI_.Draw();
}

void GameContext::DrawShotEffects() const
{
    if (cameraController_.GetActiveType() != CameraType::Player)
    {
        return;
    }

    const Camera3D camera =
        cameraController_.GetPlayerCamera().GetRaylibCamera();

    const Color flashColor = { 255, 225, 140, 255 };
    const Color tracerColor = { 255, 220, 110, 255 };

    rlDrawRenderBatchActive();
    rlDisableDepthMask();

    BeginBlendMode(BLEND_ADDITIVE);

    if (tracerActive_ && tracerDuration_ > 0.0f)
    {
        const float progress = Clamp(
            tracerAge_ / tracerDuration_, 0.0f, 1.0f);

        const float head = Clamp(
            progress * 1.25f + 0.15f, 0.0f, 1.0f);

        const float tail = Clamp(
            progress * 1.25f - 0.10f, 0.0f, 1.0f);

        const Vector3 start =
            Vector3Lerp(tracerStart_, tracerEnd_, tail);

        const Vector3 end =
            Vector3Lerp(tracerStart_, tracerEnd_, head);

        DrawLine3D(start, end, tracerColor);
    }

    if (muzzleFlashTimer_ > 0.0f &&
        muzzleFlashTexture_.id != 0 &&
        player_.IsAlive())
    {
        const float frameWidth =
            muzzleFlashTexture_.width / 4.0f;

        const Rectangle source = {
            muzzleFlashFrame_ * frameWidth,
            0.0f,
            frameWidth,
            static_cast<float>(muzzleFlashTexture_.height)
        };

        const Vector3 muzzle =
            player_.GetMuzzlePosition(camera);

        DrawBillboardRec(
            camera,
            muzzleFlashTexture_,
            source,
            muzzle,
            { 0.12f, 0.12f },
            flashColor);
    }

    EndBlendMode();

    rlDrawRenderBatchActive();
    rlEnableDepthMask();
}

void GameContext::DrawBulletMarks() const
{
    constexpr int segments = 16;
    constexpr float outerRadius = 0.075f;
    constexpr float innerRadius = 0.038f;

    for (const BulletMark& mark : bulletMarks_)
    {
        const float fade = Clamp(
            mark.remainingTime / bulletMarkLifetime_,
            0.0f, 1.0f);

        if (fade <= 0.0f)
            continue;

        if (Vector3LengthSqr(mark.normal) < 0.0001f)
            continue;

        const Vector3 normal = Vector3Normalize(mark.normal);

        const Vector3 reference = std::fabs(normal.y) > 0.9f
            ? Vector3{ 1.0f, 0.0f, 0.0f }
        : Vector3{ 0.0f, 1.0f, 0.0f };

        const Vector3 tangent = Vector3Normalize(
            Vector3CrossProduct(reference, normal));
        const Vector3 bitangent = Vector3CrossProduct(normal, tangent);

        auto drawDisc = [&](float radius, float offset, Color color)
            {
                const Vector3 center = Vector3Add(
                    mark.position, Vector3Scale(normal, offset));

                for (int i = 0; i < segments; ++i)
                {
                    const float angleA = 2.0f * PI * i / segments;
                    const float angleB = 2.0f * PI * (i + 1) / segments;

                    const Vector3 pointA = Vector3Add(
                        center,
                        Vector3Add(
                            Vector3Scale(tangent, std::cos(angleA) * radius),
                            Vector3Scale(bitangent, std::sin(angleA) * radius)));

                    const Vector3 pointB = Vector3Add(
                        center,
                        Vector3Add(
                            Vector3Scale(tangent, std::cos(angleB) * radius),
                            Vector3Scale(bitangent, std::sin(angleB) * radius)));

                    DrawTriangle3D(center, pointA, pointB, color);
                }
            };

        drawDisc(outerRadius, 0.008f, Fade(DARKGRAY, 0.65f * fade));
        drawDisc(innerRadius, 0.011f, Fade(BLACK, 0.95f * fade));
    }
}