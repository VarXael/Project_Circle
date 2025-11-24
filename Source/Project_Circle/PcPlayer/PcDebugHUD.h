#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "PcDebugHUD.generated.h"

UCLASS()
class PROJECT_CIRCLE_API APcDebugHUD : public AHUD
{
	GENERATED_BODY()
	
public:
	virtual void DrawHUD() override;
};