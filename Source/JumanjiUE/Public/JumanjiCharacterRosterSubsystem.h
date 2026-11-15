#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "JumanjiCharacterDefinition.h"
#include "JumanjiCharacterRosterSubsystem.generated.h"

/** Runtime catalog for the nine playable Great Spirits. */
UCLASS(Config = Game)
class JUMANJIUE_API UJumanjiCharacterRosterSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    /** C++ accessor; the returned array is owned by this subsystem. */
    const TArray<FJumanjiCharacterDefinition>& GetRoster() const
    {
        return CharacterDefinitions;
    }

    /** Blueprint-friendly copy of the catalog for a UMG selection screen. */
    UFUNCTION(BlueprintCallable, Category = "JUMANJI|Roster")
    void GetRosterForBlueprint(TArray<FJumanjiCharacterDefinition>& OutRoster) const;

    UFUNCTION(BlueprintCallable, Category = "JUMANJI|Roster")
    bool GetCharacterDefinition(FName CharacterId, FJumanjiCharacterDefinition& OutDefinition) const;

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Roster")
    bool IsPlayableCharacter(FName CharacterId) const;

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Roster")
    FName GetDefaultCharacterId() const;

private:
    void BuildDefaultRoster();
    bool LoadConfiguredRosterData();

    /** Optional DataTable override. Leave unset to use the built-in nine-spirit catalog. */
    UPROPERTY(Config, EditDefaultsOnly, Category = "JUMANJI|Roster")
    TSoftObjectPtr<UDataTable> ConfiguredRosterDataTable;

    UPROPERTY(Transient)
    TArray<FJumanjiCharacterDefinition> CharacterDefinitions;
};
