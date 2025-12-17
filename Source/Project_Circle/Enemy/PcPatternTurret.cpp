// ==========================================
// FILE: PcPatternTurret.cpp
// PATH: Source/Project_Circle/Enemy/PcPatternTurret.cpp
// ==========================================
#include "PcPatternTurret.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Project_Circle/GravitySystem/PcGravityMovementComponent.h"
#include "Project_Circle/GravitySystem/PcGravityZone.h" 
#include "Project_Circle/PcPlayer/PcProjectile.h"
#include "Project_Circle/MusicSystem/MusicGameplaySystem/PcMusicAnalysisSubsystem.h" 
#include "DrawDebugHelpers.h" 

APcPatternTurret::APcPatternTurret()
{
	PrimaryActorTick.bCanEverTick = true; 

	// MeshComp, HitBox, GravityComp created in Base Class

	MuzzleLoc = CreateDefaultSubobject<USceneComponent>(TEXT("MuzzleLoc"));
	MuzzleLoc->SetupAttachment(MeshComp);
	MuzzleLoc->SetRelativeLocation(FVector(0, 0, 50)); 
}

void APcPatternTurret::BeginPlay()
{
	Super::BeginPlay(); // Call Base (Sets up Material for hit flash)

	// 1. MUSIC SYNC SETUP
	if (UWorld* World = GetWorld())
	{
		if (UPcMusicAnalysisSubsystem* MusicSys = World->GetSubsystem<UPcMusicAnalysisSubsystem>())
		{
			if (bFireOnMetronome)
			{
				// METRONOME MODE: Fire on steady beat
				MusicSys->OnBeatTriggered.AddDynamic(this, &APcPatternTurret::OnBeatTriggered);
			}
			else
			{
				// CHART MODE: Fire on specific notes
				MusicSys->OnNoteHit.AddDynamic(this, &APcPatternTurret::OnMusicNoteHit);
			}
		}
	}

	// 2. DEBUG TIMER
	if (bAutoFireDebug)
	{
		GetWorldTimerManager().SetTimer(TimerHandle_TestFire, this, &APcPatternTurret::TriggerBeatShot, FireRate, true);
	}
}

void APcPatternTurret::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		if (UPcMusicAnalysisSubsystem* MusicSys = World->GetSubsystem<UPcMusicAnalysisSubsystem>())
		{
			// Try to unbind both to be safe
			MusicSys->OnNoteHit.RemoveDynamic(this, &APcPatternTurret::OnMusicNoteHit);
			MusicSys->OnBeatTriggered.RemoveDynamic(this, &APcPatternTurret::OnBeatTriggered);
		}
	}
	Super::EndPlay(EndPlayReason);
}

void APcPatternTurret::OnMusicNoteHit(int32 Timestamp, int32 NoteType, int32 HitSound)
{
	// Only fire here if we are NOT using Metronome mode (and not debugging)
	if (!bAutoFireDebug && !bFireOnMetronome)
	{
		TriggerBeatShot();
	}
}

void APcPatternTurret::OnBeatTriggered(float BeatTimestamp)
{
	// Only fire here if we ARE using Metronome mode (and not debugging)
	if (!bAutoFireDebug && bFireOnMetronome)
	{
		BeatCounter++;
		if (BeatCounter % FMath::Max(1, FireEveryNBeats) == 0)
		{
			TriggerBeatShot();
		}
	}
}

void APcPatternTurret::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// VISUALIZE THE "ELASTIC" ARENA RINGS
	if (bDrawDebugArena)
	{
		float SpacingScale = 1.0f;
		if (UWorld* World = GetWorld())
		{
			if (UPcMusicAnalysisSubsystem* MusicSys = World->GetSubsystem<UPcMusicAnalysisSubsystem>())
			{
				float BPM = MusicSys->GetCurrentBPM();
				if (BPM > 0.1f)
				{
					SpacingScale = 120.0f / FMath::Max(60.0f, BPM);
					SpacingScale = FMath::Clamp(SpacingScale, 0.5f, 1.0f);
				}
			}
		}
		float DynamicSpacing = RingSpacing * SpacingScale;

		FVector Center = GetActorLocation();
		APcGravityZone* Zone = nullptr;
		FVector SurfaceNormal = FVector::UpVector;
		FVector PlanetCenter = Center - (FVector::UpVector * 10000.0f); 
		float PlanetRadius = 10000.0f;

		TArray<AActor*> FoundZones;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), APcGravityZone::StaticClass(), FoundZones);
		if (FoundZones.Num() > 0) Zone = Cast<APcGravityZone>(FoundZones[0]);

		if (Zone)
		{
			FVector G = Zone->GetGravityDirection(Center);
			if (!G.IsZero()) SurfaceNormal = -G;
			PlanetCenter = Zone->GetActorLocation();
			PlanetRadius = FVector::Dist(Center, PlanetCenter);
		}
		else if (GravityComp)
		{
			SurfaceNormal = GravityComp->GetSurfaceNormal();
		}

		int32 Segments = 32;
		float AngleStep = 360.0f / Segments;
		FVector RadiusVec = Center - PlanetCenter; 
		FVector TangentX = GetActorForwardVector();
		TangentX = FVector::VectorPlaneProject(TangentX, SurfaceNormal).GetSafeNormal();

		for (int32 r = 1; r <= ArenaRingCount; r++)
		{
			float ArcLength = DynamicSpacing * r;
			float ConeAngleRad = ArcLength / PlanetRadius;
			float ConeAngleDeg = FMath::RadiansToDegrees(ConeAngleRad);
			FVector LastPoint = FVector::ZeroVector;
			bool bLastPointValid = false;

			for (int32 s = 0; s <= Segments; s++)
			{
				float AzimuthDeg = s * AngleStep; 
				FVector SegmentDir = TangentX.RotateAngleAxis(AzimuthDeg, SurfaceNormal);
				FVector RotAxis = FVector::CrossProduct(SurfaceNormal, SegmentDir).GetSafeNormal();
				FVector RingPointVec = RadiusVec.RotateAngleAxis(ConeAngleDeg, RotAxis);
				FVector IdealPoint = PlanetCenter + RingPointVec;
				FVector SnapPoint = IdealPoint;
				FVector SnapUp = (IdealPoint - PlanetCenter).GetSafeNormal();
				
				if (Zone) 
				{
					FVector G = Zone->GetGravityDirection(IdealPoint);
					if (!G.IsZero()) SnapUp = -G;
				}

				FVector TraceStart = IdealPoint + (SnapUp * 1000.0f);
				FVector TraceEnd = IdealPoint - (SnapUp * 1000.0f);
				FHitResult Hit;
				FCollisionQueryParams P; P.AddIgnoredActor(this);

				if (GetWorld()->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_WorldStatic, P))
				{
					SnapPoint = Hit.Location + (Hit.ImpactNormal * 5.0f); 
					if (bLastPointValid && s > 0)
					{
						FColor LineColor = FColor::Cyan;
						if (SpacingScale < 0.8f) LineColor = FColor::Orange; 
						DrawDebugLine(GetWorld(), LastPoint, SnapPoint, LineColor, false, -1.0f, 0, 5.0f);
					}
					LastPoint = SnapPoint;
					bLastPointValid = true;
				}
				else { bLastPointValid = false; }
			}
		}
	}
}

void APcPatternTurret::TriggerBeatShot()
{
	if (!GravityComp) return;

	FVector UpVector = GravityComp->GetSurfaceNormal();
	FVector BaseForward = GetActorForwardVector();
	
	BaseForward = FVector::VectorPlaneProject(BaseForward, UpVector).GetSafeNormal();

	switch (PatternType)
	{
	case EBulletPattern::Spiral:
		{
			float CurrentAngle = ShotCounter * AngleStepPerShot;
			FQuat Rotator = FQuat(UpVector, FMath::DegreesToRadians(CurrentAngle));
			FVector FireDir = Rotator.RotateVector(BaseForward);
			SpawnBullet(FireDir);
		}
		break;
	case EBulletPattern::DoubleHelix:
		{
			float CurrentAngle = ShotCounter * AngleStepPerShot;
			FQuat RotA = FQuat(UpVector, FMath::DegreesToRadians(CurrentAngle));
			SpawnBullet(RotA.RotateVector(BaseForward));
			FQuat RotB = FQuat(UpVector, FMath::DegreesToRadians(CurrentAngle + 180.0f));
			SpawnBullet(RotB.RotateVector(BaseForward));
		}
		break;
	case EBulletPattern::Ring:
		{
			float AnglePerBullet = 360.0f / FMath::Max(1, BulletsPerPulse);
			for (int32 i = 0; i < BulletsPerPulse; ++i)
			{
				float Angle = i * AnglePerBullet;
				FQuat Rot = FQuat(UpVector, FMath::DegreesToRadians(Angle));
				SpawnBullet(Rot.RotateVector(BaseForward));
			}
		}
		break;
	case EBulletPattern::Shotgun:
		{
			float Sine = FMath::Sin(ShotCounter * 0.2f);
			float FanAngle = Sine * 45.0f;
			FQuat Rot = FQuat(UpVector, FMath::DegreesToRadians(FanAngle));
			SpawnBullet(Rot.RotateVector(BaseForward));
		}
		break;
	case EBulletPattern::Chaos:
		{
			// Basic random forward spray
			float RandYaw = FMath::RandRange(-30.0f, 30.0f);
			FQuat Rot = FQuat(UpVector, FMath::DegreesToRadians(RandYaw));
			SpawnBullet(Rot.RotateVector(BaseForward));
		}
		break;
	}

	ShotCounter++;
}

void APcPatternTurret::SpawnBullet(FVector Direction)
{
	if (!ProjectileClass || !MuzzleLoc) return;

	FVector SpawnLoc = MuzzleLoc->GetComponentLocation();
	FRotator SpawnRot = Direction.Rotation();

	FActorSpawnParameters P;
	P.Owner = this;

	auto* Proj = GetWorld()->SpawnActor<APcProjectile>(ProjectileClass, SpawnLoc, SpawnRot, P);
	if (Proj)
	{
		Proj->InitializeProjectile(Direction, nullptr, false, RingSpacing, ArenaRingCount);
	}
}