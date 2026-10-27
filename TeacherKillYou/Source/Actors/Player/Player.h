#pragma once
#include "GameObject.h"
#include "WorldScale.h"
#include "Inventory.h"

class Stage;
class PlayerCamera;
struct PlayerCheckpoint;

class Player : public GameObject
{
public:
    Player() : GameObject(GameObjectType::Player, "Player") {}
    ~Player() override = default;

    void Init();
    void Reset();

    void Update(float deltaTime, const Stage& stage,
        Vector3 forward = { 0, 0, 1 },
        Vector3 right = { 1, 0, 0 });

    void Draw(const PlayerCamera* camera = nullptr) const;
    void DrawAmmoHUD() const;
    void DrawHealthHUD() const;
    void DrawDamageOverlay() const;
    void DrawStaminaHUD() const;
    void DrawColliderDebug() const;
    void End();

    void TakeDamage(int amount);

    int GetHp() const { return hp_; }
    int GetMaxHp() const { return maxHp_; }
    bool IsAlive() const { return hp_ > 0; }
    bool IsGrounded() const;
    bool IsSprinting() const;
    PlayerCheckpoint CaptureCheckpoint() const;
    void RestoreCheckpoint(const PlayerCheckpoint& data);

    float GetRadius() const { return playerRadius_; }
    float GetHeight() const { return playerHeight_; }

    bool IsDebugDrawEnabled() const { return debugDrawEnabled_; }
    void SetDebugDrawEnabled(bool enabled) { debugDrawEnabled_ = enabled; }

    // Item effects
    void Heal(int amount);
    void AddReserveAmmo(int amount);

    Vector3 GetMuzzlePosition(const Camera3D& view) const;


    void SetColliderSize(float radius, float height)
    {
        playerRadius_ = radius;
        playerHeight_ = height;
    }

    bool ConsumeShot()
    {
        const bool fired = shotPending_;
        shotPending_ = false;
        return fired;
    }

    bool ConsumeDamageImpact();

private:
    Vector3 GetInputMoveDirection(Vector3 forward, Vector3 right) const;
    void MoveWithCollision(Vector3 moveAmount, const Stage& stage);
    void UpdateVerticalPhysics(float deltaTime, const Stage& stage);
    void UpdateWeapon(float deltaTime);
    void UpdateWeaponMotion(float deltaTime, bool hasMoveInput);

    Vector3 velocity_{ 0.0f, 0.0f, 0.0f };
    Vector2 weaponSway_{ 0.0f, 0.0f };
    Vector3 weaponMotionOffset_{ 0.0f, 0.0f, 0.0f };

    float moveSpeed_{ 2.5f };

    float playerRadius_{ WorldScale::Player.radius };
    float playerHeight_{ WorldScale::Player.height };

    Inventory inventory_;

    float gravity_{ -9.81f };
    float jumpForce_{ 3.0f };
    bool isGrounded_{ false };

    float viewModelScale_{ 0.01f };

    bool isShooting_{ false };
    bool isReloading_{ false };
    bool shotPending_{ false };

	bool damageImpactPending_{ false };

    float shootTimer_{ 0.0f };
    float reloadTimer_{ 0.0f };

    float shootDuration_{ 0.417f };
    float reloadDuration_{ 2.291667f };

    // HUD画像はInitで読み込み、Resetでは再利用し、Endで解放する。
    Texture2D ammoHudTexture_{};
    Rectangle ammoHudSource_{};
    Texture2D healthHudTexture_{};
    Rectangle healthHudSource_{};

    int magazineCapacity_{ 15 };
    int currentAmmo_{ 15 };
    int reserveAmmo_{ 30 };

    int maxHp_{ 100 };
    int hp_{ 100 };

    float damageFlashTimer_{ 0.0f };
    float damageFlashDuration_{ 0.6f };

    float sprintSpeed_{ 4.5f };
    bool isSprinting_{ false };
    bool sprintExhausted_{ false };

    float maxStamina_{ 100.0f };
    float stamina_{ 100.0f };

    float staminaDrainRate_{ 25.0f };
    float staminaRegenRate_{ 20.0f };
    float staminaRecoveryDelay_{ 0.75f };
    float staminaRecoveryTimer_{ 0.0f };

    float weaponBobTime_ = 0.0f;
    float weaponBobBlend_ = 0.0f;
    float weaponSprintBlend_ = 0.0f;
    float weaponRoll_ = 0.0f;

	// Weapon recoil 
    float weaponRecoilBack_ = 0.0f;
    float weaponRecoilPitch_ = 0.0f;

    bool debugDrawEnabled_{ false };
};