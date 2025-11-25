#include "PcProjectile.h"
#include "Project_Circle/Planet/PcPlanet.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "PcPlayerCharacter.h"

APcProjectile::APcProjectile()
{
	PrimaryActorTick.bCanEverTick = true;

	CollisionComp = CreateDefaultSubobject<USphereComponent>(TEXT("SphereComp"));
	CollisionComp->InitSphereRadius(15.0f);
	// Vital: Use OverlapAllDynamic so we don't get stuck in the floor
	CollisionComp->SetCollisionProfileName("OverlapAllDynamic"); 
	CollisionComp->OnComponentBeginOverlap.AddDynamic(this, &APcProjectile::OnOverlap);
	RootComponent = CollisionComp;

	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
	MeshComp->SetupAttachment(CollisionComp);
	MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void APcProjectile::InitializeProjectile(FVector ShootDirection, APcPlanet* InPlanet, bool bIsPlayerOwned)
{
	CurrentPlanet = InPlanet;
	
	// Safety Check: Ensure direction isn't zero
	if (ShootDirection.IsZero()) ShootDirection = GetActorForwardVector();
	
	Velocity = ShootDirection.GetSafeNormal() * Speed;
	bIsPlayerProjectile = bIsPlayerOwned;
	bIsAirborne = true;

	// Visual Debug: Draw arrow showing launch
	DrawDebugDirectionalArrow(GetWorld(), GetActorLocation(), GetActorLocation() + Velocity * 0.1f, 20.0f, FColor::Yellow, false, 2.0f);
}

void APcProjectile::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	TimeAlive += DeltaTime;
	if (TimeAlive > LifeSpan) { Destroy(); return; }

	// DEBUG: Visualise Velocity
	// If you see a dot but no line, Speed is 0!
	DrawDebugLine(GetWorld(), GetActorLocation(), GetActorLocation() + Velocity * 0.05f, FColor::Red, false, -1.0f, 0, 2.0f);

	if (bIsAirborne) HandleAirMovement(DeltaTime);
	else HandleSurfaceMovement(DeltaTime);

	// Rotate mesh to face movement
	if (!Velocity.IsZero()) SetActorRotation(Velocity.Rotation());
}

void APcProjectile::HandleAirMovement(float DeltaTime)
{
	FVector Location = GetActorLocation();
	FVector GravityDir = FVector(0, 0, -1); // Default Flat Gravity

	if (CurrentPlanet) GravityDir = CurrentPlanet->GetGravityDirection(Location);

	// Apply Gravity
	Velocity += GravityDir * GravityStrength * DeltaTime;

	// Predict Hit
	FVector MoveDelta = Velocity * DeltaTime;
	FHitResult Hit; 
	FCollisionQueryParams P; 
	P.AddIgnoredActor(this); 
	P.AddIgnoredActor(GetOwner());

	bool bHit = GetWorld()->SweepSingleByChannel(Hit, Location, Location + MoveDelta, FQuat::Identity, ECC_WorldStatic, FCollisionShape::MakeSphere(10.0f), P);

	if (bHit)
	{
		// We hit the floor!
		bIsAirborne = false;
		
		// Snap to surface height
		SetActorLocation(Hit.Location + (Hit.Normal * HoverHeight));

		// CONVERT VELOCITY:
		// We flatten the falling velocity onto the floor so it slides instead of stopping.
		// Result = Forward Sliding Speed.
		Velocity = FVector::VectorPlaneProject(Velocity, Hit.Normal).GetSafeNormal() * Speed;
	}
	else
	{
		// Move freely through air
		AddActorWorldOffset(MoveDelta);
	}
}

void APcProjectile::HandleSurfaceMovement(float DeltaTime)
{
	FVector Location = GetActorLocation();
	FVector SurfaceNormal = FVector::UpVector;

	// 1. Find Surface Normal
	if (CurrentPlanet) 
	{
		SurfaceNormal = -CurrentPlanet->GetGravityDirection(Location);
	}
	else
	{
		// Flat Plane Fallback Trace
		FHitResult Hit; 
		FCollisionQueryParams P; P.AddIgnoredActor(this);
		// Trace down relative to our current Up vector
		FVector TraceStart = Location + (FVector::UpVector * 50.0f);
		FVector TraceEnd = Location - (FVector::UpVector * 100.0f);
		
		if (GetWorld()->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_WorldStatic, P))
		{
			SurfaceNormal = Hit.Normal;
		}
	}

	// 2. Wrap Velocity to surface (The "Slide")
	Velocity = FVector::VectorPlaneProject(Velocity, SurfaceNormal).GetSafeNormal() * Speed;
	Location += Velocity * DeltaTime;

	// 3. Snap to Floor
	if (CurrentPlanet)
	{
		FVector ToCenter = Location - CurrentPlanet->GetActorLocation();
		float TargetDist = CurrentPlanet->SurfaceRadius + HoverHeight;
		Location = CurrentPlanet->GetActorLocation() + (ToCenter.GetSafeNormal() * TargetDist);
	}
	else
	{
		// Flat Plane Snap: Just keep Z relative to the trace we did earlier? 
		// Simpler: Just rely on physics trace correction or assume Z=HoverHeight if strictly flat.
		// For now, let's just let it slide. If it sinks, increase HoverHeight.
	}

	SetActorLocation(Location);
}

void APcProjectile::OnOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// Safety Checks
	if (!OtherActor || OtherActor == this || OtherActor == GetOwner()) return;

	if (bIsPlayerProjectile)
	{
		// Hits Enemy (Future)
		// if (auto* Enemy = Cast<APcEnemyTurret>(OtherActor)) { ... }
	}
	else
	{
		// Hits Player
		if (auto* Player = Cast<APcPlayerCharacter>(OtherActor))
		{
			// HIT CONFIRM
			// Check if player is vulnerable (on ground)
			if (!Player->IsInRhythmWindow()) 
			{
				Player->TakeHit(); // Triggers BP Event
				Destroy();
			}
		}
	}
}