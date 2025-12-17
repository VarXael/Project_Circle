// ==========================================
// FILE: PcProjectile.cpp
// PATH: Source/Project_Circle/PcPlayer/PcProjectile.cpp
// ==========================================
#include "PcProjectile.h"
#include "Project_Circle/GravitySystem/PcGravityMovementComponent.h"
#include "Project_Circle/GravitySystem/PcGravityZone.h" 
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "FlowSystem/PcFloatingScore.h"
#include "Project_Circle/Enemy/PcEnemyBase.h"
#include "Project_Circle/PcPlayer/PcPlayerCharacter.h"
#include "Project_Circle/PcPlayer/FlowSystem/PcFlowMechanicComponent.h"
#include "Project_Circle/MusicSystem/MusicGameplaySystem/PcMusicAnalysisSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"

// ... (Constructor, BeginPlay, etc. omitted - NO CHANGES there) ...
// ... (InitializeProjectile, OnBeatTriggered, Tick omitted - NO CHANGES there) ...

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

void APcProjectile::BeginPlay() { Super::BeginPlay(); if (UWorld* World = GetWorld()) { if (UPcMusicAnalysisSubsystem* MusicSys = World->GetSubsystem<UPcMusicAnalysisSubsystem>()) { CurrentBPM = MusicSys->GetCurrentBPM(); if (CurrentBPM <= 0.1f) CurrentBPM = 120.0f; CurrentBeatDuration = 60.0f / CurrentBPM; MusicSys->OnBeatTriggered.AddDynamic(this, &APcProjectile::OnBeatTriggered); } } }

void APcProjectile::EndPlay(const EEndPlayReason::Type EndPlayReason) { if (UWorld* World = GetWorld()) { if (UPcMusicAnalysisSubsystem* MusicSys = World->GetSubsystem<UPcMusicAnalysisSubsystem>()) { MusicSys->OnBeatTriggered.RemoveDynamic(this, &APcProjectile::OnBeatTriggered); } } Super::EndPlay(EndPlayReason); }

void APcProjectile::InitializeProjectile(FVector ShootDirection, APcPlanet* InPlanet, bool bIsPlayerOwned, float InRingSpacing, int32 InMaxRings) { bIsPlayerProjectile = bIsPlayerOwned; TArray<AActor*> FoundZones; UGameplayStatics::GetAllActorsOfClass(GetWorld(), APcGravityZone::StaticClass(), FoundZones); if (FoundZones.Num() > 0) { CurrentZone = Cast<APcGravityZone>(FoundZones[0]); } if (InRingSpacing > 0.0f) { bIsRhythmic = true; BaseRingSpacing = InRingSpacing; MaxRingIndex = InMaxRings; CurrentRingIndex = 0; RingOffset = 0.0f; OriginLocation = GetActorLocation(); FireTangent = ShootDirection.GetSafeNormal(); StartGravityNormal = FVector::UpVector; if (CurrentZone) { FVector GravityDir = CurrentZone->GetGravityDirection(OriginLocation); if (!GravityDir.IsZero()) StartGravityNormal = -GravityDir; PlanetCenter = CurrentZone->GetActorLocation(); } else { PlanetCenter = OriginLocation - (StartGravityNormal * 10000.0f); } ReferenceRadius = FVector::Dist(OriginLocation, PlanetCenter); if (CurrentBeatDuration > 0.0f) { float CurrentTime = GetWorld()->GetTimeSeconds(); RingOffset = FMath::Fmod(CurrentTime, CurrentBeatDuration) / CurrentBeatDuration; } if (MovementComp) MovementComp->SetComponentTickEnabled(false); } else { bIsRhythmic = false; if (MovementComp) { MovementComp->MovementMode = EPcMovementMode::Projectile; MovementComp->MaxSpeed = 10000.0f; if (CollisionComp) MovementComp->PivotOffset = CollisionComp->GetScaledSphereRadius(); MovementComp->HoverHeight = 0.0f; float ExtraSpeed = 0.0f; if (auto* PC = Cast<APcPlayerCharacter>(GetOwner())) { if (PC->GravityComp) ExtraSpeed = PC->GravityComp->GetCurrentVelocity().Size(); } MovementComp->SetVelocity(ShootDirection.GetSafeNormal() * (Speed + ExtraSpeed)); } } }

void APcProjectile::OnBeatTriggered(float BeatTimestamp) { if (!bIsRhythmic) return; if (UWorld* World = GetWorld()) { if (UPcMusicAnalysisSubsystem* MusicSys = World->GetSubsystem<UPcMusicAnalysisSubsystem>()) { float NewBPM = MusicSys->GetCurrentBPM(); if (NewBPM > 0.1f) { CurrentBPM = NewBPM; CurrentBeatDuration = 60.0f / CurrentBPM; } } } CurrentRingIndex++; if (MaxRingIndex > 0 && CurrentRingIndex > MaxRingIndex) { Destroy(); return; } TimeSinceLastBeat = 0.0f; }

void APcProjectile::Tick(float DeltaTime) { Super::Tick(DeltaTime); TimeAlive += DeltaTime; if (TimeAlive > LifeSpan) { Destroy(); return; } if (bIsRhythmic) { TimeSinceLastBeat += DeltaTime; float RawAlpha = FMath::Clamp(TimeSinceLastBeat / CurrentBeatDuration, 0.0f, 1.0f); float CurveAlpha = 0.0f; if (CurrentBPM > FrenzyThresholdBPM) { CurveAlpha = FMath::Pow(RawAlpha, 5.0f); } else { CurveAlpha = FMath::InterpEaseOut(0.0f, 1.0f, RawAlpha, 2.0f); } float SpacingScale = ReferenceBPM / FMath::Max(60.0f, CurrentBPM); SpacingScale = FMath::Clamp(SpacingScale, 0.5f, 1.0f); float DynamicSpacing = BaseRingSpacing * SpacingScale; float StartRad = (CurrentRingIndex - 1 + RingOffset) * DynamicSpacing; float EndRad = (CurrentRingIndex + RingOffset) * DynamicSpacing; if (CurrentRingIndex == 0) StartRad = RingOffset * DynamicSpacing; float CurrentArcDist = FMath::Lerp(StartRad, EndRad, CurveAlpha); CurrentArcDist = FMath::Max(0.0f, CurrentArcDist); float AngleRad = CurrentArcDist / ReferenceRadius; float AngleDeg = FMath::RadiansToDegrees(AngleRad); FVector RotationAxis = FVector::CrossProduct(StartGravityNormal, FireTangent).GetSafeNormal(); FVector InitialRadiusVector = OriginLocation - PlanetCenter; FVector NewRadiusVector = InitialRadiusVector.RotateAngleAxis(AngleDeg, RotationAxis); FVector IdealPos = PlanetCenter + NewRadiusVector; FVector SnapPos = IdealPos; FVector FinalUp = NewRadiusVector.GetSafeNormal(); if (CurrentZone) { FVector GravityDir = CurrentZone->GetGravityDirection(IdealPos); if (!GravityDir.IsZero()) FinalUp = -GravityDir; FVector TraceStart = IdealPos + (FinalUp * 1000.0f); FVector TraceEnd = IdealPos - (FinalUp * 1000.0f); FHitResult Hit; FCollisionQueryParams P; P.AddIgnoredActor(this); if (GetWorld()->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_WorldStatic, P)) { SnapPos = Hit.Location + (FinalUp * HoverHeight); FinalUp = Hit.ImpactNormal; } } FVector ForwardVec = FVector::CrossProduct(RotationAxis, FinalUp); SetActorLocation(SnapPos); SetActorRotation(FRotationMatrix::MakeFromXZ(ForwardVec, FinalUp).Rotator()); } }

// === MODIFIED FUNCTION BELOW ===

void APcProjectile::OnOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!OtherActor || OtherActor == this || OtherActor == GetOwner()) return;

	if (bIsPlayerProjectile)
	{
		if (auto* Enemy = Cast<APcEnemyBase>(OtherActor))
		{
			// 1. VISUALS
			Enemy->HandleHit();

			// 2. PLAYER REWARD
			float PointsGained = 0.0f;
			if (APcPlayerCharacter* PlayerOwner = Cast<APcPlayerCharacter>(GetOwner()))
			{
				if (PlayerOwner->FlowComp)
				{
					float BasePoints = Enemy->ScoreReward;
					float Multiplier = PlayerOwner->FlowComp->CurrentMultiplier;
					PointsGained = BasePoints * (1.0f + Multiplier);

					PlayerOwner->FlowComp->AddCharge(Enemy->ChargeReward); 
					PlayerOwner->FlowComp->IncreaseMultiplier(1.0f);
					PlayerOwner->FlowComp->AddScore(BasePoints);
				}
			}

			// 3. FLOATING TEXT POSITIONING
			FVector SpawnLoc = FVector::ZeroVector;

			// Priority 1: Use the dedicated component if it exists
			if (Enemy->ScoreSpawnLoc)
			{
				SpawnLoc = Enemy->ScoreSpawnLoc->GetComponentLocation();
			}
			// Priority 2: Use Impact Point
			else if (!SweepResult.Location.IsZero())
			{
				// Nudge out slightly so it doesn't clip
				FVector DirToProj = (GetActorLocation() - SweepResult.Location).GetSafeNormal();
				SpawnLoc = SweepResult.Location + (DirToProj * 50.0f);
			}
			// Priority 3: Center of Actor + Up Offset
			else
			{
				SpawnLoc = OtherActor->GetActorLocation() + FVector(0,0,100);
			}

			// Add random spread so numbers don't stack perfectly if we machine-gun
			float Spread = 30.0f;
			SpawnLoc.X += FMath::RandRange(-Spread, Spread);
			SpawnLoc.Y += FMath::RandRange(-Spread, Spread);
			SpawnLoc.Z += FMath::RandRange(-10.0f, 10.0f);

			if (APcFloatingScore* ScoreActor = GetWorld()->SpawnActor<APcFloatingScore>(APcFloatingScore::StaticClass(), SpawnLoc, FRotator::ZeroRotator))
			{
				ScoreActor->InitializeScore(PointsGained, SpawnLoc);
			}

			// 4. Destroy Bullet
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