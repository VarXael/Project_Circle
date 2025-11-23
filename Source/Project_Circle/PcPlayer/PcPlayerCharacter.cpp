#include "PcPlayerCharacter.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Project_Circle/Planet/PcPlanet.h"

APcPlayerCharacter::APcPlayerCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	CameraComp = CreateDefaultSubobject<UCameraComponent>(TEXT("CameraComp"));
	CameraComp->SetupAttachment(GetCapsuleComponent());
	CameraComp->SetRelativeLocation(FVector(0, 0, 60.0f));
	CameraComp->bUsePawnControlRotation = false;
	bUseControllerRotationYaw = false;

	// DISABLE Standard Physics
	if (GetCharacterMovement())
	{
		GetCharacterMovement()->GravityScale = 0.0f; // We apply custom gravity
		GetCharacterMovement()->DefaultLandMovementMode = MOVE_Flying; // Free movement
		GetCharacterMovement()->AirControl = 1.0f;
	}

	// CRITICAL: Capsule must Overlap, not Block, or we can't sink
	GetCapsuleComponent()->SetCollisionResponseToAllChannels(ECR_Overlap);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block); // Still block other players
}

void APcPlayerCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	FVector Location = GetActorLocation();
	FVector GravityDir = FVector(0, 0, -1);
	float Altitude = 0.0f;

	// 1. GET PLANET DATA
	if (CurrentPlanet)
	{
		GravityDir = CurrentPlanet->GetGravityDirection(Location);
		Altitude = CurrentPlanet->GetAltitude(Location);
	}

	// 2. ORIENTATION
	// Rotate feet to point to the planet center
	FVector TargetUp = -GravityDir;
	FQuat CurrentRot = GetActorQuat();
	FQuat TargetRot = FQuat::FindBetweenNormals(GetActorUpVector(), TargetUp) * CurrentRot;
	SetActorRotation(FQuat::Slerp(CurrentRot, TargetRot, 15.0f * DeltaTime));

	// 3. CALCULATE FORCES
	FVector TotalForce = FVector::ZeroVector;

	// A. GRAVITY (Always Pulls Down)
	TotalForce += GravityDir * 980.0f;

	// B. BUOYANCY (Push Up if Sinking)
	if (Altitude < 0.0f) // Underwater / Underground
	{
		float Depth = -Altitude;

		// Spring Force: F = kx (Stiffness * Depth)
		FVector SpringForce = TargetUp * (Depth * BuoyancyStiffness);

		// Damping Force: F = -cv (Resist vertical speed)
		float VerticalSpeed = (Velocity | GravityDir); // Project velocity onto gravity axis
		FVector DampingForce = -GravityDir * (VerticalSpeed * BuoyancyDamping);

		TotalForce += SpringForce + DampingForce;
	}

	// C. INPUT MOVEMENT
	// Project input onto the surface plane so we move tangent to the sphere
	FVector CamFwd = CameraComp->GetForwardVector();
	FVector CamRight = CameraComp->GetRightVector();
	FVector MoveDir = (CamFwd * CurrentInput.X + CamRight * CurrentInput.Y).GetSafeNormal();
	MoveDir = FVector::VectorPlaneProject(MoveDir, TargetUp).GetSafeNormal();

	TotalForce += MoveDir * MoveAcceleration;

	// 4. INTEGRATE VELOCITY
	Velocity += TotalForce * DeltaTime;

	// 5. DYNAMIC FRICTION (The Surfing Logic)
	// Apply drag to horizontal velocity only
	FVector VerticalVel = (Velocity | GravityDir) * GravityDir;
	FVector HorizontalVel = Velocity - VerticalVel;
	float Speed = HorizontalVel.Size();

	if (Speed > 0.0f)
	{
		// High Speed = Low Friction (Surf)
		// Low Speed = High Friction (Stop)
		float FrictionAlpha = FMath::Clamp(Speed / 1000.0f, 0.0f, 1.0f);
		float CurrentFriction = FMath::Lerp(FrictionLowSpeed, FrictionHighSpeed, FrictionAlpha);

		// Apply Drag: v = v * (1 - friction * dt)
		HorizontalVel *= FMath::Max(0.0f, 1.0f - (CurrentFriction * DeltaTime));

		// Recombine
		Velocity = HorizontalVel + VerticalVel;
	}

	// 6. MOVE
	AddActorWorldOffset(Velocity * DeltaTime, true); // Sweep true just in case we hit a generic box
}

// --- BINDINGS ---
void APcPlayerCharacter::Input_Jump()
{
	// Manual Jump: Add upward velocity relative to planet
	if (CurrentPlanet)
	{
		FVector Up = -CurrentPlanet->GetGravityDirection(GetActorLocation());
		Velocity += Up * 600.0f;
	}
}

// Input Mapping
void APcPlayerCharacter::Input_Move(FVector2D Value)
{
	CurrentInput.X = Value.X;
	CurrentInput.Y = Value.Y;
}

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

void APcPlayerCharacter::NotifyActorBeginOverlap(AActor* Other)
{
	if (APcPlanet* P = Cast<APcPlanet>(Other)) CurrentPlanet = P;
}

void APcPlayerCharacter::NotifyActorEndOverlap(AActor* Other) { if (Other == CurrentPlanet) CurrentPlanet = nullptr; }
