#include "JumanjiPlayerController.h"

#include "Blueprint/UserWidget.h"

void AJumanjiPlayerController::BeginPlay()
{
    Super::BeginPlay();

    if (!IsLocalController() || !HUDWidgetClass)
    {
        return;
    }

    HUDWidget = CreateWidget<UUserWidget>(this, HUDWidgetClass);
    if (HUDWidget)
    {
        HUDWidget->AddToViewport();
    }
}
