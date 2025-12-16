// ==========================================
// FILE: PcProjectile.cpp
// PATH: Source/Project_Circle/PcPlayer/PcProjectile.cpp
// ==========================================
#include "PcProjectile.h"
#include "Project_Circle/GravitySystem/PcGravityMovementComponent.h"
#include "Project_Circle/GravitySystem/PcGravityZone.h" 
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Project_Circle/Enemy/PcEnemyTurret.h"
#include "Project_Circle/PcPlayer/PcPlayerCharacter.h"
#include "Project_Circle/MusicSystem/MusicGameplaySystem/PcMusicAnalysisSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"

APcProjectile::APcProjectile()
{
	PrimaryActorTick.bCanEverTick = true;

	CollisionComp = CreateDefaultSubobject<USphereComponent>(TEXT("SphereComp"));
	CollisionComp->InitSphereRadius(15.0f);
	CollisionComp->SetCollisionProfileName("OverlapAllDynamic"); 
	CollisionComp->OnComponentBeginOverlap.AddDynamic(this, &APcProjectile::OnOverlap);
	RootComponent = CollisionComp;

	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
	MeshComp->SetupAttachment(CollisionComp);
	MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	MovementComp = CreateDefaultSubobject<UPcGravityMovementComponent>(TEXT("MovementComp"));
}

void APcProjectile::BeginPlay()
{
	Super::BeginPlay();
	
	if (UWorld* World = GetWorld())
	{
		if (UPcMusicAnalysisSubsystem* MusicSys = World->GetSubsystem<UPcMusicAnalysisSubsystem>())
		{
			// Get initial state
			CurrentBPM = MusicSys->GetCurrentBPM();
			if (CurrentBPM <= 0.1f) CurrentBPM = 120.0f;
			CurrentBeatDuration = 60.0f / CurrentBPM;

			MusicSys->OnBeatTriggered.AddDynamic(this, &APcProjectile::OnBeatTriggered);
		}
	}
}

void APcProjectile::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		if (UPcMusicAnalysisSubsystem* MusicSys = World->GetSubsystem<UPcMusicAnalysisSubsystem>())
		{
			MusicSys->OnBeatTriggered.RemoveDynamic(this, &APcProjectile::OnBeatTriggered);
		}
	}
	Super::EndPlay(EndPlayReason);
}

void APcProjectile::InitializeProjectile(FVector ShootDirection, APcPlanet* InPlanet, bool bIsPlayerOwned, float InRingSpacing, int32 InMaxRings)
{
	bIsPlayerProjectile = bIsPlayerOwned;
	
	// --- FIND GRAVITY ZONE ---
	TArray<AActor*> FoundZones;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), APcGravityZone::StaticClass(), FoundZones);
	if (FoundZones.Num() > 0)
	{
		CurrentZone = Cast<APcGravityZone>(FoundZones[0]);
	}

	// --- RHYTHM MODE ---
	if (InRingSpacing > 0.0f)
	{
		bIsRhythmic = true;
		BaseRingSpacing = InRingSpacing; // Store BASE value
		MaxRingIndex = InMaxRings; // Store Limit
		CurrentRingIndex = 0;
		RingOffset = 0.0f;
		
		OriginLocation = GetActorLocation();
		FireTangent = ShootDirection.GetSafeNormal();
		
		// Determine Initial Up Vector
		StartGravityNormal = FVector::UpVector;
		if (CurrentZone)
		{
			FVector GravityDir = CurrentZone->GetGravityDirection(OriginLocation);
			if (!GravityDir.IsZero()) StartGravityNormal = -GravityDir;
			PlanetCenter = CurrentZone->GetActorLocation();
		}
		else
		{
			PlanetCenter = OriginLocation - (StartGravityNormal * 10000.0f);
		}

		ReferenceRadius = FVector::Dist(OriginLocation, PlanetCenter);

		// --- ANTI-CLUMPING OFFSET ---
		// Determine where we are in the global beat cycle. 
		// This ensures bullets fired off-beat STAY off-beat relative to each other.
		if (CurrentBeatDuration > 0.0f)
		{
			float CurrentTime = GetWorld()->GetTimeSeconds();
			// Phase 0.0 to 1.0
			RingOffset = FMath::Fmod(CurrentTime, CurrentBeatDuration) / CurrentBeatDuration;
		}

		// Disable Physics Component
		if (MovementComp) MovementComp->SetComponentTickEnabled(false); 
	}
	else
	{
		// --- STANDARD MODE ---
		bIsRhythmic = false;
		if (MovementComp)
		{
			MovementComp->MovementMode = EPcMovementMode::Projectile;
			MovementComp->MaxSpeed = 10000.0f; 
			if (CollisionComp) MovementComp->PivotOffset = CollisionComp->GetScaledSphereRadius();
			MovementComp->HoverHeight = 0.0f; 

			float ExtraSpeed = 0.0f;
			if (auto* PC = Cast<APcPlayerCharacter>(GetOwner()))
			{
				if (PC->GravityComp) ExtraSpeed = PC->GravityComp->GetCurrentVelocity().Size();
			}

			MovementComp->SetVelocity(ShootDirection.GetSafeNormal() * (Speed + ExtraSpeed));
		}
	}
}

void APcProjectile::OnBeatTriggered(float BeatTimestamp)
{
	if (!bIsRhythmic) return;

	if (UWorld* World = GetWorld())
	{
		if (UPcMusicAnalysisSubsystem* MusicSys = World->GetSubsystem<UPcMusicAnalysisSubsystem>())
		{
			float NewBPM = MusicSys->GetCurrentBPM();
			if (NewBPM > 0.1f) 
			{
				CurrentBPM = NewBPM;
				CurrentBeatDuration = 60.0f / CurrentBPM;
			}
		}
	}

	// Advance Ring
	CurrentRingIndex++;
	
	// --- ARENA LIMIT CHECK ---
	// If we exceed the arena size, destroy.
	if (MaxRingIndex > 0 && CurrentRingIndex > MaxRingIndex)
	{
		Destroy();
		return;
	}

	// Reset Timer (Visuals start sliding from New Ring Start)
	TimeSinceLastBeat = 0.0f;
}

void APcProjectile::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	TimeAlive += DeltaTime;
	if (TimeAlive > LifeSpan) 
	{
		Destroy();
		return;
	}

	if (bIsRhythmic)
	{
		TimeSinceLastBeat += DeltaTime;
		
		// 1. Calculate Progression Alpha (0.0 to 1.0)
		float RawAlpha = FMath::Clamp(TimeSinceLastBeat / CurrentBeatDuration, 0.0f, 1.0f);
		float CurveAlpha = 0.0f;

		// --- VARIABLE INTERPOLATION (Snap vs Slide) ---
		if (CurrentBPM > FrenzyThresholdBPM)
		{
			// FRENZY MODE: The Snap
			// Stays at 0 until the very end, then snaps.
			CurveAlpha = FMath::Pow(RawAlpha, 5.0f);
		}
		else
		{
			// CHILL MODE: The Slide
			// Smooth Ease Out (Fast start, slow stop)
			CurveAlpha = FMath::InterpEaseOut(0.0f, 1.0f, RawAlpha, 2.0f);
		}

		// --- ELASTIC ARENA (Spacing) ---
		// High BPM = Small Spacing. Low BPM = Wide Spacing.
		float SpacingScale = ReferenceBPM / FMath::Max(60.0f, CurrentBPM);
		SpacingScale = FMath::Clamp(SpacingScale, 0.5f, 1.0f); 

		float DynamicSpacing = BaseRingSpacing * SpacingScale;

		// --- APPLY RING OFFSET ---
		// This keeps the bullets spaced out even if they move on the same beat trigger.
		// If RingOffset is 0.5, we are physically halfway between rings.
		float StartRad = (CurrentRingIndex - 1 + RingOffset) * DynamicSpacing;
		float EndRad   = (CurrentRingIndex + RingOffset) * DynamicSpacing;

		// Prevent negative radius on spawn
		if (CurrentRingIndex == 0) StartRad = RingOffset * DynamicSpacing;

		float CurrentArcDist = FMath::Lerp(StartRad, EndRad, CurveAlpha);
		CurrentArcDist = FMath::Max(0.0f, CurrentArcDist);

		// 2. Ideal Position Calculation (The Projector)
		// Theta = Arc / Radius
		float AngleRad = CurrentArcDist / ReferenceRadius;
		float AngleDeg = FMath::RadiansToDegrees(AngleRad);

		FVector RotationAxis = FVector::CrossProduct(StartGravityNormal, FireTangent).GetSafeNormal();
		FVector InitialRadiusVector = OriginLocation - PlanetCenter;
		FVector NewRadiusVector = InitialRadiusVector.RotateAngleAxis(AngleDeg, RotationAxis);
		FVector IdealPos = PlanetCenter + NewRadiusVector;
		
		// 3. Surface Snap (The Reality Check)
		FVector SnapPos = IdealPos;
		FVector FinalUp = NewRadiusVector.GetSafeNormal(); 

		if (CurrentZone)
		{
			FVector GravityDir = CurrentZone->GetGravityDirection(IdealPos);
			if (!GravityDir.IsZero()) FinalUp = -GravityDir;

			FVector TraceStart = IdealPos + (FinalUp * 1000.0f);
			FVector TraceEnd   = IdealPos - (FinalUp * 1000.0f);

			FHitResult Hit;
			FCollisionQueryParams P; P.AddIgnoredActor(this);
			
			if (GetWorld()->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_WorldStatic, P))
			{
				SnapPos = Hit.Location + (FinalUp * HoverHeight);
				FinalUp = Hit.ImpactNormal; 
			}
		}

		// 4. Orientation
		FVector ForwardVec = FVector::CrossProduct(RotationAxis, FinalUp);

		SetActorLocation(SnapPos);
		SetActorRotation(FRotationMatrix::MakeFromXZ(ForwardVec, FinalUp).Rotator());
	}
}

void APcProjectile::OnOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!OtherActor || OtherActor == this || OtherActor == GetOwner()) return;

	if (bIsPlayerProjectile)
	{
		if (auto* Enemy = Cast<APcEnemyTurret>(OtherActor))
		{
			if (MovementComp && Enemy->GravityComp)
			{
				FVector ImpactDir = GetActorForwardVector();
				Enemy->GravityComp->AddImpulse(ImpactDir * 2000.0f);
			}
			Destroy();
		}
	}
	else
	{
		if (auto* Player = Cast<APcPlayerCharacter>(OtherActor))
		{
			Player->TakeHit(); 
			Destroy();
		}
	}
}