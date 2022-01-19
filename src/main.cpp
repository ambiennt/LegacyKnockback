#include "main.h"
#include <dllentry.h>

DEFAULT_SETTINGS(settings);

void dllenter() {}
void dllexit() {}
void PreInit() {
	// prevent projectiles from bypassing hurt cooldown by setting newEntityAttack check to false
	if (!settings.projectilesBypassHurtCooldown) {
		GetServerSymbolWithOffset<PatchSpan<3>>("?hurtEffects@Mob@@UEAA_NAEBVActorDamageSource@@H_N1@Z", 0x84)
			->VerifyPatchFunction({0x83, 0xF8, 0x02}, {0x83, 0xF8, 0x20}); // 83 F8 02 - set actorDamageCause to enum type 32 (invalid cause)
		GetServerSymbolWithOffset<PatchSpan<3>>("?_hurt@Mob@@MEAA_NAEBVActorDamageSource@@H_N1@Z", 0x48)
			->VerifyPatchFunction({0x83, 0xF8, 0x02}, {0x83, 0xF8, 0x20}); // 83 F8 02
	}
}
void PostInit() {}

__forceinline bool isComboProjectile(ActorType type) { // projectiles used for PVP combos
	switch (type) {
		case ActorType::FishingHook:
		case ActorType::Snowball:
		case ActorType::ThrownEgg:
			return true;

		default: return false;
	}
}

// KnockbackRules is a namespace, not a class
THook(bool, "?useLegacyKnockback@KnockbackRules@@YA_NAEBVLevel@@@Z", void* level) { return true; }

// only called when player attacks while sprinting
// but silently fails in 1.16 when attacking players so this function can be ignored
// the player pushable component cant be found and so they dont ever take bonus knockback
THook(void, "?doKnockbackAttack@KnockbackRules@@YAXAEAVMob@@0AEBVVec2@@M@Z",
	Mob &self, Mob &target, Vec2 const &direction, float force) {
	if (target.getEntityTypeId() == ActorType::Player_0) return;
	return original(self, target, direction, force);
}


THook(float, "?getArmorKnockbackResistance@ArmorItem@@UEBAMXZ", ArmorItem *item) {
	 if (item->mModelIndex == 7) return settings.netheriteArmorKnockbackResistance;
	 return original(item);
}

THook(bool, "?attack@Player@@UEAA_NAEAVActor@@@Z", Player *player, Actor &actor) {
	player->EZPlayerFields->mLastAttackedActorTimestamp = LocateService<Level>()->getServerTick();
	bool result = original(player, actor);
	if (settings.useJavaSprintReset && (actor.getEntityTypeId() == ActorType::Player_0)) {
		((Player*) &actor)->setSprinting(false);
	}
	return result;
}

// use these values for more accurate player pos
THook(void, "?normalTick@ServerPlayer@@UEAAXXZ", ServerPlayer *player) {
	auto fields = player->EZPlayerFields;
	fields->mRawPos = player->getPos();
	original(player);
	fields->mRawPosOld = player->getPos();
}

THook(void, "?setSprinting@Mob@@UEAAX_N@Z", Mob *mob, bool shouldSprint) {
	//std::cout << "0x" << std::hex << (int64_t)_ReturnAddress() - (int64_t)GetModuleHandle(0) << std::endl;
	if (mob->getEntityTypeId() == ActorType::Player_0) {
		uint64_t returnAddress = ((int64_t)_ReturnAddress() - (int64_t)GetModuleHandle(0));
		if (returnAddress == 0x3AA84A) { //sprint stopped via user input
			((Player*) mob)->EZPlayerFields->mHasResetSprint = true;
		}
		else if (settings.useJavaSprintReset && (returnAddress == 0x71E634)) {
			return; //sprint force-stopped when attacking (called by vanilla BDS, we want to call it on our own)
		}
	}
	original(mob, shouldSprint);
}

THook(bool, "?_hurt@Player@@MEAA_NAEBVActorDamageSource@@H_N1@Z", Player *player, ActorDamageSource &source, int dmg, bool knock, bool ignite) {
	player->EZPlayerFields->mLastHurtByDamager = source.getDamagingEntityType();
	return original(player, source, dmg, knock, ignite);
}

THook(void, "?knockback@ServerPlayer@@UEAAXPEAVActor@@HMMMMM@Z",
	ServerPlayer *player, Actor *source, int dmg, float xd, float zd, float power, float height, float heightCap) {

	// bug: vanilla calls punch arrows to do knockback on creative players
	if ((player->mPlayerGameType == GameType::Creative) || (player->mPlayerGameType == GameType::CreativeViewer)) return;

	auto lastHurtCause = player->mLastHurtCause;
	bool fromEntity = (lastHurtCause == ActorDamageCause::EntityAttack);
	bool fromProjectile = (lastHurtCause == ActorDamageCause::Projectile);

	float punchEnchantmentMultiplier = 1.0f;
	if (((int64_t)_ReturnAddress() - (int64_t)GetModuleHandle(0)) == 0x273BFE) { //hack: get punch knockback value from arrows
		punchEnchantmentMultiplier = power; // enchantment tier * 1.6
	}

	// initialize values
	power = settings.normalKnockbackPower;
	height = settings.normalKnockbackHeight;
	heightCap = settings.heightCap;

	if (fromProjectile) {

		if (settings.customProjectileKnockbackEnabled) {

			auto damager = player->EZPlayerFields->mLastHurtByDamager;
			
			if (isComboProjectile(damager)) {
				power = settings.comboProjectileKnockbackPower;
				height = settings.comboProjectileKnockbackHeight;
			}

			else if (damager == ActorType::Enderpearl) {
				power = settings.enderpearlKnockbackPower;
				height = settings.enderpearlKnockbackHeight;
			}
		}
		power *= punchEnchantmentMultiplier;
	}

	else if (source && fromEntity) {

		float kbEnchantmentBonus = (float)(((Mob*)source)->getMeleeKnockbackBonus()) * 0.4f;

		if (source->getEntityTypeId() == ActorType::Player_0) {

			auto attacker = ((Player*) source);
			if (attacker->EZPlayerFields->mHasResetSprint && attacker->isSprinting()) {
				power += settings.additionalWTapKnockbackPower;
				height += settings.additionalWTapKnockbackHeight;
				attacker->EZPlayerFields->mHasResetSprint = false;
			}
		}

		// attacking on the same tick that you receive knockback will set lower values (aka "reducing")
		uint64_t currentTick = LocateService<Level>()->getServerTick();
		if ((currentTick - player->EZPlayerFields->mLastAttackedActorTimestamp) <= 1) {
			power *= settings.knockbackReductionFactor;
		}

		power += kbEnchantmentBonus;
	}

	// calculate knockback resistance
	float knockbackResistanceValue = player->getAttributeInstanceFromId(AttributeIds::KnockbackResistance)->currentVal;
	float scaledKnockbackForce = std::fmax(1.0f - knockbackResistanceValue, 0.0f);

	if (scaledKnockbackForce < 1.0f) {
		power *= scaledKnockbackForce;
		height *= scaledKnockbackForce;
	}

	// auto posDelta = player->mStateVectorComponent.mPosDelta;
	auto posDelta = player->mTeleportedThisTick ? Vec3::ZERO : player->getRawPosDelta();
	float oldPosDeltaY = posDelta.y;

	float f = sqrt(xd * xd + zd * zd);
	if (f <= 0.0f) return;
	float f1 = 1.0f / f;
	float f2 = 1.0f / settings.knockbackFriction;

	posDelta.x *= f2;
	posDelta.z *= f2;

	posDelta.x -= xd * f1 * power;
	posDelta.z -= zd * f1 * power;

	if (settings.useJavaHeightCap) {
		posDelta.y *= f2;
		posDelta.y += height;

		if (posDelta.y > settings.heightThreshold) {
			posDelta.y = heightCap;
		}
	}
	else {
		posDelta.y = height;

		if (settings.useCustomHeightCap && (oldPosDeltaY + posDelta.y > settings.heightThreshold)) {
			posDelta.y = heightCap;
		}
	}

	/*float healthValue = player->getAttributeInstanceFromId(AttributeIds::Health)->currentVal;
	if (healthValue <= 0.0f) {
		player->mIsKnockedBackOnDeath = true; // client ignores motion packets if it's dead anyway
	}*/

	SetActorMotionPacket pkt(player->mRuntimeID, posDelta);
	player->mDimension->sendPacketForEntity(*player, pkt, nullptr);
}