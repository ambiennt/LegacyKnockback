#include "main.h"
#include <dllentry.h>

DEFAULT_SETTINGS(settings);

void dllenter() {}
void dllexit() {}

// KnockbackRules is a namespace, not a class
//THook(bool, "?useLegacyKnockback@KnockbackRules@@YA_NAEBVLevel@@@Z", void* level) { return true; }

TClasslessInstanceHook(void, "?knockback@ServerPlayer@@UEAAXPEAVActor@@HMMMMM@Z",
	void *source, int32_t damage, float dx, float dz, float power, float height, float heightCap) { return; }

TClasslessInstanceHook(void, "?knockback@Mob@@UEAAXPEAVActor@@HMMMMM@Z",
	void *source, int32_t damage, float dx, float dz, float power, float height, float heightCap) { return; }

TInstanceHook(void, "?handle@ServerNetworkHandler@@UEAAXAEBVNetworkIdentifier@@AEBVPlayerActionPacket@@@Z",
	ServerNetworkHandler, NetworkIdentifier const &netId, PlayerActionPacket const &pkt) {
	original(this, netId, pkt);
	if (pkt.mAction == PlayerActionType::STOP_SPRINT) {
		auto player = this->getServerPlayer(netId, pkt.mClientSubId);
		if (player) {
			player->mEZPlayer->mHasResetSprint = true;
		}
	}
}

TInstanceHook(float, "?getArmorKnockbackResistance@ArmorItem@@UEBAMXZ", ArmorItem) {
	 if (this->mModelIndex == 7) {
	 	return settings.netheriteArmorKBResistance;
	 }
	 return original(this);
}

TInstanceHook(bool, "?attack@Player@@UEAA_NAEAVActor@@@Z", Player, Actor &target) {

	auto& lvl = *this->mLevel;
	bool targetIsInstanceOfPlayer = target.isInstanceOfPlayer();
	bool targetIsInstanceOfMob = target.isInstanceOfMob();

	// custom stuff
	this->mEZPlayer->mLastAttackedActorTimestamp = lvl.getServerTick();

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
	auto [dmg, isCrit] = LegacyKnockback::calculateAttackDamage(*this, target);

	if (dmg <= 0) {
		this->playSynchronizedSound(LevelSoundEvent::AttackNoDamage, attachPos, -1, false);
	}
	else {
		this->playSynchronizedSound(LevelSoundEvent::AttackStrong, attachPos, -1, false);

		ActorDamageByActorSource dmgSource(*this, ActorDamageCause::EntityAttack);
		if (target.hurt(dmgSource, dmg, true, false) && targetIsInstanceOfMob) {

			this->setLastHurtMob(&target);
			this->causeFoodExhaustion(0.3f);
			if (isCrit) {
				this->_crit(target); // this is just for the crit particle animation
			}

			ItemStack selectedItemCopy(this->getSelectedItem());
			if (selectedItemCopy && !this->isInCreativeMode()) {

				if (this->getHealthAsInt() > 0) {
					selectedItemCopy.getItem()->hurtEnemy(selectedItemCopy, (Mob*)&target, this);
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

						const auto& thisPos = this->getPos();
						const auto& thatPos = attacker->getPos();
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
						this->knockback(attacker, (int32_t)damage, dx, dz, power, 0.4f, 0.4f);*/

						if (this->isInstanceOfPlayer()) {
							LegacyKnockback::calculatePlayerKnockback(*(Player*)this, source, dx, dz);
						}
						else {
							LegacyKnockback::calculateMobKnockback(*this, source, dx, dz);
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