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

	// SAFETY: Sanitize Inputs
	if (Velocity.ContainsNaN()) Velocity = FVector::ZeroVector;
	if (CurrentInput.ContainsNaN()) CurrentInput = FVector::ZeroVector;

	UpdateSurfaceInfo();
	ApplyPhysics(SafeDelta);

	CurrentInput = FVector::ZeroVector;
}

void UPcGravityMovementComponent::AddInputVector(FVector WorldInputDirection)
{
	if (!WorldInputDirection.IsZero() && !WorldInputDirection.ContainsNaN())
	{
		CurrentInput = WorldInputDirection.GetSafeNormal();
	}
}

void UPcGravityMovementComponent::AddImpulse(FVector Impulse)
{
	if (!Impulse.ContainsNaN())
	{
		Velocity += Impulse;
	}
}

void UPcGravityMovementComponent::SetVelocity(FVector NewVelocity)
{
	if (!NewVelocity.ContainsNaN())
	{
		Velocity = NewVelocity;
	}
}

void UPcGravityMovementComponent::LockCurrentAltitudeAsOrbit()
{
	if (CurrentPlanet && GetOwner())
	{
		FixedRadius = FVector::Dist(GetOwner()->GetActorLocation(), CurrentPlanet->GetActorLocation());
		bUseFixedRadius = true;
	}
}

void UPcGravityMovementComponent::UpdateSurfaceInfo()
{
	AActor* Owner = GetOwner();
	if (!Owner) return;

	FVector MyLoc = Owner->GetActorLocation();
	FVector PlanetCenter = FVector::ZeroVector;
	float PlanetRadius = 10000.0f; 

	if (CurrentPlanet) 
	{
		PlanetCenter = CurrentPlanet->GetActorLocation();
		PlanetRadius = CurrentPlanet->SurfaceRadius;
	}

	// 1. DETERMINE STATE
	FVector FromCenter = MyLoc - PlanetCenter;
	float DistToCenter = FromCenter.Size();
	float VoidThreshold = PlanetRadius * 0.5f;

	if (DistToCenter < VoidThreshold)
	{
		// VOID STATE (Inside Center)
		bSurfaceFound = false;
		
		// Inertial Coasting: Keep normal consistent to prevent flipping
		if (DistToCenter > 10.0f)
		{
			CurrentSurfaceNormal = -(FromCenter / DistToCenter);
		}
		return;
	}

	// 2. SHELL STATE (Near Surface)
	FVector DirOutwards = FromCenter / DistToCenter;
	CurrentSurfaceNormal = -DirOutwards; // Points Inwards

	// Trace from Center -> Outwards
	FVector TraceStart = PlanetCenter + (DirOutwards * VoidThreshold); 
	FVector TraceEnd = PlanetCenter + (DirOutwards * (PlanetRadius * 1.5f));

	FHitResult Hit;
	FCollisionQueryParams P;
	P.AddIgnoredActor(Owner);

	bSurfaceFound = GetWorld()->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_WorldStatic, P);

	if (bSurfaceFound)
	{
		SurfaceHitLocation = Hit.Location;
	}
	else
	{
		SurfaceHitLocation = PlanetCenter + (DirOutwards * PlanetRadius);
		bSurfaceFound = true; // Fallback
	}
}

void UPcGravityMovementComponent::ApplyPhysics(float DeltaTime)
{
	AActor* Owner = GetOwner();
	FVector StartNormal = CurrentSurfaceNormal;

	// --- A. DECOMPOSE ---
	float VerticalSpeed = FVector::DotProduct(Velocity, CurrentSurfaceNormal);
	FVector TangentVel = FVector::VectorPlaneProject(Velocity, CurrentSurfaceNormal);

	// --- B. CHECK FALLING (Modified for Jump Sync) ---
	// If the script is forcing a specific height (Jumping), we are NOT falling.
	// We are on a "Rail". This prevents Gravity from fighting the Sine Wave.
	if (bSnapToHoverHeight && bSurfaceFound)
	{
		bIsFalling = false;
	}
	else if (!bSurfaceFound)
	{
		bIsFalling = true; // Void State
	}
	else
	{
		FVector FeetLocation = Owner->GetActorLocation() - (CurrentSurfaceNormal * PivotOffset);
		float DistToFloor = FVector::DotProduct(FeetLocation - SurfaceHitLocation, CurrentSurfaceNormal);
		DistToFloor -= HoverHeight;

		if (DistToFloor > SnapDistance) bIsFalling = true;
		else if (DistToFloor <= SnapDistance && VerticalSpeed <= 0.0f) bIsFalling = false;
	}

	// --- C. HORIZONTAL PHYSICS ---
	FVector TangentInput = FVector::VectorPlaneProject(CurrentInput, CurrentSurfaceNormal).GetSafeNormal();
	
	if (MovementMode == EPcMovementMode::Skater || MovementMode == EPcMovementMode::GroundUnit)
	{
		if (!TangentInput.IsZero())
		{
			TangentVel += TangentInput * Acceleration * DeltaTime;
		}
		else if (!bIsFalling) 
		{
			// Friction (Only on ground)
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

	// --- D. VERTICAL PHYSICS ---
	if (MovementMode != EPcMovementMode::Projectile)
	{
		if (bIsFalling)
		{
			if (bSurfaceFound) VerticalSpeed -= GravityScale * DeltaTime;
		}
		else
		{
			VerticalSpeed = 0.0f; // Grounded = No vertical momentum
		}
	}

	// --- E. INTEGRATE ---
	Velocity = TangentVel + (CurrentSurfaceNormal * VerticalSpeed);
	
	if (Velocity.SizeSquared() > 50000.0f * 50000.0f) Velocity = Velocity.GetSafeNormal() * 50000.0f;

	if (!Velocity.IsZero())
	{
		Owner->AddActorWorldOffset(Velocity * DeltaTime);
	}

	// --- F. ORBITAL CORRECTION (Projectile Only) ---
	if (MovementMode == EPcMovementMode::Projectile && CurrentPlanet)
	{
		if (bUseFixedRadius && FixedRadius > 0.0f)
		{
			FVector NewLoc = Owner->GetActorLocation();
			FVector PlanetCenter = CurrentPlanet->GetActorLocation();
			FVector ToNewLoc = NewLoc - PlanetCenter;
			
			// Hard Snap to Fixed Radius
			FVector CorrectedLoc = PlanetCenter + (ToNewLoc.GetSafeNormal() * FixedRadius);
			Owner->SetActorLocation(CorrectedLoc);
		}
	}

	// --- G. HEIGHT SMOOTHING (Grounded Only) ---
	if (!bIsFalling && bSurfaceFound && MovementMode != EPcMovementMode::Projectile)
	{
		FVector PostMoveFeet = Owner->GetActorLocation() - (CurrentSurfaceNormal * PivotOffset);
		float CurrentHeight = FVector::DotProduct(PostMoveFeet - SurfaceHitLocation, CurrentSurfaceNormal);
		float TargetHeight = HoverHeight;

		float NextHeight = 0.0f;

		// DIRECT DRIVE (Jump Logic)
		if (bSnapToHoverHeight)
		{
			NextHeight = TargetHeight; // Instant Snap (No Lag)
		}
		else
		{
			NextHeight = FMath::FInterpTo(CurrentHeight, TargetHeight, DeltaTime, VerticalSmoothing); // Smooth Water Feel
		}
		
		float Adjustment = NextHeight - CurrentHeight;
		
		// Sanity Check
		if (FMath::Abs(Adjustment) > 0.01f && FMath::Abs(Adjustment) < 500.0f)
		{
			Owner->AddActorWorldOffset(CurrentSurfaceNormal * Adjustment);
		}
	}

	// --- H. ROTATION ---
	if (bSurfaceFound) 
	{
		FVector NewLocation = Owner->GetActorLocation();
		FVector EndNormal = StartNormal; 
		
		if (CurrentPlanet)
		{
			FVector FromCenter = NewLocation - CurrentPlanet->GetActorLocation();
			float D = FromCenter.Size();
			if (D > 10.0f) EndNormal = -(FromCenter / D);
		}
		CurrentSurfaceNormal = EndNormal;

		// Velocity Transport
		if (!StartNormal.Equals(EndNormal, 0.0001f) && (StartNormal | EndNormal) > -0.99f)
		{
			FQuat TransportRot = FQuat::FindBetweenNormals(StartNormal, EndNormal);
			Velocity = TransportRot.RotateVector(Velocity);
		}

		// Actor Rotation
		FQuat TargetRot;
		if (MovementMode == EPcMovementMode::Projectile)
		{
			if (!Velocity.IsZero())
			{
				TargetRot = FRotationMatrix::MakeFromX(Velocity).ToQuat();
				Owner->SetActorRotation(TargetRot);
			}
		}
		else
		{
			FQuat CurrentRot = Owner->GetActorQuat();
			FVector MyUp = Owner->GetActorUpVector();
			
			FQuat AlignRot = FQuat::Identity;
			if ((MyUp | EndNormal) > -0.99f) AlignRot = FQuat::FindBetweenNormals(MyUp, EndNormal);
			TargetRot = AlignRot * CurrentRot;

			if (bOrientRotationToMovement && TangentVel.SizeSquared() > 100.0f)
			{
				FVector CurrentFlatFwd = FVector::VectorPlaneProject(Owner->GetActorForwardVector(), EndNormal).GetSafeNormal();
				FVector FlatVel = TangentVel.GetSafeNormal();
				if (!CurrentFlatFwd.IsZero() && !FlatVel.IsZero())
				{
					FQuat FaceVelRot = FQuat::FindBetweenNormals(CurrentFlatFwd, FlatVel);
					float TurnAlpha = FMath::Min(1.0f, TurnRate * DeltaTime * 0.01f); 
					TargetRot = FQuat::Slerp(TargetRot, FaceVelRot * TargetRot, TurnAlpha);
				}
			}
			Owner->SetActorRotation(FQuat::Slerp(CurrentRot, TargetRot, 15.0f * DeltaTime));
		}
	}
}