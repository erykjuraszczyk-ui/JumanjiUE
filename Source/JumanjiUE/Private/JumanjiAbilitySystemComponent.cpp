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
        const TSoftClassPtr<UGameplayAbility>& AbilityReference,
        EJumanjiAbilitySlot ExpectedSlot)
    {
        if (AbilityReference.IsNull())
        {
            return;
        }

        UClass* LoadedAbilityClass = AbilityReference.LoadSynchronous();
        if (!LoadedAbilityClass || !LoadedAbilityClass->IsChildOf(UJumanjiGameplayAbility::StaticClass()))
        {
            return;
        }

        const UJumanjiGameplayAbility* AbilityCDO =
            Cast<UJumanjiGameplayAbility>(LoadedAbilityClass->GetDefaultObject());

        if (!AbilityCDO || AbilityCDO->GetAbilitySlot() != ExpectedSlot)
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

        if (AbilityCDO->ShouldActivateOnGranted())
        {
            TryActivateAbility(Handle);
        }
    };

    GrantAbility(CharacterDefinition.PrimaryAttackAbilityClass, EJumanjiAbilitySlot::PrimaryAttack);
    GrantAbility(CharacterDefinition.SignatureAbilityClass, EJumanjiAbilitySlot::Active);
    GrantAbility(CharacterDefinition.PassiveAbilityClass, EJumanjiAbilitySlot::Passive);
    GrantAbility(CharacterDefinition.UltimateAbilityClass, EJumanjiAbilitySlot::Ultimate);
    GrantAbility(CharacterDefinition.DodgeAbilityClass, EJumanjiAbilitySlot::Dodge);
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
