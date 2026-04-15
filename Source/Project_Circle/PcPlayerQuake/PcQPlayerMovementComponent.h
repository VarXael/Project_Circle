#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "PcQPlayerMovementComponent.generated.h"

UENUM(BlueprintType)
enum class EBhopState : uint8
{
	Idle		UMETA(DisplayName = "Idle"),
	Charging	UMETA(DisplayName = "Charging"),
	Active		UMETA(DisplayName = "Beat-Synced")
};

// -----------------------------------------------------------------------------
// MOVEMENT PRESET
// Each preset is a full movement personality applied at runtime when the
// subdivision changes. Tune these in your Blueprint defaults.
// -----------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct FPcMovementPreset
{
	GENERATED_BODY()

	/**
	 * How high the player reaches at the peak of a beat-synced jump, in cm.
	 * The system derives JumpZVelocity and FallGravityMultiplier automatically
	 * from this value + the current beat interval, so the player always reaches
	 * this height AND lands on the next beat regardless of BPM.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Jump", meta = (ClampMin = "10.0"))
	float PeakHeightCM = 200.f;

	/**
	 * Extra gravity multiplier applied ONLY when falling (Velocity.Z < 0).
	 * This is a minimum — the system may increase it further to guarantee
	 * landing on beat. Set higher for a snappier, more aggressive fall.
	 * 1.0 = symmetric. 2.0 = falls twice as fast as it rises.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Jump", meta = (ClampMin = "1.0", ClampMax = "10.0"))
	float MinFallGravityMultiplier = 2.0f;

	/** Max horizontal walk/run speed on the ground. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ClampMin = "100.0"))
	float MaxGroundSpeed = 900.f;

	/**
	 * How snappily the player redirects in the air when holding a direction.
	 * 0 = no control. 1 = instant.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AirControlFraction = 0.1f;

	/**
	 * How quickly horizontal speed bleeds off when no input is held in the air.
	 * 0 = drifts forever. 0.05 = gentle bleed. 0.2 = stops quickly.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AirDragFraction = 0.04f;

	/**
	 * How long the player must hold space to activate beat-sync mode.
	 * Faster sections can have a shorter charge for snappier activation.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bhop", meta = (ClampMin = "0.05"))
	float ChargeTime = 0.6f;
};

// -----------------------------------------------------------------------------
// DELEGATES
// -----------------------------------------------------------------------------

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBhopChargeUpdated, float, ChargeAlpha);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBhopActivated);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBhopCancelled);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBhopLanded, float, HorizontalSpeed);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPresetChanged, int32, NewSubdivision);

// -----------------------------------------------------------------------------
// COMPONENT
// -----------------------------------------------------------------------------

UCLASS(Blueprintable, BlueprintType)
class PROJECT_CIRCLE_API UPcQPlayerMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UPcQPlayerMovementComponent();

	// -------------------------------------------------------------------------
	// INPUT
	// -------------------------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "Bhop")
	void OnJumpPressed();

	UFUNCTION(BlueprintCallable, Category = "Bhop")
	void OnJumpReleased();

	// -------------------------------------------------------------------------
	// MUSIC SYNC
	// -------------------------------------------------------------------------

	/** Called every gameplay beat. Jumps if grounded, does nothing if airborne. */
	UFUNCTION(BlueprintCallable, Category = "Beat Sync")
	void TriggerBeatJump();

	/**
	 * Called when gameplay BPM changes.
	 * Recalculates jump physics and selects the correct movement preset.
	 * Subdivision is passed so we know which preset to apply.
	 */
	UFUNCTION(BlueprintCallable, Category = "Beat Sync")
	void UpdateBPM(float NewGameplayBPM, int32 Subdivision);

	// -------------------------------------------------------------------------
	// STATE QUERIES
	// -------------------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "Bhop")
	EBhopState GetBhopState() const { return BhopState; }

	UFUNCTION(BlueprintPure, Category = "Bhop")
	float GetChargeAlpha() const;

	UFUNCTION(BlueprintPure, Category = "Bhop")
	float GetHorizontalSpeed() const;

	UFUNCTION(BlueprintPure, Category = "Bhop")
	bool IsInBhopChain() const;

	UFUNCTION(BlueprintPure, Category = "Beat Sync")
	float GetCurrentBPM() const { return CurrentBPM; }

	UFUNCTION(BlueprintPure, Category = "Beat Sync")
	bool HasQueuedJump() const { return bJumpQueuedForBeat; }

	UFUNCTION(BlueprintPure, Category = "Beat Sync")
	int32 GetCurrentSubdivision() const { return CurrentSubdivision; }

	// -------------------------------------------------------------------------
	// EVENTS
	// -------------------------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "Bhop|Events")
	FOnBhopChargeUpdated OnBhopChargeUpdated;

	UPROPERTY(BlueprintAssignable, Category = "Bhop|Events")
	FOnBhopActivated OnBhopActivated;

	UPROPERTY(BlueprintAssignable, Category = "Bhop|Events")
	FOnBhopCancelled OnBhopCancelled;

	/** Fires on every hop. Bind in Blueprint for beat sound. */
	UPROPERTY(BlueprintAssignable, Category = "Bhop|Events")
	FOnBhopLanded OnBhopLanded;

	/** Fires when the movement preset changes (subdivision changed). */
	UPROPERTY(BlueprintAssignable, Category = "Bhop|Events")
	FOnPresetChanged OnPresetChanged;

	// -------------------------------------------------------------------------
	// MOVEMENT PRESETS
	// Automatically selected by subdivision. Override in Blueprint defaults.
	// -------------------------------------------------------------------------

	/** Subdivision 1 — slow sections. Big floaty hops, moderate speed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement Presets",
		meta = (DisplayName = "Slow (Subdivision 1)"))
	FPcMovementPreset PresetSlow;

	/** Subdivision 2 — normal sections. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement Presets",
		meta = (DisplayName = "Normal (Subdivision 2)"))
	FPcMovementPreset PresetNormal;

	/** Subdivision 4 — fast sections. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement Presets",
		meta = (DisplayName = "Fast (Subdivision 4)"))
	FPcMovementPreset PresetFast;

	/** Subdivision 8+ — very fast sections. Tight snappy hops. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement Presets",
		meta = (DisplayName = "Very Fast (Subdivision 8+)"))
	FPcMovementPreset PresetVeryFast;

	// -------------------------------------------------------------------------
	// TUNING — GROUND (shared across all presets)
	// -------------------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Ground", meta = (ClampMin = "1.0"))
	float GroundAccelerate = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Ground", meta = (ClampMin = "0.0"))
	float Friction = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Ground", meta = (ClampMin = "0.0"))
	float StopSpeed = 100.0f;

	// -------------------------------------------------------------------------
	// TUNING — COYOTE QUEUE
	// -------------------------------------------------------------------------

	/**
	 * Small window after a beat fires where a grounded landing will still trigger a jump.
	 * Acts as coyote time — helps the player sync back up after a minor slip.
	 * Keep small (0.05-0.12). Not a babysitter, just a quality of life buffer.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Queue", meta = (ClampMin = "0.0"))
	float BeatCoyoteWindow = 0.08f;

protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void PhysWalking(float deltaTime, int32 Iterations) override;
	virtual void PhysFalling(float deltaTime, int32 Iterations) override;
	virtual void ProcessLanded(const FHitResult& Hit, float remainingTime, int32 Iterations) override;

private:
	// State
	EBhopState BhopState          = EBhopState::Idle;
	float      ChargeTimer        = 0.f;
	float      CurrentBPM         = 0.f;
	int32      CurrentSubdivision = 1;

	// Coyote queue — small window only, not a sync enforcer
	bool  bJumpQueuedForBeat = false;
	float BeatQueueTimer     = 0.f;

	// Active physics values — derived from preset + BPM
	float ActiveFallGravityMultiplier = 2.0f;

	const FPcMovementPreset& GetPresetForSubdivision(int32 Subdivision) const;
	void ApplyPreset(const FPcMovementPreset& Preset, float BeatIntervalSeconds);

	void ActivateAutoBhop();
	void CancelAutoBhop();
	void ApplyJumpVelocity();

	void QuakeFriction(float DeltaTime);
	void QuakeAccelerateGround(const FVector& WishDir, float WishSpeed, float DeltaTime);
	void ClampHorizontalSpeed();
};