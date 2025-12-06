// ==========================================
// FILE: PcPlayerCharacter.cpp
// PATH: E:\GameDev\Unreal Engine Projects\Project_Circle\Source\Project_Circle\PcPlayer\PcPlayerCharacter.cpp
// ==========================================
#include "PcPlayerCharacter.h"
#include "PcDebugHUD.h"
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

// --- INPUTS ---

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
	DriftBufferTimer = 0.2f; // Buffer for Perfect Land
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
void APcPlayerCharacter::TakeHit() { /* Placeholder */ }

void APcPlayerCharacter::Input_JumpTrigger()
{
	// Strict Check: Only jump if grounded or valid
	if (!bIsJumping) PerformJump(); 
	else InputBufferTimer = 0.2f; 
}

// ==========================================
// JUMP LOGIC (Sine Wave + Squash + Probe)
// ==========================================

void APcPlayerCharacter::PerformJump()
{
	if (!GravityComp) return;

	bIsJumping = true;
	JumpPhaseTime = 0.0f;
	bCanComboLand = false;
	
	// Start at full height
	CurrentJumpPeak = JumpPeakHeight;
	
	// Enable Hover Snap Logic
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
	// Combo Window Decay
	if (bCanComboLand)
	{
		LandWindowTimer -= DeltaTime;
		if (LandWindowTimer <= 0.0f) { bCanComboLand = false; FlowComp->SetFrozen(false); }
	}

	// 1. PHYSICAL LANDING CHECK (Fallback for falling off ledges)
	// We still check this in case you walk off a cliff without jumping.
	bool bIsFallingNow = GravityComp->IsFalling();
	if (bWasFalling && !bIsFallingNow && !bIsJumping)
	{
		OnLandedHit();
	}
	bWasFalling = bIsFallingNow;

	if (bIsJumping)
	{
		JumpPhaseTime += DeltaTime;
		float Alpha = (JumpPhaseTime / WaveDuration); 
		
		if (Alpha >= 1.0f)
		{
			// === TIMER ENDED ===
			
			// 1. Check if the floor is actually there (Deterministic Landing)
			FVector TraceStart = GetActorLocation();
			FVector Down = -GravityComp->GetSurfaceNormal();
			
			// Distance: Capsule Half Height + Small Buffer (e.g. 20 units)
			float CheckDist = 100.0f; 
			if (UCapsuleComponent* Cap = GetCapsuleComponent()) CheckDist = Cap->GetScaledCapsuleHalfHeight() + 20.0f;

			FHitResult Hit;
			FCollisionQueryParams Params;
			Params.AddIgnoredActor(this);
			
			bool bHitFloor = GetWorld()->LineTraceSingleByChannel(Hit, TraceStart, TraceStart + (Down * CheckDist), ECC_WorldStatic, Params);

			// 2. Resolve State
			bIsJumping = false;
			GravityComp->bSnapToHoverHeight = false; 
			GravityComp->VerticalSmoothing = 10.0f;
			GravityComp->HoverHeight = 0.0f;

			// 3. Trigger Landing
			if (bHitFloor)
			{
				// We are close enough to the ground math-wise, and physics confirms it.
				// Snap the last inch and trigger the event.
				OnLandedHit();
			}
			else
			{
				// We finished the sine wave, but there is no floor (jumped over a cliff).
				// Do NOT trigger landing. Just let the player fall naturally.
			}
		}
		else
		{
			// === IN AIR (SINE WAVE) ===
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

	// DECISION TREE

	if (InputBufferTimer > 0.0f)
	{
		// BUNNY HOP
		InputBufferTimer = 0.0f;
		if (HUD) HUD->AddStyleMessage("Bunny Hop!", EStyleEventType::Good);
		PerformJump();
		return;
	}

	if (DriftBufferTimer > 0.0f)
	{
		// PERFECT LAND
		CurrentSpeed += PerfectLandSpeedBoost;
		FlowComp->InjectFlow(30.0f);
		CurrentCameraSink += 10.0f; 
		FOVImpulse = BoostFOVImpulse;
		if (HUD) HUD->AddStyleMessage("+ Perfect Land", EStyleEventType::Good);
	}
	else if (bIsDrifting)
	{
		// SOFT LAND
		CurrentSpeed -= SoftLandPenalty;
		CurrentCameraSink += LandingSinkAmount * 0.5f; 
		if (HUD) HUD->AddStyleMessage("~ Soft Land", EStyleEventType::Neutral);
	}
	else
	{
		// CRASH (The Punishment)
		// 1. Dead Stop
		CurrentSpeed = 50.0f; 
		
		// 2. Kill the Flow (Optional: Reset to Tier 0?)
		// FlowComp->ForceReset(); // If you want to be truly mean
		
		// 3. Visuals
		CurrentCameraSink += LandingSinkAmount; 
		if (HUD) HUD->AddStyleMessage("- CRASHED", EStyleEventType::Bad);
	}

	if (FlowComp) FlowComp->SetFrozen(false);
	if (LandingShake) UGameplayStatics::PlayWorldCameraShake(GetWorld(), LandingShake, GetActorLocation(), 0.0f, 500.0f);
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

	UpdateJumpLogic(DeltaTime);
	UpdateSkaterPhysics(DeltaTime);
	UpdateVisuals(DeltaTime);

	CurrentInput = FVector::ZeroVector;
}

void APcPlayerCharacter::UpdateVisuals(float DeltaTime)
{
	float TargetRestingFOV = BaseFOV + (FlowComp->CurrentTier * FOVPerTier);
	CurrentFOVMod = FMath::FInterpTo(CurrentFOVMod, TargetRestingFOV - BaseFOV, DeltaTime, 2.0f);
	FOVImpulse = FMath::FInterpTo(FOVImpulse, 0.0f, DeltaTime, 5.0f);

	if (CameraComp) CameraComp->SetFieldOfView(BaseFOV + CurrentFOVMod + FOVImpulse);
}

void APcPlayerCharacter::UpdateSkaterPhysics(float DeltaTime)
{
	float MaxSpeedForTier = BaseMoveSpeed + (FlowComp->CurrentTier * SpeedPerTier);
	
	FVector CurrentVel = GravityComp->GetCurrentVelocity();
	FVector SurfaceNormal = GravityComp->GetSurfaceNormal();
	FVector CurrentVelDir = CurrentVel.GetSafeNormal();
	if (CurrentVel.SizeSquared() < 1.0f) CurrentVelDir = GetActorForwardVector();

	// 1. INPUT
	FVector CamFwd = FVector::VectorPlaneProject(CameraComp->GetForwardVector(), SurfaceNormal).GetSafeNormal();
	FVector CamRight = FVector::VectorPlaneProject(CameraComp->GetRightVector(), SurfaceNormal).GetSafeNormal();
	
	// FIXED AXES: X=Forward, Y=Right
	FVector RawInputDir = (CamFwd * CurrentInput.X) + (CamRight * CurrentInput.Y);
	
	FVector InputDir = FVector::ZeroVector;
	if (RawInputDir.SizeSquared() > 0.01f) InputDir = RawInputDir.GetSafeNormal();

	DebugLastVelocityDir = CurrentVelDir;
	DebugLastInputDir = InputDir;

	bool bEffectiveDrift = false;
	bool bIsPhysicallyFalling = GravityComp->IsFalling();
	bool bIsAirborne = bIsJumping || bIsPhysicallyFalling;
	bIsAirborneDebug = bIsAirborne;

	// 2. PHYSICS
	if (InputDir.IsZero())
	{
		// STOPPING
		if (bIsAirborne) CurrentSpeed -= 400.0f * DeltaTime; 
		else CurrentSpeed -= BrakingDeceleration * DeltaTime; 
		
		if (CurrentSpeed < 0.0f) CurrentSpeed = 0.0f;
		
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

	// 3. APPLY
	if (bIsJumping)
	{
		// SINE WAVE OVERRIDE (Horizontal only)
		GravityComp->SetVelocity(CurrentVelDir * CurrentSpeed);
	}
	else if (bIsPhysicallyFalling)
	{
		// FALLING (Preserve Vertical)
		FVector CurrentPhysVel = GravityComp->GetCurrentVelocity();
		float VerticalMag = FVector::DotProduct(CurrentPhysVel, SurfaceNormal);
		FVector VerticalVec = SurfaceNormal * VerticalMag;
		GravityComp->SetVelocity((CurrentVelDir * CurrentSpeed) + VerticalVec);
	}
	else
	{
		// GROUND
		GravityComp->SetVelocity(CurrentVelDir * CurrentSpeed);
	}

	FlowComp->UpdateFlowLogic(DeltaTime, bEffectiveDrift);
}

// --- MODULAR PHYSICS ---

// 1. GRIP PHYSICS (Heavier Start)
void APcPlayerCharacter::ApplyGripPhysics(float DeltaTime, FVector InputDir, FVector& CurrentVelDir, bool bIsAirborne)
{
	CurrentVelDir = FMath::VInterpNormalRotationTo(CurrentVelDir, InputDir, DeltaTime, GripSteeringRate);
	DebugSlipAngle = 0.0f;

	if (CurrentSpeed > BaseMoveSpeed)
	{
		// Overspeed Drag
		float ExcessSpeed = CurrentSpeed - BaseMoveSpeed;
		float DragFactor = 1.0f + (ExcessSpeed / 500.0f); 
		float CoastDrag = 20.0f;
		if (bIsAirborne) CoastDrag = 5.0f;
		CurrentSpeed -= CoastDrag * DeltaTime;
	}
	else
	{
		// INERTIA CURVE (The "Heavy Cart")
		float AccelMult = 1.0f;
		
		// TUNING: Harder to start
		// Was 250.0f, now 400.0f (Need more speed to break free)
		// Was 0.3f, now 0.15f (Pushing is harder)
		if (CurrentSpeed < 400.0f) AccelMult = 0.15f; 
		
		CurrentSpeed += GroundAcceleration * AccelMult * DeltaTime;
		if (CurrentSpeed > BaseMoveSpeed) CurrentSpeed = BaseMoveSpeed;
	}
}

bool APcPlayerCharacter::ApplyDriftPhysics(float DeltaTime, FVector InputDir, FVector& CurrentVelDir, bool bIsAirborne, float MaxSpeedForTier)
{
	bool bInPocket = false;

	// Steering
	CurrentVelDir = FMath::VInterpNormalRotationTo(CurrentVelDir, InputDir, DeltaTime, DriftBodyTurnRate);

	// Drag
	float DragForce = 200.0f; 
	if (!bIsAirborne) CurrentSpeed -= DragForce * DeltaTime;

	// Acceleration Base
	if (CurrentSpeed < BaseMoveSpeed)
	{
		CurrentSpeed += GroundAcceleration * DeltaTime;
	}
	else
	{
		CurrentSpeed = FMath::FInterpTo(CurrentSpeed, BaseMoveSpeed, DeltaTime, 0.5f);
	}

	// Pocket Check
	float Dot = FVector::DotProduct(CurrentVelDir, InputDir.GetSafeNormal());
	float AngleDeg = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(Dot, -1.0f, 1.0f)));
	DebugSlipAngle = AngleDeg;

	if (AngleDeg > 10.0f && AngleDeg < 90.0f)
	{
		bInPocket = true;
		
		// REWARD
		if (CurrentSpeed < MaxSpeedForTier * 1.3f)
		{
			float CurveQuality = FMath::Clamp((AngleDeg - 10.0f) / 35.0f, 0.0f, 1.0f);
			
			// FIX: ALLOW AIR ACCELERATION (The Glide)
			// We remove the !bIsAirborne check here.
			// We apply full power. If you want air to be weaker, multiply by 0.5f.
			CurrentSpeed += DriftAcceleration * CurveQuality * DeltaTime;
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

	return bInPocket;
}


float APcPlayerCharacter::GetCurrentSpeed() const { return CurrentSpeed; }