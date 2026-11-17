#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "JumanjiTraversalPresentationSubsystem.h"
#include "JumanjiTraversalPromptWidget.generated.h"

class AJumanjiTraversalLink;
class UJumanjiTraversalComponent;

UENUM(BlueprintType)
enum class EJumanjiTraversalPromptStatus : uint8
{
    Hidden,
    Available,
    PendingServer,
    InProgress,
    Completed,
    Interrupted
};

/** View model consumed by the mobile context prompt Widget Blueprint. */
USTRUCT(BlueprintType)
struct JUMANJIUE_API FJumanjiTraversalPromptViewModel
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Traversal|UI")
    FName TraversalId = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Traversal|UI")
    EJumanjiTraversalType TraversalType = EJumanjiTraversalType::None;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Traversal|UI")
    EJumanjiTraversalPromptStatus Status = EJumanjiTraversalPromptStatus::Hidden;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Traversal|UI")
    FText DisplayName;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Traversal|UI")
    FText StatusText;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Traversal|UI")
    TSoftObjectPtr<UTexture2D> Icon;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Traversal|UI")
    bool bCanRequest = false;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Traversal|UI")
    bool bAllowCameraLook = true;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Traversal|UI")
    bool bShowProgress = true;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Traversal|UI")
    float DurationSeconds = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Traversal|UI")
    float PromptHoldSeconds = 0.65f;

    UPROPERTY(BlueprintReadOnly, Category = "JUMANJI|Traversal|UI")
    int32 Revision = 0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FJumanjiTraversalPromptChanged,
    const FJumanjiTraversalPromptViewModel&, ViewModel
);

/** UMG base that exposes a validated, server-confirmed traversal prompt state. */
UCLASS(Blueprintable)
class JUMANJIUE_API UJumanjiTraversalPromptWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "JUMANJI|Traversal|UI")
    bool SetFocusedTraversalLink(AJumanjiTraversalLink* TraversalLink);

    UFUNCTION(BlueprintCallable, Category = "JUMANJI|Traversal|UI")
    void ClearFocusedTraversal();

    UFUNCTION(BlueprintCallable, Category = "JUMANJI|Traversal|UI")
    bool RequestFocusedTraversal();

    UFUNCTION(BlueprintPure, Category = "JUMANJI|Traversal|UI")
    FJumanjiTraversalPromptViewModel GetPromptViewModel() const { return PromptViewModel; }

    UPROPERTY(BlueprintAssignable, Category = "JUMANJI|Traversal|UI")
    FJumanjiTraversalPromptChanged OnPromptChanged;

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
    void BindTraversalComponent();
    void UnbindTraversalComponent();
    void HandleTraversalStateChanged(const FJumanjiTraversalReplicatedState& State);
    void BroadcastPrompt();

    UPROPERTY(Transient)
    TObjectPtr<AJumanjiTraversalLink> FocusedTraversalLink;

    UPROPERTY(Transient)
    TObjectPtr<UJumanjiTraversalComponent> BoundTraversalComponent;

    UPROPERTY(Transient, BlueprintReadOnly, Category = "JUMANJI|Traversal|UI",
        meta = (AllowPrivateAccess = "true"))
    FJumanjiTraversalPromptViewModel PromptViewModel;

    float RequestDeadlineSeconds = 0.0f;
    float StatusHideDeadlineSeconds = 0.0f;
};
