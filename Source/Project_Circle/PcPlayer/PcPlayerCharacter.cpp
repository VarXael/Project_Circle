// ==========================================
// FILE: PcPlayerCharacter.cpp
// PATH: E:\GameDev\Unreal Engine Projects\Project_Circle\Source\Project_Circle\PcPlayer\PcPlayerCharacter.cpp
// ==========================================
#include "PcPlayerCharacter.h"
#include "PcDebugHUD.h"
#include "Project_Circle/SkateSystem/PcSkateComponent.h"
#include "Project_Circle/GravitySystem/PcGravityMovementComponent.h"
#include "Project_Circle/Weapon/PcWeapon.h"
#include "FlowSystem/PcFlowMechanicComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Kismet/GameplayStatics.h"

APcPlayerCharacter::APcPlayerCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	SpringArmComp = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArmComp"));
	SpringArmComp->SetupAttachment(GetCapsuleComponent());
	SpringArmComp->SetRelativeLocation(FVector(0, 0, 60.0f)); 
	SpringArmComp->TargetArmLength = 0.0f; 
	SpringArmComp->bDoCollisionTest = false; 
	SpringArmComp->bUsePawnControlRotation = false; 
	SpringArmComp->bInheritPitch = true;
	SpringArmComp->bInheritYaw = true;
	SpringArmComp->bInheritRoll = true; 
	SpringArmComp->bEnableCameraRotationLag = false;

	CameraComp = CreateDefaultSubobject<UCameraComponent>(TEXT("CameraComp"));
	CameraComp->SetupAttachment(SpringArmComp);
	CameraComp->bUsePawnControlRotation = false; 

	// Skate attached to Camera
	SkateComp = CreateDefaultSubobject<UPcSkateComponent>(TEXT("HoverboardComp"));
	SkateComp->SetupAttachment(CameraComp);
	SkateComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	if (GetCharacterMovement())
	{
		GetCharacterMovement()->GravityScale = 0.0f;
		GetCharacterMovement()->DefaultLandMovementMode = MOVE_Flying;
	}

	GravityComp = CreateDefaultSubobject<UPcGravityMovementComponent>(TEXT("GravityComp"));
	FlowComp = CreateDefaultSubobject<UPcFlowMechanicComponent>(TEXT("FlowComp"));

	DriftSparksComp = CreateDefaultSubobject<UNiagaraComponent>(TEXT("DriftSparksComp"));
	DriftSparksComp->SetupAttachment(GetCapsuleComponent());
	DriftSparksComp->SetRelativeLocation(FVector(0, 0, -80.0f)); 
	DriftSparksComp->bAutoActivate = false; 
}

void APcPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();
	CurrentSpeed = BaseMoveSpeed;
	
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (PC->PlayerCameraManager)
		{
			PC->PlayerCameraManager->ViewPitchMin = -179.9f;
			PC->PlayerCameraManager->ViewPitchMax = 179.9f;
			PC->PlayerCameraManager->ViewRollMin = -179.9f;
			PC->PlayerCameraManager->ViewRollMax = 179.9f;
		}
	}

	if (GravityComp)
	{
		GravityComp->MovementMode = EPcMovementMode::Skater;
		GravityComp->bOrientRotationToMovement = false; 
		GravityComp->MaxSpeed = 10000.0f; 
		GravityComp->Acceleration = 10000.0f;
		GravityComp->Deceleration = 0.0f; 
		GravityComp->RotationInterpSpeed = 10.0f;
		
		DriftBodyTurnRate = 140.0f;
	}

	if (StartingWeaponClass)
	{
		FActorSpawnParameters P; P.Owner = this; P.Instigator = this;
		CurrentWeapon = GetWorld()->SpawnActor<APcWeapon>(StartingWeaponClass, GetActorTransform(), P);
		if (CurrentWeapon) CurrentWeapon->AttachToPlayer(this);
	}
}

// ==========================================
// INPUTS
// ==========================================

void APcPlayerCharacter::Input_Move(FVector2D Value) 
{ 
	CurrentInput = FVector(Value.X, Value.Y, 0.0f); 
}

void APcPlayerCharacter::Input_Look(FVector2D Value)
{
	if (Value.IsZero()) return;
	AddActorLocalRotation(FRotator(0.0f, Value.X, 0.0f));
	float NewPitch = CameraPitch + Value.Y;
	NewPitch = FMath::Clamp(NewPitch, -89.0f, 89.0f);
	float PitchDelta = NewPitch - CameraPitch;
	SpringArmComp->AddLocalRotation(FRotator(PitchDelta, 0.0f, 0.0f));
	CameraPitch = NewPitch;
	if (CurrentWeapon) CurrentWeapon->ApplyInputForSway(Value);
}

void APcPlayerCharacter::Input_StartDrift() 
{ 
	bIsDrifting = true; 
	DriftScoreAccumulator = 0.0f;
	DriftBufferTimer = 0.2f; 
}

void APcPlayerCharacter::Input_StopDrift() 
{ 
	bIsDrifting = false; 
	if (DriftScoreAccumulator > 10.0f)
	{
		if (APlayerController* PC = Cast<APlayerController>(GetController()))
			if (APcDebugHUD* HUD = Cast<APcDebugHUD>(PC->GetHUD()))
				HUD->AddStyleMessage(FString::Printf(TEXT("+ Drift %.0f"), DriftScoreAccumulator), EStyleEventType::Neutral);
	}
	DriftScoreAccumulator = 0.0f;
}

void APcPlayerCharacter::Input_StartAttack() { if (CurrentWeapon) CurrentWeapon->StartPrimaryFire(); }
void APcPlayerCharacter::Input_StopAttack() { if (CurrentWeapon) CurrentWeapon->StopPrimaryFire(); }
void APcPlayerCharacter::Input_FireLaser() { if (CurrentWeapon) CurrentWeapon->FireLaserAttack(); }

void APcPlayerCharacter::TakeHit() 
{ 
	APlayerController* PC = Cast<APlayerController>(GetController());
	APcDebugHUD* HUD = PC ? Cast<APcDebugHUD>(PC->GetHUD()) : nullptr;

	CurrentSpeed = 0.0f; 
	if (FlowComp) { FlowComp->CurrentTier = 0; FlowComp->FlowPercent = 0.0f; FlowComp->CurrentState = EFlowState::Stable; }
	DriftStamina = 0.0f; 
	InfiniteStaminaTimer = 0.0f;
	CurrentCameraSink = 85.0f; // Fix: Set direct to prevent stacking
	
	if (HUD) HUD->AddStyleMessage("!!! HIT - WIPEOUT !!!", EStyleEventType::Bad);
}

void APcPlayerCharacter::Input_JumpTrigger()
{
	// Strict Check: Only jump if grounded/not already jumping
	if (!bIsJumping) PerformJump(); 
	else InputBufferTimer = 0.2f; 
}

// ==========================================
// JUMP LOGIC
// ==========================================

void APcPlayerCharacter::PerformJump()
{
	if (!GravityComp) return;

	bIsJumping = true;
	JumpPhaseTime = 0.0f;
	bCanComboLand = false;
	
	CurrentJumpPeak = JumpPeakHeight;
	
	// Snap On
	GravityComp->bSnapToHoverHeight = true; 
	GravityComp->VerticalSmoothing = 0.0f; 

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
		if (APcDebugHUD* HUD = Cast<APcDebugHUD>(PC->GetHUD()))
			HUD->AddStyleMessage("+ Jump", EStyleEventType::Neutral);

	if (FlowComp) FlowComp->SetFrozen(true);
	if (JumpLaunchFX) UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), JumpLaunchFX, GetActorLocation());
}

void APcPlayerCharacter::UpdateJumpLogic(float DeltaTime)
{
	if (bCanComboLand)
	{
		LandWindowTimer -= DeltaTime;
		if (LandWindowTimer <= 0.0f) { bCanComboLand = false; FlowComp->SetFrozen(false); }
	}

	// 1. PHYSICAL LANDING DETECTION
	// Logic: We are airborne if Physics says Falling OR if Logic says Jumping.
	bool bPhysicsSaysFalling = GravityComp->IsFalling();
	bool bIsAirborneNow = bPhysicsSaysFalling || bIsJumping;

	// Landing Check: Transition from Air -> Ground
	// Crucially, we ignore bIsJumping here. We wait for the timer to finish and Physics to confirm landing.
	if (bWasFalling && !bPhysicsSaysFalling && !bIsJumping)
	{
		OnLandedHit();
	}
	bWasFalling = bIsAirborneNow;

	if (bIsJumping)
	{
		JumpPhaseTime += DeltaTime;
		float Alpha = (JumpPhaseTime / WaveDuration); 
		
		if (Alpha >= 1.0f)
		{
			// TIMER ENDED
			// Release Control to Physics. Gravity pulls us down.
			bIsJumping = false;
			GravityComp->HoverHeight = 0.0f; 
			GravityComp->bSnapToHoverHeight = false; 
			GravityComp->VerticalSmoothing = 10.0f; 
		}
		else
		{
			float SineVal = FMath::Sin(Alpha * UE_PI); 
			GravityComp->HoverHeight = SineVal * JumpPeakHeight;
			GravityComp->bSnapToHoverHeight = true; 
			GravityComp->VerticalSmoothing = 0.0f;
		}
	}
	else
	{
		// Visual Recovery
		CurrentCameraSink = FMath::FInterpTo(CurrentCameraSink, 0.0f, DeltaTime, LandingSinkSpeed);
		if (CameraComp)
		{
			FVector NewLoc = CameraComp->GetRelativeLocation();
			NewLoc.Z = -CurrentCameraSink;
			CameraComp->SetRelativeLocation(NewLoc);
		}
	}
}

void APcPlayerCharacter::OnLandedHit()
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	APcDebugHUD* HUD = PC ? Cast<APcDebugHUD>(PC->GetHUD()) : nullptr;

	bCanComboLand = true;
	LandWindowTimer = LandComboWindow;

	if (InputBufferTimer > 0.0f)
	{
		InputBufferTimer = 0.0f;
		if (HUD) HUD->AddStyleMessage("Bunny Hop!", EStyleEventType::Good);
		PerformJump();
		return;
	}

	if (DriftBufferTimer > 0.0f)
	{
		// PERFECT
		CurrentSpeed += PerfectLandSpeedBoost;
		FlowComp->InjectFlow(30.0f);
		CurrentCameraSink = 15.0f; // Set directly
		FOVImpulse = BoostFOVImpulse;
		DriftStamina = MaxDriftStamina;
		InfiniteStaminaTimer = PerfectLandStaminaBuffer;

		if (HUD) HUD->AddStyleMessage("+ Perfect Land", EStyleEventType::Good);
	}
	else if (bIsDrifting)
	{
		// SOFT
		CurrentSpeed = FMath::Max(0.0f, CurrentSpeed - SoftLandPenalty); 
		CurrentCameraSink = 40.0f; 
		if (HUD) HUD->AddStyleMessage("~ Soft Land", EStyleEventType::Neutral);
	}
	else
	{
		// CRASH
		CurrentSpeed = 0.0f; // Dead Stop
		CurrentCameraSink = 85.0f; 
		if (HUD) HUD->AddStyleMessage("- Bad Landing", EStyleEventType::Bad);
	}

	if (FlowComp) FlowComp->SetFrozen(false);
}

// ==========================================
// MAIN PHYSICS LOOP
// ==========================================

void APcPlayerCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!GravityComp || !FlowComp) return;

	if (InputBufferTimer > 0.0f) InputBufferTimer -= DeltaTime;
	if (DriftBufferTimer > 0.0f) DriftBufferTimer -= DeltaTime;
	if (InfiniteStaminaTimer > 0.0f) InfiniteStaminaTimer -= DeltaTime;

	float Regen = bIsJumping ? StaminaRegenAir : StaminaRegenGround;
	if (!bIsDrifting && InfiniteStaminaTimer <= 0.0f)
	{
		// Air Regen handled here. Ground regen handled in ApplyGripPhysics.
		// To avoid double regen on ground, we check Airborne status.
		if (bIsJumping || GravityComp->IsFalling())
		{
			DriftStamina = FMath::Clamp(DriftStamina + (Regen * DeltaTime), 0.0f, MaxDriftStamina);
		}
	}

	UpdateJumpLogic(DeltaTime);
	UpdateSkaterPhysics(DeltaTime);
	UpdateVisuals(DeltaTime);

	// UPDATE BOARD
	if (SkateComp && CameraComp && GravityComp)
	{
		// 1. Get Velocity Direction safely
		FVector VelocityDir = GravityComp->GetCurrentVelocity().GetSafeNormal();
		if (VelocityDir.IsZero()) VelocityDir = GetActorForwardVector();

		// 2. Call the function with all 9 arguments
		SkateComp->UpdateBoardState(
			DeltaTime,
			CurrentSpeed,                    // Arg 2: Speed
			VelocityDir,                     // Arg 3: Direction
			CurrentInput.Y,                  // Arg 4: Steer Input (A/D)
			CurrentInput.X,                  // Arg 5: Fwd Input (W/S)
			bIsDrifting,                     // Arg 6: Drift State
			bIsJumping,                      // Arg 7: Jump State
			VisualDistToFloor,               // Arg 8: Distance trace result
			CameraComp->GetRelativeRotation().Pitch // Arg 9: Camera Pitch
		);
	}

	CurrentInput = FVector::ZeroVector;
}

void APcPlayerCharacter::UpdateVisuals(float DeltaTime)
{
	float TargetRestingFOV = BaseFOV; //+ (FlowComp->CurrentTier * FOVPerTier);
	CurrentFOVMod = FMath::FInterpTo(CurrentFOVMod, TargetRestingFOV - BaseFOV, DeltaTime, 2.0f);
	FOVImpulse = FMath::FInterpTo(FOVImpulse, 0.0f, DeltaTime, 5.0f);

	float TargetRoll = CurrentInput.Y * CameraTiltAmount; 
	if (bIsDrifting) TargetRoll *= 2.0f;
	CurrentCameraRoll = FMath::FInterpTo(CurrentCameraRoll, TargetRoll, DeltaTime, CameraTiltSpeed);

	if (CameraComp)
	{
		CameraComp->SetFieldOfView(BaseFOV + CurrentFOVMod + FOVImpulse);
		
		FVector NewLoc = CameraComp->GetRelativeLocation();
		NewLoc.Z = -CurrentCameraSink; 
		CameraComp->SetRelativeLocation(NewLoc);

		FRotator NewRot = CameraComp->GetRelativeRotation();
		NewRot.Roll = CurrentCameraRoll;
		CameraComp->SetRelativeRotation(NewRot);
	}

	VisualDistToFloor = -1.0f;
	if (bIsJumping || GravityComp->IsFalling())
	{
		FVector Start = GetActorLocation();
		FVector Down = -GravityComp->GetSurfaceNormal();
		FVector End = Start + (Down * 600.0f); 
		FHitResult Hit;
		FCollisionQueryParams Params; Params.AddIgnoredActor(this);
		if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_WorldStatic, Params))
		{
			VisualDistToFloor = Hit.Distance;
		}
	}
}

void APcPlayerCharacter::UpdateSkaterPhysics(float DeltaTime)
{
	float MaxSpeedForTier = BaseMoveSpeed + (FlowComp->CurrentTier * SpeedPerTier);
	
	FVector CurrentVel = GravityComp->GetCurrentVelocity();
	FVector SurfaceNormal = GravityComp->GetSurfaceNormal();
	FVector CurrentVelDir = CurrentVel.GetSafeNormal();
	if (CurrentVel.SizeSquared() < 1.0f) CurrentVelDir = GetActorForwardVector();

	// Inputs
	FVector CamFwd = FVector::VectorPlaneProject(CameraComp->GetForwardVector(), SurfaceNormal).GetSafeNormal();
	FVector CamRight = FVector::VectorPlaneProject(CameraComp->GetRightVector(), SurfaceNormal).GetSafeNormal();
	
	// FIX: X=Forward, Y=Right
	FVector RawInputDir = (CamFwd * CurrentInput.X) + (CamRight * CurrentInput.Y);
	
	FVector InputDir = FVector::ZeroVector;
	if (RawInputDir.SizeSquared() > 0.01f) InputDir = RawInputDir.GetSafeNormal();

	DebugLastVelocityDir = CurrentVelDir;
	DebugLastInputDir = InputDir;

	bool bEffectiveDrift = false;
	bool bIsPhysicallyFalling = GravityComp->IsFalling();
	bool bIsAirborne = bIsJumping || bIsPhysicallyFalling;
	bIsAirborneDebug = bIsAirborne;

	// PHYSICS
	if (InputDir.IsZero())
	{
		if (bIsAirborne) CurrentSpeed -= 400.0f * DeltaTime; 
		else CurrentSpeed -= BrakingDeceleration * DeltaTime; 
		
		CurrentSpeed = FMath::Max(0.0f, CurrentSpeed);
		
		if (DriftSparksComp->IsActive()) DriftSparksComp->Deactivate();
		FlowComp->UpdateFlowLogic(DeltaTime, false);
	}
	else if (bIsDrifting)
	{
		bEffectiveDrift = ApplyDriftPhysics(DeltaTime, InputDir, CurrentVelDir, bIsAirborne, MaxSpeedForTier);
	}
	else
	{
		ApplyGripPhysics(DeltaTime, InputDir, CurrentVelDir, bIsAirborne);
		if (DriftSparksComp->IsActive()) DriftSparksComp->Deactivate();
		FlowComp->UpdateFlowLogic(DeltaTime, false);
	}

	// APPLY
	if (bIsJumping)
	{
		// SINE WAVE OVERRIDE (Horizontal only)
		GravityComp->SetVelocity(CurrentVelDir * CurrentSpeed);
	}
	else if (bIsPhysicallyFalling)
	{
		FVector CurrentPhysVel = GravityComp->GetCurrentVelocity();
		float VerticalMag = FVector::DotProduct(CurrentPhysVel, SurfaceNormal);
		FVector VerticalVec = SurfaceNormal * VerticalMag;
		GravityComp->SetVelocity((CurrentVelDir * CurrentSpeed) + VerticalVec);
	}
	else
	{
		GravityComp->SetVelocity(CurrentVelDir * CurrentSpeed);
	}

	FlowComp->UpdateFlowLogic(DeltaTime, bEffectiveDrift);
}

void APcPlayerCharacter::ApplyGripPhysics(float DeltaTime, FVector InputDir, FVector& CurrentVelDir, bool bIsAirborne)
{
	// 1. REGEN (Ground Only)
	if (!bIsAirborne && InfiniteStaminaTimer <= 0.0f)
	{
		float RegenRate = StaminaRegenGround;
		float Dot = FVector::DotProduct(CurrentVelDir, InputDir.GetSafeNormal());
		if (Dot < 0.98f) RegenRate += StaminaRegenTurnBonus;
		
		DriftStamina = FMath::Clamp(DriftStamina + (RegenRate * DeltaTime), 0.0f, MaxDriftStamina);
	}

	CurrentVelDir = FMath::VInterpNormalRotationTo(CurrentVelDir, InputDir, DeltaTime, GripSteeringRate);
	DebugSlipAngle = 0.0f;

	if (CurrentSpeed > BaseMoveSpeed)
	{
		float ExcessSpeed = CurrentSpeed - BaseMoveSpeed;
		float DragFactor = 1.0f + (ExcessSpeed / 500.0f); 
		float CoastDrag = 20.0f;
		if (bIsAirborne) CoastDrag = 5.0f;
		CurrentSpeed -= CoastDrag * DeltaTime;
	}
	else
	{
		float AccelMult = 1.0f;
		if (CurrentSpeed < InertiaThreshold) AccelMult = 0.15f; 
		CurrentSpeed += GroundAcceleration * AccelMult * DeltaTime;
		if (CurrentSpeed > BaseMoveSpeed) CurrentSpeed = BaseMoveSpeed;
	}
	CurrentSpeed = FMath::Max(0.0f, CurrentSpeed);
}

bool APcPlayerCharacter::ApplyDriftPhysics(float DeltaTime, FVector InputDir, FVector& CurrentVelDir, bool bIsAirborne, float MaxSpeedForTier)
{
	bool bInPocket = false;

	if (DriftStamina <= 0.0f)
	{
		CurrentVelDir = FMath::VInterpNormalRotationTo(CurrentVelDir, InputDir, DeltaTime, GripSteeringRate); 
		CurrentSpeed -= DriftLinearDrag * 2.0f * DeltaTime; 
		CurrentSpeed = FMath::Max(0.0f, CurrentSpeed);
		if (DriftSparksComp->IsActive()) DriftSparksComp->Deactivate();
		return false; 
	}

	CurrentVelDir = FMath::VInterpNormalRotationTo(CurrentVelDir, InputDir, DeltaTime, DriftBodyTurnRate);

	float DragForce = 200.0f; 
	if (!bIsAirborne) CurrentSpeed -= DragForce * DeltaTime;

	if (CurrentSpeed < BaseMoveSpeed)
	{
		CurrentSpeed += GroundAcceleration * DeltaTime;
	}
	else
	{
		CurrentSpeed = FMath::FInterpTo(CurrentSpeed, BaseMoveSpeed, DeltaTime, 0.5f);
	}

	float Dot = FVector::DotProduct(CurrentVelDir, InputDir.GetSafeNormal());
	float AngleDeg = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(Dot, -1.0f, 1.0f)));
	DebugSlipAngle = AngleDeg;

	if (AngleDeg > 10.0f && AngleDeg < 90.0f)
	{
		bInPocket = true;
		
		float CurveQuality = FMath::Clamp((AngleDeg - 10.0f) / 35.0f, 0.0f, 1.0f);
		
		if (InfiniteStaminaTimer <= 0.0f)
		{
			float Drain = StaminaDrainRate * CurveQuality * DeltaTime;
			DriftStamina = FMath::Clamp(DriftStamina - Drain, 0.0f, 100.0f);
		}

		if (CurrentSpeed < MaxSpeedForTier * 1.3f)
		{
			// Air Boost
			float AirBoost = bIsAirborne ? 1.5f : 1.0f; 
			CurrentSpeed += DriftAcceleration * CurveQuality * AirBoost * DeltaTime;
		}

		if (FlowComp->CurrentState == EFlowState::Frozen) FlowComp->SetFrozen(false);
		
		float FlowAmount = 30.0f + (AngleDeg * 0.5f); 
		FlowComp->InjectFlow(FlowAmount * DeltaTime);

		DriftScoreAccumulator += FlowAmount * DeltaTime;

		float PushStrength = (AngleDeg - 5.0f) / 60.0f;
		float TargetSink = DriftCameraSinkAmount * PushStrength;
		CurrentCameraSink = FMath::FInterpTo(CurrentCameraSink, TargetSink, DeltaTime, 5.0f);
		
		if (!DriftSparksComp->IsActive()) DriftSparksComp->Activate();
	}
	else
	{
		if (DriftSparksComp->IsActive()) DriftSparksComp->Deactivate();
	}

	CurrentSpeed = FMath::Max(0.0f, CurrentSpeed);
	return bInPocket;
}

float APcPlayerCharacter::GetCurrentSpeed() const { return CurrentSpeed; }