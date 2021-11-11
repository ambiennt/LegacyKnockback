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
    normalKnockbackPower: 0.375
    normalKnockbackHeight: 0.375
    additionalWTapKnockbackPower: 0.375
    additionalWTapKnockbackHeight: 0.075000003
    knockbackReductionFactor: 0.600000024
    knockbackFriction: 2
    comboProjectileKnockbackEnabled: true
    comboProjectileKnockbackPower: 0.375
    comboProjectileKnockbackHeight: 0.375
    useJavaSprintReset: true
    useLegacySprintReset: false
    useJavaHeightCap: false
    useCustomHeightCap: true
    heightThreshold: 0.400000006
    heightCap: 0.400000006
    projectilesBypassHurtCooldown: false
    netheriteArmorKnockbackResistance: 0
```
