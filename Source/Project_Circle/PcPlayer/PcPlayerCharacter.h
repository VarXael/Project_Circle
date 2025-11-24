#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "PcPlayerCharacter.generated.h"

class UCameraComponent;
class APcPlanet;

// --- STATES ---
UENUM(BlueprintType)
enum class EMoveState : uint8
{
	Cruising,   // Standard WASD Movement (Heavy steering at low speed)
	Sliding,    // High speed, Zero Friction, Forced Snappy steering
	Air         // Airborne / Jumping
};

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
	
	UFUNCTION(BlueprintCallable) void Input_Move(FVector2D Value);
	UFUNCTION(BlueprintCallable) void Input_Look(FVector2D Value);
	UFUNCTION(BlueprintCallable) void Input_Jump();
	
	/** 
	 * The Trigger: Pressing CTRL. 
	 * Initiates the Boost and enters Slide State.
	 */
	UFUNCTION(BlueprintCallable) void Input_Pulse(); 

	/** 
	 * The Sustain: Holding CTRL.
	 * Updates the boolean flag used to maintain the slide.
	 * Bind this to "Triggered" or "Value" of the Input Action.
	 */
	UFUNCTION(BlueprintCallable) void Input_SlideHold(bool bIsHolding);

	UFUNCTION(BlueprintCallable, Category = "Debug")
	FString GetDebugInfo() const;

public:
	// --- COMPONENTS ---
	
	UPROPERTY(VisibleAnywhere, Category = "Components")
	UCameraComponent* CameraComp;

	UPROPERTY(VisibleAnywhere, Category = "Gravity")
	APcPlanet* CurrentPlanet;

	// ==============================================================================
	// CONFIGURATION
	// ==============================================================================

	// --- 1. BASE MOVEMENT ---
	UPROPERTY(EditAnywhere, Category = "Project Circle | 1. Movement")
	float BaseSpeed = 500.0f;

	UPROPERTY(EditAnywhere, Category = "Project Circle | 1. Movement")
	float Acceleration = 800.0f;

	UPROPERTY(EditAnywhere, Category = "Project Circle | 1. Movement")
	float Friction = 1.5f;

	// --- 2. STEERING FEEL (Reverse Inertia) ---
	UPROPERTY(EditAnywhere, Category = "Project Circle | 2. Steering")
	float MinSteeringRate = 0.5f; 

	UPROPERTY(EditAnywhere, Category = "Project Circle | 2. Steering")
	float MaxSteeringRate = 50.0f; 

	// --- 3. PULSE ABILITY (The Trigger) ---
	UPROPERTY(EditAnywhere, Category = "Project Circle | 3. Pulse Ability")
	float PulseCooldown = 0.4f;

	UPROPERTY(EditAnywhere, Category = "Project Circle | 3. Pulse Ability")
	float PulseMultiplierGain = 1.0f; // Per MDD: +1.0 Instant

	UPROPERTY(EditAnywhere, Category = "Project Circle | 3. Pulse Ability")
	float PulseSpeedBoost = 600.0f; 

	// --- 4. SLIDE MECHANIC (The State) ---
	
	/** Phase 1: Duration of 0 friction and passive growth. */
	UPROPERTY(EditAnywhere, Category = "Project Circle | 4. Slide Mechanic")
	float SlideHydroplaneTime = 1.5f;

	/** Phase 2: Duration of the blend back to normal friction. */
	UPROPERTY(EditAnywhere, Category = "Project Circle | 4. Slide Mechanic")
	float SlideFadeTime = 0.5f;

	/** How much the multiplier grows per second while holding slide in Phase 1. */
	UPROPERTY(EditAnywhere, Category = "Project Circle | 4. Slide Mechanic")
	float SlidePassiveGrowth = 0.2f;

	// --- 5. ECONOMY & DECAY ---
	UPROPERTY(EditAnywhere, Category = "Project Circle | 5. Economy")
	float PulseGracePeriod = 1.5f; // Time before decay starts

	UPROPERTY(EditAnywhere, Category = "Project Circle | 5. Economy")
	float PulseDropSpeed = 10.0f; // Step down speed

	UPROPERTY(EditAnywhere, Category = "Project Circle | 5. Economy")
	float DropStepAmount = 1.0f; // Size of the Tier drop

	UPROPERTY(EditAnywhere, Category = "Project Circle | 5. Economy")
	float MaxMultiplier = 3.0f;

	// --- 6. RAIL PHYSICS ---
	UPROPERTY(EditAnywhere, Category = "Project Circle | 6. Rail Physics")
	float SurfaceHardness = 10.0f;
	UPROPERTY(EditAnywhere, Category = "Project Circle | 6. Rail Physics")
	float StrengthDecay = 2.0f;
	
	// --- JUICE ---	
	UPROPERTY(EditAnywhere, Category = "Project Circle | 7. Juice")
	TSubclassOf<class UCameraShakeBase> PulseCameraShake;
	UPROPERTY(EditAnywhere, Category = "Project Circle | 7. Juice")
	float MaxCameraTilt = 10.0f;
	UPROPERTY(EditAnywhere, Category = "Project Circle | 7. Juice")
	float BaseFOV = 90.0f;
	UPROPERTY(EditAnywhere, Category = "Project Circle | 7. Juice")
	float SpeedFOV = 110.0f;

private:
	// Physics State
	FVector Velocity = FVector::ZeroVector;
	FVector CurrentInput = FVector::ZeroVector;
	bool bIsGrounded = false;

	// State Machine
	EMoveState CurrentState = EMoveState::Cruising;
	bool bIsSlideKeyDown = false; // Tracks Input Hold
	float SlideStateTimer = 0.0f; // Tracks how long we've been sliding

	// Economy State
	float CurrentMultiplier = 1.0f;
	float HighestMultiplier = 1.0f; 
	float LastPulseTime = -99.0f;
	float LastActionTime = -99.0f; // Tracks decay delay
	
	float VerticalStrength = 0.0f; 

	void UpdateMovementPhysics(float DeltaTime, FVector SurfaceNormal);
	void UpdateSlideLogic(float DeltaTime, float& OutFriction, float& OutSteeringAlpha);
	void UpdateCruisingLogic(float DeltaTime, float& OutFriction, float& OutSteeringAlpha);
	
	FVector SlideAlongSurface(const FVector& Velocity, const FVector& Normal);
};