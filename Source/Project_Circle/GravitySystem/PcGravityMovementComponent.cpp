#include "PcGravityMovementComponent.h"
#include "Project_Circle/Planet/PcPlanet.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Actor.h"
#include "DrawDebugHelpers.h" 

UPcGravityMovementComponent::UPcGravityMovementComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UPcGravityMovementComponent::BeginPlay()
{
	Super::BeginPlay();

	TArray<AActor*> Planets;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), APcPlanet::StaticClass(), Planets);
	if (Planets.Num() > 0)
	{
		CurrentPlanet = Cast<APcPlanet>(Planets[0]);
	}

	UpdateSurfaceInfo();
	// Only snap if we actually found a floor
	if (bSurfaceFound && GetOwner())
	{
		FVector SnapPos = SurfaceHitLocation + (CurrentSurfaceNormal * (PivotOffset + HoverHeight));
		GetOwner()->SetActorLocation(SnapPos);
	}
}

void UPcGravityMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!GetOwner()) return;

	float SafeDelta = FMath::Min(DeltaTime, 0.05f);

	UpdateSurfaceInfo();
	ApplyPhysics(SafeDelta);

	// Reset Input
	CurrentInput = FVector::ZeroVector;
}

void UPcGravityMovementComponent::AddInputVector(FVector WorldInputDirection)
{
	if (!WorldInputDirection.IsZero())
	{
		CurrentInput = WorldInputDirection.GetSafeNormal();
	}
}

void UPcGravityMovementComponent::AddImpulse(FVector Impulse)
{
	Velocity += Impulse;
}

void UPcGravityMovementComponent::SetVelocity(FVector NewVelocity)
{
	Velocity = NewVelocity;
}

void UPcGravityMovementComponent::UpdateSurfaceInfo()
{
	AActor* Owner = GetOwner();
	FVector MyLoc = Owner->GetActorLocation();

	// 1. SINGULARITY CHECK (The Void)
	bool bInSingularity = false;
	if (CurrentPlanet)
	{
		float Dist = FVector::Dist(MyLoc, CurrentPlanet->GetActorLocation());
		if (Dist < MinGravityDistance)
		{
			bInSingularity = true;
		}
	}

	if (bInSingularity)
	{
		// ZERO-G MODE:
		// We define "Up" as whatever the actor is currently doing, to prevent spinning.
		CurrentSurfaceNormal = Owner->GetActorUpVector();
		bSurfaceFound = false;
		// We intentionally do NOT return here. We let the rest of the logic run 
		// so bIsFalling gets set correctly later.
	}
	else
	{
		// STANDARD GRAVITY
		FVector GravityDir = FVector::DownVector;
		if (CurrentPlanet)
		{
			GravityDir = CurrentPlanet->GetGravityDirection(MyLoc);
			if (GravityDir.IsZero()) GravityDir = FVector::DownVector;
		}
		CurrentSurfaceNormal = -GravityDir; 

		// ROBUST TRACE
		FVector TraceStart = MyLoc - (GravityDir * 2000.0f);
		FVector TraceEnd = MyLoc + (GravityDir * 2000.0f);

		FHitResult Hit;
		FCollisionQueryParams P;
		P.AddIgnoredActor(Owner);

		bSurfaceFound = GetWorld()->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_WorldStatic, P);

		if (bSurfaceFound)
		{
			SurfaceHitLocation = Hit.Location;
		}
		else if (CurrentPlanet)
		{
			// Fallback Math Sphere
			FVector ToCenter = MyLoc - CurrentPlanet->GetActorLocation();
			SurfaceHitLocation = CurrentPlanet->GetActorLocation() + (ToCenter.GetSafeNormal() * CurrentPlanet->SurfaceRadius);
			// Note: We don't set bSurfaceFound=true here for the Fallback.
			// This allows the "Falling" logic to kick in if we are over a hole.
			// But for simplicity, let's say:
			bSurfaceFound = true;
		}
	}
}

void UPcGravityMovementComponent::ApplyPhysics(float DeltaTime)
{
	AActor* Owner = GetOwner();
	
	// Store state BEFORE moving
	FVector StartNormal = CurrentSurfaceNormal;
	float StartDistToCenter = 0.0f;
	if (CurrentPlanet)
	{
		StartDistToCenter = FVector::Dist(Owner->GetActorLocation(), CurrentPlanet->GetActorLocation());
	}

	// --- A. DECOMPOSE ---
	float VerticalSpeed = FVector::DotProduct(Velocity, CurrentSurfaceNormal);
	FVector TangentVel = FVector::VectorPlaneProject(Velocity, CurrentSurfaceNormal);

	// --- B. CHECK FALLING ---
	if (!bSurfaceFound)
	{
		bIsFalling = true;
	}
	else
	{
		FVector FeetLocation = Owner->GetActorLocation() - (CurrentSurfaceNormal * PivotOffset);
		float DistToFloor = FVector::DotProduct(FeetLocation - SurfaceHitLocation, CurrentSurfaceNormal);
		DistToFloor -= HoverHeight;

		if (DistToFloor > SnapDistance) bIsFalling = true;
		else if (DistToFloor <= SnapDistance && VerticalSpeed <= 0.0f) bIsFalling = false;
	}

	// --- C. APPLY INPUT ---
	FVector TangentInput = FVector::VectorPlaneProject(CurrentInput, CurrentSurfaceNormal).GetSafeNormal();
	
	if (MovementMode == EPcMovementMode::Skater || MovementMode == EPcMovementMode::GroundUnit)
	{
		if (!TangentInput.IsZero())
		{
			TangentVel += TangentInput * Acceleration * DeltaTime;
		}
		else if (!bIsFalling) 
		{
			float Speed = TangentVel.Size();
			float Drop = Deceleration * DeltaTime;
			float NewSpeed = FMath::Max(0.0f, Speed - Drop);
			if (Speed > 0) TangentVel *= (NewSpeed / Speed);
		}
	}
	
	if (TangentVel.SizeSquared() > MaxSpeed * MaxSpeed)
	{
		TangentVel = TangentVel.GetSafeNormal() * MaxSpeed;
	}

	// --- D. APPLY GRAVITY ---
	if (MovementMode != EPcMovementMode::Projectile)
	{
		if (bIsFalling && bSurfaceFound)
		{
			VerticalSpeed -= GravityScale * DeltaTime;
		}
		else
		{
			VerticalSpeed = 0.0f; 
		}
	}

	// --- E. MOVE (INTEGRATE) ---
	Velocity = TangentVel + (CurrentSurfaceNormal * VerticalSpeed);
	if (!Velocity.IsZero())
	{
		Owner->AddActorWorldOffset(Velocity * DeltaTime);
	}

	// =========================================================
	//   FIX 1: ORBITAL DRIFT CORRECTION (Projectiles Only)
	// =========================================================
	// Moving linearly in a hollow sphere brings you closer to the wall (radius increases).
	// We push the projectile back towards the center to maintain its original orbital altitude.
	if (MovementMode == EPcMovementMode::Projectile && CurrentPlanet && StartDistToCenter > 0.0f)
	{
		FVector NewLoc = Owner->GetActorLocation();
		FVector PlanetCenter = CurrentPlanet->GetActorLocation();
		FVector ToNewLoc = NewLoc - PlanetCenter;
		
		// If we drifted further out (or in), snap back to the radius we had at the start of the frame
		// This keeps the bullet flying "parallel" to the curve.
		FVector CorrectedLoc = PlanetCenter + (ToNewLoc.GetSafeNormal() * StartDistToCenter);
		Owner->SetActorLocation(CorrectedLoc);
	}


	// --- F. HEIGHT CORRECTION (Units Only) ---
	if (!bIsFalling && bSurfaceFound && MovementMode != EPcMovementMode::Projectile)
	{
		FVector PostMoveFeet = Owner->GetActorLocation() - (CurrentSurfaceNormal * PivotOffset);
		float CurrentHeight = FVector::DotProduct(PostMoveFeet - SurfaceHitLocation, CurrentSurfaceNormal);
		float TargetHeight = HoverHeight;

		float NextHeight = FMath::FInterpTo(CurrentHeight, TargetHeight, DeltaTime, VerticalSmoothing);
		float Adjustment = NextHeight - CurrentHeight;
		
		if (FMath::Abs(Adjustment) > 0.01f)
		{
			Owner->AddActorWorldOffset(CurrentSurfaceNormal * Adjustment);
		}
	}

	// =========================================================
	//   FIX 2: VELOCITY TRANSPORT
	// =========================================================
	FVector NewLocation = Owner->GetActorLocation();
	FVector EndNormal = StartNormal; 
	if (CurrentPlanet)
	{
		FVector GravityDir = CurrentPlanet->GetGravityDirection(NewLocation);
		if (!GravityDir.IsZero()) EndNormal = -GravityDir;
	}

	// Rotate velocity to match the new surface angle
	if (!StartNormal.Equals(EndNormal, 0.0001f))
	{
		FQuat TransportRot = FQuat::FindBetweenNormals(StartNormal, EndNormal);
		Velocity = TransportRot.RotateVector(Velocity);
	}
	CurrentSurfaceNormal = EndNormal;

	// =========================================================
	//   FIX 3: ROTATION (AIMING)
	// =========================================================
	FQuat TargetRot;
	
	if (MovementMode == EPcMovementMode::Projectile)
	{
		// FIX: Just face the velocity! 
		// Don't try to align to the floor, or you can't shoot up/down.
		if (!Velocity.IsZero())
		{
			TargetRot = FRotationMatrix::MakeFromX(Velocity).ToQuat();
			Owner->SetActorRotation(TargetRot);
		}
	}
	else
	{
		// Characters align to floor + velocity
		FQuat CurrentRot = Owner->GetActorQuat();
		FVector MyUp = Owner->GetActorUpVector();
		
		FQuat AlignRot = FQuat::FindBetweenNormals(MyUp, EndNormal);
		TargetRot = AlignRot * CurrentRot;

		if (TangentVel.SizeSquared() > 100.0f)
		{
			FVector FlatFwd = FVector::VectorPlaneProject(Owner->GetActorForwardVector(), EndNormal).GetSafeNormal();
			FVector FlatVel = TangentVel.GetSafeNormal();
			FQuat FaceVelRot = FQuat::FindBetweenNormals(FlatFwd, FlatVel);
			float TurnAlpha = FMath::Min(1.0f, TurnRate * DeltaTime * 0.01f); 
			TargetRot = FQuat::Slerp(TargetRot, FaceVelRot * TargetRot, TurnAlpha);
		}
		
		Owner->SetActorRotation(FQuat::Slerp(CurrentRot, TargetRot, 15.0f * DeltaTime));
	}
}