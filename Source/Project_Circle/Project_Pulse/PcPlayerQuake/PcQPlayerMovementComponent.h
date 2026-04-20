#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Curves/CurveFloat.h"
#include "Project_Circle/MusicSystem/MusicImportSystem/PcMusicAnalysisTypes.h"
#include "PcQPlayerMovementComponent.generated.h"

UENUM(BlueprintType)
enum class EBhopState : uint8 { Active, Sliding, GroundPounding, WallSwim };

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBhopChargeUpdated, float, ChargeAlpha);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBhopActivated);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBhopCancelled);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBhopLanded, float, HorizontalSpeed);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWallSwimChanged, float, SwimAlpha);

// Fired when the player successfully executes an ACTIVE on-beat action:
// bonus hop, slide continuation, or on-beat ground pound landing.
// Character subscribes to this to auto-fire the weapon.
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnActiveBeatAction);

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

	UFUNCTION(BlueprintPure) EBhopState GetBhopState()       const { return BhopState; }
	UFUNCTION(BlueprintPure) float       GetChargeAlpha()     const { return 0.f; }
	UFUNCTION(BlueprintPure) float       GetHorizontalSpeed() const;
	UFUNCTION(BlueprintPure) bool        IsInBhopChain()      const;
	UFUNCTION(BlueprintPure) bool        HasQueuedJump()      const { return bJumpQueuedForBeat; }
	UFUNCTION(BlueprintPure) bool        IsWallSwimming()     const { return BhopState == EBhopState::WallSwim; }
	UFUNCTION(BlueprintPure) bool        IsSliding()          const { return BhopState == EBhopState::Sliding; }

	// 0-1 flash alpha — set to 1 when an active beat action lands, fades to 0 over FlashDuration.
	// Read by the HUD to draw the beat-action ring pulse.
	UFUNCTION(BlueprintPure) float GetOnBeatFlash() const
	{
		return OnBeatFlashDuration > 0.f ? FMath::Clamp(OnBeatFlashTimer / OnBeatFlashDuration, 0.f, 1.f) : 0.f;
	}

	UFUNCTION(BlueprintPure, Category = "Jump Curve")
	float GetCurveJumpProgress() const
	{
		return bUsingJumpCurve ? FMath::Clamp(JumpCurveTimer / JumpCurveTotalTime, 0.f, 1.f) : -1.f;
	}

	UPROPERTY(BlueprintAssignable) FOnBhopChargeUpdated  OnBhopChargeUpdated;
	UPROPERTY(BlueprintAssignable) FOnBhopActivated      OnBhopActivated;
	UPROPERTY(BlueprintAssignable) FOnBhopCancelled      OnBhopCancelled;
	UPROPERTY(BlueprintAssignable) FOnBhopLanded         OnBhopLanded;
	UPROPERTY(BlueprintAssignable) FOnWallSwimChanged    OnWallSwimChanged;
	UPROPERTY(BlueprintAssignable) FOnActiveBeatAction   OnActiveBeatAction;

	// ── Jump Curve ───────────────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Jump Curve",
	          meta = (DisplayName = "Jump Shape Curve (optional)"))
	TObjectPtr<UCurveFloat> JumpCurve = nullptr;

	// ── Ground movement ──────────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Ground Snappiness") float CustomGroundAcceleration = 30.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Ground Snappiness") float CustomGroundFriction     = 25.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Air Snappiness")    float CustomAirAcceleration    = 15.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Air Snappiness")    float CustomAirFriction        =  0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Queue")              float BeatCoyoteWindow         =  0.25f;

	// ── Bonus hop ────────────────────────────────────────────────────────────
	// Pressing Jump within OnBeatWindowMS of a beat multiplies horizontal speed
	// on the next auto-bounce.  Fires OnActiveBeatAction + ring flash.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Bonus Hop") float BonusHopMultiplier = 1.45f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Bonus Hop") int32 OnBeatWindowMS     = 120;

	// ── Slide ────────────────────────────────────────────────────────────────
	//
	//  Slide lasts 2 beats:
	//    Phase 2 (first beat)  — full SlideSpeed, immune to auto-bounce
	//    Phase 1 (second beat) — decaying toward SlideSpeed * SlideDecayFactor, still immune
	//    Phase 0               — bounce normally
	//
	//  bSlidePerfect = true (on-beat GP on ground): BOTH phases at full speed.
	//  bSlidePerfect = false (air landing / off-beat GP): phase 2 full, phase 1 decays.
	//
	//  Pressing GP near any beat while sliding resets to Phase 2 + bSlidePerfect = true.
	//
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Slide") float SlideSpeed            = 1800.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Slide") float SlideDecayFactor      =  0.60f;  // second beat speed fraction
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Slide") float SlideDecayInterpSpeed =  3.0f;   // how fast speed decays

	// ── Ground Pound (air only) ───────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Ground Pound") float GroundPoundSlamSpeed = -4000.f;

	// ── Wall Swim ────────────────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Wall Swim") float WallSwim_EnterMinSpeed        = 200.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Wall Swim") float WallSwim_EntryVelocityRetain  = 0.55f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Wall Swim") float WallSwim_Damping              =  2.8f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Wall Swim") float WallSwim_UpDamping            =  1.2f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Wall Swim") float WallSwim_EjectHorizMultiplier =  2.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Wall Swim") float WallSwim_EjectUpKick          = 300.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Wall Swim") float WallSwim_FloorCheckDist       = 150.f;

	// ── Beat action flash ────────────────────────────────────────────────────
	// How long (seconds) the ring pulse lasts after a successful beat action.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Feedback") float OnBeatFlashDuration = 0.35f;

protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;
	virtual void PhysWalking(float deltaTime, int32 Iterations) override;
	virtual void PhysFalling(float deltaTime, int32 Iterations) override;
	virtual void ProcessLanded(const FHitResult& Hit, float remainingTime, int32 Iterations) override;
	virtual void HandleImpact(const FHitResult& Hit, float TimeSlice, const FVector& MoveDelta) override;

private:
	EBhopState BhopState          = EBhopState::Active;
	bool       bJumpQueuedForBeat = false;
	float      BeatQueueTimer     = 0.f;
	float      PreviousFrameSpeed = 0.f;

	bool bBonusHopRequested      = false;
	bool bSlideContinueRequested = false;

	// Slide phase tracking
	int32 SlidePhase    = 0;      // 2=full, 1=decaying, 0=bounce
	bool  bSlidePerfect = false;  // true = both phases at full speed

	// Flash timer counts down, read by HUD via GetOnBeatFlash()
	float OnBeatFlashTimer = 0.f;

	void TriggerOnBeatFlash();   // sets timer + broadcasts OnActiveBeatAction

	void ApplyJumpVelocity();
	void ApplyFixedBeatJump();
	void ApplyArcWithAirTime(float PeakHeightCM, float AirTimeSec);
	void ExitCurveJump();
	bool IsOnBeat() const;

	// ── Curve jump ───────────────────────────────────────────────────────────
	bool  bUsingJumpCurve     = false;
	float JumpCurveTimer      = 0.f;
	float JumpCurveTotalTime  = 0.f;
	float JumpCurvePeakHeight = 0.f;
	float JumpCurveLaunchZ    = 0.f;

	// ── Wall swim ─────────────────────────────────────────────────────────────
	FName   SwimPrevCollisionProfile = NAME_None;
	FVector SwimEntryNormal          = FVector::ZeroVector;

	void EnterWallSwim(const FHitResult& Hit);
	void EjectFromWall();
	bool IsAboveGround() const;
};