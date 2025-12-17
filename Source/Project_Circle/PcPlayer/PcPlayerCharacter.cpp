// ==========================================
// FILE: PcPlayerCharacter.cpp
// PATH: Source/Project_Circle/PcPlayer/PcPlayerCharacter.cpp
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
	
	// --- FPS LOW PROFILE ---
	SpringArmComp->SetRelativeLocation(FVector(0, 0, 20.0f)); 
	SpringArmComp->TargetArmLength = 0.0f; 
	SpringArmComp->bDoCollisionTest = false; 
	SpringArmComp->bUsePawnControlRotation = true; 
	SpringArmComp->bInheritPitch = true;
	SpringArmComp->bInheritYaw = true;
	SpringArmComp->bInheritRoll = true; 
	SpringArmComp->bEnableCameraRotationLag = true; 
	SpringArmComp->CameraRotationLagSpeed = 40.0f; 

	CameraComp = CreateDefaultSubobject<UCameraComponent>(TEXT("CameraComp"));
	CameraComp->SetupAttachment(SpringArmComp);
	CameraComp->bUsePawnControlRotation = false; 

	SkateComp = CreateDefaultSubobject<UPcSkateComponent>(TEXT("HoverboardComp"));
	SkateComp->SetupAttachment(CameraComp);
	SkateComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SkateComp->SetRelativeLocation(FVector(30, 0, -55)); 

	if (GetCharacterMovement())
	{
		GetCharacterMovement()->GravityScale = 0.0f;
		GetCharacterMovement()->DefaultLandMovementMode = MOVE_Flying;
	}

	GravityComp = CreateDefaultSubobject<UPcGravityMovementComponent>(TEXT("GravityComp"));
	
	GravityComp->HoverHeight = 25.0f; 
	GravityComp->SnapDistance = 50.0f; 
	GravityComp->MovementMode = EPcMovementMode::Skater;
	GravityComp->bOrientRotationToMovement = false; 
	GravityComp->MaxSpeed = 10000.0f; 
	GravityComp->RotationInterpSpeed = 10.0f;

	// --- PHYSICS TUNING (SNAPPY) ---
	// These control internal logic, not the component directly
	GroundAcceleration = 30000.0f; // Instant acceleration
	BrakingDeceleration = 15000.0f; // Instant stop
	GripSteeringRate = 0.0f; // Unused now on ground (Instant snap)

	FlowComp = CreateDefaultSubobject<UPcFlowMechanicComponent>(TEXT("FlowComp"));

	DriftSparksComp = CreateDefaultSubobject<UNiagaraComponent>(TEXT("DriftSparksComp"));
	DriftSparksComp->SetupAttachment(GetCapsuleComponent());
	DriftSparksComp->SetRelativeLocation(FVector(0, 0, -45.0f)); 
	DriftSparksComp->bAutoActivate = false; 

	BaseFOV = 100.0f; 
	CameraTiltAmount = 4.0f; 
	LandingSinkAmount = 15.0f; 
}

void APcPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();
	CurrentSpeed = BaseMoveSpeed;
	
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (PC->PlayerCameraManager)
		{
			PC->PlayerCameraManager->ViewPitchMin = -89.9f;
			PC->PlayerCameraManager->ViewPitchMax = 89.9f;
		}
	}
	
	if (GravityComp)
	{
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
	if (bIsWipeout) { CurrentInput = FVector::ZeroVector; return; }
	CurrentInput = FVector(Value.X, Value.Y, 0.0f);
	
	if (!Value.IsZero())
	{
		LastValidInput = Value;
	}
}

void APcPlayerCharacter::Input_Look(FVector2D Value)
{
	if (Value.IsZero()) return;
	AddControllerYawInput(Value.X);
	AddControllerPitchInput(-Value.Y); // Inverted Y fixed
	if (CurrentWeapon) CurrentWeapon->ApplyInputForSway(Value);
}

void APcPlayerCharacter::Input_StartDrift() 
{ 
	if (bIsWipeout) return;

	if (bIsJumping || (GravityComp && GravityComp->IsFalling()))
	{
		DriftInputBufferTimer = PreLandBufferTime; 
	}
	
	if (bPendingLandingResolution)
	{
		if (TimeSinceLanded <= PostLandPerfectWindow) ResolvePerfectLand();
		else ResolveSoftLand();
		bPendingLandingResolution = false; 
	}

	if (bIsWobbling) return;

	PerformDash();
}

void APcPlayerCharacter::Input_StopDrift() 
{ 
	bIsDrifting = false;
}

void APcPlayerCharacter::Input_StartAttack() { if (CurrentWeapon) CurrentWeapon->StartPrimaryFire(); }
void APcPlayerCharacter::Input_StopAttack() { if (CurrentWeapon) CurrentWeapon->StopPrimaryFire(); }
void APcPlayerCharacter::Input_FireLaser() { if (CurrentWeapon) CurrentWeapon->FireLaserAttack(); }

void APcPlayerCharacter::Input_JumpTrigger()
{
	if (bIsWipeout) return;

	if (bPendingLandingResolution && TimeSinceLanded < BunnyHopWindow)
	{
		ResolveBunnyHop();
		return; 
	}

	if (!bIsJumping) PerformJump(); 
	else InputBufferTimer = 0.2f; 
}

// ==========================================
// GAMEPLAY LOGIC
// ==========================================

void APcPlayerCharacter::TakeHit() 
{ 
	if (bIsWipeout || bIsInvulnerable) return;

	if (FlowComp) 
	{ 
		FlowComp->ResetMultiplier();

		float ChargePenalty = 1.0f;
		if (FlowComp->CurrentCharge <= 0.01f)
		{
			TriggerWipeout();
		}
		else
		{
			FlowComp->AddCharge(-ChargePenalty);
			TriggerWobble();
			bIsInvulnerable = true;
			InvulnerabilityTimer = InvulnerabilityDuration;

			if (APlayerController* PC = Cast<APlayerController>(GetController()))
				if (APcDebugHUD* HUD = Cast<APcDebugHUD>(PC->GetHUD()))
					HUD->AddStyleMessage("HIT! (-1 Charge)", EStyleEventType::Bad);
		}
	}
}

void APcPlayerCharacter::TriggerWipeout()
{
	bIsWipeout = true;
	WipeoutTimer = WipeoutSpinDuration;
	WipeoutMaxDuration = WipeoutSpinDuration;
	bIsDrifting = false;
	
	FVector PushDir = GravityComp->GetCurrentVelocity().GetSafeNormal();
	if (PushDir.IsZero()) PushDir = GetActorForwardVector();
	
	CurrentSpeed = WipeoutPushSpeed;
	GravityComp->SetVelocity(PushDir * WipeoutPushSpeed);

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
		if (APcDebugHUD* HUD = Cast<APcDebugHUD>(PC->GetHUD()))
			HUD->AddStyleMessage("!!! WIPEOUT !!!", EStyleEventType::Bad);
}

void APcPlayerCharacter::TriggerWobble()
{
	bIsWobbling = true;
	WobbleTimer = WobbleDuration;
}

void APcPlayerCharacter::ResolvePerfectLand()
{
	bPendingLandingResolution = false;
	if (FlowComp) 
	{
		FlowComp->AddCharge(2.0f);
		FlowComp->IncreaseMultiplier(1.0f);
		FlowComp->TriggerHitStop(GetWorld());
	}
	CurrentSpeed += PerfectLandSpeedBoost;
	FOVImpulse = BoostFOVImpulse;
	DriftStamina = MaxDriftStamina;
	InfiniteStaminaTimer = PerfectLandStaminaBuffer;

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
		if (APcDebugHUD* HUD = Cast<APcDebugHUD>(PC->GetHUD()))
			HUD->AddStyleMessage("PERFECT LAND", EStyleEventType::Good);
}

void APcPlayerCharacter::ResolveBunnyHop()
{
	bPendingLandingResolution = false;
	if (!CurrentInput.IsZero() && GravityComp)
	{
		FVector Vel = GravityComp->GetCurrentVelocity();
		float Speed = Vel.Size();
		
		FRotator ControlRot = GetControlRotation();
		FVector Fwd = FRotationMatrix(ControlRot).GetScaledAxis(EAxis::X);
		FVector Rgt = FRotationMatrix(ControlRot).GetScaledAxis(EAxis::Y);
		
		FVector SurfaceNormal = GravityComp->GetSurfaceNormal();
		Fwd = FVector::VectorPlaneProject(Fwd, SurfaceNormal).GetSafeNormal();
		Rgt = FVector::VectorPlaneProject(Rgt, SurfaceNormal).GetSafeNormal();
		
		FVector NewDir = (Fwd * CurrentInput.X) + (Rgt * CurrentInput.Y);
		if (!NewDir.IsZero())
		{
			NewDir.Normalize();
			GravityComp->SetVelocity(NewDir * Speed);
		}
	}
	PerformJump(); 
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
		if (APcDebugHUD* HUD = Cast<APcDebugHUD>(PC->GetHUD()))
			HUD->AddStyleMessage("Bunny Hop", EStyleEventType::Neutral);
}

void APcPlayerCharacter::ResolveSoftLand()
{
	bPendingLandingResolution = false;
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
		if (APcDebugHUD* HUD = Cast<APcDebugHUD>(PC->GetHUD()))
			HUD->AddStyleMessage("Soft Land", EStyleEventType::Neutral);
}

void APcPlayerCharacter::PerformJump()
{
	if (!GravityComp) return;
	bIsJumping = true;
	JumpPhaseTime = 0.0f;
	CurrentJumpPeak = JumpPeakHeight;
	GravityComp->bSnapToHoverHeight = true; 
	GravityComp->VerticalSmoothing = 0.0f; 
	if (JumpLaunchFX) UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), JumpLaunchFX, GetActorLocation());
}

void APcPlayerCharacter::UpdateJumpLogic(float DeltaTime)
{
	bool bPhysicsSaysFalling = GravityComp->IsFalling();
	bool bIsAirborneNow = bPhysicsSaysFalling || bIsJumping;

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
			bIsJumping = false;
			GravityComp->HoverHeight = 25.0f; 
			GravityComp->bSnapToHoverHeight = false; 
			GravityComp->VerticalSmoothing = 10.0f; 
		}
		else
		{
			float SineVal = FMath::Sin(Alpha * UE_PI); 
			GravityComp->HoverHeight = 25.0f + (SineVal * JumpPeakHeight);
			GravityComp->bSnapToHoverHeight = true; 
			GravityComp->VerticalSmoothing = 0.0f;
		}
	}
	else
	{
		CurrentCameraSink = FMath::FInterpTo(CurrentCameraSink, 0.0f, DeltaTime, LandingSinkSpeed);
	}
}

void APcPlayerCharacter::OnLandedHit()
{
	if (DriftInputBufferTimer > 0.0f) { ResolvePerfectLand(); DriftInputBufferTimer = 0.0f; InputBufferTimer = 0.0f; return; }
	if (InputBufferTimer > 0.0f) { ResolveBunnyHop(); InputBufferTimer = 0.0f; return; }
	bPendingLandingResolution = true;
	TimeSinceLanded = 0.0f;
}

// ==========================================
// MAIN LOOP
// ==========================================

void APcPlayerCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!GravityComp || !FlowComp) return;

	if (bIsInvulnerable)
	{
		InvulnerabilityTimer -= DeltaTime;
		if (InvulnerabilityTimer <= 0.0f) bIsInvulnerable = false;
	}

	if (bIsWipeout)
	{
		WipeoutTimer -= DeltaTime;
		float Alpha = 1.0f - (WipeoutTimer / WipeoutMaxDuration);
		float SpinAngle = FMath::InterpEaseOut(0.0f, 360.0f, Alpha, 2.0f);
		if (SkateComp) SkateComp->SetRelativeRotation(FRotator(0, SpinAngle, 0)); 
		CurrentSpeed = FMath::FInterpTo(CurrentSpeed, 0.0f, DeltaTime, 1.0f);
		GravityComp->SetVelocity(GravityComp->GetCurrentVelocity().GetSafeNormal() * CurrentSpeed);
		if (WipeoutTimer <= 0.0f) { bIsWipeout = false; CurrentSpeed = 0.0f; if (SkateComp) SkateComp->SetRelativeRotation(FRotator::ZeroRotator); }
		CurrentInput = FVector::ZeroVector; 
		return;
	}

	if (bPendingLandingResolution)
	{
		TimeSinceLanded += DeltaTime;
		if (TimeSinceLanded > SafeSlideWindow) bPendingLandingResolution = false;
	}

	if (DriftInputBufferTimer > 0.0f) DriftInputBufferTimer -= DeltaTime;
	if (InputBufferTimer > 0.0f) InputBufferTimer -= DeltaTime;
	if (bIsWobbling) { WobbleTimer -= DeltaTime; if (WobbleTimer <= 0.0f) bIsWobbling = false; }

	if (DriftBufferTimer > 0.0f) DriftBufferTimer -= DeltaTime;
	if (InfiniteStaminaTimer > 0.0f) InfiniteStaminaTimer -= DeltaTime;
	if (!bIsDrifting) DriftStamina = MaxDriftStamina;

	UpdateJumpLogic(DeltaTime);
	UpdateSkaterPhysics(DeltaTime);
	UpdateVisuals(DeltaTime);

	if (SkateComp && CameraComp && GravityComp)
	{
		FVector VelocityDir = GravityComp->GetCurrentVelocity().GetSafeNormal();
		if (VelocityDir.IsZero()) VelocityDir = GetActorForwardVector();
		
		SkateComp->UpdateBoardState(DeltaTime, CurrentSpeed, VelocityDir, CurrentInput.Y, CurrentInput.X, bIsDrifting, bIsJumping, VisualDistToFloor, CameraComp->GetRelativeRotation().Pitch, bIsWobbling ? 1.0f : 0.0f);
	}

	CurrentInput = FVector::ZeroVector;
}

void APcPlayerCharacter::UpdateVisuals(float DeltaTime)
{
	float TargetRestingFOV = BaseFOV; 
	CurrentFOVMod = FMath::FInterpTo(CurrentFOVMod, TargetRestingFOV - BaseFOV, DeltaTime, 2.0f);
	FOVImpulse = FMath::FInterpTo(FOVImpulse, 0.0f, DeltaTime, 5.0f);

	if (CameraComp)
	{
		CameraComp->SetFieldOfView(BaseFOV + CurrentFOVMod + FOVImpulse);
		
		FVector NewLoc = FVector(0,0,0); 
		NewLoc.Z = -CurrentCameraSink; 
		CameraComp->SetRelativeLocation(NewLoc);

		float TargetRoll = CurrentInput.Y * CameraTiltAmount; 
		if (bIsDrifting) TargetRoll *= 2.0f;
		CurrentCameraRoll = FMath::FInterpTo(CurrentCameraRoll, TargetRoll, DeltaTime, CameraTiltSpeed);

		FRotator NewRot = CameraComp->GetRelativeRotation();
		NewRot.Roll = CurrentCameraRoll;
		CameraComp->SetRelativeRotation(NewRot);
	}

	VisualDistToFloor = -1.0f;
	if (GravityComp)
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
	FRotator ViewRot = GetControlRotation();
	FVector SurfaceNormal = GravityComp->GetSurfaceNormal();
	
	FVector ViewFwd = FRotationMatrix(ViewRot).GetScaledAxis(EAxis::X);
	FVector ViewRgt = FRotationMatrix(ViewRot).GetScaledAxis(EAxis::Y);
	ViewFwd = FVector::VectorPlaneProject(ViewFwd, SurfaceNormal).GetSafeNormal();
	ViewRgt = FVector::VectorPlaneProject(ViewRgt, SurfaceNormal).GetSafeNormal();

	FVector RawInputDir = (ViewFwd * CurrentInput.X) + (ViewRgt * CurrentInput.Y);
	FVector InputDir = FVector::ZeroVector;
	if (RawInputDir.SizeSquared() > 0.01f) InputDir = RawInputDir.GetSafeNormal();

	FVector CurrentVel = GravityComp->GetCurrentVelocity();
	FVector CurrentVelDir = CurrentVel.GetSafeNormal();
	if (CurrentVel.SizeSquared() < 1.0f) CurrentVelDir = GetActorForwardVector();

	DebugLastVelocityDir = CurrentVelDir;
	DebugLastInputDir = InputDir;

	bool bIsPhysicallyFalling = GravityComp->IsFalling();
	bool bIsAirborne = bIsJumping || bIsPhysicallyFalling;

	if (InputDir.IsZero())
	{
		if (bIsAirborne) CurrentSpeed -= 400.0f * DeltaTime; 
		else CurrentSpeed -= BrakingDeceleration * DeltaTime; 
		CurrentSpeed = FMath::Max(0.0f, CurrentSpeed);
		if (DriftSparksComp->IsActive()) DriftSparksComp->Deactivate();
	}
	else if (bIsDrifting)
	{
		ApplyDriftPhysics(DeltaTime, InputDir, CurrentVelDir, bIsAirborne);
	}
	else
	{
		ApplyGripPhysics(DeltaTime, InputDir, CurrentVelDir, bIsAirborne);
		if (DriftSparksComp->IsActive()) DriftSparksComp->Deactivate();
	}

	if (bIsJumping)
	{
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
}

void APcPlayerCharacter::ApplyGripPhysics(float DeltaTime, FVector InputDir, FVector& CurrentVelDir, bool bIsAirborne)
{
	if (!bIsAirborne && InfiniteStaminaTimer <= 0.0f)
	{
		float RegenRate = StaminaRegenGround;
		if (!InputDir.IsZero())
		{
			float Dot = FVector::DotProduct(CurrentVelDir, InputDir.GetSafeNormal());
			if (Dot < 0.98f) RegenRate += StaminaRegenTurnBonus;
		}
		DriftStamina = FMath::Clamp(DriftStamina + (RegenRate * DeltaTime), 0.0f, MaxDriftStamina);
	}

	float ActiveBaseSpeed = FlowComp && FlowComp->IsOverdrive() ? 1200.0f : 800.0f;
	bool bIsOverspeed = CurrentSpeed > (ActiveBaseSpeed + 10.0f);

	if (bIsOverspeed)
	{
		float DragFactor = bIsAirborne ? 0.5f : 5.0f; 
		CurrentSpeed = FMath::FInterpTo(CurrentSpeed, ActiveBaseSpeed, DeltaTime, DragFactor);

		if (!InputDir.IsZero())
		{
			if (bIsAirborne)
			{
				float AirTurn = 500.0f; // Snappier air turn
				if (bIsWobbling) AirTurn *= 0.2f;
				CurrentVelDir = FMath::VInterpNormalRotationTo(CurrentVelDir, InputDir, DeltaTime, AirTurn);
			}
			else
			{
				CurrentVelDir = InputDir;
			}
		}
	}
	else
	{
		if (InputDir.IsZero())
		{
			// Instant stop is handled in UpdateSkaterPhysics via BrakingDeceleration
			// But for safety in FInterp logic:
			float StopSpeed = bIsAirborne ? 1.0f : 25.0f; 
			CurrentSpeed = FMath::FInterpTo(CurrentSpeed, 0.0f, DeltaTime, StopSpeed);
		}
		else
		{
			if (bIsAirborne)
			{
				float AirTurn = 500.0f; 
				CurrentVelDir = FMath::VInterpNormalRotationTo(CurrentVelDir, InputDir, DeltaTime, AirTurn);
			}
			else
			{
				// === INSTANT TURN (GRIP) ===
				// No interpolation. You go where you press.
				CurrentVelDir = InputDir;
			}
			
			// === INSTANT ACCEL (GRIP) ===
			float AccelSpeed = bIsAirborne ? 2.0f : 25.0f; // High interpolation speed
			CurrentSpeed = FMath::FInterpTo(CurrentSpeed, ActiveBaseSpeed, DeltaTime, AccelSpeed);
		}
	}

	CurrentSpeed = FMath::Max(0.0f, CurrentSpeed);
	DebugSlipAngle = 0.0f;
}

bool APcPlayerCharacter::ApplyDriftPhysics(float DeltaTime, FVector InputDir, FVector& CurrentVelDir, bool bIsAirborne)
{
	float Drain = StaminaDrainRate * DeltaTime;
	DriftStamina = FMath::Clamp(DriftStamina - Drain, 0.0f, MaxDriftStamina);
	float BlendAlpha = MaxDriftStamina > 0.0f ? DriftStamina / MaxDriftStamina : 0.0f;

	if (DriftStamina <= 0.001f)
	{
		// Hard snap back to grip
		CurrentVelDir = InputDir;
		if (!bIsAirborne) CurrentSpeed -= DriftLinearDrag * 2.0f * DeltaTime;
		CurrentSpeed = FMath::Max(0.0f, CurrentSpeed);
		if (DriftSparksComp->IsActive()) DriftSparksComp->Deactivate();
		return false; 
	}

	float TargetTurnRate = FMath::Lerp(1000.0f, DriftBodyTurnRate, BlendAlpha);
	if (bIsWobbling) TargetTurnRate *= 0.2f;
	CurrentVelDir = FMath::VInterpNormalRotationTo(CurrentVelDir, InputDir, DeltaTime, TargetTurnRate);

	float DragForce = 200.0f; 
	if (!bIsAirborne) CurrentSpeed -= DragForce * DeltaTime;

	float TargetSpeed = FlowComp && FlowComp->IsOverdrive() ? 1200.0f : 800.0f;

	if (CurrentSpeed < TargetSpeed) CurrentSpeed += GroundAcceleration * DeltaTime;
	else CurrentSpeed = FMath::FInterpTo(CurrentSpeed, TargetSpeed, DeltaTime, 0.5f);

	float Dot = FVector::DotProduct(CurrentVelDir, InputDir.GetSafeNormal());
	float AngleDeg = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(Dot, -1.0f, 1.0f)));
	DebugSlipAngle = AngleDeg;

	bool bInPocket = false;
	if (AngleDeg > 10.0f && AngleDeg < 90.0f)
	{
		bInPocket = true;
		float CurveQuality = FMath::Clamp((AngleDeg - 10.0f) / 35.0f, 0.0f, 1.0f);
		FlowComp->AddCharge(0.8f * DeltaTime);

		if (CurrentSpeed < TargetSpeed * 1.3f)
		{
			float AirBoost = bIsAirborne ? 1.5f : 1.0f; 
			CurrentSpeed += DriftAcceleration * CurveQuality * AirBoost * DeltaTime;
		}

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

void APcPlayerCharacter::PerformDash()
{
	if (!bCanDash || !GravityComp || !FlowComp) return;

	if (!FlowComp->TrySpendCharge(1.0f)) return;

	FVector2D InputToUse = FVector2D(CurrentInput.X, CurrentInput.Y);
	if (CurrentInput.IsZero() && !LastValidInput.IsZero()) InputToUse = LastValidInput;

	FRotator ViewRot = GetControlRotation();
	FVector SurfaceNormal = GravityComp->GetSurfaceNormal();
	FVector ViewFwd = FRotationMatrix(ViewRot).GetScaledAxis(EAxis::X);
	FVector ViewRgt = FRotationMatrix(ViewRot).GetScaledAxis(EAxis::Y);
	ViewFwd = FVector::VectorPlaneProject(ViewFwd, SurfaceNormal).GetSafeNormal();
	ViewRgt = FVector::VectorPlaneProject(ViewRgt, SurfaceNormal).GetSafeNormal();

	FVector DashDir;
	if (InputToUse.IsZero()) DashDir = ViewFwd;
	else
	{
		DashDir = (ViewFwd * InputToUse.X) + (ViewRgt * InputToUse.Y);
		DashDir.Normalize();
	}

	float TargetDashSpeed = BaseMoveSpeed + DashImpulseStrength;
	if (CurrentSpeed < TargetDashSpeed) CurrentSpeed = TargetDashSpeed;

	GravityComp->SetVelocity(DashDir * CurrentSpeed);
	bIsDrifting = true;

	FOVImpulse = 20.0f;
	TriggerWobble(); 
	WobbleTimer = 0.15f;
	
	if (DriftSparksComp) DriftSparksComp->Activate(true);

	bCanDash = false;
	GetWorld()->GetTimerManager().SetTimer(TimerHandle_DashCooldown, this, &APcPlayerCharacter::ResetDashCooldown, DashCooldown, false);

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
		if (APcDebugHUD* HUD = Cast<APcDebugHUD>(PC->GetHUD()))
			HUD->AddStyleMessage("DASH", EStyleEventType::Neutral);
}

void APcPlayerCharacter::ResetDashCooldown() { bCanDash = true; }