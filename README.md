# LegacyKnockback
A highly customizable knockback editor for BDS+EZ 1.16.20

## Features:

- client-auth sprint resetting mechanics (W-tapping increases knockback to damaged entity)
- server-auth sprint resetting mechanics (attacking force-cancels your sprint), choose between:
    - force cancel for every swing on an opponent (but not limited to a registered hit) - this matches java pre-1.9 behavior
    - just every hit registered on an opponent - this matches vanilla bedrock behavior
    - no forced sprint reset at all (what many bedrock PVP servers currently employ)
- fixed a bug where being hit with a projectile then by melee would bypass the 10 tick hurt cooldown (configurable)
- introduce a "reducing" system, where attacking on the same tick that knockback is administered will multiply your knockback by a configurable factor
- configurable normal and w-tap knockback, respectively
- configurable netherite armor knockback resistance
- configurable projectile knockback (for fishing rods, snowballs, and eggs currently)
- configurable heightcap; set max posDelta.y threshold
- configurable knockback friction (sets how much the target's momentum affects their knockback)

my personal favorite config:

```
  LegacyKnockback:
    enabled: true
    normalKBPower: 0.0.4
    normalKBHeight: 0.375
    additionalWTapKBPower: 0.35
    additionalWTapKBHeight: 0.085
    KBReductionFactor: 0.65
    KBFriction: 1.6
    maxHorizontalDisplacement: 0.2
    customProjectileKBEnabled: true
    comboProjectileKBPower: 0.4
    comboProjectileKBHeight: 0.36
    enderpearlKBPower: 0.8
    enderpearlKBHeight: 0.36
    useJavaSprintReset: true
    useJavaHeightCap: false
    useCustomHeightCap: true
    heightThreshold: 0.425
    heightCap: 0.4
    projectilesBypassHurtCooldown: false
    netheriteArmorKBResistance: 0
```
