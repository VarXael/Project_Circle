// ==========================================
// FILE: PcSkateComponent.cpp
// PATH: Source/Project_Circle/PcPlayer/PcSkateComponent.cpp
// ==========================================
#include "PcSkateComponent.h"
#include "Kismet/KismetMathLibrary.h"

UPcSkateComponent::UPcSkateComponent()
{
	PrimaryComponentTick.bCanEverTick = false; 
	bUseAttachParentBound = false;
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetCastShadow(true);

	// Defaults
	DefaultViewPosition = FVector(50.0f, 0.0f, -35.0f);
	SetRelativeLocation(DefaultViewPosition); 
	SetRelativeScale3D(FVector(1.0f, 1.0f, 1.0f));
	
	VisualInterpSpeed = 5.0f; 
}

void UPcSkateComponent::BeginPlay()
{
	Super::BeginPlay();
	CurrentRotation = FRotator::ZeroRotator;
	CurrentLocation = DefaultViewPosition;
}

void UPcSkateComponent::UpdateBoardState(float DeltaTime, float CurrentSpeed, FVector VelocityDir, float RawSteerInput, float RawFwdInput, bool bIsDrifting, bool bIsJumping, float DistToFloor, float CameraPitch, float WobbleIntensity)
{
    // 0. INPUT SMOOTHING
    float TargetInputInterp = bIsJumping ? AirInputInterpSpeed : InputInterpSpeed;

    CurrentSteer = FMath::FInterpTo(CurrentSteer, RawSteerInput, DeltaTime, TargetInputInterp);
    CurrentFwd   = FMath::FInterpTo(CurrentFwd, RawFwdInput, DeltaTime, TargetInputInterp);

    FRotator TargetRot = FRotator::ZeroRotator;
    FVector TargetPos = DefaultViewPosition;

    // 1. LATERAL SWAY (Drag Left)
    float IntensityMult = bIsDrifting ? 1.5f : 1.0f;
    TargetPos.Y = -CurrentSteer * MaxStrafeSway * IntensityMult; 

    // 2. ROLL (Banking)
    TargetRot.Roll = CurrentSteer * MaxRollLean * IntensityMult;

    // 3. YAW
    if (bIsDrifting) TargetRot.Yaw = CurrentSteer * MaxDriftYaw;

    // 4. PITCH
    float InputPitch = -CurrentFwd * MaxPitchLean; // W = Down
    if (RawFwdInput < -0.1f) InputPitch = FMath::Abs(CurrentFwd) * MaxPitchBrake; 

    if (bIsJumping)
    {
        TargetRot.Pitch = JumpPitchAngle + InputPitch;
        TargetPos.Z += JumpLiftHeight;

        // Flare
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
        TargetRot.Pitch = InputPitch;
    }

    // --- 5. WOBBLE EFFECT (NEW) ---
	if (WobbleIntensity > 0.0f)
	{
		float Time = GetWorld()->GetTimeSeconds();
        
		// 1. ROTATION SHAKE (Violent Rattle)
		// Increased frequencies (75/65) and amplitudes (10.0/8.0)
		float NoiseRoll  = FMath::Sin(Time * 75.0f) * 12.0f * WobbleIntensity; 
		float NoisePitch = FMath::Cos(Time * 65.0f) * 8.0f * WobbleIntensity; 
		float NoiseYaw   = FMath::Sin(Time * 55.0f) * 8.0f * WobbleIntensity;   

		TargetRot.Roll  += NoiseRoll;
		TargetRot.Pitch += NoisePitch;
		TargetRot.Yaw   += NoiseYaw;

		// 2. POSITION SHAKE (The "Loose Screws" feel)
		// Shaking the position makes it feel much more physical/unstable.
		// It looks like the mag-lev engine is failing.
		float PosNoiseX = FMath::Sin(Time * 85.0f) * 3.0f * WobbleIntensity;
		float PosNoiseY = FMath::Cos(Time * 80.0f) * 4.0f * WobbleIntensity;
		float PosNoiseZ = FMath::Sin(Time * 70.0f) * 4.0f * WobbleIntensity;

		TargetPos.X += PosNoiseX;
		TargetPos.Y += PosNoiseY;
		TargetPos.Z += PosNoiseZ;
	}

    // --- 6. APPLY ---
    float CurrentSmooth = (bIsJumping) ? 25.0f : VisualInterpSpeed;

    CurrentRotation = FMath::RInterpTo(CurrentRotation, TargetRot, DeltaTime, CurrentSmooth);
    CurrentLocation = FMath::VInterpTo(CurrentLocation, TargetPos, DeltaTime, CurrentSmooth);

    SetRelativeLocationAndRotation(CurrentLocation, BaseOffset + CurrentRotation);
}