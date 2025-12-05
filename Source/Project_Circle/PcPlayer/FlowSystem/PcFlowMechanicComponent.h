#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PcFlowMechanicComponent.generated.h"

UENUM(BlueprintType)
enum class EFlowState : uint8
{
	Stable      UMETA(DisplayName = "Stable"),
	Draining    UMETA(DisplayName = "Draining"),
	Charging    UMETA(DisplayName = "Charging"),
	Frozen      UMETA(DisplayName = "Frozen (Air)"),
	Panic       UMETA(DisplayName = "PANIC (0%)")
};

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class PROJECT_CIRCLE_API UPcFlowMechanicComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UPcFlowMechanicComponent();

protected:
	virtual void BeginPlay() override;

public:	
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// --- CONFIGURATION ---
	UPROPERTY(EditAnywhere, Category = "Flow Config")
	int32 MaxTiers = 3;

	// How fast the bar drains per second (in %)
	UPROPERTY(EditAnywhere, Category = "Flow Config")
	float BaseDecayRate = 15.0f;
	
	// How long (seconds) you have at 0% before you demote
	UPROPERTY(EditAnywhere, Category = "Flow Config")
	float PanicDuration = 1.0f;

	// --- RUNTIME STATE ---
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Flow State")
	int32 CurrentTier = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Flow State")
	float FlowPercent = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Flow State")
	EFlowState CurrentState = EFlowState::Stable;

	// --- INTERFACE (Called by Player) ---
	
	/** Called every frame by the player. bIsDrifting = is the player actively generating flow? */
	void UpdateFlowLogic(float DeltaTime, bool bIsDrifting);

	/** Called when player does a specific action (Combo, Perfect Land) */
	void InjectFlow(float Amount);

	/** Called when Jumping (Freezes decay) */
	void SetFrozen(bool bFreeze);

	/** Called on Jump Launch (Forces next tier, sets bar to 0) */
	void ApplyJumpBonus();

	/** Returns true if we are in the grace period of 0% */
	bool IsInPanic() const { return CurrentState == EFlowState::Panic; }

private:
	float PanicTimer = 0.0f;
	void DemoteTier();
	void PromoteTier();
};