#include "main.h"
#include <dllentry.h>

DEFAULT_SETTINGS(settings);

void dllenter() {}
void dllexit() {}

namespace LegacyKnockback {

float generateRandomFloat() {
	std::uniform_real_distribution<float> genFloatFunc(0.f, 1.f);
	return genFloatFunc(rng);
}

int32_t getOnFireTime(Actor *projectile) {
	if (!projectile) return 0;
	auto component = projectile->tryGetComponent<ProjectileComponent>();
	if (component) {
		if (projectile->isOnFire() || component->mCatchFire) {
			return (uint32_t)(std::roundf(component->mOnFireTime));
		}
	}
	return 0;
}

float getPunchEnchantmentMultiplier(Actor* projectile) {
	if (!projectile) return 1.f;
	auto component = projectile->tryGetComponent<ProjectileComponent>();
	if (component) {
		return std::fmax(1.f, component->mKnockbackForce);
	}
	return 1.f;
}

void calculateMobKnockback(Mob *_this, ActorDamageSource const& source, float dx, float dz) {

	float knockbackResistanceValue = _this->getMutableAttribute(AttributeID::KnockbackResistance)->mCurrentValue;
	if (knockbackResistanceValue >= 1.f) return;

	auto& lvl = *_this->mLevel;

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
					if (playerAttacker->EZPlayerFields->mHasResetSprint && playerAttacker->isSprinting()) {
						power += 0.4f;
						height += 0.1f;
						playerAttacker->EZPlayerFields->mHasResetSprint = false;
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

	//auto& stateVector = _this->mStateVectorComponent;
	//auto newDelta = stateVector.mPosDelta;
	auto newDelta = _this->getRawActorPosDelta();

	if (!_this->mTeleportedThisTick) {
		newDelta *= friction;
	}

	newDelta.x -= dx * vector * power;
	newDelta.y += height;
	newDelta.z -= dz * vector * power;

	if (newDelta.y > heightCap) {
		newDelta.y = heightCap;
	}

	if (_this->getHealthAsInt() <= 0) {
		_this->mIsKnockedBackOnDeath = true;
	}

	_this->mStateVectorComponent.mPosDelta = newDelta;
}

void calculatePlayerKnockback(Player *_this, ActorDamageSource const& source, float dx, float dz) {

	float knockbackResistanceValue = _this->getMutableAttribute(AttributeID::KnockbackResistance)->mCurrentValue;
	if (knockbackResistanceValue >= 1.f) return;

	auto& lvl = *_this->mLevel;

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
					if (playerAttacker->EZPlayerFields->mHasResetSprint && playerAttacker->isSprinting()) {
						power += settings.additionalWTapKBPower;
						height += settings.additionalWTapKBHeight;
						playerAttacker->EZPlayerFields->mHasResetSprint = false;
					}
					break;
				}
				default: break;
			}

			// attacking on the same tick that you receive knockback will set lower values (aka "reducing")
			uint64_t currentTick = lvl.getServerTick();
			if ((currentTick - _this->EZPlayerFields->mLastAttackedActorTimestamp) <= 1) {
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

	//auto newDelta = _this->mStateVectorComponent.mPosDelta;
	auto newDelta = _this->getRawPlayerPosDelta();

	if (!_this->mTeleportedThisTick) {

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
	if (_this->getHealthAsInt() <= 0) {
		_this->mIsKnockedBackOnDeath = true;
	}

	// for some reason vanilla broadcasts this but its just wasteful and doesn't do anything
	// instead we can just send the packet only to the player who was knocked back
	//_this->mDimension->sendPacketForEntity(*_this, SetActorMotionPacket(_this->mRuntimeID, newDelta), nullptr);
	SetActorMotionPacket motionPkt(_this->mRuntimeID, newDelta);
	_this->sendNetworkPacket(motionPkt);
}

} // namespace LegacyKnockback

// KnockbackRules is a namespace, not a class
//THook(bool, "?useLegacyKnockback@KnockbackRules@@YA_NAEBVLevel@@@Z", void* level) { return true; }

TClasslessInstanceHook(void, "?knockback@ServerPlayer@@UEAAXPEAVActor@@HMMMMM@Z",
	void *source, int32_t damage, float dx, float dz, float power, float height, float heightCap) { return; }

TClasslessInstanceHook(void, "?knockback@Mob@@UEAAXPEAVActor@@HMMMMM@Z",
	void *source, int32_t damage, float dx, float dz, float power, float height, float heightCap) { return; }

// we need to use custom values for more accurate player pos deltas
// because the vanilla fields seems to zero the delta out
TInstanceHook(void, "?normalTick@ServerPlayer@@UEAAXXZ", ServerPlayer) {
	auto fields = this->EZPlayerFields;
	fields->mRawPosOld = this->getPos();
	original(this);
	fields->mRawPos = this->getPos();
}

TInstanceHook(void, "?handle@ServerNetworkHandler@@UEAAXAEBVNetworkIdentifier@@AEBVPlayerActionPacket@@@Z",
	ServerNetworkHandler, NetworkIdentifier const &netId, PlayerActionPacket const &pkt) {
	original(this, netId, pkt);
	if (pkt.mAction == PlayerActionType::STOP_SPRINT) {
		auto player = this->getServerPlayer(netId, pkt.mClientSubId);
		if (player) {
			player->EZPlayerFields->mHasResetSprint = true;
		}
	}
}

TInstanceHook(float, "?getArmorKnockbackResistance@ArmorItem@@UEBAMXZ", ArmorItem) {
	 if (this->mModelIndex == 7) {
	 	return settings.netheriteArmorKBResistance;
	 }
	 return original(this);
}

TInstanceHook(bool, "?attack@Player@@UEAA_NAEAVActor@@@Z", Player, Actor &actor) {

	auto& lvl = *this->mLevel;
	bool targetIsInstanceOfPlayer = actor.isInstanceOfPlayer();
	bool targetIsInstanceOfMob = actor.isInstanceOfMob();

	// custom stuff
	this->EZPlayerFields->mLastAttackedActorTimestamp = lvl.getServerTick();

	if (targetIsInstanceOfMob &&
		!targetIsInstanceOfPlayer &&
		!this->canUseAbility(AbilitiesIndex::AttackMobs)) {
		return false;
	}

	if (targetIsInstanceOfPlayer) {

		if (!this->canUseAbility(AbilitiesIndex::AttackPlayers)) {
			return false;
		}

		bool isPvpEnabled = lvl.getGameRuleValue<bool>(GameRulesIndex::Pvp);
		if (!isPvpEnabled) {
			return false;
		}
	}

	auto attachPos = this->getAttachPos(ActorLocation::Body, 0.f);
	int32_t damage = this->calculateAttackDamage(actor);
	if (damage <= 0) {
		this->playSynchronizedSound(LevelSoundEvent::AttackNoDamage, attachPos, -1, false);
	}
	else {
		this->playSynchronizedSound(LevelSoundEvent::AttackStrong, attachPos, -1, false);

		bool isCrit = false;
		if (settings.playersCanCrit &&
			(this->mFallDistance > 0.f) &&
			!this->mOnGround &&
			!this->onLadder() &&
			!this->isInWater() &&
			!this->hasEffect(*MobEffect::BLINDNESS) &&
			targetIsInstanceOfMob) {

			damage = (int32_t)(((float)(damage)) * 1.5f);
			isCrit = true;
		}

		ActorDamageByActorSource dmgSource(*this, ActorDamageCause::EntityAttack);
		if (actor.hurt(dmgSource, damage, true, false) && targetIsInstanceOfMob) {

			this->setLastHurtMob(&actor);
			this->causeFoodExhaustion(0.3f);
			if (isCrit) {
				this->_crit(actor); // this is just for the crit particle animation
			}

			ItemStack selectedItemCopy(this->getSelectedItem());
			if (selectedItemCopy.mValid &&
				selectedItemCopy.mItem &&
				!selectedItemCopy.isNull() &&
				(selectedItemCopy.mCount > 0) &&
				!this->isInCreativeMode()) {

				if (this->getHealthAsInt() > 0) {
					selectedItemCopy.getItem()->hurtEnemy(selectedItemCopy, (Mob*)&actor, this);
				}

				if (!this->mDead) {
					this->setSelectedItem(selectedItemCopy);
				}
			}
		}
	}
	if (settings.useJavaSprintReset && targetIsInstanceOfPlayer) { // only stop sprint when attacking players
		this->setSprinting(false);
	}
	return true;
}

TInstanceHook(bool, "?_hurt@Mob@@MEAA_NAEBVActorDamageSource@@H_N1@Z",
	Mob, ActorDamageSource &source, int32_t damage, bool knock, bool ignite) {

	auto cause = source.mCause;

	if (cause == ActorDamageCause::Suicide) {
		this->mLastHurtCause = ActorDamageCause::None;
		this->mLastHurt = 0;
	}
	else if ((this->mInvulnerableTime == settings.hurtCooldownTicks) && this->mChainedDamageEffects) { // defaults to 10 when hurt
		damage += this->mLastHurt;
	}
	else if ((float)(this->mInvulnerableTime) <= 0) {
		this->mLastHurt = 0;
		this->mLastHurtCause = ActorDamageCause::None;
	}
	else if (damage <= this->mLastHurt) {
		return false;
	}

	if (!this->_damageSensorComponentHurt(damage, this->mLastHurt, source)) {
		return false;
	}

	this->mNoActionTime = 0;
	this->mWalkAnimSpeed = 1.5f;

	if (source.isEntitySource()) {

		auto attacker = this->mLevel->fetchEntity(source.getEntityUniqueID(), false);
		if (attacker) {
			if (attacker->isInstanceOfMob()) {
				if (attacker->isInstanceOfPlayer()) {
					this->setLastHurtByPlayer((Player*)attacker);
				}
				else {
					this->setLastHurtByMob((Mob*)attacker);
				}
				EnchantUtils::doPostHurtEffects(*this, *(Mob*)attacker);
			}
		}
	}

	this->actuallyHurt(damage - this->mLastHurt, source, false);
	return this->hurtEffects(source, damage, knock, ignite);
}

TInstanceHook(bool, "?hurtEffects@Mob@@UEAA_NAEBVActorDamageSource@@H_N1@Z",
	Mob, ActorDamageSource &source, int32_t damage, bool knock, bool ignite) {

	if (this->isInstanceOfPlayer() &&
		(((Player*)this)->getAbilityValue<bool>(AbilitiesIndex::Invulnerable) ||
		this->isInvulnerableTo(source))) {
		return false;
	}

	auto cause = source.mCause;
	auto& lvl = *this->mLevel;
	uint64_t currentTick = lvl.getServerTick();
	bool hurt = false;
	bool chainedHurt = false;
	bool enoughDamage = false;

	if ((cause == ActorDamageCause::Suicide) || ((float)(this->mInvulnerableTime) <= 0)) {
		hurt                     = true;
		enoughDamage             = (damage > 0);
		this->mLastHurt          = damage;
		this->mLastHurtCause     = cause;
		this->mLastHealth        = this->getHealthAsInt();
		this->mLastHurtTimestamp = currentTick;
		this->mInvulnerableTime  = settings.hurtCooldownTicks; // defaults to 10 when hurt
		this->mHurtTime          = settings.hurtCooldownTicks; // defaults to 10 when hurt
		this->mHurtDuration      = settings.hurtCooldownTicks; // defaults to 10 when hurt
	}
	else if ((this->mInvulnerableTime == settings.hurtCooldownTicks) && this->mChainedDamageEffects) { // defaults to 10 when hurt
		chainedHurt              = (damage > 0);
		enoughDamage             = (damage > 0);
		this->mLastHurt          = damage;
		this->mLastHurtCause     = cause;
		this->mLastHealth        = this->getHealthAsInt();
		this->mLastHurtTimestamp = currentTick;
	}
	else if ((float)(this->mInvulnerableTime) > 0) {
		if (damage <= this->mLastHurt) {
			return false;
		}
		this->mLastHurt          = damage;
		this->mLastHurtCause     = cause;
		this->mLastHurtTimestamp = currentTick;
	}

	if (!this->isFireImmune()) {

		auto sourceActor = lvl.fetchEntity(source.getEntityUniqueID(), false);
		auto childActor = lvl.fetchEntity(source.getDamagingEntityUniqueID(), false);

		int32_t sourceActorBurnDuration = LegacyKnockback::getOnFireTime(sourceActor);
		int32_t childActorBurnDuration = LegacyKnockback::getOnFireTime(childActor);

		if (ignite || (sourceActorBurnDuration > 0) || (childActorBurnDuration > 0)) {
			int32_t givenBurnDuration = (int32_t)std::max(sourceActorBurnDuration, childActorBurnDuration);
			if (givenBurnDuration <= 0) {
				givenBurnDuration = 5;
			}
			this->setOnFire(givenBurnDuration);
		}
	}

	this->mHurtDirection = 0.f;
	if (!this->hasEffect(*MobEffect::HEAL)) { // instant health effect

		if (hurt) {

			if ((this->getHealthAsInt() > 0) || ((this->getHealthAsInt() <= 0) && this->checkTotemDeathProtection(source))) {
				lvl.broadcastActorEvent(*this, ActorEvent::HURT, -1); // (int32_t)cause
			}

			this->markHurt();

			if (source.isEntitySource()) {

				// note to self about getEntityUniqueID() vs getDamagingEntityUniqueID()
				// - getEntityUniqueID() will always refer to the parent actor (example: the player who shot a projectile)
				// - getDamagingEntityUniqueID() will refer to the child actor (ex: a projectile) if it exists, else it will refer to the parent actor
				auto attacker = lvl.fetchEntity(source.getEntityUniqueID(), false);

				if (attacker) {

					if (knock) {

						auto thisPos = this->getPos();
						auto thatPos = attacker->getPos();
						float dx = thatPos.x - thisPos.x;
						float dz = thatPos.z - thisPos.z;
						float distSqr = (float)(std::sqrtf((dx * dx) + (dz * dz)));
						if (distSqr < 0.0001f) {
							dx = (LegacyKnockback::generateRandomFloat() - LegacyKnockback::generateRandomFloat()) * 0.01f;
							dz = (LegacyKnockback::generateRandomFloat() - LegacyKnockback::generateRandomFloat()) * 0.01f;
						}
						this->mHurtDirection = (float)(std::atan2f(dz, dx) * RADIAN_DEGREES) - this->mRot.y;

						/*float power = 1.f + (float)attacker->getMeleeKnockbackBonus();
						if (attacker->isSprinting()) {
							power += 1.f;
						}
						this->knockback(attacker, (uint32_t)damage, dx, dz, power, 0.4f, 0.4f);*/

						if (this->isInstanceOfPlayer()) {
							LegacyKnockback::calculatePlayerKnockback((Player*)this, source, dx, dz);
						}
						else {
							LegacyKnockback::calculateMobKnockback(this, source, dx, dz);
						}
					}

					if (attacker->isInstanceOfPlayer()) {
						this->setLastHurtByPlayer((Player*)attacker);
					}
					else if (attacker->isInstanceOfMob()) {
						this->setLastHurtByMob((Mob*)attacker);
					}
				}
			}
			else {
				this->mHurtDirection = (float)((int32_t)(LegacyKnockback::generateRandomFloat() * 2.f) * 180.f);
			}
		}

		if ((this->getHealthAsInt() <= 0) && !this->checkTotemDeathProtection(source)) {
			this->die(source);
			return true;
		}

		if (enoughDamage && (hurt || chainedHurt)) {
			return true;
		}
	}
	return false;
}