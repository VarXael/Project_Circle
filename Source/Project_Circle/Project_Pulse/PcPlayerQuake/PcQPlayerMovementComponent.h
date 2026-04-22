#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Curves/CurveFloat.h"
#include "Project_Circle/MusicSystem/MusicImportSystem/PcMusicAnalysisTypes.h"
#include "PcQPlayerMovementComponent.generated.h"

UENUM(BlueprintType)
enum class EBhopState : uint8 { Active, PowerBoost, GroundPounding, WallSwim };

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBhopChargeUpdated, float, ChargeAlpha);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBhopActivated);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBhopCancelled);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBhopLanded, float, HorizontalSpeed);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWallSwimChanged, float, SwimAlpha);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnActiveBeatAction);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnComboEvent, const FString&, Label, FLinearColor, Color);

UCLASS(Blueprintable, BlueprintType)
class PROJECT_CIRCLE_API UPcQPlayerMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UPcQPlayerMovementComponent();

	UFUNCTION(BlueprintCallable, Category = "Movement") void OnJumpPressed();
	UFUNCTION(BlueprintCallable, Category = "Movement") void OnJumpReleased();
	UFUNCTION(BlueprintCallable, Category = "Movement") void OnGroundPoundPressed();
	UFUNCTION(BlueprintCallable, Category = "Movement") void OnGroundPoundReleased();
	UFUNCTION(BlueprintCallable, Category = "Movement") void OnSlidePressed();   // starts impulse window — next jump is boosted
	UFUNCTION(BlueprintCallable, Category = "Movement") void OnSlideReleased();  // clears impulse
	UFUNCTION(BlueprintCallable, Category = "Movement") void DoBriefDash();      // quick velocity burst in WASD direction
	UFUNCTION(BlueprintCallable, Category = "Movement") void ActivateSlide();    // enters PowerBoost slide
	UFUNCTION(BlueprintCallable, Category = "Beat Sync") void TriggerBeatJump();
	UFUNCTION(BlueprintCallable, Category = "Movement") void NotifyGunFired();

	UFUNCTION(BlueprintPure) EBhopState GetBhopState()       const { return BhopState; }
	UFUNCTION(BlueprintPure) float       GetChargeAlpha()     const { return 0.f; }
	UFUNCTION(BlueprintPure) float       GetHorizontalSpeed() const;
	UFUNCTION(BlueprintPure) bool        IsInBhopChain()      const;
	UFUNCTION(BlueprintPure) bool        HasQueuedJump()      const { return bJumpQueuedForBeat; }
	UFUNCTION(BlueprintPure) bool        IsWallSwimming()     const { return BhopState == EBhopState::WallSwim; }
	UFUNCTION(BlueprintPure) bool        IsPowerBoosting()    const { return BhopState == EBhopState::PowerBoost; }
	UFUNCTION(BlueprintPure) bool        IsAutoJumping()      const { return bAutoJumpActive; }

	UFUNCTION(BlueprintPure) float GetBoostCooldownAlpha()      const;
	UFUNCTION(BlueprintPure) float GetBoostActiveAlpha()        const;
	UFUNCTION(BlueprintPure) float GetDoubleJumpCooldownAlpha() const;
	UFUNCTION(BlueprintPure) float GetBeatSnappedDuration(float BaseSec) const;

	UFUNCTION(BlueprintPure) float GetOnBeatFlash() const
	{
		return OnBeatFlashDuration > 0.f ? FMath::Clamp(OnBeatFlashTimer / OnBeatFlashDuration, 0.f, 1.f) : 0.f;
	}
	UFUNCTION(BlueprintPure) float GetCurveJumpProgress() const
	{
		return bUsingJumpCurve ? FMath::Clamp(JumpCurveTimer / JumpCurveTotalTime, 0.f, 1.f) : -1.f;
	}

	UPROPERTY(BlueprintAssignable) FOnBhopChargeUpdated OnBhopChargeUpdated;
	UPROPERTY(BlueprintAssignable) FOnBhopActivated     OnBhopActivated;
	UPROPERTY(BlueprintAssignable) FOnBhopCancelled     OnBhopCancelled;
	UPROPERTY(BlueprintAssignable) FOnBhopLanded        OnBhopLanded;
	UPROPERTY(BlueprintAssignable) FOnWallSwimChanged   OnWallSwimChanged;
	UPROPERTY(BlueprintAssignable) FOnActiveBeatAction  OnActiveBeatAction;
	UPROPERTY(BlueprintAssignable) FOnComboEvent        OnComboEvent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Jump Curve")
	TObjectPtr<UCurveFloat> JumpCurve = nullptr;

	// ── Auto jump ─────────────────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Auto Jump")
	bool bAutoJumpEnabled = false;

	// ── Ground movement ───────────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Ground") float CustomGroundAcceleration = 30.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Ground") float CustomGroundFriction     = 25.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Air")   float CustomAirAcceleration    = 15.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Air")   float CustomAirFriction        =  0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Queue") float BeatCoyoteWindow         =  0.25f;

	// ── Speed management ──────────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Speed") float OverspeedDecayRate = 180.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Speed") float HardSpeedCapMult   =   4.f;

	// ── Jump input buffer ─────────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Jump Buffer") float JumpInputBufferWindow = 0.22f;

	// ── Jump chain bonuses ────────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Jump Chain") float GPJumpHorizBoost     = 400.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Jump Chain") float GPBoostJumpHorizMult = 1.8f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Jump Chain") float GPComboWindowSec     = 0.45f;

	// ── On-beat bonus hop ─────────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Bonus Hop") float BonusHopSpeedBoost = 300.f;
	
	// WIDENED THE RHYTHM WINDOW (Was 120, now 160 for forgiveness)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Bonus Hop") int32 OnBeatWindowMS     = 160;

	// ── Power Boost ───────────────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Power Boost") float BoostSpeedMultiplier = 1.55f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Power Boost") float BoostBaseDurationSec = 2.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Power Boost") float BoostBaseCooldownSec = 3.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Power Boost") float BoostExtendPerShot   = 0.5f;
	// Short window after boost expires where any beat re-activates it for free.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Power Boost") float BoostGraceWindowSec  = 0.35f;

	// ── Double Jump ───────────────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Double Jump") float DoubleJumpBaseCooldownSec = 2.0f;

	// ── Ground Pound ──────────────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Ground Pound") float GroundPoundSlamSpeed = -2800.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Ground Pound") float GroundPoundCancelDelay = 0.18f;

	// ── Slide ─────────────────────────────────────────────────────────────────
	// Impulse window: how long after pressing slide the next jump counts as a boost jump.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Slide") float SlideImpulseDuration = 0.45f;
	// Brief dash speed when slide is quickly tapped on the ground.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Slide") float DashImpulseSpeed     = 1400.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Slide") float SlideEntryBoost      = 400.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Slide") float SlideFriction      =   1.2f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Slide") float SlideSteerStrength =  10.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Slide") float SlideExitMinSpeed  = 250.f;

	// ── Wall Spring ───────────────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Wall") float Wall_EnterMinSpeed    = 200.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Wall") float Wall_CompressionSec   = 0.22f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Wall") float Wall_CompressionDecay =  14.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Wall") float Wall_EjectSpeed       = 1200.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Wall") float Wall_BeatEjectBoost   =  400.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Wall") float Wall_JumpEjectUpKick  =  350.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Wall") float Wall_FloorCheckDist   =  150.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Wall") float Wall_EjectImmunitySec =   0.40f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Wall") float Wall_AutoEjectUpKick   =  450.f;

	// ── Beat feedback ─────────────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Feedback") float OnBeatFlashDuration = 0.35f;

protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void PhysWalking(float deltaTime, int32 Iterations) override;
	virtual void PhysFalling(float deltaTime, int32 Iterations) override;
	virtual void ProcessLanded(const FHitResult& Hit, float remainingTime, int32 Iterations) override;
	virtual void HandleImpact(const FHitResult& Hit, float TimeSlice, const FVector& MoveDelta) override;

private:
	EBhopState BhopState          = EBhopState::Active;
	bool       bJumpQueuedForBeat = false;
	float      BeatQueueTimer     = 0.f;
	float      PreviousFrameSpeed = 0.f;

	// Input buffers
	bool  bJumpInputBuffered   = false;
	float JumpInputBufferTimer = 0.f;
	bool  bGPInputBuffered     = false;
	float GPInputBufferTimer   = 0.f;
	
	bool  bBonusHopRequested   = false;
	bool  bAutoJumpActive      = false;  

	float GPCancelTimer      = 0.f;
	bool  bSlideImpulseActive = false;  // set on slide press; makes next jump boosted
	float SlideImpulseTimer   = 0.f;   // counts down; cleared when 0

	bool  bGPLandedRecently     = false;
	bool  bBoostActiveOnGPLand  = false;  
	float GPComboTimer          = 0.f;

	float BoostTimer    = 0.f;
	float BoostCooldown = 0.f;
	float BoostGraceTimer = 0.f;  // counts down after boost expires

	bool  bDoubleJumpUsed    = false;
	float DoubleJumpCooldown = 0.f;

	float OnBeatFlashTimer = 0.f;
	void  TriggerOnBeatFlash();
	void  PushCombo(const FString& Label, FLinearColor Color);  

	bool  IsOnBeat()   const;
	bool  BoostReady() const { return BoostCooldown <= 0.f && BhopState != EBhopState::PowerBoost; }
	bool  DJumpReady() const { return DoubleJumpCooldown <= 0.f && !bDoubleJumpUsed; }
	
	// Helper to dynamically check if we are close enough to the floor to buffer a jump (prevents eating DJ)
	bool  CanBufferLanding() const;

	void  ActivateBoost();
	void  ExitBoost();
	void  ExecutePlayerJump(bool bFromBoost = false);  

	void ApplyJumpVelocity();
	void ApplyFixedBeatJump();
	void ApplyArcWithAirTime(float PeakHeightCM, float AirTimeSec);
	void ExitCurveJump();

	bool  bUsingJumpCurve     = false;
	float JumpCurveTimer      = 0.f;
	float JumpCurveTotalTime  = 0.f;
	float JumpCurvePeakHeight = 0.f;
	float JumpCurveLaunchZ    = 0.f;

	FName   WallPrevCollisionProfile = NAME_None;
	FVector WallEntryNormal          = FVector::ZeroVector;
	float   WallEntrySpeed           = 0.f;    // speed at wall entry, used for exit speed calc
	float   WallCompressionTimer     = 0.f;
	bool    bWallBeatPending         = false;  
	float   WallEjectImmunityTimer   = 0.f;    

	void  EnterWallSpring(const FHitResult& Hit);
	void  EjectFromWall(bool bBeatBoost, bool bJumpEject);
	bool  IsAboveGround() const;
};