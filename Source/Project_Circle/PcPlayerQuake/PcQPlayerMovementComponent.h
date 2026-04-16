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
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float PeakHeightCM = 200.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float MinFallGravityMultiplier = 2.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float MaxGroundSpeed = 900.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float AirControlFraction = 0.1f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float AirDragFraction = 0.04f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float ChargeTime = 0.6f;
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
	UFUNCTION(BlueprintCallable, Category = "Beat Sync") void UpdateBPM(float NewGameplayBPM, int32 Subdivision, EPcMovementPresetOverride PresetOverride);

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presets") FPcMovementPreset PresetSlow;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presets") FPcMovementPreset PresetNormal;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presets") FPcMovementPreset PresetFast;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presets") FPcMovementPreset PresetVeryFast;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Ground") float GroundAccelerate = 10.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Ground") float Friction = 4.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Ground") float StopSpeed = 100.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Queue") float BeatCoyoteWindow = 0.08f;

protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void PhysWalking(float deltaTime, int32 Iterations) override;
	virtual void PhysFalling(float deltaTime, int32 Iterations) override;
	virtual void ProcessLanded(const FHitResult& Hit, float remainingTime, int32 Iterations) override;

private:
	EBhopState BhopState = EBhopState::Idle;
	float ChargeTimer = 0.f;
	float CurrentBPM = 0.f;
	int32 CurrentSubdivision = 1;
	EPcMovementPresetOverride CurrentPresetOverride = EPcMovementPresetOverride::Auto;

	bool bJumpQueuedForBeat = false;
	float BeatQueueTimer = 0.f;
	float ActiveFallGravityMultiplier = 2.0f;

	const FPcMovementPreset& GetActivePreset() const;
	void ApplyPreset(const FPcMovementPreset& Preset, float BeatIntervalSeconds);
	void ActivateAutoBhop();
	void CancelAutoBhop();
	void ApplyJumpVelocity();
	void QuakeFriction(float DeltaTime);
	void QuakeAccelerateGround(const FVector& WishDir, float WishSpeed, float DeltaTime);
	void ClampHorizontalSpeed();
};