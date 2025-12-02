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
	
	// DEBUG VISUALIZATION
	if (bDrawDebug)
	{
		DrawDebugLine(GetWorld(), Owner->GetActorLocation(), Owner->GetActorLocation() + (Velocity * 0.5f), FColor::Yellow, false, -1, 0, 3.0f);
		DrawDebugLine(GetWorld(), Owner->GetActorLocation(), Owner->GetActorLocation() + (CurrentSurfaceNormal * 100.0f), FColor::Blue, false, -1, 0, 1.0f);
	}

	// --- A. DECOMPOSE ---
	float VerticalSpeed = FVector::DotProduct(Velocity, CurrentSurfaceNormal);
	FVector TangentVel = FVector::VectorPlaneProject(Velocity, CurrentSurfaceNormal);

	// --- B. CHECK FALLING ---
	// If Surface Not Found (e.g. Center Void), we are falling/floating.
	if (!bSurfaceFound)
	{
		bIsFalling = true;
	}
	else
	{
		FVector FeetLocation = Owner->GetActorLocation() - (CurrentSurfaceNormal * PivotOffset);
		float DistToFloor = FVector::DotProduct(FeetLocation - SurfaceHitLocation, CurrentSurfaceNormal);
		DistToFloor -= HoverHeight;

		// Falling Hysteresis
		if (DistToFloor > SnapDistance) bIsFalling = true;
		else if (DistToFloor <= SnapDistance && VerticalSpeed <= 0.0f) bIsFalling = false;
	}

	// --- C. APPLY FORCES (Horizontal/Tangent) ---
	FVector TangentInput = FVector::VectorPlaneProject(CurrentInput, CurrentSurfaceNormal).GetSafeNormal();
	
	// Allow movement logic if Skater/Unit
	if (MovementMode == EPcMovementMode::Skater || MovementMode == EPcMovementMode::GroundUnit)
	{
		if (!TangentInput.IsZero())
		{
			TangentVel += TangentInput * Acceleration * DeltaTime;
		}
		else 
		{
			// Only apply friction if Grounded!
			// If in Void (Falling), no friction = drift.
			if (!bIsFalling)
			{
				float Speed = TangentVel.Size();
				float Drop = Deceleration * DeltaTime;
				float NewSpeed = FMath::Max(0.0f, Speed - Drop);
				if (Speed > 0) TangentVel *= (NewSpeed / Speed);
			}
		}
	}
	
	if (TangentVel.SizeSquared() > MaxSpeed * MaxSpeed)
	{
		TangentVel = TangentVel.GetSafeNormal() * MaxSpeed;
	}

	// --- D. APPLY FORCES (Vertical/Gravity) ---
	if (MovementMode != EPcMovementMode::Projectile)
	{
		if (bIsFalling)
		{
			// If we are in the Void (Surface Not Found), disable Gravity!
			// Otherwise they fall infinitely "Down" relative to themselves.
			if (bSurfaceFound)
			{
				VerticalSpeed -= GravityScale * DeltaTime;
			}
		}
		else
		{
			VerticalSpeed = 0.0f; // Grounded = Stick
		}
	}

	// --- E. MOVE ---
	Velocity = TangentVel + (CurrentSurfaceNormal * VerticalSpeed);
	if (!Velocity.IsZero())
	{
		Owner->AddActorWorldOffset(Velocity * DeltaTime);
	}

	// --- F. HEIGHT CORRECTION (Grounded Only) ---
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

	// --- G. ROTATION ---
	// If in Void, don't force rotation alignment (prevents jitter), just maintain.
	if (bSurfaceFound)
	{
		FQuat CurrentRot = Owner->GetActorQuat();
		FVector MyUp = Owner->GetActorUpVector();
		
		FQuat AlignRot = FQuat::FindBetweenNormals(MyUp, CurrentSurfaceNormal);
		FQuat TargetRot = AlignRot * CurrentRot;

		// Face Velocity (only if moving fast enough)
		if ((MovementMode == EPcMovementMode::Skater || MovementMode == EPcMovementMode::GroundUnit) 
			&& TangentVel.SizeSquared() > 100.0f)
		{
			FVector CurrentFlatFwd = FVector::VectorPlaneProject(Owner->GetActorForwardVector(), CurrentSurfaceNormal).GetSafeNormal();
			FVector TargetFwd = TangentVel.GetSafeNormal();
			
			// Safety against zero vectors
			if (!CurrentFlatFwd.IsZero() && !TargetFwd.IsZero())
			{
				FQuat FaceVelRot = FQuat::FindBetweenNormals(CurrentFlatFwd, TargetFwd);
				float TurnAlpha = FMath::Min(1.0f, TurnRate * DeltaTime * 0.01f); 
				TargetRot = FQuat::Slerp(TargetRot, FaceVelRot * TargetRot, TurnAlpha);
			}
		}

		Owner->SetActorRotation(FQuat::Slerp(CurrentRot, TargetRot, 15.0f * DeltaTime));
		
		// --- H. TRANSPORT VELOCITY ---
		FVector NewUp = Owner->GetActorUpVector();
		if (!CurrentSurfaceNormal.Equals(NewUp, 0.01f))
		{
			FQuat TransportRot = FQuat::FindBetweenNormals(CurrentSurfaceNormal, NewUp);
			Velocity = TransportRot.RotateVector(Velocity);
		}
	}
}