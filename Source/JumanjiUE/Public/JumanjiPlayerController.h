#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "JumanjiPlayerController.generated.h"

class UUserWidget;

/**
 * Local player controller responsible for creating the configured HUD widget.
 * The actual visual layout is authored as a Widget Blueprint in Unreal Editor.
 */
UCLASS()
class JUMANJIUE_API AJumanjiPlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    virtual void BeginPlay() override;

protected:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JUMANJI|UI")
    TSubclassOf<UUserWidget> HUDWidgetClass;

private:
    UPROPERTY()
    TObjectPtr<UUserWidget> HUDWidget;
};
