#include "main.h"
#include <dllentry.h>

DEFAULT_SETTINGS(settings);

static std::unordered_set<Mob*> hasResetSprint;
static std::unordered_map<Player*, ActorType> dmgSource;
static std::unordered_map<Player*, uint64_t> attackTimestamp;

void dllenter() {}
void dllexit() {}
void PreInit() {
    GetServerSymbolWithOffset<PatchSpan<8>>("?doKnockbackAttack@KnockbackRules@@YAXAEAVMob@@0AEBVVec2@@M@Z", 0x117)
        ->VerifyPatchFunction({0xF3, 0x0F, 0x59, 0x1D, 0x59, 0x6D, 0xAB, 0x00}, NopFilled{}); // F3 0F 59 1D 59 6D AB 00
    GetServerSymbolWithOffset<PatchSpan<8>>("?doKnockbackAttack@KnockbackRules@@YAXAEAVMob@@0AEBVVec2@@M@Z", 0x124)
        ->VerifyPatchFunction({0xF3, 0x0F, 0x59, 0x15, 0x4C, 0x6D, 0xAB, 0x00}, NopFilled{}); // F3 0F 59 15 4C 6D AB 00

    // prevent projectiles from bypassing hurt cooldown by setting newEntityAttack check to false
    if (!settings.projectilesBypassHurtCooldown) {
        GetServerSymbolWithOffset<PatchSpan<3>>("?hurtEffects@Mob@@UEAA_NAEBVActorDamageSource@@H_N1@Z", 0x84)
            ->VerifyPatchFunction({0x83, 0xF8, 0x02}, {0x83, 0xF8, 0x20}); // 83 F8 02 - set actorDamageCause to enum type 32 (invalid cause)
        GetServerSymbolWithOffset<PatchSpan<3>>("?_hurt@Mob@@MEAA_NAEBVActorDamageSource@@H_N1@Z", 0x48)
            ->VerifyPatchFunction({0x83, 0xF8, 0x02}, {0x83, 0xF8, 0x20}); // 83 F8 02
    }

    Mod::PlayerDatabase::GetInstance().AddListener(SIG("left"), [](Mod::PlayerEntry const &entry) {
        dmgSource.erase(entry.player);
        hasResetSprint.erase(entry.player);
        attackTimestamp.erase(entry.player);
    });
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

TClasslessInstanceHook(bool, "?useLegacyKnockback@KnockbackRules@@YA_NAEBVLevel@@@Z", void* level) { return true; }

THook(float, "?getArmorKnockbackResistance@ArmorItem@@UEBAMXZ", ArmorItem *item) {
     if (item->mModelIndex == 7) return settings.netheriteArmorKnockbackResistance;
     return original(item);
}

THook(bool, "?attack@Player@@UEAA_NAEAVActor@@@Z", Player *player, Actor *actor) {
    attackTimestamp[player] = LocateService<Level>()->GetServerTick();
    bool result = original(player, actor);
    if (settings.useJavaSprintReset && (actor->getEntityTypeId() == ActorType::Player_0)) {
        ((Player*)actor)->setSprinting(false);
    }
    return result;
}

THook(void, "?setSprinting@Mob@@UEAAX_N@Z", Mob *mob, bool shouldSprint) {
    //std::cout << "0x" << std::hex << (int64_t)_ReturnAddress() - (int64_t)GetModuleHandle(0) << std::endl;
    if (mob->getEntityTypeId() == ActorType::Player_0) {
        uint64_t returnAddress = ((int64_t)_ReturnAddress() - (int64_t)GetModuleHandle(0));
        if (returnAddress == 0x3AA84A) { //sprint stopped via user input
            hasResetSprint.insert(mob);
        }
        else if ((!settings.useLegacySprintReset || settings.useJavaSprintReset) || (!settings.useLegacySprintReset && !settings.useJavaSprintReset))  {
            if (returnAddress == 0x71E634) return; //sprint force-stopped when attacking
        }
    }
    original(mob, shouldSprint);
}

THook(bool, "?_hurt@Player@@MEAA_NAEBVActorDamageSource@@H_N1@Z", Player *player, ActorDamageSource &source, int dmg, bool knock, bool ignite) {
    dmgSource[player] = source.getDamagingEntityType();
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

            if (isComboProjectile(dmgSource[player])) {
                power = settings.comboProjectileKnockbackPower;
                height = settings.comboProjectileKnockbackHeight;
            }

            else if (dmgSource[player] == ActorType::Enderpearl) {
                power = settings.enderpearlKnockbackPower;
                height = settings.enderpearlKnockbackHeight;
            }
        }
        power *= punchEnchantmentMultiplier;
    }
        
    else if (source && fromEntity) {

        auto attacker = ((Mob*)source);
        float kbEnchantmentBonus = ((float)(attacker->getMeleeKnockbackBonus()) * 0.4f);

        if (hasResetSprint.count(attacker) && attacker->isSprinting()) {
            hasResetSprint.erase(attacker);
            power += settings.additionalWTapKnockbackPower;
            height += settings.additionalWTapKnockbackHeight;
        }

        // attacking on the same tick that you receive knockback will set lower values (aka "reducing")
        uint64_t currentTick = LocateService<Level>()->GetServerTick();
        if ((currentTick - attackTimestamp[player]) <= 1) {
            power *= settings.knockbackReductionFactor;
        }
        power += kbEnchantmentBonus;
    }

    // calculate knockback resistance
    float knockbackResistanceValue = getAttribute(player, 9)->currentVal;
    float scaledKnockbackForce = std::fmax(1.0f - knockbackResistanceValue, 0.0f);

    if (scaledKnockbackForce < 1.0f) {
        power *= scaledKnockbackForce;
        height *= scaledKnockbackForce;
    }

    // auto posDelta = player->mStateVectorComponent.mPosDelta;
    auto posDelta = player->mTeleportedThisTick ? Vec3::ZERO : player->getPosDelta();
    float oldPosDeltaY = posDelta.y;
    float f = sqrt(xd * xd + zd * zd);
    if (f <= 0.0f) return;

    if (settings.useJavaHeightCap) {
        posDelta.x /= settings.knockbackFriction;
        posDelta.y /= settings.knockbackFriction;
        posDelta.z /= settings.knockbackFriction;

        posDelta.x -= xd / f * power;
        posDelta.y += height;
        posDelta.z -= zd / f * power;

        if (posDelta.y > settings.heightThreshold) {
            posDelta.y = heightCap;
        }
    }
    else {
        posDelta.x /= settings.knockbackFriction;
        posDelta.z /= settings.knockbackFriction;

        posDelta.x -= xd / f * power;
        posDelta.y = height;
        posDelta.z -= zd / f * power;

        if (settings.useCustomHeightCap && (oldPosDeltaY + posDelta.y > settings.heightThreshold)) {
            posDelta.y = heightCap;
        }
    }

    /*float healthValue = getAttribute(player, 7)->currentVal;
    if (healthValue <= 0.0f) {
        player->mIsKnockedBackOnDeath = true; // client ignores motion packets if it's dead anyway
    }*/

    SetActorMotionPacket pkt;
    pkt.mRuntimeId = player->mRuntimeID;
    pkt.mMotion = posDelta;
    player->mDimension->sendPacketForEntity(*player, pkt, nullptr);
}