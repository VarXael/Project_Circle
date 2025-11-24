#include "PcProjectile.h"
#include "Project_Circle/Planet/PcPlanet.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/KismetMathLibrary.h"

APcProjectile::APcProjectile()
{
	PrimaryActorTick.bCanEverTick = true;

	CollisionComp = CreateDefaultSubobject<USphereComponent>(TEXT("SphereComp"));
	CollisionComp->InitSphereRadius(20.0f);
	CollisionComp->SetCollisionProfileName("OverlapAllDynamic"); 
	CollisionComp->OnComponentBeginOverlap.AddDynamic(this, &APcProjectile::OnOverlap);
	RootComponent = CollisionComp;

	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
	MeshComp->SetupAttachment(CollisionComp);
	MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void APcProjectile::InitializeProjectile(FVector ShootDirection, APcPlanet* InPlanet)
{
	CurrentPlanet = InPlanet;
	Velocity = ShootDirection.GetSafeNormal() * Speed;
	bIsAirborne = true;
}

void APcProjectile::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	TimeAlive += DeltaTime;
	if (TimeAlive > LifeSpan) { Destroy(); return; }

	if (bIsAirborne) HandleAirMovement(DeltaTime);
	else HandleSurfaceMovement(DeltaTime);

	if (!Velocity.IsZero()) SetActorRotation(Velocity.Rotation());
}

void APcProjectile::HandleAirMovement(float DeltaTime)
{
	FVector Location = GetActorLocation();
	FVector GravityDir = (CurrentPlanet) ? CurrentPlanet->GetGravityDirection(Location) : FVector(0,0,-1);

	Velocity += GravityDir * GravityStrength * DeltaTime;

	FVector MoveDelta = Velocity * DeltaTime;
	FHitResult Hit; FCollisionQueryParams P; P.AddIgnoredActor(this); P.AddIgnoredActor(GetOwner());

	bool bHit = GetWorld()->SweepSingleByChannel(Hit, Location, Location + MoveDelta, FQuat::Identity, ECC_WorldStatic, FCollisionShape::MakeSphere(10.0f), P);

	if (bHit)
	{
		bIsAirborne = false;
		SetActorLocation(Hit.Location + (Hit.Normal * HoverHeight));
		// Flatten velocity to slide
		Velocity = FVector::VectorPlaneProject(Velocity, Hit.Normal).GetSafeNormal() * Speed;
	}
	else
	{
		AddActorWorldOffset(MoveDelta);
	}
}

void APcProjectile::HandleSurfaceMovement(float DeltaTime)
{
	FVector Location = GetActorLocation();
	FVector SurfaceNormal = FVector::UpVector;

	if (CurrentPlanet) SurfaceNormal = -CurrentPlanet->GetGravityDirection(Location);
	else
	{
		FHitResult Hit; FCollisionQueryParams P; P.AddIgnoredActor(this);
		if (GetWorld()->LineTraceSingleByChannel(Hit, Location + FVector(0,0,50), Location - FVector(0,0,100), ECC_WorldStatic, P))
			SurfaceNormal = Hit.Normal;
	}

	Velocity = FVector::VectorPlaneProject(Velocity, SurfaceNormal).GetSafeNormal() * Speed;
	Location += Velocity * DeltaTime;

	if (CurrentPlanet)
	{
		FVector ToCenter = Location - CurrentPlanet->GetActorLocation();
		float TargetDist = CurrentPlanet->SurfaceRadius + HoverHeight;
		Location = CurrentPlanet->GetActorLocation() + (ToCenter.GetSafeNormal() * TargetDist);
	}

	SetActorLocation(Location);
}

void APcProjectile::OnOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (OtherActor && OtherActor != this && OtherActor != GetOwner())
	{
		if (!OtherActor->IsA(APawn::StaticClass())) return; // Only destroy on hitting pawns
		Destroy();
	}
}