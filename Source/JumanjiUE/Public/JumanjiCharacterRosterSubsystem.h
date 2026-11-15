#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "JumanjiCharacterDefinition.h"
#include "JumanjiCharacterRosterSubsystem.generated.h"

/** Runtime catalog for the nine playable Great Spirits. */
UCLASS()
class JUMANJIUE_API UJumanjiCharacterRosterSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Roster")
    const TArray<FJumanjiCharacterDefinition>& GetRoster() const
    {
        return CharacterDefinitions;
    }

    UFUNCTION(BlueprintCallable, Category = "JUMANJI|Roster")
    bool GetCharacterDefinition(FName CharacterId, FJumanjiCharacterDefinition& OutDefinition) const;

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Roster")
    bool IsPlayableCharacter(FName CharacterId) const;

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Roster")
    FName GetDefaultCharacterId() const;

private:
    void BuildDefaultRoster();

    UPROPERTY(Transient)
    TArray<FJumanjiCharacterDefinition> CharacterDefinitions;
};
