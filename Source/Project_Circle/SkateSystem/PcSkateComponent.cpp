// ==========================================
// FILE: PcSkateComponent.cpp
// PATH: E:\GameDev\Unreal Engine Projects\Project_Circle\Source\Project_Circle\PcPlayer\PcSkateComponent.cpp
// ==========================================
#include "PcSkateComponent.h"
#include "Kismet/KismetMathLibrary.h"

UPcSkateComponent::UPcSkateComponent()
{
	PrimaryComponentTick.bCanEverTick = false; 
	bUseAttachParentBound = false;
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetCastShadow(true);

	// OVERRIDE DEFAULTS
	DefaultViewPosition = FVector(50.0f, 0.0f, -35.0f);
	SetRelativeLocation(DefaultViewPosition); 
	SetRelativeScale3D(FVector(1.0f, 1.0f, 1.0f));
	
	// Slower = Floaty
	VisualInterpSpeed = 5.0f; 
}

void UPcSkateComponent::BeginPlay()
{
	Super::BeginPlay();
	CurrentRotation = FRotator::ZeroRotator;
	CurrentLocation = DefaultViewPosition;
}

void UPcSkateComponent::UpdateBoardState(float DeltaTime, float CurrentSpeed, FVector VelocityDir, float RawSteerInput, float RawFwdInput, bool bIsDrifting, bool bIsJumping, float DistToFloor, float CameraPitch)
{
    // =========================================================================
    // 0. INPUT SMOOTHING
    // =========================================================================
    // Use Air speed if jumping, otherwise Ground speed.
    // Both should be low (e.g. 3.0) to get that "Water Flow" feel.
    float TargetInputInterp = bIsJumping ? AirInputInterpSpeed : InputInterpSpeed;

    CurrentSteer = FMath::FInterpTo(CurrentSteer, RawSteerInput, DeltaTime, TargetInputInterp);
    CurrentFwd   = FMath::FInterpTo(CurrentFwd, RawFwdInput, DeltaTime, TargetInputInterp);

    FRotator TargetRot = FRotator::ZeroRotator;
    FVector TargetPos = DefaultViewPosition;

    // --- 1. LATERAL SWAY (Position) ---
    float IntensityMult = bIsDrifting ? 1.5f : 1.0f;
    TargetPos.Y = -CurrentSteer * MaxStrafeSway * IntensityMult; 

    // --- 2. ROLL (Banking) ---
    TargetRot.Roll = CurrentSteer * MaxRollLean * IntensityMult;

    // --- 3. YAW (Carve) ---
    if (bIsDrifting)
    {
        TargetRot.Yaw = CurrentSteer * MaxDriftYaw;
    }

    // --- 4. PITCH (Throttle) ---
    // Calculate the Base Pitch from input (W = Down, S = Up)
    float InputPitch = -CurrentFwd * MaxPitchLean;

    if (RawFwdInput < -0.1f) // Braking override
    {
        // Smoothly blend to brake angle using the smoothed input
        InputPitch = FMath::Abs(CurrentFwd) * MaxPitchBrake; 
    }

    // Apply Pitch based on State
    if (bIsJumping)
    {
        // AIR LOGIC:
        // Combine the "Jump Pose" (Nose Up) with your Input.
        // This allows you to "Dive" (press W) or "Pull Up" (press S) while mid-air.
        TargetRot.Pitch = JumpPitchAngle + InputPitch;
        TargetPos.Z += JumpLiftHeight;

        // Flare Logic (Landing Anticipation)
        if (DistToFloor >= 0.0f && DistToFloor < LandingProbeDist)
        {
            float Proximity = 1.0f - (DistToFloor / LandingProbeDist);
            float FlareAlpha = FMath::Pow(Proximity, 3.0f);

            TargetRot.Pitch = FMath::Lerp(TargetRot.Pitch, LandingFlareAngle, FlareAlpha);
            TargetPos.Z -= BraceExtendAmount * FlareAlpha;
        }
    }
    else
    {
        // GROUND LOGIC:
        TargetRot.Pitch = InputPitch;
    }

    // --- 5. APPLY ---
    // THE FIX: Use VisualInterpSpeed even in air. 
    // This makes the transition from "Ground" to "Jump Pose" slow and fluid,
    // instead of snapping instantly.
    
    CurrentRotation = FMath::RInterpTo(CurrentRotation, TargetRot, DeltaTime, VisualInterpSpeed);
    CurrentLocation = FMath::VInterpTo(CurrentLocation, TargetPos, DeltaTime, VisualInterpSpeed);

    SetRelativeLocationAndRotation(CurrentLocation, BaseOffset + CurrentRotation);
}