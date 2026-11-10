#include "GameContext.h"
#include "ResourceManager.h"
#include "../Events/EventManager.h"
#include "raymath.h"
#include "rlgl.h"
#include <algorithm>
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

static void DrawGuidanceArrow(const Camera3D& camera, Vector3 target, Color color, Color outline)
{
    const Vector3 forward = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
    const Vector3 offset = Vector3Subtract(target, camera.position);
    const Vector2 screenCenter{GetScreenWidth() * 0.5f, GetScreenHeight() * 0.5f};
    constexpr float margin = 52.0f;
    const Vector2 edge{screenCenter.x - margin, screenCenter.y - margin};
    if (edge.x <= 0.0f || edge.y <= 0.0f) return;

    const bool ahead = Vector3DotProduct(offset, forward) > 0.0f;
    Vector2 center = ahead ? GetWorldToScreen(target, camera) : screenCenter;
    Vector2 direction{0.0f, 1.0f};
    float size = 40.0f * Clamp(6.0f / (Vector3Length(offset) + 1.0f), 0.5f, 1.5f);

    const Vector2 projected = Vector2Subtract(center, screenCenter);
    if (!ahead || std::fabs(projected.x) > edge.x || std::fabs(projected.y) > edge.y) {
        const Vector3 right = Vector3CrossProduct(forward, camera.up);
        const float side = Vector3DotProduct(offset, right) < 0.0f ? -1.0f : 1.0f;
        direction = ahead ? Vector2Normalize(projected) : Vector2{side, 0.0f};
        const float extent = (std::max)(std::fabs(direction.x) / edge.x, std::fabs(direction.y) / edge.y);
        center = Vector2Add(screenCenter, Vector2Scale(direction, 1.0f / extent));
        size = 28.0f;
    } else {
        center.y += std::sin(static_cast<float>(GetTime()) * 4.0f) * size * 0.1f;
    }

    const Vector2 tangent{direction.y, -direction.x};
    const Vector2 base = Vector2Subtract(center, Vector2Scale(direction, size * 0.5f));
    const Vector2 tip = Vector2Add(center, Vector2Scale(direction, size * 0.5f));
    const Vector2 left = Vector2Subtract(base, Vector2Scale(tangent, size * 0.45f));
    const Vector2 right = Vector2Add(base, Vector2Scale(tangent, size * 0.45f));
    DrawTriangle(left, tip, right, color);
    DrawTriangleLines(left, tip, right, outline);
}

void GameContext::DrawGuidance(const Camera3D& camera) const
{
    const auto onCurrentFloor = [this](Vector3 position) {
        return std::fabs(position.y - player_.GetPosition().y) < 5.0f;
    };
    const auto draw = [&](Vector3 position, float height, Color color, Color outline) {
        if (!onCurrentFloor(position)) return false;
        position.y += height;
        DrawGuidanceArrow(camera, position, color, outline);
        return true;
    };
    const auto drawItem = [&](ItemType type, Color color, Color outline) {
        const auto* item = inventory_.FindUncollectedItem(type);
        return item && draw(item->GetPosition(), 0.5f, color, outline);
    };
    const auto findDoor = [&](DoorOpenConditionType condition) -> const DoorMove* {
        const auto door = std::find_if(doors_.begin(), doors_.end(), [&](const DoorMove& entry) {
            return entry.GetOpenConditionType() == condition && onCurrentFloor(entry.GetPosition());
        });
        return door != doors_.end() ? &*door : nullptr;
    };
    const auto nearestDoor = [&](Vector3 position) -> const DoorMove* {
        const DoorMove* nearest = nullptr;
        float distance = 7.0f;
        for (const auto& door : doors_) {
            if (!onCurrentFloor(door.GetPosition())) continue;
            const float candidate = Vector3Distance(door.GetPosition(), position);
            if (candidate >= distance) continue;
            distance = candidate;
            nearest = &door;
        }
        return nearest;
    };
    const auto drawThroughDoor = [&](const DoorMove* door, Vector3 target, Color color, Color outline) {
        if (!door) return false;
        const float yaw = door->GetRotation().y * DEG2RAD;
        const Vector3 normal{std::sin(yaw), 0.0f, std::cos(yaw)};
        const Vector3 center = door->GetPosition();
        const float playerSide = Vector3DotProduct(Vector3Subtract(player_.GetPosition(), center), normal);
        const float targetSide = Vector3DotProduct(Vector3Subtract(target, center), normal);
        return playerSide * targetSide < 0.0f && draw(center, 1.0f, color, outline);
    };
    const auto drawElevator = [&](Color color, Color outline) {
        const auto elevator = std::find_if(teleportPoints_.begin(), teleportPoints_.end(), [&](const TeleportPoint& point) {
            return point.GetTeleportType() == TeleportType::Source && onCurrentFloor(point.GetPosition());
        });
        return elevator != teleportPoints_.end() && draw(elevator->GetPosition(), 1.5f, color, outline);
    };

    if (const auto* shutter = findDoor(DoorOpenConditionType::Crowbar)) {
        if (shutter->IsFullyOpen()) {
            draw(goalPoint_.GetPosition(), 1.6f, GREEN, LIME);
            return;
        }
        if (guidancePins_.shutterInspected && !inventory_.HasCrowbar() && drawItem(ItemType::Crowbar, GOLD, ORANGE)) return;
        draw(shutter->GetPosition(), 0.4f, GREEN, LIME);
        return;
    }

    if (const auto* serviceDoor = findDoor(DoorOpenConditionType::EmergencyStairsKey)) {
        if (serviceDoor->IsOpen() && drawElevator(GREEN, LIME)) return;
        if (guidancePins_.serviceKey && drawItem(ItemType::EmergencyStairsKey, GOLD, ORANGE)) return;
        draw(serviceDoor->GetPosition(), 1.0f, GREEN, LIME);
        return;
    }

    const auto* securityDoor = findDoor(DoorOpenConditionType::SecurityTeleportEntered);
    if (const auto* mainDoor = findDoor(DoorOpenConditionType::SecurityPCActivated)) {
        if (mainDoor->IsOpenConditionSatisfied()) {
            if (drawThroughDoor(securityDoor, mainDoor->GetPosition(), MAGENTA, PURPLE)) return;
            if (!mainDoor->IsOpen() || !drawElevator(MAGENTA, PURPLE))
                draw(mainDoor->GetPosition(), 1.0f, MAGENTA, PURPLE);
            return;
        }
    }

    if (const auto* phone = inventory_.FindUncollectedItem(ItemType::LostItem)) {
        if (!onCurrentFloor(phone->GetPosition())) return;
        const auto frontDoor = std::find_if(doors_.begin(), doors_.end(), [&](const DoorMove& door) {
            return door.GetLockType() == DoorLockType::LostItemCollected && onCurrentFloor(door.GetPosition());
        });
        if (frontDoor != doors_.end() && drawThroughDoor(&*frontDoor, phone->GetPosition(), YELLOW, ORANGE)) return;
        draw(phone->GetPosition(), 0.5f, YELLOW, ORANGE);
        return;
    }
    if (!inventory_.IsLostItemCollected()) return;

    const auto securityEntrance = std::find_if(securityTeleportPoints_.begin(), securityTeleportPoints_.end(),
        [&](const SecurityTeleportPoint& point) {
            return point.GetTeleportType() == SecurityTeleportType::Source && onCurrentFloor(point.GetPosition());
        });
    const bool securityReached = inventory_.IsGunCollected() ||
        (securityEntrance != securityTeleportPoints_.end() && securityEntrance->IsTriggered()) ||
        std::any_of(doors_.begin(), doors_.end(), [](const DoorMove& door) {
            return door.GetOpenConditionType() == DoorOpenConditionType::SecurityTeleportEntered &&
                door.IsOpenConditionSatisfied();
        });
    if (!securityReached && securityEntrance != securityTeleportPoints_.end()) {
        const auto rearExit = std::find_if(eventDialoguePoints_.begin(), eventDialoguePoints_.end(),
            [](const EventDialoguePoint& point) { return point.GetDialogueId() == "zombie_encounter"; });
        if (rearExit != eventDialoguePoints_.end() && !rearExit->IsTriggered()) {
            const auto* rearDoor = nearestDoor(rearExit->GetPosition());
            const Vector3 waypoint = rearDoor ? rearDoor->GetPosition() : rearExit->GetPosition();
            if (draw(waypoint, 1.5f, GREEN, BLUE)) return;
        }
        draw(securityEntrance->GetPosition(), 1.0f, GREEN, BLUE);
        return;
    }

    if (const auto* gun = inventory_.FindUncollectedItem(ItemType::Gun)) {
        if (drawThroughDoor(securityDoor, gun->GetPosition(), YELLOW, ORANGE) ||
            draw(gun->GetPosition(), 0.5f, YELLOW, ORANGE)) return;
    }

    const auto& items = inventory_.GetWorldItems();
    const auto authKey = std::find_if(items.begin(), items.end(), [](const Item& item) {
        return item.GetType() == ItemType::AuthenticationKey;
    });
    const auto* storageDoor = authKey != items.end() ? nearestDoor(authKey->GetPosition()) : nullptr;
    if (guidancePins_.authKey && authKey != items.end() && !authKey->IsCollected()) {
        const Vector3 target = authKey->GetPosition();
        if (drawThroughDoor(securityDoor, target, GREEN, DARKGREEN) ||
            drawThroughDoor(storageDoor, target, GREEN, DARKGREEN) ||
            draw(target, 0.5f, GREEN, DARKGREEN)) return;
    }

    if (!securityPC_.IsActivated()) {
        const Vector3 target = securityPC_.GetPosition();
        if (drawThroughDoor(storageDoor, target, SKYBLUE, BLUE) ||
            drawThroughDoor(securityDoor, target, SKYBLUE, BLUE)) return;
        draw(target, 0.8f, SKYBLUE, BLUE);
    }
}

void GameContext::Draw() const
{
    const bool playerView =
        cameraController_.GetActiveType() == CameraType::Player;

    lighting_.BeginFrame(
        cameraController_.GetActiveRaylibCamera(),
        cameraController_.GetPlayerCamera().GetRaylibCamera(),
        stage_,
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

        const auto& font = JPFont();
        std::string prompt;
        const bool canInteract = !IsMessageActive() && !gunTutorialUI_.IsOpen() && !gameFade_.IsFading();
        if (const auto* door = canInteract ? GetNearbyDoor() : nullptr) {
            if (door->IsLocked(&inventory_))
                prompt = font.IsLoaded() ? u8"[E] \u8abf\u3079\u308b" : "[E] Inspect";
            else if (door->GetOpenConditionType() == DoorOpenConditionType::Crowbar)
                prompt = font.IsLoaded() ? u8"[E] \u3053\u3058\u958b\u3051\u308b" : "[E] Pry open";
            else
                prompt = door->IsOpen()
                    ? (font.IsLoaded() ? u8"[E] \u9589\u3081\u308b" : "[E] Close")
                    : (font.IsLoaded() ? u8"[E] \u958b\u3051\u308b" : "[E] Open");
        }
        else if (canInteract)
        {
            if (GetNearbyElevator())
                prompt = font.IsLoaded() ? u8"[E] エレベーターに乗る" : "[E] Use elevator";
            else if (securityPC_.CanInteract(player_.GetPosition()))
                prompt = font.IsLoaded() ? u8"[E] \u7aef\u672b\u3092\u8abf\u3079\u308b" : "[E] Inspect PC";
        }

        if (!prompt.empty())
        {
            const int size = 36;
            const int y = GetScreenHeight() / 2 + 30;
            if (font.IsLoaded()) {
                const int x = int((GetScreenWidth() - font.MeasureText(prompt, size).x) / 2);
                font.DrawText(prompt, x + 2, y + 2, size, BLACK);
                font.DrawText(prompt, x, y, size, RAYWHITE);
            } else {
                const int x = (GetScreenWidth() - MeasureText(prompt.c_str(), size)) / 2;
                DrawText(prompt.c_str(), x, y, size, RAYWHITE);
            }
        }

        if (!gameFade_.IsFading()) DrawGuidance(activePlayerCam->GetRaylibCamera());
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