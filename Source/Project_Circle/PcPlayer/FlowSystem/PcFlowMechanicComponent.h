// ==========================================
// FILE: PcFlowMechanicComponent.h
// PATH: Source/Project_Circle/PcPlayer/FlowSystem/PcFlowMechanicComponent.h
// ==========================================
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PcFlowMechanicComponent.generated.h"

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
	UPROPERTY(EditAnywhere, Category = "Flow Economy")
	float MaxCharge = 3.0f;

	UPROPERTY(EditAnywhere, Category = "Flow Economy")
	float OverdriveThreshold = 2.0f; // Above this, speed increases

	// --- RUNTIME STATE ---
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Flow Economy")
	float CurrentCharge = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Flow Economy")
	bool bInOverdrive = false;

	// --- SCORING & MULTIPLIER ---
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Flow Scoring")
	float CurrentScore = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Flow Scoring")
	float CurrentMultiplier = 0.0f;

	// --- API ---
	
	// Returns true if successfully spent
	bool TrySpendCharge(float Amount);
	
	void AddCharge(float Amount);

	// Scoring Logic
	void AddScore(float BasePoints);
	void IncreaseMultiplier(float Amount);
	void ResetMultiplier();
	
	// Trigger HitStop visual effect (moved from old implementation)
	void TriggerHitStop(UWorld* WorldContext);
	void ResetTimeDilation();

	UFUNCTION(BlueprintPure)
	bool IsOverdrive() const { return bInOverdrive; }

private:
	FTimerHandle TimerHandle_HitStop;
};