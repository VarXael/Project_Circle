#include "PcPlayerCharacter.h"
#include "Project_Circle/GravitySystem/PcGravityMovementComponent.h"
#include "Project_Circle/Weapon/PcWeapon.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetMathLibrary.h"

APcPlayerCharacter::APcPlayerCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	CameraComp = CreateDefaultSubobject<UCameraComponent>(TEXT("CameraComp"));
	CameraComp->SetupAttachment(GetCapsuleComponent());
	CameraComp->SetRelativeLocation(FVector(0, 0, 60.0f));
	CameraComp->bUsePawnControlRotation = false;

	if (GetCharacterMovement())
	{
		GetCharacterMovement()->GravityScale = 0.0f;
		GetCharacterMovement()->DefaultLandMovementMode = MOVE_Flying;
	}

	GravityComp = CreateDefaultSubobject<UPcGravityMovementComponent>(TEXT("GravityComp"));
}

void APcPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();
	
	CurrentSpeed = BaseMoveSpeed;
	ComboTimer = MaxComboTime; // Start fresh

	if (GravityComp)
	{
		GravityComp->MovementMode = EPcMovementMode::Skater;
		GravityComp->bOrientRotationToMovement = false; 
		GravityComp->PivotOffset = 88.0f; 
		GravityComp->HoverHeight = 0.0f;
		GravityComp->MaxSpeed = 10000.0f; 
		GravityComp->Acceleration = 10000.0f;
		GravityComp->Deceleration = 0.0f; 
		GravityComp->VerticalSmoothing = 15.0f; 
	}

	if (StartingWeaponClass)
	{
		FActorSpawnParameters P; P.Owner = this; P.Instigator = this;
		CurrentWeapon = GetWorld()->SpawnActor<APcWeapon>(StartingWeaponClass, GetActorTransform(), P);
		if (CurrentWeapon) CurrentWeapon->AttachToPlayer(this);
	}
}

// --- INPUTS ---

void APcPlayerCharacter::Input_Move(FVector2D Value) { CurrentInput = FVector(Value.X, Value.Y, 0.0f); }

void APcPlayerCharacter::Input_Look(FVector2D Value)
{
	if (Value.X != 0.0f) AddActorLocalRotation(FRotator(0, Value.X, 0));
	if (Value.Y != 0.0f)
	{
		FRotator Rot = CameraComp->GetRelativeRotation();
		Rot.Pitch = FMath::Clamp(Rot.Pitch + Value.Y, -85.0f, 85.0f);
		CameraComp->SetRelativeRotation(Rot);
	}
	if (CurrentWeapon) CurrentWeapon->ApplyInputForSway(Value);
}

void APcPlayerCharacter::Input_StartAttack() { if (CurrentWeapon) CurrentWeapon->StartPrimaryFire(); }
void APcPlayerCharacter::Input_StopAttack() { if (CurrentWeapon) CurrentWeapon->StopPrimaryFire(); }
void APcPlayerCharacter::Input_FireLaser() { if (CurrentWeapon) CurrentWeapon->FireLaserAttack(); }

void APcPlayerCharacter::TakeHit() { CurrentSpeed = BaseMoveSpeed; }

// --- RHYTHM JUMP LOGIC ---

void APcPlayerCharacter::Input_JumpTrigger()
{
	// 1. If currently in the Perfect Window (Landed recently), Jump Immediately
	if (bInPerfectWindow)
	{
		PerformJump(true); // Perfect!
		return;
	}

	// 2. If Normal Grounded (Missed the window, or just starting), Jump Immediately
	if (!bIsJumping)
	{
		PerformJump(false); // Normal
		return;
	}

	// 3. If Airborne, Buffer the Input
	// This creates the "forgiveness" if you press 0.1s before landing
	if (bIsJumping)
	{
		InputBufferTimer = InputBufferAllowance;
	}
}

void APcPlayerCharacter::PerformJump(bool bIsPerfect)
{
	bIsJumping = true;
	JumpPhaseTime = 0.0f;
	bInPerfectWindow = false;
	WindowTimer = 0.0f;
	
	// Reset Buffer
	InputBufferTimer = 0.0f;

	if (bIsPerfect)
	{
		// SUCCESS: Boost Speed and Refresh Combo
		CurrentSpeed += JumpBoostSpeed;
		ComboTimer = MaxComboTime; 
	}
	else
	{
		// NORMAL/MISS: Reset Combo? Or just decay?
		// As per instructions: "If you miss a perfect rhythm jump, you lose the speed boost"
		// We drop speed back down towards base (smoothly or instantly)
		
		// Let's drop it significantly but not full stop
		float SpeedLoss = (CurrentSpeed - BaseMoveSpeed) * 0.5f; 
		CurrentSpeed -= SpeedLoss; 
		
		// Reset streak logic?
		// For now, let's keep the timer running down, it will naturally decay speed in Tick
	}

	CurrentSpeed = FMath::Clamp(CurrentSpeed, BaseMoveSpeed, MaxSkimSpeed);
}

bool APcPlayerCharacter::IsInRhythmWindow() const
{
	return bInPerfectWindow; 
}

FString APcPlayerCharacter::GetDebugInfo() const
{
	return FString::Printf(TEXT("SPEED: %.0f\nCOMBO: %.2f\nPERFECT: %s"), 
		CurrentSpeed, 
		ComboTimer, 
		bInPerfectWindow ? TEXT("READY") : TEXT("NO"));
}

// --- PHYSICS LOOP ---

void APcPlayerCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	if (!GravityComp) return;

	// 1. Handle Input Buffer
	if (InputBufferTimer > 0.0f)
	{
		InputBufferTimer -= DeltaTime;
	}

	UpdateSkaterPhysics(DeltaTime);
	UpdateJumpLogic(DeltaTime);

	// Camera Bounce Recovery
	FVector CamLoc = CameraComp->GetRelativeLocation();
	float NewZ = FMath::FInterpTo(CamLoc.Z, 60.0f, DeltaTime, 10.0f);
	CameraComp->SetRelativeLocation(FVector(CamLoc.X, CamLoc.Y, NewZ));
}

void APcPlayerCharacter::UpdateSkaterPhysics(float DeltaTime)
{
	// --- STATE ---
	FVector CurrentVel = GravityComp->GetCurrentVelocity();
	FVector SurfaceNormal = GravityComp->GetSurfaceNormal();
	FVector CurrentDir = CurrentVel.GetSafeNormal();
	if (CurrentDir.IsZero()) CurrentDir = GetActorForwardVector();

	// --- INPUT ---
	FVector CamFwd = CameraComp->GetForwardVector();
	FVector CamRight = CameraComp->GetRightVector();
	CamFwd = FVector::VectorPlaneProject(CamFwd, SurfaceNormal).GetSafeNormal();
	CamRight = FVector::VectorPlaneProject(CamRight, SurfaceNormal).GetSafeNormal();
	FVector InputDir = (CamFwd * CurrentInput.X) + (CamRight * CurrentInput.Y);
	InputDir.Normalize();

	// --- STEERING ---
	if (!InputDir.IsZero())
	{
		FVector NewDir = FMath::VInterpNormalRotationTo(CurrentDir, InputDir, DeltaTime, SteeringRate);
		CurrentDir = NewDir;
	}

	// --- SPEED LOGIC (Carve & Decay) ---
	
	// 1. Apply Carve/Drag
	if (!InputDir.IsZero())
	{
		float Dot = (CurrentDir | InputDir);
		if (Dot < 0.99f) CurrentSpeed += CarveAcceleration * DeltaTime; // Carve
		else CurrentSpeed -= PassiveDrag * DeltaTime; // Drag
	}
	else
	{
		CurrentSpeed -= PassiveDrag * 3.0f * DeltaTime; // Friction
	}

	// 2. Combo Decay Logic
	// If we are grounded and NOT in the perfect window, the Combo Timer ticks down
	if (!bIsJumping && !bInPerfectWindow)
	{
		ComboTimer -= DeltaTime;
		
		// If Combo runs out, force speed decay
		if (ComboTimer <= 0.0f)
		{
			// Decays fast towards base speed
			CurrentSpeed = FMath::FInterpTo(CurrentSpeed, BaseMoveSpeed, DeltaTime, 2.0f);
		}
	}
	else
	{
		// Freeze Timer if Jumping or in Perfect Window
		// (No decay logic here, speed is maintained)
	}

	// Limits
	CurrentSpeed = FMath::Clamp(CurrentSpeed, BaseMoveSpeed, MaxSkimSpeed);
	if (CurrentInput.IsZero() && CurrentSpeed <= BaseMoveSpeed + 10.0f) CurrentSpeed = 0.0f;

	// --- MOTOR ---
	GravityComp->SetVelocity(CurrentDir * CurrentSpeed);
}

void APcPlayerCharacter::UpdateJumpLogic(float DeltaTime)
{
	// 1. AIRBORNE PHASE
	if (bIsJumping)
	{
		JumpPhaseTime += DeltaTime;
		GravityComp->bSnapToHoverHeight = true; // Direct Drive

		// Check for Landing
		if (JumpPhaseTime >= WaveDuration)
		{
			// LANDED!
			bIsJumping = false;
			JumpPhaseTime = 0.0f;
			
			// Enter Perfect Window
			bInPerfectWindow = true;
			WindowTimer = 0.0f;
			
			// Snap to floor
			GravityComp->HoverHeight = 0.0f;
			CameraComp->AddLocalOffset(FVector(0, 0, -15.0f));

			// CHECK BUFFER: Did they press jump slightly before landing?
			if (InputBufferTimer > 0.0f)
			{
				PerformJump(true); // Immediate Perfect Jump!
			}
			return;
		}

		// Calculate Sine Wave
		float Alpha = (JumpPhaseTime / WaveDuration); 
		float SineVal = FMath::Sin(Alpha * PI); 
		GravityComp->HoverHeight = SineVal * JumpPeakHeight;
		return;
	}

	// 2. PERFECT WINDOW PHASE
	if (bInPerfectWindow)
	{
		GravityComp->bSnapToHoverHeight = true; // Stay locked to floor
		WindowTimer += DeltaTime;

		if (WindowTimer >= PerfectWindowDuration)
		{
			// Window Missed -> Transition to Normal Ground
			bInPerfectWindow = false;
			
			// Speed Punishment? Or just rely on ComboTimer decay in UpdateSkaterPhysics?
			// Let's force a small penalty for missing the rhythm
			CurrentSpeed -= 100.0f; 
		}
		return;
	}

	// 3. NORMAL GROUND PHASE
	// Smoothing ON for water feel
	GravityComp->bSnapToHoverHeight = false; 
	GravityComp->HoverHeight = FMath::FInterpTo(GravityComp->HoverHeight, 0.0f, DeltaTime, 5.0f);
}