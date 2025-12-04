// ==========================================
// FILE: PcGravityMovementComponent.cpp
// PATH: E:\GameDev\Unreal Engine Projects\Project_Circle\Source\Project_Circle\GravitySystem\PcGravityMovementComponent.cpp
// ==========================================
#include "PcGravityMovementComponent.h"
#include "Project_Circle/GravitySystem/PcGravityZone.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Actor.h"
#include "Components/CapsuleComponent.h"
#include "DrawDebugHelpers.h" 

UPcGravityMovementComponent::UPcGravityMovementComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UPcGravityMovementComponent::BeginPlay()
{
	Super::BeginPlay();

	TArray<AActor*> Zones;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), APcGravityZone::StaticClass(), Zones);
	if (Zones.Num() > 0)
	{
		CurrentZone = Cast<APcGravityZone>(Zones[0]);
	}

	AutoCalibratePivot();
	UpdateSurfaceInfo();
	if (bSurfaceFound && GetOwner())
	{
		FVector SnapPos = SurfaceHitLocation + (CurrentSurfaceNormal * (PivotOffset + HoverHeight));
		GetOwner()->SetActorLocation(SnapPos);
	}
}

void UPcGravityMovementComponent::AutoCalibratePivot()
{
	AActor* Owner = GetOwner();
	if (!Owner) return;

	if (UCapsuleComponent* Capsule = Owner->FindComponentByClass<UCapsuleComponent>())
	{
		PivotOffset = Capsule->GetScaledCapsuleHalfHeight();
		return;
	}

	FVector Origin, BoxExtent;
	Owner->GetActorBounds(true, Origin, BoxExtent);
	PivotOffset = BoxExtent.Z; 
}

void UPcGravityMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!GetOwner()) return;

	float SafeDelta = FMath::Min(DeltaTime, 0.05f);

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
	if (!Impulse.ContainsNaN()) Velocity += Impulse;
}

void UPcGravityMovementComponent::SetVelocity(FVector NewVelocity)
{
	if (!NewVelocity.ContainsNaN()) Velocity = NewVelocity;
}

void UPcGravityMovementComponent::UpdateSurfaceInfo()
{
	AActor* Owner = GetOwner();
	if (!Owner) return;

	FVector MyLoc = Owner->GetActorLocation();
	
	FVector GravityDir = FVector::ZeroVector;
	if (CurrentZone) GravityDir = CurrentZone->GetGravityDirection(MyLoc);

	if (GravityDir.IsZero())
	{
		bInZeroG = true;
		bSurfaceFound = false;
		return; 
	}

	bInZeroG = false;
	CurrentSurfaceNormal = -GravityDir; 

	FVector IdealSurface = CurrentZone->GetIdealSurfaceLocation(MyLoc);
	FVector TraceStart = IdealSurface + (CurrentSurfaceNormal * 1000.0f); 
	FVector TraceEnd = IdealSurface - (CurrentSurfaceNormal * 1000.0f);

	FHitResult Hit;
	FCollisionQueryParams P;
	P.AddIgnoredActor(Owner);

	bSurfaceFound = GetWorld()->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_WorldStatic, P);

	if (bSurfaceFound) SurfaceHitLocation = Hit.Location;
	else
	{
		SurfaceHitLocation = IdealSurface;
		bSurfaceFound = true;
	}
}

void UPcGravityMovementComponent::ApplyPhysics(float DeltaTime)
{
	AActor* Owner = GetOwner();
	FVector StartNormal = CurrentSurfaceNormal;

	// --- A. ANALYZE STATE ---
	float VerticalSpeed = FVector::DotProduct(Velocity, CurrentSurfaceNormal);
	FVector TangentVel = FVector::VectorPlaneProject(Velocity, CurrentSurfaceNormal);

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

	// --- B. HORIZONTAL PHYSICS ---
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

	// --- C. VERTICAL PHYSICS ---
	if (bIsFalling)
	{
		if (bSurfaceFound) VerticalSpeed -= GravityScale * DeltaTime;
	}
	else
	{
		VerticalSpeed = 0.0f; 
	}

	// --- D. INTEGRATE ---
	Velocity = TangentVel + (CurrentSurfaceNormal * VerticalSpeed);
	
	if (Velocity.SizeSquared() > 50000.0f * 50000.0f) Velocity = Velocity.GetSafeNormal() * 50000.0f;

	if (!Velocity.IsZero())
	{
		Owner->AddActorWorldOffset(Velocity * DeltaTime);
	}

	// --- E. ORBITAL LOCK (Bowling Ball) ---
	if (MovementMode == EPcMovementMode::Projectile && CurrentZone && bUseFixedRadius && FixedRadius > 0.0f && !bInZeroG)
	{
		FVector NewLoc = Owner->GetActorLocation();
		FVector Center = CurrentZone->GetZoneCenter();
		FVector ToNewLoc = NewLoc - Center;
		
		FVector CorrectedLoc = Center + (ToNewLoc.GetSafeNormal() * FixedRadius);
		Owner->SetActorLocation(CorrectedLoc);
	}

	// --- F. HEIGHT SMOOTHING ---
	if (!bIsFalling && bSurfaceFound && !bInZeroG && MovementMode != EPcMovementMode::Projectile)
	{
		FVector PostMoveFeet = Owner->GetActorLocation() - (CurrentSurfaceNormal * PivotOffset);
		float CurrentHeight = FVector::DotProduct(PostMoveFeet - SurfaceHitLocation, CurrentSurfaceNormal);
		float TargetHeight = HoverHeight;

		float NextHeight = 0.0f;
		if (bSnapToHoverHeight) NextHeight = TargetHeight;
		else NextHeight = FMath::FInterpTo(CurrentHeight, TargetHeight, DeltaTime, VerticalSmoothing);
		
		float Adjustment = NextHeight - CurrentHeight;
		if (FMath::Abs(Adjustment) > 0.01f) Owner->AddActorWorldOffset(CurrentSurfaceNormal * Adjustment);
	}

	// --- G. ROTATION ---
	if (bSurfaceFound && !bInZeroG) 
	{
		// 1. Recalculate Normal
		FVector NewLocation = Owner->GetActorLocation();
		FVector EndNormal = StartNormal; 
		
		if (CurrentZone)
		{
			FVector G = CurrentZone->GetGravityDirection(NewLocation);
			if (!G.IsZero()) EndNormal = -G;
		}
		CurrentSurfaceNormal = EndNormal;

		// 2. CALCULATE FLOOR DELTA (How much did the world curve?)
		FQuat FloorDelta = FQuat::Identity;
		if ((StartNormal | EndNormal) > -0.99f)
		{
			FloorDelta = FQuat::FindBetweenNormals(StartNormal, EndNormal);
		}

		// 3. TRANSPORT VELOCITY (Keep momentum tangential)
		if (!FloorDelta.IsIdentity())
		{
			Velocity = FloorDelta.RotateVector(Velocity);
		}

		// 4. ALIGN ACTOR (Drift Fix: MakeFromXZ)
		// We reconstruct a clean orientation based on Forward and Up to eliminate Yaw drift.
		FVector CurrentFwd = Owner->GetActorForwardVector();
		FVector ConstrainedFwd = FVector::VectorPlaneProject(CurrentFwd, EndNormal).GetSafeNormal();
		
		if (ConstrainedFwd.IsZero()) ConstrainedFwd = Owner->GetActorForwardVector();

		FMatrix TargetMatrix = FRotationMatrix::MakeFromXZ(ConstrainedFwd, EndNormal);
		FQuat TargetRot = TargetMatrix.ToQuat();

		// Face Velocity (AI/Projectiles)
		if (bOrientRotationToMovement && TangentVel.SizeSquared() > 100.0f)
		{
			FVector FlatVel = TangentVel.GetSafeNormal();
			if (!FlatVel.IsZero())
			{
				FMatrix VelMatrix = FRotationMatrix::MakeFromXZ(FlatVel, EndNormal);
				TargetRot = VelMatrix.ToQuat();
			}
		}
		
		Owner->SetActorRotation(FQuat::Slerp(Owner->GetActorQuat(), TargetRot, RotationInterpSpeed * DeltaTime));
		
		// FIX: Removed CAMERA ALIGNMENT (Hard Lock)
		// We no longer manually rotate the PC controller. The camera is now attached directly 
		// to the pawn via the SpringArm, so it follows Step 4 automatically.
	}
}