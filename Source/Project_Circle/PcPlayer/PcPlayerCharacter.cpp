// ==========================================
// FILE: PcPlayerCharacter.cpp
// PATH: E:\GameDev\Unreal Engine Projects\Project_Circle\Source\Project_Circle\PcPlayer\PcPlayerCharacter.cpp
// ==========================================
#include "PcPlayerCharacter.h"
#include "PcDebugHUD.h"
#include "Project_Circle/GravitySystem/PcGravityMovementComponent.h"
#include "Project_Circle/Weapon/PcWeapon.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "FlowSystem/PcFlowMechanicComponent.h"
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
	
	// LANDING COMBO (The Drop-In)
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
}

void APcPlayerCharacter::Input_StartAttack() { if (CurrentWeapon) CurrentWeapon->StartPrimaryFire(); }
void APcPlayerCharacter::Input_StopAttack() { if (CurrentWeapon) CurrentWeapon->StopPrimaryFire(); }
void APcPlayerCharacter::Input_FireLaser() { if (CurrentWeapon) CurrentWeapon->FireLaserAttack(); }
void APcPlayerCharacter::TakeHit() { /* Placeholder */ }

void APcPlayerCharacter::Input_JumpTrigger()
{
	// Strict Check: Only jump if actually grounded/not jumping
	if (!bIsJumping)
	{
		PerformJump(); 
	}
	else
	{
		InputBufferTimer = 0.2f; 
	}
}

// --- JUMP IMPLEMENTATION (SINE WAVE) ---

void APcPlayerCharacter::PerformJump()
{
	if (!GravityComp) return;

	bIsJumping = true;
	JumpPhaseTime = 0.0f;
	bCanComboLand = false;
	
	// SINE WAVE LOGIC:
	// We force the hover height to snap to our math curve.
	GravityComp->bSnapToHoverHeight = true; 
	GravityComp->VerticalSmoothing = 0.0f; // Instant snap (Visuals will hide this)

	// FLOW: Just Freeze.
	if (FlowComp) FlowComp->SetFrozen(true);

	// VFX
	if (JumpLaunchFX) UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), JumpLaunchFX, GetActorLocation());
}

// --- GAMEPLAY LOOP ---

void APcPlayerCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!GravityComp || !FlowComp) return;

	if (InputBufferTimer > 0.0f) InputBufferTimer -= DeltaTime;

	UpdateJumpLogic(DeltaTime);
	UpdateSkaterPhysics(DeltaTime);
	UpdateVisuals(DeltaTime);
}

void APcPlayerCharacter::UpdateVisuals(float DeltaTime)
{
	// 1. DYNAMIC FOV
	float TargetRestingFOV = BaseFOV + (FlowComp->CurrentTier * FOVPerTier);
	CurrentFOVMod = FMath::FInterpTo(CurrentFOVMod, TargetRestingFOV - BaseFOV, DeltaTime, 2.0f);
	FOVImpulse = FMath::FInterpTo(FOVImpulse, 0.0f, DeltaTime, 5.0f);

	// 2. STEADYCAM (Hides the Sine Wave Snap)
	// We calculate where the camera *wants* to be (TargetCameraSink).
	// But we move SmoothedCameraHeight slowly.
	
	// If bouncing up/down rapidly, SmoothedCameraHeight won't keep up, creating a natural lag.
	// We apply the difference to the camera Z.
	
	SmoothedCameraHeight = FMath::FInterpTo(SmoothedCameraHeight, TargetCameraSink, DeltaTime, VerticalCameraLagSpeed);

	if (CameraComp)
	{
		CameraComp->SetFieldOfView(BaseFOV + CurrentFOVMod + FOVImpulse);
		
		FVector NewLoc = CameraComp->GetRelativeLocation();
		// Apply the smoothed sink value
		NewLoc.Z = -SmoothedCameraHeight; 
		CameraComp->SetRelativeLocation(NewLoc);
	}
}

void APcPlayerCharacter::UpdateJumpLogic(float DeltaTime)
{
	// Combo Window
	if (bCanComboLand)
	{
		LandWindowTimer -= DeltaTime;
		if (LandWindowTimer <= 0.0f)
		{
			bCanComboLand = false;
			FlowComp->SetFrozen(false); 
		}
	}

	if (bIsJumping)
	{
		JumpPhaseTime += DeltaTime;
		
		// SINE WAVE MATH
		// 0 to 1 over WaveDuration
		float Alpha = (JumpPhaseTime / WaveDuration); 
		
		if (Alpha >= 1.0f)
		{
			// === LANDED ===
			bIsJumping = false;
			JumpPhaseTime = 0.0f;
			GravityComp->HoverHeight = 0.0f;
			
			OnLandedHit(); 
		}
		else
		{
			// === IN AIR ===
			// Calculate Height: sin(0 to PI) * Peak
			float SineVal = FMath::Sin(Alpha * UE_PI); 
			GravityComp->HoverHeight = SineVal * JumpPeakHeight;
			
			// Force Snap so physics matches math
			GravityComp->bSnapToHoverHeight = true; 
		}
	}
	else
	{
		// Walking Logic
		GravityComp->bSnapToHoverHeight = false; 
		GravityComp->VerticalSmoothing = 10.0f; 
		GravityComp->HoverHeight = FMath::FInterpTo(GravityComp->HoverHeight, 0.0f, DeltaTime, 5.0f);
		
		// VISUALS: Relax the camera sink back to 0
		TargetCameraSink = 0.0f;
	}
}

void APcPlayerCharacter::OnLandedHit()
{
	// Open Combo Window
	bCanComboLand = true;
	LandWindowTimer = LandComboWindow;
	
	// VISUALS: The Dunk Trigger
	// We set the target deep, smoothing will catch up then recover
	TargetCameraSink = LandingSinkAmount; 
	FOVImpulse = BoostFOVImpulse;
	
	if (LandingShake) UGameplayStatics::PlayWorldCameraShake(GetWorld(), LandingShake, GetActorLocation(), 0.0f, 500.0f);

	// PHYSICS: Rotational Throw
	FVector CurrentVelDir = GravityComp->GetCurrentVelocity().GetSafeNormal();
	FVector SurfaceNormal = GravityComp->GetSurfaceNormal();
	FVector CamFwd = FVector::VectorPlaneProject(CameraComp->GetForwardVector(), SurfaceNormal).GetSafeNormal();
	FVector CamRight = FVector::VectorPlaneProject(CameraComp->GetRightVector(), SurfaceNormal).GetSafeNormal();
	FVector InputDir = (CamFwd * CurrentInput.X) + (CamRight * CurrentInput.Y);

	if (!InputDir.IsZero())
	{
		float Dot = FVector::DotProduct(CurrentVelDir, InputDir);
		float AngleDeg = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(Dot, -1.0f, 1.0f)));
		FVector Cross = FVector::CrossProduct(CurrentVelDir, InputDir);
		float Direction = (FVector::DotProduct(Cross, SurfaceNormal) > 0) ? 1.0f : -1.0f;

		if (AngleDeg > 15.0f)
		{
			CurrentAngularVelocity = AngleDeg * Direction * LandingSpinBoost;
			CurrentAngularVelocity = FMath::Clamp(CurrentAngularVelocity, -350.0f, 350.0f);
		}
	}

	// CHECK BUFFER
	if (InputBufferTimer > 0.0f)
	{
		InputBufferTimer = 0.0f;
		PerformJump();
	}
}

void APcPlayerCharacter::UpdateSkaterPhysics(float DeltaTime)
{
	float MaxSpeedForTier = BaseMoveSpeed + (FlowComp->CurrentTier * SpeedPerTier);
	float DriftSpeedCap = MaxSpeedForTier * 1.2f; 
	
	FVector CurrentVel = GravityComp->GetCurrentVelocity();
	FVector SurfaceNormal = GravityComp->GetSurfaceNormal();
	FVector CurrentVelDir = CurrentVel.GetSafeNormal();
	if (CurrentVel.SizeSquared() < 1.0f) CurrentVelDir = GetActorForwardVector();

	// Calculate Input
	FVector CamFwd = FVector::VectorPlaneProject(CameraComp->GetForwardVector(), SurfaceNormal).GetSafeNormal();
	FVector CamRight = FVector::VectorPlaneProject(CameraComp->GetRightVector(), SurfaceNormal).GetSafeNormal();
	FVector InputDir = (CamFwd * CurrentInput.X) + (CamRight * CurrentInput.Y);
	InputDir.Normalize();

	DebugLastVelocityDir = CurrentVelDir;
	DebugLastInputDir = InputDir;

	bool bEffectiveDrift = false;
	
	// STOP LOGIC (Only on ground)
	// Note: We don't check IsFalling() here because Sine Wave handles Z.
	// We only check bIsJumping to know if we are airborne.
	if (InputDir.IsZero() && !bIsDrifting && CurrentAngularVelocity == 0.0f && !bIsJumping)
	{
		CurrentSpeed -= 1500.0f * DeltaTime; 
		if (CurrentSpeed < 0.0f) CurrentSpeed = 0.0f;
	}
	else
	{
		if (bIsDrifting)
		{
			// === DRIFT MODE ===
			
			// 1. Angular Velocity
			float TurnInput = 0.0f;
			if (!InputDir.IsZero())
			{
				FVector Cross = FVector::CrossProduct(CurrentVelDir, InputDir);
				float Sign = FVector::DotProduct(Cross, SurfaceNormal);
				TurnInput = (Sign > 0) ? 1.0f : -1.0f;
				if (FVector::DotProduct(CurrentVelDir, InputDir) > 0.99f) TurnInput = 0.0f; 
			}

			float TargetSpin = TurnInput * 150.0f; 
			CurrentAngularVelocity = FMath::FInterpTo(CurrentAngularVelocity, TargetSpin, DeltaTime, 5.0f);
			if (TurnInput == 0.0f) CurrentAngularVelocity = FMath::FInterpTo(CurrentAngularVelocity, 0.0f, DeltaTime, RotationalDrag);

			CurrentVelDir = CurrentVelDir.RotateAngleAxis(CurrentAngularVelocity * DeltaTime, SurfaceNormal);

			// 2. Efficiency
			float Dot = FVector::DotProduct(CurrentVelDir, InputDir);
			float AngleDeg = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(Dot, -1.0f, 1.0f)));
			DebugSlipAngle = AngleDeg;

			if (AngleDeg > 25.0f && AngleDeg < 85.0f)
			{
				bEffectiveDrift = true;
				
				float Efficiency = (AngleDeg - 25.0f) / 60.0f;
				Efficiency = FMath::Clamp(Efficiency, 0.0f, 1.0f);

				// VISUALS: Set Drift Sink
				// If we carve deep, target sink increases
				TargetCameraSink = DriftCameraSinkAmount * Efficiency;

				if (!bIsJumping && CurrentSpeed < DriftSpeedCap)
				{
					CurrentSpeed += DriftAcceleration * Efficiency * DeltaTime;
				}
				
				if (FlowComp->CurrentState == EFlowState::Frozen) FlowComp->SetFrozen(false);
				float FlowReward = 10.0f + (Efficiency * 50.0f); 
				FlowComp->InjectFlow(FlowReward * DeltaTime); 
			}
			else
			{
				// Penalty
				if (!bIsJumping) 
				{
					float Drag = (AngleDeg >= 85.0f) ? 600.0f : 150.0f;
					CurrentSpeed -= Drag * DeltaTime;
				}
				// Reset sink if not drifting efficiently
				TargetCameraSink = 0.0f;
			}
		}
		else
		{
			// === GRIP MODE ===
			CurrentAngularVelocity = 0.0f; 
			CurrentVelDir = FMath::VInterpNormalRotationTo(CurrentVelDir, InputDir, DeltaTime, GripSteeringRate);
			DebugSlipAngle = 0.0f;
			TargetCameraSink = 0.0f; // Reset Visuals
			
			if (CurrentSpeed > MaxSpeedForTier) CurrentSpeed = FMath::FInterpTo(CurrentSpeed, MaxSpeedForTier, DeltaTime, 0.5f);
			else CurrentSpeed = FMath::FInterpTo(CurrentSpeed, MaxSpeedForTier, DeltaTime, 2.0f);
		}
	}
	
	if (bEffectiveDrift) { if (!DriftSparksComp->IsActive()) DriftSparksComp->Activate(); }
	else { if (DriftSparksComp->IsActive()) DriftSparksComp->Deactivate(); }

	// --- FINAL VELOCITY ---
	// We only set Horizontal Speed. 
	// The Gravity Component applies HoverHeight (Z) AFTER this function runs.
	GravityComp->SetVelocity(CurrentVelDir * CurrentSpeed);
	
	FlowComp->UpdateFlowLogic(DeltaTime, bEffectiveDrift);
}

float APcPlayerCharacter::GetCurrentSpeed() const { return CurrentSpeed; }