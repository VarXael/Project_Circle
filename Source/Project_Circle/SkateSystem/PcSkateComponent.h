#pragma once

#include "CoreMinimal.h"
#include "Components/StaticMeshComponent.h"
#include "PcSkateComponent.generated.h"

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class PROJECT_CIRCLE_API UPcSkateComponent : public UStaticMeshComponent
{
	GENERATED_BODY()

public:
	UPcSkateComponent();

protected:
	virtual void BeginPlay() override;

public:
	// --- CORE CONFIG ---
	UPROPERTY(EditAnywhere, Category = "Skate Config")
	FVector DefaultViewPosition = FVector(50.0f, 0.0f, -35.0f);

	UPROPERTY(EditAnywhere, Category = "Skate Config")
	FRotator BaseOffset = FRotator::ZeroRotator;

	// Lower = Floaty/Heavy. Higher = Snappy.
	UPROPERTY(EditAnywhere, Category = "Skate Config")
	float VisualInterpSpeed = 5.0f;

	// --- ANIMATION LIMITS ---
	
	// How far the board moves sideways when steering (A/D)
	UPROPERTY(EditAnywhere, Category = "Skate Config|Anim")
	float MaxStrafeSway = 15.0f;

	// How much the board Rolls (Banks) when steering
	UPROPERTY(EditAnywhere, Category = "Skate Config|Anim")
	float MaxRollLean = 15.0f; 

	// How much the board Turns (Yaws) ONLY when drifting
	UPROPERTY(EditAnywhere, Category = "Skate Config|Anim")
	float MaxDriftYaw = 30.0f; 
	
	// Braking Tilt (Nose Up - Digging the tail) - ADD THIS
	UPROPERTY(EditAnywhere, Category = "Skate Config|Anim")
	float MaxPitchBrake = 15.0f;
	
	// Nose Up/Down tilt when Accelerating/Braking
	UPROPERTY(EditAnywhere, Category = "Skate Config|Anim")
	float MaxPitchLean = 5.0f; 
	
	UPROPERTY(EditAnywhere, Category = "Skate Config")
	float InputInterpSpeed = 3.0f;

	// How fast inputs shift in the air.
	// Usually slightly higher than ground (e.g. 5.0) to feel like you have "Air Control",
	// but keep it low (3.0) if you want it to feel exactly like water.
	UPROPERTY(EditAnywhere, Category = "Skate Config")
	float AirInputInterpSpeed = 6.0f;
	
	// --- DYNAMIC FEEL SETTINGS ---

	// At this speed, the board reaches its maximum responsiveness (Snappiest).
	UPROPERTY(EditAnywhere, Category = "Skate Config|Feel")
	float ReferenceMaxSpeed = 1500.0f;

	// --- INPUT SMOOTHING (The "Driver") ---
    
	// Smoothing speed when standing still or moving slow (Heavy/Floaty).
	UPROPERTY(EditAnywhere, Category = "Skate Config|Feel")
	float InputInterpMin = 2.0f;

	// Smoothing speed when moving at Max Speed (Responsive).
	UPROPERTY(EditAnywhere, Category = "Skate Config|Feel")
	float InputInterpMax = 8.0f;

	// --- VISUAL SMOOTHING (The "Board") ---

	// Board lag when slow.
	UPROPERTY(EditAnywhere, Category = "Skate Config|Feel")
	float VisualInterpMin = 3.0f;

	// Board lag when fast.
	UPROPERTY(EditAnywhere, Category = "Skate Config|Feel")
	float VisualInterpMax = 15.0f;
	
	// --- JUMP / LANDING ---

	// Extra pitch up when jumping
	UPROPERTY(EditAnywhere, Category = "Skate Config|Jump")
	float JumpPitchAngle = 20.0f; 
	
	// Visual lift height when jumping
	UPROPERTY(EditAnywhere, Category = "Skate Config|Jump")
	float JumpLiftHeight = 15.0f; 

	// How far above the ground to start the "Brace for Impact" animation
	UPROPERTY(EditAnywhere, Category = "Skate Config|Land")
	float LandingProbeDist = 600.0f; 

	// Angle to tilt nose up just before landing
	UPROPERTY(EditAnywhere, Category = "Skate Config|Land")
	float LandingFlareAngle = 35.0f; 

	// How far to push the board down (visually extending legs) before landing
	UPROPERTY(EditAnywhere, Category = "Skate Config|Land")
	float BraceExtendAmount = 25.0f;

	// --- API ---
	// Note: I removed the unused args (CameraPitch) based on your cpp, 
	// but added them back if your player class calls them.
	void UpdateBoardState(float DeltaTime, float CurrentSpeed, FVector VelocityDir, float RawSteerInput, float RawFwdInput, bool bIsDrifting, bool bIsJumping, float DistToFloor, float CameraPitch);

private:
	// Internal state for input smoothing
	float CurrentSteer = 0.0f;
	float CurrentFwd = 0.0f;
	FRotator CurrentRotation = FRotator::ZeroRotator;
	FVector CurrentLocation = FVector::ZeroVector;
};