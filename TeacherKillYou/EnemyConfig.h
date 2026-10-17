#pragma once
#include "raylib.h"

namespace demo {

    struct EnemyConfig {
        int maxHp = 100;
        float moveSpeed = 3.0f;
        float turnSpeed = PI;
        float bodyRadius = 0.4f;
        float bodyHeight = 1.8f;
        float detectionRange = 10.0f;
        float loseRange = 15.0f;
        float stopDistance = 1.2f;
        float resumeDistance = 1.5f;
        float corpseDuration = 2.0f;
        int attackDamage = 15;
        float attackRange = 1.6f;
        float attackHitFraction = 0.4f;
        float attackRecovery = 0.65f;

        // •s³‚È’l‚ª“ü‚ç‚È‚¢‚æ‚¤‚É©“®‚Å’²®‚·‚éŠÖ”
        void Validate();
    };

}