#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "PcPlayerCharacter.generated.h"

class UCameraComponent;
class APcPlanet;

// --- DATA TYPES ---

UENUM(BlueprintType)
enum class EMoveState : uint8
{
	Cruising,   // Standard WASD Movement
	Sliding,    // Momentum preservation mode
	Air         // Airborne / Jumping
};

USTRUCT(BlueprintType)
struct FMoveConfig
{
	GENERATED_BODY()

	/** The maximum Speed Multiplier allowed in this state. */
	UPROPERTY(EditAnywhere) float MaxMultiplier = 1.0f;

	/** Rate at which the multiplier increases when providing input. */
	UPROPERTY(EditAnywhere) float GrowthRate = 0.5f;

	/** Rate at which the multiplier decreases when above the cap or providing no input. */
	UPROPERTY(EditAnywhere) float DecayRate = 2.0f;

	/** Drag applied to the velocity vector. Higher values stop the character faster. */
	UPROPERTY(EditAnywhere) float Friction = 4.0f;

	/** Base steering responsiveness. Defines how fast the velocity vector rotates towards input. */
	UPROPERTY(EditAnywhere) float SteeringRate = 1.0f;
};

// ---------------------

UCLASS()
class PROJECT_CIRCLE_API APcPlayerCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	APcPlayerCharacter();

protected:
	virtual void Tick(float DeltaTime) override;
	virtual void NotifyActorBeginOverlap(AActor* OtherActor) override;
	virtual void NotifyActorEndOverlap(AActor* OtherActor) override;

public:
	// --- INPUT INTERFACE ---
	
	/** Handles WASD movement input (Directional). */
	UFUNCTION(BlueprintCallable) void Input_Move(FVector2D Value);
	
	/** Handles Mouse Look input. */
	UFUNCTION(BlueprintCallable) void Input_Look(FVector2D Value);
	
	/** Handles Jump input. Break surface tension. */
	UFUNCTION(BlueprintCallable) void Input_Jump();
	
	/** Triggers the Rhythm Pulse (Kick). Adds speed and snaps steering. */
	UFUNCTION(BlueprintCallable) void Input_Pulse(); 

	/** Returns formatted debug string for HUD display. */
	UFUNCTION(BlueprintCallable, Category = "Debug")
	FString GetDebugInfo() const;

public:
	// --- COMPONENTS ---
	
	UPROPERTY(VisibleAnywhere, Category = "Components")
	UCameraComponent* CameraComp;

	/** The planet actor currently affecting this character's gravity. */
	UPROPERTY(VisibleAnywhere, Category = "Gravity")
	APcPlanet* CurrentPlanet;


	// ==============================================================================
	// CONFIGURATION
	// ==============================================================================

	// --- 1. BASE MOVEMENT ---

	/** The base unit speed. Final speed is BaseSpeed * CurrentMultiplier. */
	UPROPERTY(EditAnywhere, Category = "Project Circle | 1. Movement")
	float BaseSpeed = 500.0f;

	/** How fast the character accelerates up to the current target speed. */
	UPROPERTY(EditAnywhere, Category = "Project Circle | 1. Movement")
	float Acceleration = 800.0f;

	/** Global friction applied when cruising. */
	UPROPERTY(EditAnywhere, Category = "Project Circle | 1. Movement")
	float Friction = 1.5f;


	// --- 2. STEERING FEEL (Reverse Inertia) ---

	/** Turn rate when at 1.0x Multiplier (Heavy/Slow). */
	UPROPERTY(EditAnywhere, Category = "Project Circle | 2. Steering")
	float MinSteeringRate = 0.5f; 

	/** Turn rate when at Max Multiplier (Snappy/Responsive). */
	UPROPERTY(EditAnywhere, Category = "Project Circle | 2. Steering")
	float MaxSteeringRate = 50.0f; 


	// --- 3. PULSE ABILITY (Rhythm Kick) ---

	/** Minimum time in seconds between allowed pulses. */
	UPROPERTY(EditAnywhere, Category = "Project Circle | 3. Pulse Ability")
	float PulseCooldown = 0.4f;

	/** How much to increase the multiplier per successful pulse. */
	UPROPERTY(EditAnywhere, Category = "Project Circle | 3. Pulse Ability")
	float PulseMultiplierGain = 0.5f;

	/** Instant velocity added in the look direction on pulse. */
	UPROPERTY(EditAnywhere, Category = "Project Circle | 3. Pulse Ability")
	float PulseSpeedBoost = 600.0f; 

	/** Duration after a pulse where steering remains perfectly snappy regardless of speed. */
	UPROPERTY(EditAnywhere, Category = "Project Circle | 3. Pulse Ability")
	float PulseControlDuration = 0.3f;


	// --- 4. COMBO DECAY (Tiered Drop) ---

	/** Time in seconds the multiplier holds steady before dropping a tier. */
	UPROPERTY(EditAnywhere, Category = "Project Circle | 4. Combo Decay")
	float PulseGracePeriod = 1.0f;

	/** Interpolation speed when dropping between tiers. Higher is snappier. */
	UPROPERTY(EditAnywhere, Category = "Project Circle | 4. Combo Decay")
	float PulseDropSpeed = 5.0f;

	/** How much multiplier value is lost per grace period expiration. */
	UPROPERTY(EditAnywhere, Category = "Project Circle | 4. Combo Decay")
	float DropStepAmount = 1.0f;

	/** Absolute maximum limit for the speed multiplier. */
	UPROPERTY(EditAnywhere, Category = "Project Circle | 4. Combo Decay")
	float MaxMultiplier = 3.0f;


	// --- 5. RAIL PHYSICS (Gravity/Surface) ---

	/** Strength required to break away from the surface. */
	UPROPERTY(EditAnywhere, Category = "Project Circle | 5. Rail Physics")
	float SurfaceHardness = 10.0f;

	/** Rate at which Vertical Strength decays back to 0 (re-locking to the rail). */
	UPROPERTY(EditAnywhere, Category = "Project Circle | 5. Rail Physics")
	float StrengthDecay = 2.0f;
	
	
	// --- JUICE SETTINGS ---	
	
	// The Shake to play when you Pulse
	UPROPERTY(EditAnywhere, Category = "Project Circle | 6. Juice")
	TSubclassOf<class UCameraShakeBase> PulseCameraShake;

	// How much the camera tilts sideways when steering (Degrees)
	UPROPERTY(EditAnywhere, Category = "Project Circle | 6. Juice")
	float MaxCameraTilt = 10.0f;

	// Base FOV (Standing still)
	UPROPERTY(EditAnywhere, Category = "Project Circle | 6. Juice")
	float BaseFOV = 90.0f;

	// Max FOV (At max speed multiplier)
	UPROPERTY(EditAnywhere, Category = "Project Circle | 6. Juice")
	float SpeedFOV = 110.0f;

private:
	// Physics State
	FVector Velocity = FVector::ZeroVector;
	FVector CurrentInput = FVector::ZeroVector;
	bool bIsGrounded = false;

	// Gameplay State
	float CurrentMultiplier = 1.0f;
	float HighestMultiplier = 1.0f; // Tracks the peak of the current combo for decay logic
	float LastPulseTime = -99.0f;
	
	// Rail State (0 = Locked to surface, >SurfaceHardness = Free movement)
	float VerticalStrength = 0.0f; 

	// Helper Functions
	void UpdateMovementPhysics(float DeltaTime, FVector SurfaceNormal);
	FVector SlideAlongSurface(const FVector& Velocity, const FVector& Normal);
};