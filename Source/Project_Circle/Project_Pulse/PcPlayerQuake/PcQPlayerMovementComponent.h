#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Curves/CurveFloat.h"
#include "Project_Circle/MusicSystem/MusicImportSystem/PcMusicAnalysisTypes.h"
#include "PcQPlayerMovementComponent.generated.h"

UENUM(BlueprintType)
enum class EBhopState : uint8 { Idle, Charging, Active, GroundPounding, WallCushioned };

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
	UFUNCTION(BlueprintCallable, Category = "Bhop") void OnGroundPoundPressed();
	UFUNCTION(BlueprintCallable, Category = "Beat Sync") void TriggerBeatJump();

	UFUNCTION(BlueprintPure) EBhopState  GetBhopState()      const { return BhopState; }
	UFUNCTION(BlueprintPure) float        GetChargeAlpha()    const;
	UFUNCTION(BlueprintPure) float        GetHorizontalSpeed() const;
	UFUNCTION(BlueprintPure) bool         IsInBhopChain()     const;
	UFUNCTION(BlueprintPure) bool         HasQueuedJump()     const { return bJumpQueuedForBeat; }

	UFUNCTION(BlueprintPure, Category = "Jump Curve")
	float GetCurveJumpProgress() const { return bUsingJumpCurve ? FMath::Clamp(JumpCurveTimer / JumpCurveTotalTime, 0.f, 1.f) : -1.f; }

	UPROPERTY(BlueprintAssignable) FOnBhopChargeUpdated OnBhopChargeUpdated;
	UPROPERTY(BlueprintAssignable) FOnBhopActivated     OnBhopActivated;
	UPROPERTY(BlueprintAssignable) FOnBhopCancelled     OnBhopCancelled;
	UPROPERTY(BlueprintAssignable) FOnBhopLanded        OnBhopLanded;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Jump Curve", meta = (DisplayName = "Jump Shape Curve (optional)"))
	TObjectPtr<UCurveFloat> JumpCurve = nullptr;

	// ── Ground movement ──────────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Ground Snappiness") float CustomGroundAcceleration = 30.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Ground Snappiness") float CustomGroundFriction     = 25.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Air Snappiness")    float CustomAirAcceleration    = 15.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Air Snappiness")    float CustomAirFriction        =  0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Queue")              float BeatCoyoteWindow         =  0.25f;

	// ── Ground pound ─────────────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Ground Pound") float GroundPoundSlamSpeed = -4000.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Ground Pound") float GroundPoundDashSpeed =  2500.f;

	// ── Magnetic Wall Cushion ────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Wall Cushion") float WallCushionThickness = 120.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Wall Cushion") float WallCushionStiffness = 14.f; 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Wall Cushion") float WallBounceForce      = 1200.f;

protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void PhysWalking(float deltaTime, int32 Iterations) override;
	virtual void PhysFalling(float deltaTime, int32 Iterations) override;
	virtual void ProcessLanded(const FHitResult& Hit, float remainingTime, int32 Iterations) override;

private:
	EBhopState BhopState          = EBhopState::Idle;
	float      ChargeTimer        = 0.f;
	bool       bJumpQueuedForBeat = false;
	float      BeatQueueTimer     = 0.f;
	float      PreviousFrameSpeed = 0.f;

	// Wall Cushion Data
	FVector    MagneticWallNormal   = FVector::ZeroVector;
	float      MagneticWallDistance = 0.f;

	void ActivateAutoBhop();
	void CancelAutoBhop();
	void ApplyJumpVelocity();

	bool  bUsingJumpCurve      = false;
	float JumpCurveTimer       = 0.f;  
	float JumpCurveTotalTime   = 0.f;  
	float JumpCurvePeakHeight  = 0.f;  
	float JumpCurveLaunchZ     = 0.f;  

	void  ExitCurveJump();             
};