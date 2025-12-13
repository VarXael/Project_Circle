// ==========================================
// FILE: PcPlayerCharacter.h
// PATH: Source/Project_Circle/PcPlayer/PcPlayerCharacter.h
// ==========================================
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "PcPlayerCharacter.generated.h"

// Forward Declarations
class UCameraComponent;
class USpringArmComponent;
class APcWeapon; 
class UPcGravityMovementComponent; 
class UPcFlowMechanicComponent;
class UPcSkateComponent;
class UNiagaraComponent;
class UNiagaraSystem;
class UCameraShakeBase;

UCLASS()
class PROJECT_CIRCLE_API APcPlayerCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	APcPlayerCharacter();

protected:
	virtual void Tick(float DeltaTime) override;
	virtual void BeginPlay() override;

public:
	// --- INPUTS ---
	UFUNCTION(BlueprintCallable) void Input_Move(FVector2D Value);
	UFUNCTION(BlueprintCallable) void Input_Look(FVector2D Value);
	UFUNCTION(BlueprintCallable) void Input_JumpTrigger();
	
	// Replaces Drift Toggle -> Now triggers Dash + Drift Chain
	UFUNCTION(BlueprintCallable) void Input_StartDrift();
	UFUNCTION(BlueprintCallable) void Input_StopDrift();
	
	UFUNCTION(BlueprintCallable) void Input_StartAttack();
	UFUNCTION(BlueprintCallable) void Input_StopAttack();
	UFUNCTION(BlueprintCallable) void Input_FireLaser(); 
	UFUNCTION(BlueprintCallable) void TakeHit();

	UFUNCTION(BlueprintCallable, Category = "Flow")
	float GetCurrentSpeed() const { return CurrentSpeed; }

	UFUNCTION(BlueprintCallable, Category = "Flow")
	float GetDriftStamina() const { return DriftStamina; }

	// --- COMPONENTS ---
	UPROPERTY(VisibleAnywhere, Category = "Components")
	USpringArmComponent* SpringArmComp;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	UCameraComponent* CameraComp;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	UPcSkateComponent* SkateComp; 

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UPcGravityMovementComponent* GravityComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UPcFlowMechanicComponent* FlowComp;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	UNiagaraComponent* DriftSparksComp;

	// --- WEAPON (ACTOR BASED) ---
	UPROPERTY(EditAnywhere, Category = "Components")
	TSubclassOf<APcWeapon> StartingWeaponClass;
	
	UPROPERTY(BlueprintReadOnly, Category = "Components")
	APcWeapon* CurrentWeapon;

	// --- LANDING LOGIC CONFIG ---
	UPROPERTY(EditAnywhere, Category = "Landing Logic")
	float PreLandBufferTime = 0.2f; 

	UPROPERTY(EditAnywhere, Category = "Landing Logic")
	float PostLandPerfectWindow = 0.2f; 

	UPROPERTY(EditAnywhere, Category = "Landing Logic")
	float BunnyHopWindow = 0.3f;    

	UPROPERTY(EditAnywhere, Category = "Landing Logic")
	float SafeSlideWindow = 0.4f; 

	// --- DAMAGE & PUNISHMENT CONFIG ---
	UPROPERTY(EditAnywhere, Category = "Damage Logic")
	float WobbleDuration = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Damage Logic")
	float WipeoutSpinDuration = 1.0f; 

	UPROPERTY(EditAnywhere, Category = "Damage Logic")
	float WipeoutPushSpeed = 800.0f; 

	UPROPERTY(EditAnywhere, Category = "Damage Logic")
	float InvulnerabilityDuration = 1.5f;

	// --- MOVEMENT: DASH CONFIG (NEW) ---
	UPROPERTY(EditAnywhere, Category = "Movement|Dash")
	float DashImpulseStrength = 2000.0f; 

	UPROPERTY(EditAnywhere, Category = "Movement|Dash")
	float DashCooldown = 1.0f;

	// --- VISUAL FEEDBACK CONFIG ---
	UPROPERTY(EditAnywhere, Category = "Feedback|FOV")
	float BaseFOV = 90.0f;
	UPROPERTY(EditAnywhere, Category = "Feedback|FOV")
	float BoostFOVImpulse = 10.0f; 

	UPROPERTY(EditAnywhere, Category = "Feedback|Camera")
	float LandingSinkAmount = 60.0f; 
	UPROPERTY(EditAnywhere, Category = "Feedback|Camera")
	float LandingSinkSpeed = 5.0f; 
	UPROPERTY(EditAnywhere, Category = "Feedback|Camera")
	float DriftCameraSinkAmount = 30.0f; 

	UPROPERTY(EditAnywhere, Category = "Feedback|Camera")
	float CameraTiltAmount = 2.5f; 
	UPROPERTY(EditAnywhere, Category = "Feedback|Camera")
	float CameraTiltSpeed = 3.0f;
	
	UPROPERTY(EditAnywhere, Category = "Feedback|VFX")
	UNiagaraSystem* JumpLaunchFX; 

	// --- PHYSICS CONFIG ---
	UPROPERTY(EditAnywhere, Category = "Movement|Base")
	float BaseMoveSpeed = 800.0f;
	// Removed SpeedPerTier (Deprecated)

	// GRIP MODE
	UPROPERTY(EditAnywhere, Category = "Movement|Grip")
	float GripSteeringRate = 300.0f; 
	UPROPERTY(EditAnywhere, Category = "Movement|Grip")
	float GroundAcceleration = 1500.0f;
	UPROPERTY(EditAnywhere, Category = "Movement|Grip")
	float InertiaThreshold = 250.0f; 
	UPROPERTY(EditAnywhere, Category = "Movement|Grip")
	float BrakingDeceleration = 2500.0f; 

	// DRIFT MODE
	UPROPERTY(EditAnywhere, Category = "Movement|Drift")
	float DriftBodyTurnRate = 140.0f; 
	UPROPERTY(EditAnywhere, Category = "Movement|Drift")
	float DriftAcceleration = 2000.0f; 
	UPROPERTY(EditAnywhere, Category = "Movement|Drift")
	float DriftLinearDrag = 600.0f; 

	// STAMINA
	UPROPERTY(EditAnywhere, Category = "Movement|Stamina")
	float MaxDriftStamina = 100.0f;
	UPROPERTY(EditAnywhere, Category = "Movement|Stamina")
	float StaminaDrainRate = 80.0f; 
	UPROPERTY(EditAnywhere, Category = "Movement|Stamina")
	float StaminaRegenGround = 5.0f; 
	UPROPERTY(EditAnywhere, Category = "Movement|Stamina")
	float StaminaRegenAir = 15.0f; 
	UPROPERTY(EditAnywhere, Category = "Movement|Stamina")
	float StaminaRegenTurnBonus = 30.0f;

	// --- JUMP CONFIG ---
	UPROPERTY(EditAnywhere, Category = "Jump")
	float JumpPeakHeight = 200.0f; 
	UPROPERTY(EditAnywhere, Category = "Jump")
	float WaveDuration = 0.6f; 
	UPROPERTY(EditAnywhere, Category = "Jump")
	float LandComboWindow = 0.25f; 

	// TECH / REWARDS
	UPROPERTY(EditAnywhere, Category = "Jump|Tech")
	float PerfectLandStaminaBuffer = 0.5f; 
	UPROPERTY(EditAnywhere, Category = "Jump|Tech")
	float PerfectLandSpeedBoost = 400.0f;
	UPROPERTY(EditAnywhere, Category = "Jump|Tech")
	float SoftLandPenalty = 150.0f;

	// --- DEBUG ---
	FVector DebugLastVelocityDir;
	FVector DebugLastInputDir;
	float DebugSlipAngle; 
	bool bIsAirborneDebug = false;

private:
	FVector CurrentInput = FVector::ZeroVector;
	FVector2D LastValidInput = FVector2D::ZeroVector; // New: Cache for Dash direction
	float CurrentSpeed = 0.0f;
	float CameraPitch = 0.0f; 

	// Visuals
	float CurrentFOVMod = 0.0f; 
	float FOVImpulse = 0.0f;    
	float CurrentCameraSink = 0.0f; 
	float CurrentCameraRoll = 0.0f; 
	float VisualDistToFloor = -1.0f; 

	// Physics State
	bool bIsDrifting = false; // Logic handled by Fuse now, but maintained for State
	float DriftStamina = 100.0f; 
	float InfiniteStaminaTimer = 0.0f; 

	// Dash State
	bool bCanDash = true;
	FTimerHandle TimerHandle_DashCooldown;

	// Jump Logic
	bool bIsJumping = false;
	float JumpPhaseTime = 0.0f;
	float CurrentJumpPeak = 0.0f; 
	bool bWasFalling = false; 
	
	// Buffers
	float InputBufferTimer = 0.0f;      
	float DriftBufferTimer = 0.0f;      
	float DriftInputBufferTimer = 0.0f; 

	// LANDING & DAMAGE STATE
	bool bPendingLandingResolution = false;
	float TimeSinceLanded = 0.0f;
	
	bool bIsWobbling = false;
	float WobbleTimer = 0.0f;

	bool bIsWipeout = false;
	float WipeoutTimer = 0.0f;
	float WipeoutMaxDuration = 0.0f;
	
	bool bIsInvulnerable = false;
	float InvulnerabilityTimer = 0.0f;

	// Helpers
	void ResolvePerfectLand();
	void ResolveBunnyHop();
	void ResolveSoftLand();
	void TriggerWobble();
	void TriggerWipeout();

	// Dash Logic
	void PerformDash();
	void ResetDashCooldown();

	// Internal Functions
	void UpdateSkaterPhysics(float DeltaTime);
	void ApplyGripPhysics(float DeltaTime, FVector InputDir, FVector& CurrentVelDir, bool bIsAirborne);
	bool ApplyDriftPhysics(float DeltaTime, FVector InputDir, FVector& CurrentVelDir, bool bIsAirborne);

	void UpdateJumpLogic(float DeltaTime);
	void UpdateVisuals(float DeltaTime); 
	
	void PerformJump();
	void OnLandedHit(); 
};