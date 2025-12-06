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
		
		// Defaults
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
	
	if (bCanComboLand && LandWindowTimer > 0.0f)
	{
		FlowComp->InjectFlow(50.0f); 
		CurrentSpeed += 400.0f; 
		bCanComboLand = false;
		FOVImpulse = BoostFOVImpulse; 
		if (APlayerController* PC = Cast<APlayerController>(GetController()))
			if (APcDebugHUD* HUD = Cast<APcDebugHUD>(PC->GetHUD()))
				HUD->AddStyleMessage("PERFECT LANDING!", EStyleEventType::Good);
	}
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
	if (!bIsJumping) PerformJump(); 
	else InputBufferTimer = 0.2f; 
}

// ==========================================
// JUMP LOGIC (SINE WAVE)
// ==========================================

void APcPlayerCharacter::PerformJump()
{
	if (!GravityComp) return;

	bIsJumping = true;
	JumpPhaseTime = 0.0f;
	bCanComboLand = false;
	
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

	// 1. PHYSICAL LANDING CHECK
	// We rely on the GravityComponent to tell us when we hit the floor.
	bool bIsFallingNow = GravityComp->IsFalling();
	
	// If we were falling/jumping, and now we are not, we landed.
	// We check !bIsJumping to ensure we don't trigger this mid-air if the raycast hits a wall momentarily
	// But mostly we rely on the fact that while jumping, we are high up.
	
	// FIX: We rely purely on physics state transitions to trigger the Land Event.
	if (bWasFalling && !bIsFallingNow)
	{
		// Safety: Don't trigger land instantly on launch (0.1s buffer)
		if (JumpPhaseTime > 0.1f)
		{
			OnLandedHit();
			bIsJumping = false; // Ensure state is synced
		}
	}
	bWasFalling = bIsFallingNow;

	if (bIsJumping)
	{
		JumpPhaseTime += DeltaTime;
		float Alpha = (JumpPhaseTime / WaveDuration); 
		
		if (Alpha >= 1.0f)
		{
			// === TIMER ENDED ===
			// RELEASE THE SNAP.
			// We do NOT force OnLandedHit here.
			// We just stop forcing the height. Gravity takes over.
			// The block above (Physical Landing Check) will fire when we actually touch grass.
			bIsJumping = false;
			
			GravityComp->HoverHeight = 0.0f; 
			GravityComp->bSnapToHoverHeight = false; 
			GravityComp->VerticalSmoothing = 10.0f; // Smooth out the remaining distance
		}
		else
		{
			// === IN AIR ===
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
	bCanComboLand = true;
	LandWindowTimer = LandComboWindow;
	CurrentCameraSink += LandingSinkAmount; 
	FOVImpulse = BoostFOVImpulse;
	
	if (LandingShake) UGameplayStatics::PlayWorldCameraShake(GetWorld(), LandingShake, GetActorLocation(), 0.0f, 500.0f);
	if (InputBufferTimer > 0.0f) { InputBufferTimer = 0.0f; PerformJump(); }
}

// ==========================================
// MAIN PHYSICS LOOP
// ==========================================

void APcPlayerCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!GravityComp || !FlowComp) return;

	if (InputBufferTimer > 0.0f) InputBufferTimer -= DeltaTime;

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

	// --- 1. CALCULATE INPUT ---
	FVector CamFwd = FVector::VectorPlaneProject(CameraComp->GetForwardVector(), SurfaceNormal).GetSafeNormal();
	FVector CamRight = FVector::VectorPlaneProject(CameraComp->GetRightVector(), SurfaceNormal).GetSafeNormal();
	
	// FIX: Y=Forward, X=Right. This aligns with Standard UE5 Enhanced Input
	FVector RawInputDir = (CamFwd * CurrentInput.X) + (CamRight * CurrentInput.Y);

	
	FVector InputDir = FVector::ZeroVector;
	if (RawInputDir.SizeSquared() > 0.01f) InputDir = RawInputDir.GetSafeNormal();

	DebugLastVelocityDir = CurrentVelDir;
	DebugLastInputDir = InputDir;

	bool bEffectiveDrift = false;
	bool bIsPhysicallyFalling = GravityComp->IsFalling();
	bool bIsAirborne = bIsJumping || bIsPhysicallyFalling;
	bIsAirborneDebug = bIsAirborne;

	// --- 2. PHYSICS BRANCHING ---
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
		// DRIFTING (Heavy WASD)
		bEffectiveDrift = ApplyDriftPhysics(DeltaTime, InputDir, CurrentVelDir, bIsAirborne, MaxSpeedForTier);
	}
	else
	{
		// GRIP
		ApplyGripPhysics(DeltaTime, InputDir, CurrentVelDir, bIsAirborne);
		if (DriftSparksComp->IsActive()) DriftSparksComp->Deactivate();
		FlowComp->UpdateFlowLogic(DeltaTime, false);
	}

	// --- 3. APPLY VELOCITY ---
	if (bIsJumping)
	{
		FVector OldVel = GravityComp->GetCurrentVelocity();
		float VertSpeed = FVector::DotProduct(OldVel, SurfaceNormal);
		GravityComp->SetVelocity((CurrentVelDir * CurrentSpeed) + (SurfaceNormal * VertSpeed));
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

// --- MODULAR PHYSICS ---

void APcPlayerCharacter::ApplyGripPhysics(float DeltaTime, FVector InputDir, FVector& CurrentVelDir, bool bIsAirborne)
{
	CurrentVelDir = FMath::VInterpNormalRotationTo(CurrentVelDir, InputDir, DeltaTime, GripSteeringRate);
	DebugSlipAngle = 0.0f;

	if (CurrentSpeed > BaseMoveSpeed)
	{
		float ExcessSpeed = CurrentSpeed - BaseMoveSpeed;
		float DragFactor = 1.0f + (ExcessSpeed / 500.0f); 
		// Low drag for coasting
		float CoastDrag = 20.0f;
		if (bIsAirborne) CoastDrag = 5.0f;
		
		CurrentSpeed -= CoastDrag * DeltaTime;
	}
	else
	{
		float AccelMult = 1.0f;
		if (CurrentSpeed < InertiaThreshold) AccelMult = 0.3f; 
		CurrentSpeed += GroundAcceleration * AccelMult * DeltaTime;
		if (CurrentSpeed > BaseMoveSpeed) CurrentSpeed = BaseMoveSpeed;
	}
}

bool APcPlayerCharacter::ApplyDriftPhysics(float DeltaTime, FVector InputDir, FVector& CurrentVelDir, bool bIsAirborne, float MaxSpeedForTier)
{
	bool bInPocket = false;

	// 1. STEERING (Heavy)
	CurrentVelDir = FMath::VInterpNormalRotationTo(CurrentVelDir, InputDir, DeltaTime, DriftBodyTurnRate);

	// 2. DRAG (Grinding)
	float DragForce = 200.0f; 
	if (!bIsAirborne) CurrentSpeed -= DragForce * DeltaTime;

	// 3. ACCELERATION (Base)
	if (CurrentSpeed < BaseMoveSpeed)
	{
		CurrentSpeed += GroundAcceleration * DeltaTime;
	}
	else
	{
		CurrentSpeed = FMath::FInterpTo(CurrentSpeed, BaseMoveSpeed, DeltaTime, 0.5f);
	}

	// 4. CHECK POCKET
	float Dot = FVector::DotProduct(CurrentVelDir, InputDir.GetSafeNormal());
	float AngleDeg = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(Dot, -1.0f, 1.0f)));
	DebugSlipAngle = AngleDeg;

	if (AngleDeg > 10.0f && AngleDeg < 90.0f)
	{
		bInPocket = true;
		
		// REWARD: Curve Boost
		if (!bIsAirborne && CurrentSpeed < MaxSpeedForTier * 1.3f)
		{
			float CurveQuality = FMath::Clamp((AngleDeg - 10.0f) / 35.0f, 0.0f, 1.0f);
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