// ==========================================
// FILE: PcSkateComponent.h
// PATH: Source/Project_Circle/SkateSystem/PcSkateComponent.h
// ==========================================
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

	UPROPERTY(EditAnywhere, Category = "Skate Config")
	float VisualInterpSpeed = 5.0f;

	// --- ANIMATION LIMITS ---
	UPROPERTY(EditAnywhere, Category = "Skate Config|Anim")
	float MaxStrafeSway = 15.0f;

	UPROPERTY(EditAnywhere, Category = "Skate Config|Anim")
	float MaxRollLean = 15.0f; 

	UPROPERTY(EditAnywhere, Category = "Skate Config|Anim")
	float MaxDriftYaw = 30.0f; 
	
	UPROPERTY(EditAnywhere, Category = "Skate Config|Anim")
	float MaxPitchBrake = 15.0f;
	
	UPROPERTY(EditAnywhere, Category = "Skate Config|Anim")
	float MaxPitchLean = 5.0f; 
	
	// --- DYNAMIC FEEL ---
	UPROPERTY(EditAnywhere, Category = "Skate Config|Feel")
	float ReferenceMaxSpeed = 1500.0f;

	UPROPERTY(EditAnywhere, Category = "Skate Config|Feel")
	float InputInterpMin = 2.0f;

	UPROPERTY(EditAnywhere, Category = "Skate Config|Feel")
	float InputInterpMax = 8.0f;

	UPROPERTY(EditAnywhere, Category = "Skate Config|Feel")
	float VisualInterpMin = 3.0f;

	UPROPERTY(EditAnywhere, Category = "Skate Config|Feel")
	float VisualInterpMax = 15.0f;
	
	UPROPERTY(EditAnywhere, Category = "Skate Config")
	float InputInterpSpeed = 3.0f;

	UPROPERTY(EditAnywhere, Category = "Skate Config")
	float AirInputInterpSpeed = 6.0f;
	
	// --- JUMP / LANDING ---
	UPROPERTY(EditAnywhere, Category = "Skate Config|Jump")
	float JumpPitchAngle = 20.0f; 
	
	UPROPERTY(EditAnywhere, Category = "Skate Config|Jump")
	float JumpLiftHeight = 15.0f; 

	UPROPERTY(EditAnywhere, Category = "Skate Config|Land")
	float LandingProbeDist = 600.0f; 

	UPROPERTY(EditAnywhere, Category = "Skate Config|Land")
	float LandingFlareAngle = 35.0f; 

	UPROPERTY(EditAnywhere, Category = "Skate Config|Land")
	float BraceExtendAmount = 25.0f;

	// --- API ---
	// ADDED: float WobbleIntensity (0.0 to 1.0)
	void UpdateBoardState(float DeltaTime, float CurrentSpeed, FVector VelocityDir, float RawSteerInput, float RawFwdInput, bool bIsDrifting, bool bIsJumping, float DistToFloor, float CameraPitch, float WobbleIntensity);

private:
	float CurrentSteer = 0.0f;
	float CurrentFwd = 0.0f;
	FRotator CurrentRotation = FRotator::ZeroRotator;
	FVector CurrentLocation = FVector::ZeroVector;
};