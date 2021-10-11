#pragma once

#include <hook.h>
#include <base/base.h>
#include <base/log.h>
#include <base/playerdb.h>
#include <yaml.h>
#include <Math/Vec2.h>
#include <Math/Vec3.h>
#include <Actor/Actor.h>
#include <Actor/Mob.h>
#include <Actor/Player.h>
#include <Actor/ServerPlayer.h>
#include <Actor/ActorType.h>
#include <Actor/ActorRuntimeID.h>
#include <Actor/ActorDamageSource.h>
#include <Core/Packet.h>
#include <Packet/SetActorMotionPacket.h>
#include <Level/Level.h>
#include <Level/Dimension.h>

#include <cmath>
#include <stdio.h>
#include <intrin.h>

struct Settings {
    float normalKnockbackPower = 0.4f;
    float normalKnockbackHeight = 0.4f;
    float sprintResetKnockbackPower = 0.8f;
    float sprintResetKnockbackHeight = 0.4f;
    float knockbackReductionFactor = 0.6f;
    bool comboProjectileKnockbackEnabled = false;
    float comboProjectileKnockbackPower = 0.4f;
    float comboProjectileKnockbackHeight = 0.4f;
    bool useJavaSprintReset = true;
    bool useLegacySprintReset = false;
    bool projectilesBypassHurtCooldown = false;
    float netheriteArmorKnockbackResistance = 0.0f;

  template <typename IO> static inline bool io(IO f, Settings &settings, YAML::Node &node) { 
    return f(settings.normalKnockbackPower, node["normalKnockbackPower"]) &&
           f(settings.normalKnockbackHeight, node["normalKnockbackHeight"]) &&
           f(settings.sprintResetKnockbackPower, node["sprintResetKnockbackPower"]) &&
           f(settings.sprintResetKnockbackHeight, node["sprintResetKnockbackHeight"]) &&
           f(settings.knockbackReductionFactor, node["knockbackReductionFactor"]) &&
           f(settings.comboProjectileKnockbackEnabled, node["comboProjectileKnockbackEnabled"]) &&
           f(settings.comboProjectileKnockbackPower, node["comboProjectileKnockbackPower"]) &&
           f(settings.comboProjectileKnockbackHeight, node["comboProjectileKnockbackHeight"]) &&
           f(settings.useJavaSprintReset, node["useJavaSprintReset"]) &&
           f(settings.useLegacySprintReset, node["useLegacySprintReset"]) &&
           f(settings.projectilesBypassHurtCooldown, node["projectilesBypassHurtCooldown"]) &&
           f(settings.netheriteArmorKnockbackResistance, node["netheriteArmorKnockbackResistance"]);
    }
};

DEF_LOGGER("LegacyKnockback");

extern Settings settings;