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
    FRotator TargetRot = FRotator::ZeroRotator;
    FVector TargetPos = DefaultViewPosition;

    // --- 1. LATERAL SWAY (Position) ---
    // Drag Logic: Steer Right (D) -> Board lags Left (-Y).
    float IntensityMult = bIsDrifting ? 1.5f : 1.0f;
    TargetPos.Y = -RawSteerInput * MaxStrafeSway * IntensityMult; 

    // --- 2. ROLL (Banking) ---
    // THE FIX: Removed the "if (RawFwdInput < -0.1f)" check.
    // Now, Steer Input (A/D) maps directly to Roll, regardless of Forward/Backward movement.
    // W + A = Bank Left.
    // S + A = Bank Left.
    
    TargetRot.Roll = RawSteerInput * MaxRollLean * IntensityMult;

    // --- 3. YAW (Carve) ---
    if (bIsDrifting)
    {
        TargetRot.Yaw = RawSteerInput * MaxDriftYaw;
    }

    // --- 4. PITCH (Acceleration / Jump) ---
    // W = Nose Down. S = Nose Up.
    TargetRot.Pitch = -RawFwdInput * MaxPitchLean;

    // Braking Override (Extra Nose Up when moving back)
    if (RawFwdInput < -0.1f)
    {
        TargetRot.Pitch = MaxPitchBrake; 
    }

    if (bIsJumping)
    {
        TargetRot.Pitch = JumpPitchAngle;
        TargetPos.Z += JumpLiftHeight;

        // Flare Logic
        if (DistToFloor >= 0.0f && DistToFloor < LandingProbeDist)
        {
            float Proximity = 1.0f - (DistToFloor / LandingProbeDist);
            float FlareAlpha = FMath::Pow(Proximity, 3.0f);

            TargetRot.Pitch = FMath::Lerp(TargetRot.Pitch, LandingFlareAngle, FlareAlpha);
            TargetPos.Z -= BraceExtendAmount * FlareAlpha;
        }
    }

    // --- 5. APPLY ---
    float CurrentSmooth = (bIsJumping) ? 25.0f : VisualInterpSpeed;

    CurrentRotation = FMath::RInterpTo(CurrentRotation, TargetRot, DeltaTime, CurrentSmooth);
    CurrentLocation = FMath::VInterpTo(CurrentLocation, TargetPos, DeltaTime, CurrentSmooth);

    SetRelativeLocationAndRotation(CurrentLocation, BaseOffset + CurrentRotation);
}