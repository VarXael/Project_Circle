#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "PcPlayerCharacter.generated.h"

class UCameraComponent;
class USpringArmComponent;
class APcWeapon;
class UPcGravityMovementComponent; 
class UPcFlowMechanicComponent;
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
	
	UFUNCTION(BlueprintCallable) void Input_StartDrift();
	UFUNCTION(BlueprintCallable) void Input_StopDrift();
	
	UFUNCTION(BlueprintCallable) void Input_StartAttack();
	UFUNCTION(BlueprintCallable) void Input_StopAttack();
	UFUNCTION(BlueprintCallable) void Input_FireLaser(); 
	UFUNCTION(BlueprintCallable) void TakeHit();

	UFUNCTION(BlueprintCallable, Category = "Flow")
	float GetCurrentSpeed() const;

	// --- COMPONENTS ---
	UPROPERTY(VisibleAnywhere, Category = "Components")
	USpringArmComponent* SpringArmComp;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	UCameraComponent* CameraComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UPcGravityMovementComponent* GravityComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UPcFlowMechanicComponent* FlowComp;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	UNiagaraComponent* DriftSparksComp;

	UPROPERTY(EditAnywhere, Category = "Components")
	TSubclassOf<APcWeapon> StartingWeaponClass;
	
	UPROPERTY(BlueprintReadOnly, Category = "Components")
	APcWeapon* CurrentWeapon;

	// --- VISUAL FEEDBACK CONFIG ---
	UPROPERTY(EditAnywhere, Category = "Feedback|FOV")
	float BaseFOV = 90.0f;
	UPROPERTY(EditAnywhere, Category = "Feedback|FOV")
	float FOVPerTier = 10.0f; 
	UPROPERTY(EditAnywhere, Category = "Feedback|FOV")
	float BoostFOVImpulse = 10.0f; 

	UPROPERTY(EditAnywhere, Category = "Feedback|Camera")
	float LandingSinkAmount = 60.0f; 
	UPROPERTY(EditAnywhere, Category = "Feedback|Camera")
	float LandingSinkSpeed = 5.0f; 
	UPROPERTY(EditAnywhere, Category = "Feedback|Camera")
	float DriftCameraSinkAmount = 30.0f; 
	UPROPERTY(EditAnywhere, Category = "Feedback|Camera")
	float CameraSinkSmoothing = 5.0f; 

	UPROPERTY(EditAnywhere, Category = "Feedback|Shake")
	TSubclassOf<UCameraShakeBase> LandingShake;
	
	UPROPERTY(EditAnywhere, Category = "Feedback|VFX")
	UNiagaraSystem* JumpLaunchFX; 

	// --- PHYSICS CONFIG ---
	UPROPERTY(EditAnywhere, Category = "Movement|Base")
	float BaseMoveSpeed = 800.0f;
	UPROPERTY(EditAnywhere, Category = "Movement|Base")
	float SpeedPerTier = 600.0f; 

	// GRIP MODE
	UPROPERTY(EditAnywhere, Category = "Movement|Grip")
	float GripSteeringRate = 300.0f; 
	UPROPERTY(EditAnywhere, Category = "Movement|Grip")
	float GroundAcceleration = 1500.0f;
	UPROPERTY(EditAnywhere, Category = "Movement|Grip")
	float InertiaThreshold = 250.0f; 
	UPROPERTY(EditAnywhere, Category = "Movement|Grip")
	float BrakingDeceleration = 2500.0f; 

	// DRIFT MODE (Blade Physics)
	UPROPERTY(EditAnywhere, Category = "Movement|Drift")
	float DriftBodyTurnRate = 140.0f; // Heavy Steering
	
	UPROPERTY(EditAnywhere, Category = "Movement|Drift")
	float DriftAcceleration = 2000.0f; 
	
	UPROPERTY(EditAnywhere, Category = "Movement|Drift")
	float DriftLinearDrag = 600.0f; 

	// --- JUMP CONFIG (Sine Wave) ---
	UPROPERTY(EditAnywhere, Category = "Jump")
	float JumpPeakHeight = 200.0f; 
	UPROPERTY(EditAnywhere, Category = "Jump")
	float WaveDuration = 0.6f; 
	UPROPERTY(EditAnywhere, Category = "Jump")
	float LandComboWindow = 0.25f; 

	// --- DEBUG ---
	FVector DebugLastVelocityDir;
	FVector DebugLastInputDir;
	float DebugSlipAngle; 
	float DriftScoreAccumulator = 0.0f;
	bool bIsAirborneDebug = false;

private:
	// --- INTERNAL STATE ---
	FVector CurrentInput = FVector::ZeroVector;
	float CurrentSpeed = 0.0f;
	float CameraPitch = 0.0f; 

	// Visuals State
	float CurrentFOVMod = 0.0f; 
	float FOVImpulse = 0.0f;    
	float CurrentCameraSink = 0.0f; 

	// Physics State
	bool bIsDrifting = false;
	
	// Jump Logic
	bool bIsJumping = false;
	float JumpPhaseTime = 0.0f;
	
	bool bWasFalling = false; 
	float LandWindowTimer = 0.0f;      
	bool bCanComboLand = false;

	float InputBufferTimer = 0.0f;

	// Internal Functions
	void UpdateSkaterPhysics(float DeltaTime);
	void ApplyGripPhysics(float DeltaTime, FVector InputDir, FVector& CurrentVelDir, bool bIsAirborne);
	bool ApplyDriftPhysics(float DeltaTime, FVector InputDir, FVector& CurrentVelDir, bool bIsAirborne, float MaxSpeedForTier);

	void UpdateJumpLogic(float DeltaTime);
	void UpdateVisuals(float DeltaTime); 
	
	void PerformJump();
	void OnLandedHit(); 
};