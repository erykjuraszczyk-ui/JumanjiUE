#include "JumanjiTraversalPresentationSubsystem.h"

#include "Engine/DataTable.h"

void UJumanjiTraversalPresentationSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    LoadConfiguredManifest();
}

bool UJumanjiTraversalPresentationSubsystem::LoadConfiguredManifest()
{
    Definitions.Reset();

    if (PresentationDataTable.IsNull())
    {
        return false;
    }

    UDataTable* Table = PresentationDataTable.LoadSynchronous();
    if (!Table
        || Table->GetRowStruct() != FJumanjiTraversalPresentationDefinition::StaticStruct())
    {
        UE_LOG(LogTemp, Warning,
            TEXT("JUMANJI traversal presentation: manifest is missing or uses the wrong row struct; built-in labels will be used."));
        return false;
    }

    const TArray<FName> RowNames = Table->GetRowNames();
    if (RowNames.Num() < 1 || RowNames.Num() > 128)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("JUMANJI traversal presentation: manifest row count is outside the supported range."));
        return false;
    }

    TMap<FName, FJumanjiTraversalPresentationDefinition> ValidatedDefinitions;

    for (const FName RowName : RowNames)
    {
        const FJumanjiTraversalPresentationDefinition* Row =
            Table->FindRow<FJumanjiTraversalPresentationDefinition>(
                RowName,
                TEXT("JUMANJI traversal presentation validation"),
                false
            );

        if (!Row
            || RowName.IsNone()
            || Row->TraversalType == EJumanjiTraversalType::None
            || Row->DisplayName.IsEmpty()
            || Row->PromptText.IsEmpty()
            || Row->InProgressText.IsEmpty()
            || Row->CompletedText.IsEmpty()
            || Row->InterruptedText.IsEmpty()
            || !FMath::IsFinite(Row->PromptHoldSeconds)
            || Row->PromptHoldSeconds < 0.2f
            || Row->PromptHoldSeconds > 3.0f
            || ValidatedDefinitions.Contains(RowName))
        {
            UE_LOG(LogTemp, Warning,
                TEXT("JUMANJI traversal presentation: invalid or duplicate manifest row '%s'; using built-in labels."),
                *RowName.ToString());
            return false;
        }

        ValidatedDefinitions.Add(RowName, *Row);
    }

    Definitions = MoveTemp(ValidatedDefinitions);
    UE_LOG(LogTemp, Log,
        TEXT("JUMANJI traversal presentation: loaded %d manifest rows."),
        Definitions.Num());
    return true;
}

bool UJumanjiTraversalPresentationSubsystem::GetPresentationDefinition(
    FName TraversalId,
    EJumanjiTraversalType TraversalType,
    FJumanjiTraversalPresentationDefinition& OutDefinition) const
{
    if (TraversalType == EJumanjiTraversalType::None)
    {
        return false;
    }

    if (!TraversalId.IsNone())
    {
        if (const FJumanjiTraversalPresentationDefinition* Found = Definitions.Find(TraversalId))
        {
            if (Found->TraversalType != TraversalType)
            {
                return false;
            }

            OutDefinition = *Found;
            return true;
        }
    }

    BuildFallbackDefinition(TraversalType, OutDefinition);
    return true;
}

void UJumanjiTraversalPresentationSubsystem::GetAllPresentationDefinitions(
    TArray<FJumanjiTraversalPresentationDefinition>& OutDefinitions) const
{
    OutDefinitions.Reset();
    Definitions.GenerateValueArray(OutDefinitions);
}

void UJumanjiTraversalPresentationSubsystem::BuildFallbackDefinition(
    EJumanjiTraversalType TraversalType,
    FJumanjiTraversalPresentationDefinition& OutDefinition) const
{
    OutDefinition = FJumanjiTraversalPresentationDefinition();
    OutDefinition.TraversalType = TraversalType;
    OutDefinition.bAllowCameraLook = true;
    OutDefinition.bShowProgress = true;
    OutDefinition.PromptHoldSeconds = 0.65f;

    switch (TraversalType)
    {
    case EJumanjiTraversalType::ContextJump:
        OutDefinition.DisplayName = FText::FromString(TEXT("Skok kontekstowy"));
        OutDefinition.PromptText = FText::FromString(TEXT("Użyj przejścia"));
        OutDefinition.InProgressText = FText::FromString(TEXT("W trakcie skoku"));
        OutDefinition.CompletedText = FText::FromString(TEXT("Przejście zakończone"));
        OutDefinition.InterruptedText = FText::FromString(TEXT("Przejście przerwane"));
        break;

    case EJumanjiTraversalType::Climb:
        OutDefinition.DisplayName = FText::FromString(TEXT("Wspinaczka"));
        OutDefinition.PromptText = FText::FromString(TEXT("Rozpocznij wspinaczkę"));
        OutDefinition.InProgressText = FText::FromString(TEXT("Wspinanie w toku"));
        OutDefinition.CompletedText = FText::FromString(TEXT("Dotarto na górę"));
        OutDefinition.InterruptedText = FText::FromString(TEXT("Wspinaczka przerwana"));
        break;

    case EJumanjiTraversalType::Zipline:
        OutDefinition.DisplayName = FText::FromString(TEXT("Tyrolka"));
        OutDefinition.PromptText = FText::FromString(TEXT("Użyj tyrolki"));
        OutDefinition.InProgressText = FText::FromString(TEXT("Przejazd tyrolką"));
        OutDefinition.CompletedText = FText::FromString(TEXT("Dotarto na drugi koniec"));
        OutDefinition.InterruptedText = FText::FromString(TEXT("Przejazd przerwany"));
        break;

    case EJumanjiTraversalType::Lift:
        OutDefinition.DisplayName = FText::FromString(TEXT("Winda"));
        OutDefinition.PromptText = FText::FromString(TEXT("Wejdź na platformę"));
        OutDefinition.InProgressText = FText::FromString(TEXT("Platforma w ruchu"));
        OutDefinition.CompletedText = FText::FromString(TEXT("Platforma dotarła"));
        OutDefinition.InterruptedText = FText::FromString(TEXT("Przejazd przerwany"));
        break;

    default:
        OutDefinition.DisplayName = FText::FromString(TEXT("Przejście"));
        OutDefinition.PromptText = FText::FromString(TEXT("Użyj przejścia"));
        OutDefinition.InProgressText = FText::FromString(TEXT("Przejście w toku"));
        OutDefinition.CompletedText = FText::FromString(TEXT("Przejście zakończone"));
        OutDefinition.InterruptedText = FText::FromString(TEXT("Przejście przerwane"));
        break;
    }
}
