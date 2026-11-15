#include "JumanjiAbilitySystemComponent.h"

#include "JumanjiGameplayAbility.h"

bool UJumanjiAbilitySystemComponent::TryActivateAbilitiesByInputTag(FGameplayTag InputTag)
{
    if (!InputTag.IsValid())
    {
        return false;
    }

    TArray<FGameplayAbilitySpecHandle> MatchingHandles;
    for (const FGameplayAbilitySpec& Spec : GetActivatableAbilities().Items)
    {
        const UJumanjiGameplayAbility* JumanjiAbility = Cast<UJumanjiGameplayAbility>(Spec.Ability);
        if (JumanjiAbility && JumanjiAbility->GetInputTag().MatchesTagExact(InputTag))
        {
            MatchingHandles.Add(Spec.Handle);
        }
    }

    bool bActivatedAny = false;
    for (const FGameplayAbilitySpecHandle& Handle : MatchingHandles)
    {
        bActivatedAny |= TryActivateAbility(Handle);
    }

    return bActivatedAny;
}

void UJumanjiAbilitySystemComponent::GrantCharacterAbilityLoadout(
    const FJumanjiCharacterDefinition& CharacterDefinition,
    TArray<FGameplayAbilitySpecHandle>& OutGrantedHandles)
{
    OutGrantedHandles.Reset();

    if (!GetOwner() || !GetOwner()->HasAuthority())
    {
        return;
    }

    const auto GrantAbility = [this, &OutGrantedHandles](
        const TSoftClassPtr<UGameplayAbility>& AbilityReference)
    {
        if (AbilityReference.IsNull())
        {
            return;
        }

        UClass* LoadedAbilityClass = AbilityReference.LoadSynchronous();
        if (!LoadedAbilityClass || !LoadedAbilityClass->IsChildOf(UGameplayAbility::StaticClass()))
        {
            return;
        }

        const FGameplayAbilitySpec AbilitySpec(LoadedAbilityClass, 1, INDEX_NONE, GetOwner());
        const FGameplayAbilitySpecHandle Handle = GiveAbility(AbilitySpec);
        if (!Handle.IsValid())
        {
            return;
        }

        OutGrantedHandles.Add(Handle);

        const UJumanjiGameplayAbility* AbilityCDO =
            Cast<UJumanjiGameplayAbility>(LoadedAbilityClass->GetDefaultObject());

        if (AbilityCDO && AbilityCDO->ShouldActivateOnGranted())
        {
            TryActivateAbility(Handle);
        }
    };

    GrantAbility(CharacterDefinition.PrimaryAttackAbilityClass);
    GrantAbility(CharacterDefinition.SignatureAbilityClass);
    GrantAbility(CharacterDefinition.PassiveAbilityClass);
    GrantAbility(CharacterDefinition.UltimateAbilityClass);
    GrantAbility(CharacterDefinition.DodgeAbilityClass);
}

void UJumanjiAbilitySystemComponent::ClearGrantedAbilities(
    const TArray<FGameplayAbilitySpecHandle>& GrantedHandles)
{
    if (!GetOwner() || !GetOwner()->HasAuthority())
    {
        return;
    }

    for (const FGameplayAbilitySpecHandle& Handle : GrantedHandles)
    {
        if (Handle.IsValid())
        {
            ClearAbility(Handle);
        }
    }
}
