#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "PcDebugHUD.generated.h"

// --- TYPES ---

UENUM(BlueprintType)
enum class EStyleEventType : uint8
{
	Good    UMETA(DisplayName = "Good (+)"),
	Bad     UMETA(DisplayName = "Bad (-)"),
	Neutral UMETA(DisplayName = "Neutral")
};

struct FStyleLogMessage
{
	FString Text;
	float TimeRemaining;
	EStyleEventType Type;
};

// --- HUD CLASS ---

UCLASS()
class PROJECT_CIRCLE_API APcDebugHUD : public AHUD
{
	GENERATED_BODY()
	
public:
	virtual void DrawHUD() override;

	UFUNCTION(BlueprintCallable, Category = "Style")
	void AddStyleMessage(FString Message, EStyleEventType Type);

private:
	TArray<FStyleLogMessage> MessageLog;

	// Visual Helpers
	void DrawPhysicsDebug(class APcPlayerCharacter* Player);
	void DrawFlowDashboard(class APcPlayerCharacter* Player);
	void DrawActionLog(float BottomAnchorY, float RightAnchorX);

	// Settings
	const float MessageLifetime = 3.0f; 
	const float BarWidth = 300.0f;
	const float BarHeight = 25.0f;
};