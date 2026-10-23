#include "EnemyConfig.h"
#include "raymath.h"
#include <algorithm>
#include <cmath>

namespace {
    float AtLeast(float value, float minimum, float fallback) {
        return (std::max)(minimum, std::isfinite(value) ? value : fallback);
    }
}

namespace demo {

    void EnemyConfig::Validate() {
        const EnemyConfig defaults;
        if (type != EnemyType::Monster && type != EnemyType::FemaleZombie) type = defaults.type;
        maxHp = (std::max)(1, maxHp);
        moveSpeed = AtLeast(moveSpeed, 0.0f, defaults.moveSpeed);
        animationSpeed = Clamp(AtLeast(animationSpeed, 0.1f, defaults.animationSpeed), 0.1f, 4.0f);
        turnSpeed = AtLeast(turnSpeed, 0.0f, defaults.turnSpeed);
        bodyRadius = AtLeast(bodyRadius, 0.01f, defaults.bodyRadius);
        bodyHeight = AtLeast(bodyHeight, 0.01f, defaults.bodyHeight);
        detectionRange = AtLeast(detectionRange, 0.0f, defaults.detectionRange);
        loseRange = AtLeast(loseRange, detectionRange + 0.01f, defaults.loseRange);
        stopDistance = AtLeast(stopDistance, 0.0f, defaults.stopDistance);
        resumeDistance = AtLeast(resumeDistance, stopDistance + 0.01f, defaults.resumeDistance);
        corpseDuration = AtLeast(corpseDuration, 0.0f, defaults.corpseDuration);
        attackDamage = (std::max)(0, attackDamage);
        attackRange = AtLeast(attackRange, stopDistance + 0.05f, defaults.attackRange);
        attackHitFraction = Clamp(AtLeast(attackHitFraction, 0.01f, defaults.attackHitFraction), 0.01f, 0.99f);
        attackRecovery = AtLeast(attackRecovery, 0.0f, defaults.attackRecovery);
    }

}