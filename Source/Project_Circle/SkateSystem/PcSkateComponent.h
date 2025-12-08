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
	FRotator CurrentRotation = FRotator::ZeroRotator;
	FVector CurrentLocation = FVector::ZeroVector;
};