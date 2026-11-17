#pragma once

#include "CoreMinimal.h"
#include "JumanjiWorldEncounter.generated.h"

class AJumanjiBossCharacter;

UENUM(BlueprintType)
enum class EJumanjiWorldEncounterPhase : uint8
{
    Inactive UMETA(DisplayName = "Inactive"),
    Trials UMETA(DisplayName = "Great Spirit Trials"),
    Combat UMETA(DisplayName = "Enemy Waves"),
    Boss UMETA(DisplayName = "Boss Encounter"),
    Reward UMETA(DisplayName = "Reward Processing"),
    Completed UMETA(DisplayName = "Completed"),
    Failed UMETA(DisplayName = "Failed")
};

/** Shared, replicated encounter progress. The server is the only writer. */
USTRUCT(BlueprintType)
struct JUMANJIUE_API FJumanjiWorldEncounterState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Encounter")
    FName EncounterId = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Encounter")
    EJumanjiWorldEncounterPhase Phase = EJumanjiWorldEncounterPhase::Inactive;

    /** One-based during combat; zero while trials, boss, reward or terminal state is active. */
    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Encounter")
    int32 CurrentWave = 0;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Encounter")
    int32 TotalWaves = 0;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Encounter")
    int32 ActiveEnemyCount = 0;

    /** Replicated actor reference for the encounter HUD. Set by the server when the boss spawns. */
    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Encounter")
    TObjectPtr<AJumanjiBossCharacter> BossActor = nullptr;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Encounter")
    int32 Revision = 0;
};
