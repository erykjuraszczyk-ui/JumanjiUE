#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "JumanjiMeleeAttackAbility.h"
#include "JumanjiCombatDefaults.generated.h"

/**
 * Native fallback damage Gameplay Effect.
 * Applies the negative Data.Damage SetByCaller magnitude to Health.
 * A designer-authored GE can override this class in an ability Blueprint.
 */
UCLASS()
class JUMANJIUE_API UJumanjiDefaultDamageGameplayEffect : public UGameplayEffect
{
    GENERATED_BODY()

public:
    UJumanjiDefaultDamageGameplayEffect();
};

/** Prototype enemy attributes used when no initialization GE is assigned. */
UCLASS()
class JUMANJIUE_API UJumanjiEnemyInitializationGameplayEffect : public UGameplayEffect
{
    GENERATED_BODY()

public:
    UJumanjiEnemyInitializationGameplayEffect();
};

/** Prototype boss attributes used when no initialization GE is assigned. */
UCLASS()
class JUMANJIUE_API UJumanjiBossInitializationGameplayEffect : public UGameplayEffect
{
    GENERATED_BODY()

public:
    UJumanjiBossInitializationGameplayEffect();
};

/** Generic prototype primary action so all nine spirits can test combat before unique assets exist. */
UCLASS()
class JUMANJIUE_API UJumanjiDefaultPlayerMeleeAbility : public UJumanjiMeleeAttackAbility
{
    GENERATED_BODY()

public:
    UJumanjiDefaultPlayerMeleeAbility();
};

/** Generic prototype enemy action requested by the perception controller. */
UCLASS()
class JUMANJIUE_API UJumanjiDefaultEnemyMeleeAbility : public UJumanjiMeleeAttackAbility
{
    GENERATED_BODY()

public:
    UJumanjiDefaultEnemyMeleeAbility();
};
