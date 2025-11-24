#include "PcPlayerCharacter.h"
#include "PcProjectile.h"
#include "Project_Circle/Planet/PcPlanet.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

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
	
	GetCapsuleComponent()->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	
	CurrentSpeedCap = BaseMoveSpeed;
}

void APcPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();
	CurrentSpeedCap = BaseMoveSpeed;
}

// ==============================================================================
// INPUT
// ==============================================================================

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
}

void APcPlayerCharacter::Input_JumpTrigger() 
{ 
	// --- FIXED RHYTHM LOGIC ---
	
	// 1. If Grounded (Wading in water) -> Start Jump
	if (!bIsWaveActive)
	{
		bIsWaveActive = true;
		WavePhase = 0.0f;
		return;
	}
	
	// 2. If Airborne -> Check Window
	float AirEnd = PI; // 3.14
	float CoyoteZone = AirEnd - (AirEnd * CoyoteThreshold); 
	
	bool bInRhythmWindow = WavePhase > CoyoteZone;

	if (bInRhythmWindow)
	{
		// SUCCESS: Perfect timing
		CurrentSpeedCap += SpeedBonusPerHop;
		CurrentSpeedCap = FMath::Min(CurrentSpeedCap, MaxBoostSpeed);

		// Chain the jump
		WavePhase = 0.0f;
	}
	else
	{
		// EARLY PRESS:
		// We do NOTHING. We ignore the input. 
		// We do NOT reset speed. This allows you to mash/press early without penalty.
		// The penalty only comes if you fail to press it at all and hit the floor.
	}
}

void APcPlayerCharacter::Input_StartAttack()
{
	// Ensure Projectile Class is assigned in Blueprint!
	if (ProjectileClass)
	{
		// Fire first shot immediately
		FireProjectile(); 
		// Start timer for subsequent shots
		GetWorldTimerManager().SetTimer(TimerHandle_Attack, this, &APcPlayerCharacter::FireProjectile, FireRate, true);
	}
}

void APcPlayerCharacter::Input_StopAttack()
{
	GetWorldTimerManager().ClearTimer(TimerHandle_Attack);
}

void APcPlayerCharacter::FireProjectile()
{
	if (!ProjectileClass) return;
	
	FVector SpawnLoc = GetActorLocation() + (GetActorForwardVector() * 80.0f);
	FVector GravityDir = (CurrentPlanet) ? CurrentPlanet->GetGravityDirection(SpawnLoc) : FVector(0,0,-1);
	FVector SurfaceNormal = -GravityDir;
	
	// Flatten aim to surface
	FVector CamFwd = CameraComp->GetForwardVector();
	FVector ShootDir = FVector::VectorPlaneProject(CamFwd, SurfaceNormal).GetSafeNormal();

	FActorSpawnParameters P; 
	P.Owner = this; 
	P.Instigator = this;
	
	auto* Proj = GetWorld()->SpawnActor<APcProjectile>(ProjectileClass, SpawnLoc, ShootDir.Rotation(), P);
	if (Proj)
	{
		Proj->InitializeProjectile(ShootDir, CurrentPlanet);
	}
}

// ==============================================================================
// DEBUG
// ==============================================================================

FString APcPlayerCharacter::GetDebugInfo() const
{
	float Progress = (bIsWaveActive) ? (WavePhase / (2.0f * PI)) * 100.0f : 0.0f;
	return FString::Printf(TEXT("SPEED: %.0f / %.0f\nPHASE: %.0f%%\nALT: %.1f"),
		CurrentSpeedCap, MaxBoostSpeed, Progress, SmoothedAltitude);
}

bool APcPlayerCharacter::IsInRhythmWindow() const
{
	if (!bIsWaveActive) return false;
	float AirEnd = PI; 
	float CoyoteZone = AirEnd - (AirEnd * CoyoteThreshold);
	return WavePhase > CoyoteZone;
}

// ==============================================================================
// MOVEMENT LOOP
// ==============================================================================

void APcPlayerCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	ApplyDeterministicMovement(DeltaTime);
}

void APcPlayerCharacter::ApplyDeterministicMovement(float DeltaTime)
{
	// 1. FLOOR SETUP
	FVector ActorLoc = GetActorLocation();
	FVector UpVector = GetActorUpVector();
	FHitResult GroundHit; FCollisionQueryParams P; P.AddIgnoredActor(this);
	
	FVector TraceStart = ActorLoc + (UpVector * 500.0f); 
	FVector TraceEnd = ActorLoc - (UpVector * 500.0f);
	bool bFoundGround = GetWorld()->LineTraceSingleByChannel(GroundHit, TraceStart, TraceEnd, ECC_WorldStatic, P);

	if (!bFoundGround) return;
	FVector SurfaceNormal = GroundHit.Normal;

	// Orient to floor
	FQuat CurrentRot = GetActorQuat();
	FQuat TargetRot = FQuat::FindBetweenNormals(GetActorUpVector(), SurfaceNormal) * CurrentRot;
	SetActorRotation(FQuat::Slerp(CurrentRot, TargetRot, 20.0f * DeltaTime));

	// --------------------------------------------------------
	// 2. DRAG PHYSICS (Water vs Air)
	// --------------------------------------------------------
	
	float TargetDrag = 0.0f;
	float TargetAccel = 0.0f;

	if (bIsWaveActive)
	{
		// IN AIR: Low Drag, Low Control (Slippery)
		TargetDrag = AirDrag;
		TargetAccel = AirAccel;
	}
	else
	{
		// ON GROUND: High Drag (Water Resistance), High Control
		TargetDrag = GroundDrag;
		TargetAccel = GroundAccel;
	}
	
	// Speed Bonus: If we are boosted, we feel slightly less drag on the ground
	// This rewards high speed play
	if (!bIsWaveActive && CurrentSpeedCap > BaseMoveSpeed)
	{
		// Reduce drag slightly based on speed tier
		float BoostFactor = (CurrentSpeedCap - BaseMoveSpeed) / MaxBoostSpeed; // 0 to 1
		TargetDrag = FMath::Lerp(GroundDrag, GroundDrag * 0.5f, BoostFactor);
	}

	// --------------------------------------------------------
	// 3. HORIZONTAL MOVE
	// --------------------------------------------------------
	FVector Forward = FVector::VectorPlaneProject(CameraComp->GetForwardVector(), SurfaceNormal).GetSafeNormal();
	FVector Right = FVector::VectorPlaneProject(CameraComp->GetRightVector(), SurfaceNormal).GetSafeNormal();
	FVector TargetDir = (Forward * CurrentInput.X) + (Right * CurrentInput.Y);
	
	FVector TargetVel = TargetDir.GetSafeNormal() * CurrentSpeedCap;
	if (CurrentInput.IsZero()) TargetVel = FVector::ZeroVector;

	float InterpSpeed = CurrentInput.IsZero() ? TargetDrag : TargetAccel;
	HorizontalVelocity = FMath::VInterpTo(HorizontalVelocity, TargetVel, DeltaTime, InterpSpeed);

	// --------------------------------------------------------
	// 4. VERTICAL OFFSET
	// --------------------------------------------------------
	float TargetAltitude = 0.0f;

	if (bIsWaveActive)
	{
		float PhaseSpeed = (2.0f * PI) / WaveDuration;
		WavePhase += PhaseSpeed * DeltaTime;

		float RawSine = FMath::Sin(WavePhase);
		if (RawSine >= 0.0f) TargetAltitude = RawSine * JumpHeight; 
		else TargetAltitude = RawSine * SinkDepth; 

		if (WavePhase >= 2.0f * PI)
		{
			// WAVE ENDED
			bIsWaveActive = false;
			WavePhase = 0.0f;
			TargetAltitude = 0.0f;
			
			// If we reached here, it means we hit the floor WITHOUT chaining a jump.
			// The Water catches us. Reset Speed.
			CurrentSpeedCap = BaseMoveSpeed;
		}
	}

	// Visual Smoothing
	SmoothedAltitude = FMath::FInterpTo(SmoothedAltitude, TargetAltitude, DeltaTime, 15.0f);

	// 5. FINAL POS
	FVector NewGroundPos = GroundHit.Location + (HorizontalVelocity * DeltaTime);
	SetActorLocation(NewGroundPos + (SurfaceNormal * SmoothedAltitude));
}

void APcPlayerCharacter::NotifyActorBeginOverlap(AActor* Other) { if (APcPlanet* P = Cast<APcPlanet>(Other)) CurrentPlanet = P; }
void APcPlayerCharacter::NotifyActorEndOverlap(AActor* Other) { if (Other == CurrentPlanet) CurrentPlanet = nullptr; }