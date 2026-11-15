#include "JumanjiPlayerState.h"

#include "JumanjiAbilitySystemComponent.h"
#include "JumanjiAttributeSet.h"
#include "Net/UnrealNetwork.h"

AJumanjiPlayerState::AJumanjiPlayerState()
{
    bReplicates = true;
    NetUpdateFrequency = 100.0f;

    AbilitySystemComponent = CreateDefaultSubobject<UJumanjiAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
    AbilitySystemComponent->SetIsReplicated(true);
    AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

    AttributeSet = CreateDefaultSubobject<UJumanjiAttributeSet>(TEXT("AttributeSet"));
}

UAbilitySystemComponent* AJumanjiPlayerState::GetAbilitySystemComponent() const
{
    return AbilitySystemComponent;
}

void AJumanjiPlayerState::SetSelectedCharacterId(FName CharacterId)
{
    if (!HasAuthority() || CharacterId.IsNone() || SelectedCharacterId == CharacterId)
    {
        return;
    }

    SelectedCharacterId = CharacterId;
    OnSelectedCharacterChanged.Broadcast(SelectedCharacterId);
    ForceNetUpdate();
}

void AJumanjiPlayerState::OnRep_SelectedCharacterId()
{
    OnSelectedCharacterChanged.Broadcast(SelectedCharacterId);
}

void AJumanjiPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AJumanjiPlayerState, SelectedCharacterId);
}
