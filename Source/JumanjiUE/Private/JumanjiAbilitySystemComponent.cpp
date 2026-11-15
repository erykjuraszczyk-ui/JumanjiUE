#include "JumanjiAbilitySystemComponent.h"

#include "JumanjiCombatDefaults.h"
#include "JumanjiGameplayAbility.h"

bool UJumanjiAbilitySystemComponent::TryActivateAbilitiesByInputTag(FGameplayTag InputTag)
{
    if (!InputTag.IsValid())
    {
        return false;
    }

    TArray<FGameplayAbilitySpecHandle> MatchingHandles;
    for (const FGameplayAbilitySpec& Spec : GetActivatableAbilities())
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
        UClass* FallbackAbilityClass,
        EJumanjiAbilitySlot ExpectedSlot)
    {
        UClass* LoadedAbilityClass = AbilityReference.IsNull()
            ? FallbackAbilityClass
            : AbilityReference.LoadSynchronous();

        if (!LoadedAbilityClass)
        {
            return;
        }
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

        FName ExpectedInputTagName = NAME_None;
        switch (ExpectedSlot)
        {
        case EJumanjiAbilitySlot::PrimaryAttack:
            ExpectedInputTagName = FName(TEXT("Input.Ability.Primary"));
            break;
        case EJumanjiAbilitySlot::Active:
            ExpectedInputTagName = FName(TEXT("Input.Ability.Active"));
            break;
        case EJumanjiAbilitySlot::Ultimate:
            ExpectedInputTagName = FName(TEXT("Input.Ability.Ultimate"));
            break;
        case EJumanjiAbilitySlot::Dodge:
            ExpectedInputTagName = FName(TEXT("Input.Ability.Dodge"));
            break;
        case EJumanjiAbilitySlot::Passive:
        default:
            break;
        }

        if (ExpectedInputTagName.IsNone())
        {
            if (AbilityCDO->GetInputTag().IsValid())
            {
                return;
            }
        }
        else
        {
            const FGameplayTag ExpectedInputTag =
                FGameplayTag::RequestGameplayTag(ExpectedInputTagName, false);

            if (!ExpectedInputTag.IsValid()
                || !AbilityCDO->GetInputTag().MatchesTagExact(ExpectedInputTag))
            {
                return;
            }
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

    // Generic melee is a temporary playable fallback. A roster DataTable class
    // takes precedence as soon as a character-specific ability asset is assigned.
    GrantAbility(
        CharacterDefinition.PrimaryAttackAbilityClass,
        UJumanjiDefaultPlayerMeleeAbility::StaticClass(),
        EJumanjiAbilitySlot::PrimaryAttack);
    GrantAbility(CharacterDefinition.SignatureAbilityClass, nullptr, EJumanjiAbilitySlot::Active);
    GrantAbility(CharacterDefinition.PassiveAbilityClass, nullptr, EJumanjiAbilitySlot::Passive);
    GrantAbility(CharacterDefinition.UltimateAbilityClass, nullptr, EJumanjiAbilitySlot::Ultimate);
    GrantAbility(CharacterDefinition.DodgeAbilityClass, nullptr, EJumanjiAbilitySlot::Dodge);
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
