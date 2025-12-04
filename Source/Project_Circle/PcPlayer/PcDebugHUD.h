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
	// Main draw loop
	virtual void DrawHUD() override;

	/** 
	 * Called by Player to show a message (e.g. "PERFECT JUMP!") 
	 * Adds it to the scrolling log on the right.
	 */
	UFUNCTION(BlueprintCallable, Category = "Style")
	void AddStyleMessage(FString Message, EStyleEventType Type);

private:
	// The list of active messages to render
	TArray<FStyleLogMessage> MessageLog;
	
	// --- VISUAL SETTINGS (Constants) ---
	
	const float MessageLifetime = 3.0f; // How long text stays on screen
	
	// Layout
	const float MarginRight = 300.0f;   // Distance from right edge
	const float MarginTop = 100.0f;     // Start Y position
	
	// Fuse Bar Dimensions
	const float BarWidth = 250.0f;
	const float BarHeight = 20.0f;
};