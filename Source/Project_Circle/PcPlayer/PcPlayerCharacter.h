#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "PcPlayerCharacter.generated.h"

class UCameraComponent;
class APcWeapon;
class UPcGravityMovementComponent; 

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
	UFUNCTION(BlueprintCallable)
	void Input_Move(FVector2D Value);
	
	UFUNCTION(BlueprintCallable)
	void Input_Look(FVector2D Value);
	
	UFUNCTION(BlueprintCallable)
	void Input_JumpTrigger();

	// --- COMBAT ---
	UFUNCTION(BlueprintCallable)
	void Input_StartAttack();
	UFUNCTION(BlueprintCallable)
	void Input_StopAttack();
	UFUNCTION(BlueprintCallable)
	void Input_FireLaser();
	UFUNCTION(BlueprintCallable)
	void TakeHit();

	// --- DEBUG ---
	UFUNCTION(BlueprintCallable, Category = "Debug")
	FString GetDebugInfo() const;

public:
	// --- COMPONENTS ---
	UPROPERTY(VisibleAnywhere, Category = "Components")
	UCameraComponent* CameraComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UPcGravityMovementComponent* GravityComp;

	UPROPERTY(EditAnywhere, Category = "Combat")
	TSubclassOf<APcWeapon> StartingWeaponClass;
	
	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	APcWeapon* CurrentWeapon;

	// --- MOVEMENT PHYSICS ---
	UPROPERTY(EditAnywhere, Category = "Movement | Speed")
	float BaseMoveSpeed = 600.0f;
	
	UPROPERTY(EditAnywhere, Category = "Movement | Speed")
	float MaxSkimSpeed = 2500.0f;
	
	UPROPERTY(EditAnywhere, Category = "Movement | Speed")
	float SteeringRate = 300.0f; 

	UPROPERTY(EditAnywhere, Category = "Movement | Physics")
	float CarveAcceleration = 1200.0f; 
	
	UPROPERTY(EditAnywhere, Category = "Movement | Physics")
	float PassiveDrag = 400.0f; 

	// --- JUMP SETTINGS ---
	UPROPERTY(EditAnywhere, Category = "Movement | Jump")
	float JumpPeakHeight = 200.0f; 
	
	UPROPERTY(EditAnywhere, Category = "Movement | Jump")
	float JumpBoostSpeed = 800.0f;

	UPROPERTY(EditAnywhere, Category = "Movement | Jump")
	float WaveDuration = 0.6f; 

	// --- RHYTHM SYSTEM ---
	/** How long after landing does the "Perfect" window last? */
	UPROPERTY(EditAnywhere, Category = "Movement | Rhythm")
	float PerfectWindowDuration = 0.2f;

	/** How forgiving is the input? (Allows pressing space slightly before landing) */
	UPROPERTY(EditAnywhere, Category = "Movement | Rhythm")
	float InputBufferAllowance = 0.15f;

	/** Time allowed between jumps to keep the streak (e.g. 2x jump time) */
	UPROPERTY(EditAnywhere, Category = "Movement | Rhythm")
	float MaxComboTime = 1.2f;
	
	bool IsInRhythmWindow() const;

private:
	// --- INTERNAL STATE ---
	FVector CurrentInput = FVector::ZeroVector;
	float CurrentSpeed = 0.0f;
	
	// Jump State
	bool bIsJumping = false;
	bool bInPerfectWindow = false; // True immediately after landing
	
	// Timers
	float JumpPhaseTime = 0.0f;    // Tracks the sine wave
	float WindowTimer = 0.0f;      // Tracks the 0.2s after landing
	float ComboTimer = 0.0f;       // Tracks the "Streak" logic
	float InputBufferTimer = 0.0f; // Tracks pre-press forgiveness

	// Logic Helpers
	void UpdateSkaterPhysics(float DeltaTime);
	void UpdateJumpLogic(float DeltaTime);
	void PerformJump(bool bIsPerfect);
};