#include "main.h"
#include <dllentry.h>

DEFAULT_SETTINGS(settings);

static std::unordered_set<Actor*> hasResetSprint;
static std::unordered_set<Player*> dmgSource;
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

class AttributeInstance {
public:
    char pad[0x84];
    float currentVal;
    float min;
    float max;
    float defaultVal;
};

class BaseAttributeMap {
public:
    class AttributeInstance* getMutableInstance(unsigned int);
};

AttributeInstance* getAttribute(Actor* actor, unsigned int id) {
    auto attributes = direct_access<BaseAttributeMap*>(actor, 0x438);
    return attributes->getMutableInstance(id);
}

bool isComboProjectile(ActorDamageSource &source) { // projectiles used for PVP combos
    switch (source.getDamagingEntityType()) {
        case ActorType::FishingHook:
        case ActorType::Snowball:
        case ActorType::ThrownEgg:
            return true;
            
        default: return false;
    }
}

Vec3 getPosDelta(const Vec3& prevPos, const Vec3& currPos) {
    Vec3 posDelta;
    posDelta.x = currPos.x - prevPos.x;
    posDelta.y = currPos.y - prevPos.y;
    posDelta.z = currPos.z - prevPos.z;
    return posDelta;
}

TClasslessInstanceHook(bool, "?useLegacyKnockback@KnockbackRules@@YA_NAEBVLevel@@@Z", void* level) { return true; }

THook(float, "?getArmorKnockbackResistance@ArmorItem@@UEBAMXZ", void* armorItem) {

     bool isNetherite = (direct_access<int>(armorItem, 0x1C0) == 7);
     if (isNetherite) {
        return settings.netheriteArmorKnockbackResistance;
     }
     return original(armorItem);
}

THook(bool, "?attack@Player@@UEAA_NAEAVActor@@@Z", Player *player, Actor *actor) {
    
    attackTimestamp[player] = LocateService<Level>()->GetServerTick();
    bool result = original(player, actor);
    if (settings.useJavaSprintReset && actor->getEntityTypeId() == ActorType::Player_0) {
        CallServerClassMethod<void>("?setSprinting@Mob@@UEAAX_N@Z", player, false);
    }
    return result;
}

THook(void, "?setSprinting@Mob@@UEAAX_N@Z", Mob *mob, bool shouldSprint) {
    
    //std::cout << "0x" << std::hex << (__int64)_ReturnAddress() - (__int64)GetModuleHandle(0) << std::endl;
    if (mob->getEntityTypeId() == ActorType::Player_0) {
        if (_ReturnAddress() == (void*)((uintptr_t) GetModuleHandle(0) + 0x3AA84A)) { //sprint stopped via user input
            hasResetSprint.insert(mob);
        }
        else if ((!settings.useLegacySprintReset || settings.useJavaSprintReset) || (!settings.useLegacySprintReset && !settings.useJavaSprintReset))  {
            if (_ReturnAddress() == (void*)((uintptr_t) GetModuleHandle(0) + 0x71E634)) return; //sprint force-stopped when attacking
        }
    }
    original(mob, shouldSprint);
}

THook(bool, "?_hurt@Player@@MEAA_NAEBVActorDamageSource@@H_N1@Z", Player *player, ActorDamageSource &source, int dmg, bool knock, bool ignite) {

    if (settings.comboProjectileKnockbackEnabled) {
        if (isComboProjectile(source)) { dmgSource.insert(player); }
        else { dmgSource.erase(player); }
    } 
    return original(player, source, dmg, knock, ignite);
}

THook(void, "?knockback@ServerPlayer@@UEAAXPEAVActor@@HMMMMM@Z",
    ServerPlayer *player, Actor *source, int dmg, float xd, float zd, float power, float height, float heightCap) {

    auto lastHurtCause = direct_access<ActorDamageCause>(player, 0x700);
    if (lastHurtCause == ActorDamageCause::None) return;

    float kbEnchantmentBonus = CallServerClassMethod<float>("?getMeleeKnockbackBonus@Mob@@UEAAHXZ", source) * 0.4f;
    float punchEnchantmentMultiplier = 1.0f;
    if (_ReturnAddress() == (void*)((uintptr_t) GetModuleHandle(0) + 0x273BFE)) { //hack: get punch knockback value from arrows
        punchEnchantmentMultiplier = power; // enchantment tier * 1.6
    }

    if (lastHurtCause == ActorDamageCause::Projectile) {
        if (dmgSource.count(player)) {
            power = settings.comboProjectileKnockbackPower;
            height = settings.comboProjectileKnockbackPower;
        }
        else {
            power = settings.normalKnockbackPower;
            height = settings.normalKnockbackHeight;
        }
        power *= punchEnchantmentMultiplier;
    }
    else if (lastHurtCause == ActorDamageCause::EntityAttack) {
        if (hasResetSprint.count(source) && CallServerClassMethod<bool>("?isSprinting@Mob@@UEBA_NXZ", source)) {
            hasResetSprint.erase(source);
            power = settings.sprintResetKnockbackPower + kbEnchantmentBonus;
            height = settings.sprintResetKnockbackHeight;
        }
        else {
            power = settings.normalKnockbackPower + kbEnchantmentBonus;
            height = settings.normalKnockbackHeight;
        }
    }
    else {
        power = settings.normalKnockbackPower;
        height = settings.normalKnockbackHeight;
    }
    heightCap = settings.heightCap;

    auto knockbackResistanceAttribute = getAttribute(player, 9);
    float scaledKnockbackForce = std::max(1.0f - knockbackResistanceAttribute->currentVal, 0.0f);
    if (scaledKnockbackForce < 1.0f) {
        power *= scaledKnockbackForce;
        height *= scaledKnockbackForce;
    }

    uint64_t currentTick = LocateService<Level>()->GetServerTick();
    if (currentTick - attackTimestamp[player] <= 1) {
        power *= settings.knockbackReductionFactor; // attacking on the same tick that you receive knockback will set lower values (aka "reducing")
    }

    //auto posDelta = direct_access<Vec3>(player, 0x494);
    auto posDelta = getPosDelta(player->getPosOld(), player->getPos());
    float f = sqrt(xd * xd + zd * zd);
    if (f <= 0.0f) return;

    if (settings.useHeightCap) {
        posDelta.x /= 2.0f;
        posDelta.y /= 2.0f;
        posDelta.z /= 2.0f;

        posDelta.x -= xd / f * power;
        posDelta.y += height;
        posDelta.z -= zd / f * power;

        if (posDelta.y > heightCap) {
            posDelta.y = heightCap;
        }
    }
    else {
        posDelta.x /= 2.0f;
        posDelta.z /= 2.0f;

        posDelta.x -= xd / f * power;
        posDelta.y = height;
        posDelta.z -= zd / f * power;
    }

    /*auto healthAttribute = getAttribute(player, 7);
    float healthValue = healthAttribute->currentVal;
    if (healthValue <= 0.0f) {
        direct_access<bool>(player, 0x570) = true; //mIsKnockedBackOnDeath - client ignores motion packets if it's dead
    }*/

    SetActorMotionPacket pkt;
    pkt.rid = player->getRuntimeID();
    pkt.motion = posDelta;
    Dimension* dimension = player->getDimension();
    return dimension->sendPacketForEntity(*player, pkt, nullptr);
}