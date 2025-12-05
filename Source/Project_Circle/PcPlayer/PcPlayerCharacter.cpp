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
	
	// LANDING COMBO LOGIC (The Drop-In)
	if (bCanComboLand && LandWindowTimer > 0.0f)
	{
		FlowComp->InjectFlow(50.0f); 
		CurrentSpeed += 400.0f; 
		bCanComboLand = false;
		
		FOVImpulse = BoostFOVImpulse; 
		if (APlayerController* PC = Cast<APlayerController>(GetController()))
			if (APcDebugHUD* HUD = Cast<APcDebugHUD>(PC->GetHUD()))
				HUD->AddStyleMessage("DROP-IN!", EStyleEventType::Good);
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
	
	// SINE WAVE START
	// We force the hover height to snap to our math curve.
	GravityComp->bSnapToHoverHeight = true; 
	GravityComp->VerticalSmoothing = 0.0f; 

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

	// 2. CAMERA SINK RECOVERY
	// Logic moves camera to 0. Events (Land/Drift) push it up/down.
	CurrentCameraSink = FMath::FInterpTo(CurrentCameraSink, 0.0f, DeltaTime, LandingSinkSpeed);

	if (CameraComp)
	{
		CameraComp->SetFieldOfView(BaseFOV + CurrentFOVMod + FOVImpulse);
		
		FVector NewLoc = CameraComp->GetRelativeLocation();
		// Apply negative sink
		NewLoc.Z = -CurrentCameraSink; 
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
		
		// SINE WAVE LOGIC
		float Alpha = (JumpPhaseTime / WaveDuration); 
		
		if (Alpha >= 1.0f)
		{
			// === TIMER ENDED ===
			// RELEASE THE SNAP. Do NOT force landing.
			// Let Gravity take over naturally.
			bIsJumping = false;
			GravityComp->bSnapToHoverHeight = false;
			GravityComp->VerticalSmoothing = 10.0f;
		}
		else
		{
			// === IN AIR ===
			float SineVal = FMath::Sin(Alpha * UE_PI); 
			GravityComp->HoverHeight = SineVal * JumpPeakHeight;
			
			// Force Snap so physics matches math while rising
			GravityComp->bSnapToHoverHeight = true; 
			GravityComp->VerticalSmoothing = 0.0f;
		}
	}
	else
	{
		// NOT JUMPING STATE
		// Check physics to see if we actually hit the floor
		bool bIsFallingNow = GravityComp->IsFalling();
		
		if (bWasFalling && !bIsFallingNow)
		{
			OnLandedHit(); // PHYSICAL IMPACT
		}
		bWasFalling = bIsFallingNow;
	}
}

void APcPlayerCharacter::OnLandedHit()
{
	// Open Combo Window
	bCanComboLand = true;
	LandWindowTimer = LandComboWindow;
	
	// VISUALS: The Dunk Trigger
	CurrentCameraSink += LandingSinkAmount; 
	FOVImpulse = BoostFOVImpulse;
	
	if (LandingShake) UGameplayStatics::PlayWorldCameraShake(GetWorld(), LandingShake, GetActorLocation(), 0.0f, 500.0f);

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
	
	// Get Vectors
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
	bool bIsAirborne = bIsJumping || GravityComp->IsFalling(); 

	// --- 1. STOPPING LOGIC (The Hockey Stop) ---
	if (InputDir.IsZero() && !bIsAirborne)
	{
		CurrentSpeed -= 2000.0f * DeltaTime; 
		if (CurrentSpeed < 0.0f) CurrentSpeed = 0.0f;
	}
	else
	{
		if (bIsDrifting)
		{
			// === DRIFT MODE (Blade Physics) ===
			
			// A. Steer Slow ("Heavy" Body)
			CurrentVelDir = FMath::VInterpNormalRotationTo(CurrentVelDir, InputDir, DeltaTime, DriftSteeringRate);

			// B. The Edge Math
			float Dot = FVector::DotProduct(CurrentVelDir, InputDir);
			float AngleDeg = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(Dot, -1.0f, 1.0f)));
			DebugSlipAngle = AngleDeg;

			// C. The Push (Generation)
			// Angle 20 to 85 is the sweet spot
			if (AngleDeg > 20.0f && AngleDeg < 85.0f)
			{
				bEffectiveDrift = true;
				
				// Efficiency: 0.0 to 1.0 based on depth
				float PushStrength = (AngleDeg - 20.0f) / 65.0f;
				
				// Add Speed
				if (!bIsAirborne && CurrentSpeed < MaxSpeedForTier * 1.3f)
				{
					CurrentSpeed += DriftAcceleration * PushStrength * DeltaTime;
				}

				// Fill Flow
				if (FlowComp->CurrentState == EFlowState::Frozen) FlowComp->SetFrozen(false);
				FlowComp->InjectFlow(PushStrength * 60.0f * DeltaTime);
			}
			else
			{
				// Inefficient Angle
				if (!bIsAirborne) CurrentSpeed -= 300.0f * DeltaTime;
			}
		}
		else
		{
			// === GRIP MODE (The Glide) ===
			
			// Snappy Steering
			CurrentVelDir = FMath::VInterpNormalRotationTo(CurrentVelDir, InputDir, DeltaTime, GripSteeringRate);
			DebugSlipAngle = 0.0f;

			// D. Aggressive Drag (Must Drift to Speed)
			if (CurrentSpeed > BaseMoveSpeed)
			{
				float ExcessSpeed = CurrentSpeed - BaseMoveSpeed;
				float DragFactor = 1.0f + (ExcessSpeed / 500.0f); 
				CurrentSpeed -= 400.0f * DragFactor * DeltaTime;
			}
			else
			{
				// Recovery to base speed
				CurrentSpeed = FMath::FInterpTo(CurrentSpeed, BaseMoveSpeed, DeltaTime, 2.0f);
			}
		}
	}
	
	if (bEffectiveDrift) { if (!DriftSparksComp->IsActive()) DriftSparksComp->Activate(); }
	else { if (DriftSparksComp->IsActive()) DriftSparksComp->Deactivate(); }

	// --- APPLY PHYSICS ---
	if (bIsJumping)
	{
		// Preserve Vertical, Set Horizontal
		FVector OldVel = GravityComp->GetCurrentVelocity();
		float VertSpeed = FVector::DotProduct(OldVel, SurfaceNormal);
		GravityComp->SetVelocity((CurrentVelDir * CurrentSpeed) + (SurfaceNormal * VertSpeed));
	}
	else
	{
		GravityComp->SetVelocity(CurrentVelDir * CurrentSpeed);
	}

	FlowComp->UpdateFlowLogic(DeltaTime, bEffectiveDrift);
}

float APcPlayerCharacter::GetCurrentSpeed() const { return CurrentSpeed; }