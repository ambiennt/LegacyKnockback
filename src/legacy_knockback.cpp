#include "main.h"

float LegacyKnockback::generateRandomFloat(float min, float max) {
	std::uniform_real_distribution<float> genFloatFunc(min, max);
	return genFloatFunc(RNG_INSTANCE);
}

int32_t LegacyKnockback::getOnFireTime(Actor *projectile) {
	if (!projectile) return 0;
	auto component = projectile->tryGetComponent<ProjectileComponent>();
	if (component) {
		if (projectile->isOnFire() || component->mCatchFire) {
			return (uint32_t)(std::roundf(component->mOnFireTime));
		}
	}
	return 0;
}

float LegacyKnockback::getPunchEnchantmentMultiplier(Actor* projectile) {
	if (!projectile) return 1.f;
	auto component = projectile->tryGetComponent<ProjectileComponent>();
	if (component) {
		return std::fmax(1.f, component->mKnockbackForce);
	}
	return 1.f;
}

bool LegacyKnockback::shouldInvokeCriticalHit(const Player &attacker, const Actor &target) {
	return (settings.playersCanCrit &&
			(attacker.mFallDistance > 0.f) &&
			!attacker.mOnGround &&
			!attacker.onLadder() &&
			!attacker.isInWater() &&
			!attacker.hasEffect(*MobEffect::BLINDNESS) &&
			!attacker.hasEffect(*MobEffect::SLOW_FALLING) &&
			target.isInstanceOfMob());
}

LegacyKnockback::DamageInfo LegacyKnockback::calculateAttackDamage(Player &attacker, Actor &target) {

	double baseAttackDmg = (double)attacker.getMutableAttribute(AttributeID::AttackDamage)->mCurrentValue;
	const auto& equippedItem = attacker.getCarriedItem();
	double combinedAttackDmg = baseAttackDmg + (double)(equippedItem.getAttackDamage());

	bool isCriticalHit = LegacyKnockback::shouldInvokeCriticalHit(attacker, target);
	if (isCriticalHit) {
		combinedAttackDmg *= 1.5;
	}

	if (target.isInstanceOfMob()) {
		double meleeDmgBonus = (double)EnchantUtils::getMeleeDamageBonus(target, attacker);
		combinedAttackDmg += meleeDmgBonus;
		EnchantUtils::doPostDamageEffects(target, attacker);
	}

	if (attacker.hasEffect(*MobEffect::DAMAGE_BOOST)) {
		int32_t dmgBoostCount = attacker.getEffect(*MobEffect::DAMAGE_BOOST)->mAmplifier + 1;
		for (int32_t i = 0; i < dmgBoostCount; i++) {
			combinedAttackDmg = (combinedAttackDmg * 1.3) + 1.0;
		}
	}

	if (attacker.hasEffect(*MobEffect::WEAKNESS)) {
		int32_t weaknessCount = attacker.getEffect(*MobEffect::WEAKNESS)->mAmplifier + 1;
		for (int32_t j = 0; j < weaknessCount; j++) {
			combinedAttackDmg = (combinedAttackDmg * 0.8) - 0.5;
			if (combinedAttackDmg < 0.0) {
				combinedAttackDmg = 0.0;
				break;
			}
		}
	}

	if (combinedAttackDmg > (double)DamageInfo::MAX_DAMAGE) {
		return {DamageInfo::MAX_DAMAGE, isCriticalHit};
	}
	return {(int32_t)combinedAttackDmg, isCriticalHit};
}

void LegacyKnockback::calculateMobKnockback(Mob &target, const ActorDamageSource &source, float dx, float dz) {

	float knockbackResistanceValue = target.getMutableAttribute(AttributeID::KnockbackResistance)->mCurrentValue;
	if (knockbackResistanceValue >= 1.f) return;

	auto& lvl = *target.mLevel;

	float power = 0.4f;
	float height = 0.4f;
	float heightCap = 0.45f;

	if (source.isChildEntitySource()) {
		auto damager = lvl.fetchEntity(source.getDamagingEntityUniqueID(), false);
		power *= LegacyKnockback::getPunchEnchantmentMultiplier(damager);
	}

	else if (source.isEntitySource()) {

		auto attacker = lvl.fetchEntity(source.getEntityUniqueID(), false);
		if (attacker && attacker->isInstanceOfMob()) {

			switch (attacker->getEntityTypeId()) {
				case ActorType::Dragon: {
					power *= 4.f;
					height *= 4.f;
					heightCap *= 4.f;
					break;
				}
				case ActorType::IronGolem: {
					power *= 2.f;
					height *= 2.f;
					heightCap *= 2.f;
					break;
				}
				case ActorType::Player_0: {
					auto playerAttacker = ((Player*)attacker);
					if (playerAttacker->mEZPlayer->mHasResetSprint && playerAttacker->isSprinting()) {
						power += 0.4f;
						height += 0.1f;
						playerAttacker->mEZPlayer->mHasResetSprint = false;
					}
					break;
				}
				default: break;
			}

			float kbEnchantmentBonus = (float)(((Mob*)attacker)->getMeleeKnockbackBonus()) * 0.4f;
			power += kbEnchantmentBonus;
		}
	}

	float scaledKnockbackForce = std::fmax(0.f, 1.f - knockbackResistanceValue);
	if (scaledKnockbackForce < 1.f) {
		power *= scaledKnockbackForce;
		height *= scaledKnockbackForce;
	}

	float vector = std::sqrtf((dx * dx) + (dz * dz));
	if (vector < 0.0001f) return;
	vector = 1.f / vector;
	float friction = 0.5f;

	//auto& stateVector = target.mStateVectorComponent;
	//auto newDelta = stateVector.mPosDelta;
	auto newDelta = target.getRawActorPosDelta();

	if (!target.mTeleportedThisTick) {
		newDelta *= friction;
	}

	newDelta.x -= dx * vector * power;
	newDelta.y += height;
	newDelta.z -= dz * vector * power;

	if (newDelta.y > heightCap) {
		newDelta.y = heightCap;
	}

	if (target.getHealthAsInt() <= 0) {
		target.mIsKnockedBackOnDeath = true;
	}

	target.mStateVectorComponent.mPosDelta = newDelta;
}

void LegacyKnockback::calculatePlayerKnockback(Player &target, const ActorDamageSource &source, float dx, float dz) {

	float knockbackResistanceValue = target.getMutableAttribute(AttributeID::KnockbackResistance)->mCurrentValue;
	if (knockbackResistanceValue >= 1.f) return;

	auto& lvl = *target.mLevel;

	// initializations
	float power = settings.normalKBPower;
	float height = settings.normalKBHeight;
	float heightCap = settings.heightCap;
	float heightThreshold = settings.heightThreshold;

	if (source.isChildEntitySource()) {
		auto damager = lvl.fetchEntity(source.getDamagingEntityUniqueID(), false);
		if (damager) {

			if (settings.customProjectileKBEnabled) {

				switch (damager->getEntityTypeId()) {
					case ActorType::FishingHook:
					case ActorType::Snowball:
					case ActorType::ThrownEgg: {
						power = settings.comboProjectileKBPower; // projectiles used for pvp combos
						height = settings.comboProjectileKBHeight;
						break;
					}
					case ActorType::Enderpearl: {
						power = settings.enderpearlKBPower;
						height = settings.enderpearlKBHeight;
						break;
					}
					default: break;
				}

			}
			power *= LegacyKnockback::getPunchEnchantmentMultiplier(damager);
		}
	}

	else if (source.isEntitySource()) {

		auto attacker = lvl.fetchEntity(source.getEntityUniqueID(), false);
		if (attacker && attacker->isInstanceOfMob()) {

			switch (attacker->getEntityTypeId()) {
				case ActorType::Dragon: {
					power *= 4.f;
					height *= 4.f;
					heightCap *= 4.f;
					break;
				}
				case ActorType::IronGolem: {
					power *= 2.f;
					height *= 2.f;
					heightCap *= 2.f;
					break;
				}
				case ActorType::Player_0: {
					auto playerAttacker = ((Player*)attacker);
					if (playerAttacker->mEZPlayer->mHasResetSprint && playerAttacker->isSprinting()) {
						power += settings.additionalWTapKBPower;
						height += settings.additionalWTapKBHeight;
						playerAttacker->mEZPlayer->mHasResetSprint = false;
					}
					break;
				}
				default: break;
			}

			// attacking on the same tick that you receive knockback will set lower values (aka "reducing")
			uint64_t currentTick = lvl.getServerTick();
			if ((currentTick - target.mEZPlayer->mLastAttackedActorTimestamp) <= 1) {
				power *= settings.KBReductionFactor;
			}

			float kbEnchantmentBonus = (float)(((Mob*)attacker)->getMeleeKnockbackBonus()) * settings.normalKBPower;
			power += kbEnchantmentBonus;
		}
	}

	// calculate knockback resistance
	float scaledKnockbackForce = std::fmax(0.f, 1.f - knockbackResistanceValue);
	if (scaledKnockbackForce < 1.f) {
		power *= scaledKnockbackForce;
		height *= scaledKnockbackForce;
	}

	// momentum calculations
	float vector = std::sqrtf((dx * dx) + (dz * dz));
	if (vector < 0.0001f) return;
	vector = 1.f / vector;
	float horizontalFriction = 1.f;
	float verticalFriction = 1.f;

	//auto newDelta = target.mStateVectorComponent.mPosDelta;
	auto newDelta = target.getRawPlayerPosDelta();

	if (!target.mTeleportedThisTick) {

		horizontalFriction = 1.f / settings.horizontalKBFriction;
		verticalFriction = 1.f / settings.verticalKBFriction;

		// clamp pos delta
		float d = std::sqrtf((newDelta.x * newDelta.x) + (newDelta.z * newDelta.z));
		if (d > settings.maxHorizontalDisplacement) {
			newDelta.normalizeXZ();
			newDelta.x *= settings.maxHorizontalDisplacement;
			newDelta.z *= settings.maxHorizontalDisplacement;
		}
		newDelta.y = (float)std::clamp(newDelta.y, -settings.maxVerticalDisplacement, settings.maxVerticalDisplacement);

		newDelta.x *= horizontalFriction;
		newDelta.z *= horizontalFriction;
	}

	newDelta.x -= dx * vector * power;
	newDelta.z -= dz * vector * power;

	// heightcap stuff
	if (settings.useJavaHeightCap) {
		newDelta.y *= verticalFriction;
		newDelta.y += height;

		if (newDelta.y > heightThreshold) {
			newDelta.y = heightCap;
		}
	}
	else if (settings.useCustomHeightCap) {
		float oldDeltaY = newDelta.y;
		newDelta.y *= verticalFriction;
		newDelta.y += height;

		if ((newDelta.y + oldDeltaY) > heightThreshold) {
			newDelta.y = heightCap;
		}
	}
	else { // if no heightcap is configured
		  newDelta.y = height;
	}

	// client ignores motion packets if it's dead anyway so this seems to be pointless code for players
	// but keeping it in for the sake of vanilla consistency in case it gets used elsewhere
	if (target.getHealthAsInt() <= 0) {
		target.mIsKnockedBackOnDeath = true;
	}

	// for some reason vanilla broadcasts this but its just wasteful and doesn't do anything
	// instead we can just send the packet only to the player who was knocked back
	//target.mDimension->sendPacketForEntity(target, SetActorMotionPacket(target.mRuntimeID, newDelta), nullptr);
	SetActorMotionPacket motionPkt(target.mRuntimeID, newDelta);
	target.sendNetworkPacket(motionPkt);
}