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

inline bool isComboProjectile(ActorType type) { // projectiles used for PVP combos
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

TInstanceHook(float, "?getArmorKnockbackResistance@ArmorItem@@UEBAMXZ", ArmorItem) {
	 if (this->mModelIndex == 7) return settings.netheriteArmorKBResistance;
	 return original(this);
}

// use these values for more accurate player pos
TInstanceHook(void, "?normalTick@ServerPlayer@@UEAAXXZ", ServerPlayer) {
	auto fields = this->EZPlayerFields;
	fields->mRawPosOld = this->getPos();
	original(this);
	fields->mRawPos = this->getPos();
}

TInstanceHook(bool, "?attack@Player@@UEAA_NAEAVActor@@@Z", Player, Actor &actor) {
	this->EZPlayerFields->mLastAttackedActorTimestamp = LocateService<Level>()->getServerTick();
	bool result = original(this, actor);
	if (settings.useJavaSprintReset && (actor.getEntityTypeId() == ActorType::Player_0)) {
		this->setSprinting(false);
	}
	return result;
}

TInstanceHook(void, "?setSprinting@Mob@@UEAAX_N@Z", Mob, bool shouldSprint) {
	// std::cout << "return address: 0x" << std::hex << ((int64_t)_ReturnAddress() - (int64_t)GetModuleHandle(0)) << std::endl;
	// sprint force-stopped when attacking (called by vanilla BDS, we want to call it on our own)
	if (!shouldSprint && (this->getEntityTypeId() == ActorType::Player_0)) {
		if (((int64_t)_ReturnAddress() - (int64_t)GetModuleHandle(0)) == 0x71E634) return;
	}
	original(this, shouldSprint);
}

TInstanceHook(void, "?handle@ServerNetworkHandler@@UEAAXAEBVNetworkIdentifier@@AEBVPlayerActionPacket@@@Z",
	ServerNetworkHandler, NetworkIdentifier const &netId, PlayerActionPacket const &pkt) {
	original(this, netId, pkt);
	if (pkt.mAction == PlayerActionType::STOP_SPRINT) {
		auto player = this->_getServerPlayer(netId, pkt.mClientSubId);
		if (player) {
			player->EZPlayerFields->mHasResetSprint = true;
		}
	}
}

TInstanceHook(bool, "?_hurt@Player@@MEAA_NAEBVActorDamageSource@@H_N1@Z",
	Player, ActorDamageSource &source, int dmg, bool knock, bool ignite) {
	this->EZPlayerFields->mLastHurtByDamager = source.getDamagingEntityType();
	return original(this, source, dmg, knock, ignite);
}

TInstanceHook(void, "?knockback@ServerPlayer@@UEAAXPEAVActor@@HMMMMM@Z",
	ServerPlayer, Actor *source, int dmg, float xd, float zd, float power, float height, float heightCap) {

	// bug: vanilla calls punch arrows to do knockback on creative players
	if (this->isInCreativeOrCreativeViewerMode()) return;

	auto lastHurtCause = this->mLastHurtCause;
	bool fromEntity = (lastHurtCause == ActorDamageCause::EntityAttack);
	bool fromProjectile = (lastHurtCause == ActorDamageCause::Projectile);

	float punchEnchantmentMultiplier = 1.0f;
	if (((int64_t)_ReturnAddress() - (int64_t)GetModuleHandle(0)) == 0x273BFE) { // hack: get punch knockback value from arrows
		punchEnchantmentMultiplier = power; // enchantment tier * 1.6
	}

	// initialize values
	power = settings.normalKBPower;
	height = settings.normalKBHeight;
	heightCap = settings.heightCap;

	if (fromProjectile) {

		if (settings.customProjectileKBEnabled) {

			auto damager = this->EZPlayerFields->mLastHurtByDamager;

			if (isComboProjectile(damager)) {
				power = settings.comboProjectileKBPower;
				height = settings.comboProjectileKBHeight;
			}

			else if (damager == ActorType::Enderpearl) {
				power = settings.enderpearlKBPower;
				height = settings.enderpearlKBHeight;
			}
		}
		power *= punchEnchantmentMultiplier;
	}

	else if (source && fromEntity) {

		float kbEnchantmentBonus = (float)(((Mob*) source)->getMeleeKnockbackBonus()) * 0.4f;

		if (source->getEntityTypeId() == ActorType::Player_0) {

			auto attacker = ((Player*) source);
			if (attacker->EZPlayerFields->mHasResetSprint && attacker->isSprinting()) {
				power += settings.additionalWTapKBPower;
				height += settings.additionalWTapKBHeight;
				attacker->EZPlayerFields->mHasResetSprint = false;
			}
		}

		// attacking on the same tick that you receive knockback will set lower values (aka "reducing")
		uint64_t currentTick = LocateService<Level>()->getServerTick();
		if ((currentTick - this->EZPlayerFields->mLastAttackedActorTimestamp) <= 1) {
			power *= settings.KBReductionFactor;
		}

		power += kbEnchantmentBonus;
	}

	// calculate knockback resistance
	float knockbackResistanceValue = this->getAttributeInstanceFromId(AttributeIds::KnockbackResistance)->currentVal;
	float scaledKnockbackForce = std::fmax(1.0f - knockbackResistanceValue, 0.0f);

	if (scaledKnockbackForce < 1.0f) {
		power *= scaledKnockbackForce;
		height *= scaledKnockbackForce;
	}

	float f = std::sqrtf(xd * xd + zd * zd);
	if (f <= 0.0f) return;
	float f1 = 1.0f / f;
	float f2 = 1.0f / settings.KBFriction;

	// auto posDelta = this->mStateVectorComponent.mPosDelta;
	auto posDelta = this->getRawPosDelta();
	float oldPosDeltaY = posDelta.y; // save later for custom heightcap mechanics

	// clamp pos delta
	float d = std::sqrtf(posDelta.x * posDelta.x + posDelta.z * posDelta.z);
	if (d > settings.maxHorizontalDisplacement) {
		posDelta.normalizeXZ();
		posDelta.x *= settings.maxHorizontalDisplacement;
		posDelta.z *= settings.maxHorizontalDisplacement;
	}

	posDelta.x *= f2;
	posDelta.z *= f2;

	posDelta.x -= xd * f1 * power;
	posDelta.z -= zd * f1 * power;

	// heightcap stuff
	if (settings.useJavaHeightCap) {
		posDelta.y *= f2;
		posDelta.y += height;

		if (posDelta.y > settings.heightThreshold) {
			posDelta.y = heightCap;
		}
	}
	else {
		posDelta.y = height;

		if (settings.useCustomHeightCap && ((oldPosDeltaY + posDelta.y) > settings.heightThreshold)) {
			posDelta.y = heightCap;
		}
	}

	/*float healthValue = this->getAttributeInstanceFromId(AttributeIds::Health)->currentVal;
	if (healthValue <= 0.f) {
		this->mIsKnockedBackOnDeath = true; // client ignores motion packets if it's dead
	}*/

	this->mDimension->sendPacketForEntity(*this, SetActorMotionPacket(this->mRuntimeID, posDelta), nullptr);
}