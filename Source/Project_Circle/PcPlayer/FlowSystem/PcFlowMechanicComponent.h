// ==========================================
// FILE: PcFlowMechanicComponent.h
// PATH: Source/Project_Circle/PcPlayer/FlowSystem/PcFlowMechanicComponent.h
// ==========================================
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

	UPROPERTY(EditAnywhere, Category = "Flow Config")
	float BaseDecayRate = 15.0f;
	
	UPROPERTY(EditAnywhere, Category = "Flow Config")
	float PanicDuration = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Flow Config")
	float DecayDelay = 1.0f;

	// --- RUNTIME STATE ---
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Flow State")
	int32 CurrentTier = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Flow State")
	float FlowPercent = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Flow State")
	EFlowState CurrentState = EFlowState::Stable;

	// --- INTERFACE ---
	void UpdateFlowLogic(float DeltaTime, bool bIsDrifting);
	void InjectFlow(float Amount);
	void SetFrozen(bool bFreeze);
	
	// Called by Player when Perfect Land happens
	void ForceTierUp(); 

	// Called by Player when hit. Returns TRUE if this was a Wipeout (Tier 0 hit).
	bool ApplyDamage();

	// Freezes the game briefly for impact
	void TriggerHitStop(UWorld* WorldContext);

	bool IsInPanic() const { return CurrentState == EFlowState::Panic; }

private:
	float PanicTimer = 0.0f;
	float DecayDelayTimer = 0.0f;
	FTimerHandle TimerHandle_HitStop;
	
	void DemoteTier();
	void PromoteTier();
	void ResetTimeDilation();
};