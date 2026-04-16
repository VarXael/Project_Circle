#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "PcQPlayerMovementComponent.h"
#include "PcQDebugHUD.generated.h"

UCLASS()
class PROJECT_CIRCLE_API APcQDebugHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

	UPROPERTY(EditAnywhere) float DotRingRadius = 7.f;
	UPROPERTY(EditAnywhere) float DotRingThickness = 1.2f;
	UPROPERTY(EditAnywhere) float DotSize = 2.f;
	UPROPERTY(EditAnywhere) FLinearColor CrosshairColor = FLinearColor(1.f, 1.f, 1.f, 0.92f);

private:
	void DrawDotCrosshair();
	void DrawCircleHUD(float CX, float CY, float Radius, FLinearColor Color, float Thickness, int32 Segments);
	void DrawBhopDebug(UPcQPlayerMovementComponent* MC);
	FLinearColor GetStateColor(EBhopState State) const;
};