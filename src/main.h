#pragma once

#include <hook.h>
#include <base/base.h>
#include <base/log.h>
#include <base/playerdb.h>
#include <yaml.h>
#include <Math/Vec2.h>
#include <Math/Vec3.h>
#include <Actor/ServerPlayer.h>
#include <Actor/Attribute.h>
#include <Actor/ActorType.h>
#include <Actor/ActorDamageSource.h>
#include <Level/Level.h>
#include <Level/Dimension.h>
#include <Level/GameRules.h>
#include <Item/ItemStack.h>
#include <Item/ArmorItem.h>
#include <Packet/SetActorMotionPacket.h>
#include <Packet/PlayerActionPacket.h>
#include <Net/ServerNetworkHandler.h>
#include <Item/Enchant.h>
#include <Component/ProjectileComponent.h>
#include <Actor/MobEffect.h>
#include <Actor/ActorEvent.h>
#include <Actor/StateVectorComponent.h>

#include <cmath>
#include <random>

inline struct Settings {
	float normalKBPower                = 0.4f;
	float normalKBHeight               = 0.4f;
	float additionalWTapKBPower        = 0.4f;
	float additionalWTapKBHeight       = 0.0f;
	float KBReductionFactor            = 0.6f;
	float KBFriction                   = 2.0f;
	float maxHorizontalDisplacement    = 0.2f;
	bool customProjectileKBEnabled     = false;
	float comboProjectileKBPower       = 0.4f;
	float comboProjectileKBHeight      = 0.4f;
	float enderpearlKBPower            = 0.4f;
	float enderpearlKBHeight           = 0.4f;
	bool useJavaSprintReset            = true;
	bool useJavaHeightCap              = false;
	bool useCustomHeightCap            = true;
	float heightThreshold              = 0.4f;
	float heightCap                    = 0.4f;
	float netheriteArmorKBResistance   = 0.0f;
	bool playersCanCrit                = true;
	uint32_t hurtCooldownTicks         = 10;

	template <typename IO> static inline bool io(IO f, Settings &settings, YAML::Node &node) {
		return f(settings.normalKBPower, node["normalKBPower"]) &&
			   f(settings.normalKBHeight, node["normalKBHeight"]) &&
			   f(settings.additionalWTapKBPower, node["additionalWTapKBPower"]) &&
			   f(settings.additionalWTapKBHeight, node["additionalWTapKBHeight"]) &&
			   f(settings.KBReductionFactor, node["KBReductionFactor"]) &&
			   f(settings.KBFriction, node["KBFriction"]) &&
			   f(settings.maxHorizontalDisplacement, node["maxHorizontalDisplacement"]) &&
			   f(settings.customProjectileKBEnabled, node["customProjectileKBEnabled"]) &&
			   f(settings.comboProjectileKBPower, node["comboProjectileKBPower"]) &&
			   f(settings.comboProjectileKBHeight, node["comboProjectileKBHeight"]) &&
			   f(settings.enderpearlKBPower, node["enderpearlKBPower"]) &&
			   f(settings.enderpearlKBHeight, node["enderpearlKBHeight"]) &&
			   f(settings.useJavaSprintReset, node["useJavaSprintReset"]) &&
			   f(settings.useJavaHeightCap, node["useJavaHeightCap"]) &&
			   f(settings.useCustomHeightCap, node["useCustomHeightCap"]) &&
			   f(settings.heightThreshold, node["heightThreshold"]) &&
			   f(settings.heightCap, node["heightCap"]) &&
			   f(settings.netheriteArmorKBResistance, node["netheriteArmorKBResistance"]) &&
			   f(settings.playersCanCrit, node["playersCanCrit"]) &&
			   f(settings.hurtCooldownTicks, node["hurtCooldownTicks"]);
		}
} settings;

DEF_LOGGER("LegacyKnockback");