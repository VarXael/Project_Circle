#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Project_Circle/MusicSystem/MusicImportSystem/PcMusicAnalysisTypes.h"
#include "PcQPlayerMovementComponent.generated.h"

UENUM(BlueprintType)
enum class EBhopState : uint8 { Idle, Charging, Active };

USTRUCT(BlueprintType)
struct FPcMovementPreset
{
	GENERATED_BODY()

	/** ACTION ECONOMY: How many gameplay beats this jump costs to complete. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Jump")
	float BeatsPerJump = 1.0f;

	/** Exactly how high the jump goes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Jump") 
	float PeakHeightCM = 200.f;

	/** Top speed while on the ground */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Speed") 
	float MaxGroundSpeed = 900.f;
	
	/** Top speed while in the air */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Speed") 
	float MaxAirSpeed = 900.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bhop") 
	float ChargeTime = 0.6f;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBhopChargeUpdated, float, ChargeAlpha);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBhopActivated);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBhopCancelled);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBhopLanded, float, HorizontalSpeed);

UCLASS(Blueprintable, BlueprintType)
class PROJECT_CIRCLE_API UPcQPlayerMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UPcQPlayerMovementComponent();

	UFUNCTION(BlueprintCallable, Category = "Bhop") void OnJumpPressed();
	UFUNCTION(BlueprintCallable, Category = "Bhop") void OnJumpReleased();
	UFUNCTION(BlueprintCallable, Category = "Beat Sync") void TriggerBeatJump();
	UFUNCTION(BlueprintCallable, Category = "Beat Sync") void UpdateBPM(float NewGameplayBPM, float Subdivision, EPcMovementPresetOverride PresetOverride);

	UFUNCTION(BlueprintPure) EBhopState GetBhopState() const { return BhopState; }
	UFUNCTION(BlueprintPure) float GetChargeAlpha() const;
	UFUNCTION(BlueprintPure) float GetHorizontalSpeed() const;
	UFUNCTION(BlueprintPure) bool IsInBhopChain() const;
	UFUNCTION(BlueprintPure) float GetCurrentBPM() const { return CurrentBPM; }
	UFUNCTION(BlueprintPure) bool HasQueuedJump() const { return bJumpQueuedForBeat; }
	UFUNCTION(BlueprintPure) FString GetActivePresetName() const;

	UPROPERTY(BlueprintAssignable) FOnBhopChargeUpdated OnBhopChargeUpdated;
	UPROPERTY(BlueprintAssignable) FOnBhopActivated OnBhopActivated;
	UPROPERTY(BlueprintAssignable) FOnBhopCancelled OnBhopCancelled;
	UPROPERTY(BlueprintAssignable) FOnBhopLanded OnBhopLanded;

	// --- PRESETS (Speed, Height, and Beat Multipliers) ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presets") FPcMovementPreset PresetSlow;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presets") FPcMovementPreset PresetNormal;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presets") FPcMovementPreset PresetFast;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presets") FPcMovementPreset PresetVeryFast;

	// --- GLOBAL SNAPPINESS (Muscle Memory - Never changes per preset!) ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Ground Snappiness") float CustomGroundAcceleration = 30.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Ground Snappiness") float CustomGroundFriction = 25.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Air Snappiness") float CustomAirAcceleration = 15.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Air Snappiness") float CustomAirFriction = 0.0f;

	// Increased to 0.25f for better rhythm forgiveness!
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Queue") float BeatCoyoteWindow = 0.25f;

protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void PhysWalking(float deltaTime, int32 Iterations) override;
	virtual void PhysFalling(float deltaTime, int32 Iterations) override;
	virtual void ProcessLanded(const FHitResult& Hit, float remainingTime, int32 Iterations) override;

private:
	EBhopState BhopState = EBhopState::Idle;
	float ChargeTimer = 0.f;
	float CurrentBPM = 0.f;
	float CurrentSubdivision = 1.0f;
	EPcMovementPresetOverride CurrentPresetOverride = EPcMovementPresetOverride::Auto;

	bool bJumpQueuedForBeat = false;
	float BeatQueueTimer = 0.f;
	
	// DEBUG TRACKER: Memory of speed from the previous frame to catch wipeouts
	float PreviousFrameSpeed = 0.f; 

	const FPcMovementPreset& GetActivePreset() const;
	void ApplyPreset(const FPcMovementPreset& Preset, float BeatIntervalSeconds);
	void ActivateAutoBhop();
	void CancelAutoBhop();
	void ApplyJumpVelocity();
};