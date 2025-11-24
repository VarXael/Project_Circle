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
	
	// IMPORTANT: Overlap allows sinking. Block stops it.
	GetCapsuleComponent()->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	
	// Initialize Speed
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

void APcPlayerCharacter::Input_JumpStart() 
{ 
	bIsHoldingJump = true;

	// --- RHYTHM LOGIC ---
	
	// 1. If we are just starting from the ground (Not waving yet)
	if (!bIsWaveActive)
	{
		bIsWaveActive = true;
		WavePhase = 0.0f;
		CurrentJumpPeak = MaxJumpHeight;
	}
	else 
	{
		// 2. We are already in the air. Check for Perfect Timing.
		// "Sink" phase starts at PI (3.14). We allow input slightly before that.
		
		float AirEnd = PI; // 3.14
		float CoyoteZone = AirEnd - (AirEnd * CoyoteThreshold); 
		bool bInRhythmWindow = WavePhase > CoyoteZone;

		if (bInRhythmWindow)
		{
			// --- SUCCESS (BUNNY HOP) ---
			CurrentSpeedCap += SpeedBonusPerHop;
			CurrentSpeedCap = FMath::Min(CurrentSpeedCap, MaxBoostSpeed);

			// Chain the jump immediately
			WavePhase = 0.0f;
			CurrentJumpPeak = MaxJumpHeight;
		}
		else
		{
			// --- FAIL (Too Early) ---
			CurrentSpeedCap = BaseMoveSpeed;
		}
	}
}

void APcPlayerCharacter::Input_JumpStop() { bIsHoldingJump = false; }

void APcPlayerCharacter::Input_PrimaryAttack()
{
	if (!ProjectileClass) return;
	FVector SpawnLoc = GetActorLocation() + (GetActorForwardVector() * 80.0f);
	FVector GravityDir = (CurrentPlanet) ? CurrentPlanet->GetGravityDirection(SpawnLoc) : FVector(0,0,-1);
	FVector SurfaceNormal = -GravityDir;
	FVector CamFwd = CameraComp->GetForwardVector();
	FVector ShootDir = FVector::VectorPlaneProject(CamFwd, SurfaceNormal).GetSafeNormal();

	FActorSpawnParameters P; P.Owner = this; P.Instigator = this;
	if (auto* Proj = GetWorld()->SpawnActor<APcProjectile>(ProjectileClass, SpawnLoc, ShootDir.Rotation(), P))
		Proj->InitializeProjectile(ShootDir, CurrentPlanet);
}

// ==============================================================================
// DEBUG INTERFACE
// ==============================================================================

FString APcPlayerCharacter::GetDebugInfo() const
{
	FString StateStr = bIsWaveActive ? TEXT("WAVING") : TEXT("GROUND");
	float Progress = (bIsWaveActive) ? (WavePhase / (2.0f * PI)) * 100.0f : 0.0f;

	return FString::Printf(
		TEXT("STATE: %s\nSPEED: %.0f / %.0f\nPHASE: %.0f%%\nALT: %.1f"),
		*StateStr, CurrentSpeedCap, MaxBoostSpeed, Progress, 
		CurrentJumpPeak * FMath::Sin(WavePhase)
	);
}

bool APcPlayerCharacter::IsInRhythmWindow() const
{
	if (!bIsWaveActive) return false;
	float AirEnd = PI; 
	float CoyoteZone = AirEnd - (AirEnd * CoyoteThreshold);
	return WavePhase > CoyoteZone;
}

// ==============================================================================
// LOGIC LOOP
// ==============================================================================

void APcPlayerCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	ApplyDeterministicMovement(DeltaTime);
}

void APcPlayerCharacter::ApplyDeterministicMovement(float DeltaTime)
{
	// 1. FLOOR & ORIENTATION
	FVector ActorLoc = GetActorLocation();
	FVector UpVector = GetActorUpVector();
	FHitResult GroundHit; FCollisionQueryParams P; P.AddIgnoredActor(this);
	
	FVector TraceStart = ActorLoc + (UpVector * 500.0f); 
	FVector TraceEnd = ActorLoc - (UpVector * 500.0f);
	bool bFoundGround = GetWorld()->LineTraceSingleByChannel(GroundHit, TraceStart, TraceEnd, ECC_WorldStatic, P);

	if (!bFoundGround) return;

	FVector GroundLocation = GroundHit.Location;
	FVector SurfaceNormal = GroundHit.Normal;

	FQuat CurrentRot = GetActorQuat();
	FQuat TargetRot = FQuat::FindBetweenNormals(GetActorUpVector(), SurfaceNormal) * CurrentRot;
	SetActorRotation(FQuat::Slerp(CurrentRot, TargetRot, 20.0f * DeltaTime));

	// --------------------------------------------------------
	// 2. DYNAMIC HANDLING (Speed = Control)
	// --------------------------------------------------------

	float CurrentAccel = bIsWaveActive ? AirAcceleration : GroundAcceleration;
	
	float SpeedRatio = (CurrentSpeedCap - BaseMoveSpeed) / (MaxBoostSpeed - BaseMoveSpeed);
	SpeedRatio = FMath::Clamp(SpeedRatio, 0.0f, 1.0f);

	// Add Snappiness (MaxControlBonus) as we get faster
	CurrentAccel += (SpeedRatio * MaxControlBonus);

	// --------------------------------------------------------
	// 3. HORIZONTAL MOVEMENT
	// --------------------------------------------------------
	FVector Forward = FVector::VectorPlaneProject(CameraComp->GetForwardVector(), SurfaceNormal).GetSafeNormal();
	FVector Right = FVector::VectorPlaneProject(CameraComp->GetRightVector(), SurfaceNormal).GetSafeNormal();
	FVector TargetDir = (Forward * CurrentInput.X) + (Right * CurrentInput.Y);
	
	FVector TargetVel = TargetDir.GetSafeNormal() * CurrentSpeedCap;
	if (CurrentInput.IsZero()) TargetVel = FVector::ZeroVector;

	float InterpSpeed = CurrentInput.IsZero() ? Deceleration : CurrentAccel;
	HorizontalVelocity = FMath::VInterpTo(HorizontalVelocity, TargetVel, DeltaTime, InterpSpeed);

	// 4. VERTICAL OFFSET (The Deterministic Wave)
	float VerticalOffset = 0.0f;

	if (bIsWaveActive)
	{
		// Cut jump height if button released while going up
		if (!bIsHoldingJump && WavePhase < HALF_PI)
		{
			CurrentJumpPeak = FMath::Sin(WavePhase) * MaxJumpHeight;
			WavePhase = HALF_PI; // Fast forward to peak
		}

		float PhaseSpeed = (2.0f * PI) / WaveDuration;
		WavePhase += PhaseSpeed * DeltaTime;

		float RawSine = FMath::Sin(WavePhase);
		if (RawSine >= 0.0f) VerticalOffset = RawSine * CurrentJumpPeak; // Air
		else VerticalOffset = RawSine * SinkDepth; // Sink

		// End Cycle
		if (WavePhase >= 2.0f * PI)
		{
			bIsWaveActive = false;
			WavePhase = 0.0f;
			VerticalOffset = 0.0f;
			
			// Missed Opportunity: Landed without chaining -> Reset Speed
			CurrentSpeedCap = BaseMoveSpeed;
		}
	}

	// 5. FINAL POSITION
	FVector NewGroundPos = GroundLocation + (HorizontalVelocity * DeltaTime);
	SetActorLocation(NewGroundPos + (SurfaceNormal * VerticalOffset));
}

void APcPlayerCharacter::NotifyActorBeginOverlap(AActor* Other) { if (APcPlanet* P = Cast<APcPlanet>(Other)) CurrentPlanet = P; }
void APcPlayerCharacter::NotifyActorEndOverlap(AActor* Other) { if (Other == CurrentPlanet) CurrentPlanet = nullptr; }