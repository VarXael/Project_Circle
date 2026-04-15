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

	/** Radius of the outer ring. */
	UPROPERTY(EditAnywhere, Category = "Crosshair")
	float DotRingRadius = 7.f;

	/** Thickness of the outer ring lines. */
	UPROPERTY(EditAnywhere, Category = "Crosshair")
	float DotRingThickness = 1.2f;

	/** Half-size of the center filled dot. */
	UPROPERTY(EditAnywhere, Category = "Crosshair")
	float DotSize = 2.f;

	UPROPERTY(EditAnywhere, Category = "Crosshair")
	FLinearColor CrosshairColor = FLinearColor(1.f, 1.f, 1.f, 0.92f);

private:
	void DrawDotCrosshair();
	void DrawCircleHUD(float CX, float CY, float Radius, FLinearColor Color, float Thickness, int32 Segments);
	void DrawBhopDebug(UPcQPlayerMovementComponent* MC);
	FLinearColor GetStateColor(EBhopState State) const;
};