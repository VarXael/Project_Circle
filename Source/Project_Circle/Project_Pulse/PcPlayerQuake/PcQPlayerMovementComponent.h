#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Curves/CurveFloat.h"
#include "Project_Circle/Project_Pulse/Core/PcBeatSurfaceComponent.h"
#include "PcQPlayerMovementComponent.generated.h"

// What the player body is physically doing right now.
// No beat dependency at all — this is pure movement state.
UENUM(BlueprintType)
enum class EPlayerMoveState : uint8
{
	Normal        UMETA(DisplayName = "Normal"),
	Sliding       UMETA(DisplayName = "Sliding"),
	GroundPounding UMETA(DisplayName = "Ground Pounding"),
	WallSwim      UMETA(DisplayName = "Wall Swimming"),
};

// Slide entered via ground input (weak) or via landing/ground pound (strong).
UENUM(BlueprintType)
enum class ESlideStrength : uint8
{
	Weak   UMETA(DisplayName = "Weak  (ground input)"),
	Strong UMETA(DisplayName = "Strong (landing / GP)"),
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBhopLanded,      float, HorizontalSpeed);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWallSwimChanged, float, SwimAlpha);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnActiveBeatAction);  // on-beat shoot signal

// Legacy delegates — kept so existing Blueprint bindings don't break.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBhopChargeUpdated, float, ChargeAlpha);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBhopActivated);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBhopCancelled);

UCLASS(Blueprintable, BlueprintType)
class PROJECT_CIRCLE_API UPcQPlayerMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UPcQPlayerMovementComponent();

	// ── Input surface ────────────────────────────────────────────────────────
	UFUNCTION(BlueprintCallable, Category = "Movement") void OnJumpPressed();
	UFUNCTION(BlueprintCallable, Category = "Movement") void OnJumpReleased();
	UFUNCTION(BlueprintCallable, Category = "Movement") void OnGroundPoundPressed();
	UFUNCTION(BlueprintCallable, Category = "Movement") void OnSlidePressed();
	UFUNCTION(BlueprintCallable, Category = "Movement") void OnSlideReleased();

	// ── Queries ──────────────────────────────────────────────────────────────
	UFUNCTION(BlueprintPure) EPlayerMoveState GetMoveState()       const { return MoveState; }
	UFUNCTION(BlueprintPure) ESlideStrength   GetSlideStrength()   const { return CurrentSlideStrength; }
	UFUNCTION(BlueprintPure) float             GetHorizontalSpeed() const;
	UFUNCTION(BlueprintPure) bool              IsInBhopChain()      const;
	UFUNCTION(BlueprintPure) bool              IsSliding()          const { return MoveState == EPlayerMoveState::Sliding; }
	UFUNCTION(BlueprintPure) bool              IsWallSwimming()     const { return MoveState == EPlayerMoveState::WallSwim; }

	// Returns 0-1 flash alpha set when an on-beat action fires.  HUD reads this.
	UFUNCTION(BlueprintPure) float GetOnBeatFlash() const
	{
		return OnBeatFlashDuration > 0.f
		       ? FMath::Clamp(OnBeatFlashTimer / OnBeatFlashDuration, 0.f, 1.f) : 0.f;
	}

	// Legacy no-ops — kept for blueprint compat
	UFUNCTION(BlueprintPure) float GetChargeAlpha() const { return 0.f; }
	UFUNCTION(BlueprintPure) bool  HasQueuedJump()  const { return false; }

	// ── Delegates ────────────────────────────────────────────────────────────
	UPROPERTY(BlueprintAssignable) FOnBhopLanded        OnBhopLanded;
	UPROPERTY(BlueprintAssignable) FOnWallSwimChanged   OnWallSwimChanged;
	UPROPERTY(BlueprintAssignable) FOnActiveBeatAction  OnActiveBeatAction;
	// Legacy
	UPROPERTY(BlueprintAssignable) FOnBhopChargeUpdated OnBhopChargeUpdated;
	UPROPERTY(BlueprintAssignable) FOnBhopActivated     OnBhopActivated;
	UPROPERTY(BlueprintAssignable) FOnBhopCancelled     OnBhopCancelled;

	// ── Jump Curve ───────────────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Jump Curve",
	          meta = (DisplayName = "Jump Shape Curve (optional)"))
	TObjectPtr<UCurveFloat> JumpCurve = nullptr;

	// ── Ground movement ──────────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Ground") float CustomGroundAcceleration = 30.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Ground") float CustomGroundFriction     = 25.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Air")   float CustomAirAcceleration    = 15.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Air")   float CustomAirFriction        =  0.f;

	// ── Jump ─────────────────────────────────────────────────────────────────
	// Fixed peak height for all player-initiated jumps (not beat-synced).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Jump") float JumpPeakHeightCM = 200.f;
	// Fixed air time in seconds for all player-initiated jumps.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Jump") float JumpAirTimeSec   = 0.55f;

	// ── Ground Pound ─────────────────────────────────────────────────────────
	// After jumping, player cannot ground pound for this long (prevents instant slam spam).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Ground Pound") float GPBufferSec      = 0.28f;
	// Downward slam velocity.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Ground Pound") float GPSlamSpeed      = -4000.f;
	// After GP landing: freeze duration if player does NOT immediately slide.
	// Gives the "thud" feel without punishing if slide is pressed.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Ground Pound") float GPImpactFreezeSec = 0.06f;

	// ── Slide ─────────────────────────────────────────────────────────────────
	// Weak slide (ground input): speed preserved but no boost.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Slide") float WeakSlideMaxSpeed    = 900.f;
	// Strong slide (landing / GP): entry speed from impact velocity.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Slide") float StrongSlideMinSpeed  = 1400.f;
	// How quickly slide decelerates (weak slide decays faster).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Slide") float WeakSlideDecay       =  6.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Slide") float StrongSlideDecay     =  1.8f;
	// Minimum speed before slide auto-exits back to Normal.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Slide") float SlideExitSpeed       = 250.f;

	// ── Wall Swim ─────────────────────────────────────────────────────────────
	// Min impact speed to trigger wall entry.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Wall Swim") float WallSwim_EnterMinSpeed       = 200.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Wall Swim") float WallSwim_EntryVelocityRetain = 0.55f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Wall Swim") float WallSwim_Damping             =  2.8f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Wall Swim") float WallSwim_UpDamping           =  1.2f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Wall Swim") float WallSwim_FloorCheckDist      = 150.f;
	// Manual eject (Jump pressed while swimming): flat kick in WASD direction.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Wall Swim") float WallSwim_ManualEjectSpeed    = 900.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Wall Swim") float WallSwim_ManualEjectUpKick   = 400.f;

	// ── Beat feedback ─────────────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Feedback") float OnBeatFlashDuration = 0.35f;

protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;
	virtual void PhysWalking(float deltaTime, int32 Iterations) override;
	virtual void PhysFalling(float deltaTime, int32 Iterations) override;
	virtual void ProcessLanded(const FHitResult& Hit, float remainingTime, int32 Iterations) override;
	virtual void HandleImpact(const FHitResult& Hit, float TimeSlice, const FVector& MoveDelta) override;

private:
	EPlayerMoveState MoveState             = EPlayerMoveState::Normal;
	ESlideStrength   CurrentSlideStrength  = ESlideStrength::Weak;

	// Ground pound buffer
	float GPBufferTimer    = 0.f;    // counts DOWN after jump; GP disabled while > 0
	bool  bGPImpactPending = false;  // true on GP landing; slide converts it, else freeze
	float GPFreezeTimer    = 0.f;    // counts DOWN during freeze

	// Slide input state
	bool bSlideHeld = false;

	// Beat feedback flash
	float OnBeatFlashTimer = 0.f;

	void TriggerOnBeatFlash();

	// Jump arc
	void  ApplyJumpArc(float PeakHeightCM, float AirTimeSec);
	void  ExitCurveJump();

	// Curve jump state
	bool  bUsingJumpCurve     = false;
	float JumpCurveTimer      = 0.f;
	float JumpCurveTotalTime  = 0.f;
	float JumpCurvePeakHeight = 0.f;

	// Slide helpers
	void  EnterSlide(ESlideStrength Strength, float EntrySpeed = 0.f);
	void  ExitSlide();
	float GetCurrentSlideDecay() const;

	// Wall swim
	FName   SwimPrevCollisionProfile = NAME_None;
	FVector SwimEntryNormal          = FVector::ZeroVector;
	TWeakObjectPtr<UPcBeatSurfaceComponent> SwimWallSurface; // the surface we entered

	void  EnterWallSwim(const FHitResult& Hit);
	void  EjectFromWall(bool bBeatEject);  // bBeatEject = launched by surface pulse
	bool  IsAboveGround() const;

	// Floor pulse check — called each tick while grounded
	void  CheckFloorPulse();
};