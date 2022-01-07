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
#include <Actor/Attribute.h>
#include <Actor/ActorType.h>
#include <Actor/ActorDamageSource.h>
#include <Packet/SetActorMotionPacket.h>
#include <Level/Level.h>
#include <Level/Dimension.h>
#include <Item/ArmorItem.h>

inline struct Settings {
	float normalKnockbackPower               = 0.4f;
	float normalKnockbackHeight              = 0.4f;
	float additionalWTapKnockbackPower       = 0.4f;
	float additionalWTapKnockbackHeight      = 0.0f;
	float knockbackReductionFactor           = 0.6f;
	float knockbackFriction                  = 2.0f;
	bool customProjectileKnockbackEnabled    = false;
	float comboProjectileKnockbackPower      = 0.4f;
	float comboProjectileKnockbackHeight     = 0.4f;
	float enderpearlKnockbackPower           = 0.4f;
	float enderpearlKnockbackHeight          = 0.4f;
	bool useJavaSprintReset                  = true;
	bool useLegacySprintReset                = false;
	bool useJavaHeightCap                    = false;
	bool useCustomHeightCap                  = true;
	float heightThreshold                    = 0.4f;
	float heightCap                          = 0.4f;
	bool projectilesBypassHurtCooldown       = false;
	float netheriteArmorKnockbackResistance  = 0.0f;

	template <typename IO> static inline bool io(IO f, Settings &settings, YAML::Node &node) {
		return f(settings.normalKnockbackPower, node["normalKnockbackPower"]) &&
			   f(settings.normalKnockbackHeight, node["normalKnockbackHeight"]) &&
			   f(settings.additionalWTapKnockbackPower, node["additionalWTapKnockbackPower"]) &&
			   f(settings.additionalWTapKnockbackHeight, node["additionalWTapKnockbackHeight"]) &&
			   f(settings.knockbackReductionFactor, node["knockbackReductionFactor"]) &&
			   f(settings.knockbackFriction, node["knockbackFriction"]) &&
			   f(settings.customProjectileKnockbackEnabled, node["customProjectileKnockbackEnabled"]) &&
			   f(settings.comboProjectileKnockbackPower, node["comboProjectileKnockbackPower"]) &&
			   f(settings.comboProjectileKnockbackHeight, node["comboProjectileKnockbackHeight"]) &&
			   f(settings.enderpearlKnockbackPower, node["enderpearlKnockbackPower"]) &&
			   f(settings.enderpearlKnockbackHeight, node["enderpearlKnockbackHeight"]) &&
			   f(settings.useJavaSprintReset, node["useJavaSprintReset"]) &&
			   f(settings.useLegacySprintReset, node["useLegacySprintReset"]) &&
			   f(settings.useJavaHeightCap, node["useJavaHeightCap"]) &&
			   f(settings.useCustomHeightCap, node["useCustomHeightCap"]) &&
			   f(settings.heightThreshold, node["heightThreshold"]) &&
			   f(settings.heightCap, node["heightCap"]) &&
			   f(settings.projectilesBypassHurtCooldown, node["projectilesBypassHurtCooldown"]) &&
			   f(settings.netheriteArmorKnockbackResistance, node["netheriteArmorKnockbackResistance"]);
		}
} settings;

DEF_LOGGER("LegacyKnockback");