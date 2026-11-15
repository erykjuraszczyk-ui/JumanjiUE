#pragma once

#include "CoreMinimal.h"
#include "JumanjiAdventureChapter.generated.h"

UENUM(BlueprintType)
enum class EJumanjiAdventureChapterPhase : uint8
{
    Inactive UMETA(DisplayName = "Inactive"),
    Active UMETA(DisplayName = "Active"),
    Completed UMETA(DisplayName = "Completed")
};

/** Public, replicated chapter summary consumed by the adventure HUD. */
USTRUCT(BlueprintType)
struct FJumanjiAdventureChapterState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Adventure")
    FName ChapterId = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Adventure")
    FName BoardEventId = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Adventure")
    FText DisplayName;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Adventure")
    FText Description;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Adventure")
    EJumanjiAdventureChapterPhase Phase = EJumanjiAdventureChapterPhase::Inactive;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Adventure")
    int32 Revision = 0;
};
