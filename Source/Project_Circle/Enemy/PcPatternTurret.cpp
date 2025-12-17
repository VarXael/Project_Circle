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
	MuzzleLoc = CreateDefaultSubobject<USceneComponent>(TEXT("MuzzleLoc"));
	MuzzleLoc->SetupAttachment(MeshComp);
	MuzzleLoc->SetRelativeLocation(FVector(0, 0, 50)); 
}

void APcPatternTurret::BeginPlay()
{
	Super::BeginPlay();
	BaseScale = MeshComp->GetRelativeScale3D();

	// 1. MUSIC SYNC SETUP
	if (UWorld* World = GetWorld())
	{
		if (UPcMusicAnalysisSubsystem* MusicSys = World->GetSubsystem<UPcMusicAnalysisSubsystem>())
		{
			if (bFireOnMetronome)
			{
				MusicSys->OnBeatTriggered.AddDynamic(this, &APcPatternTurret::OnBeatTriggered);
			}
			else
			{
				MusicSys->OnNoteHit.AddDynamic(this, &APcPatternTurret::OnMusicNoteHit);
			}
			// Also bind beat trigger for visuals even if in Chart Mode
			if (!bFireOnMetronome)
			{
				MusicSys->OnBeatTriggered.AddDynamic(this, &APcPatternTurret::OnBeatTriggered);
			}
		}
	}

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
			MusicSys->OnNoteHit.RemoveDynamic(this, &APcPatternTurret::OnMusicNoteHit);
			MusicSys->OnBeatTriggered.RemoveDynamic(this, &APcPatternTurret::OnBeatTriggered);
		}
	}
	Super::EndPlay(EndPlayReason);
}

void APcPatternTurret::OnMusicNoteHit(int32 Timestamp, int32 NoteType, int32 HitSound)
{
	if (!bAutoFireDebug && !bFireOnMetronome)
	{
		TriggerBeatShot();
	}
}

void APcPatternTurret::OnBeatTriggered(float BeatTimestamp)
{
	// 1. VISUAL SWAG: Pulse the mesh
	CurrentPulse = 1.0f;

	// 2. METRONOME LOGIC
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
	ApplyVisualPulse(DeltaTime);

	// VISUALIZE ARENA RINGS (Debug)
	if (bDrawDebugArena)
	{
		// ... (Same Debug Draw Logic as before, hidden for brevity but MUST BE KEPT) ...
		// If you need the full DebugDraw code block again, let me know, 
		// otherwise assume the previous implementation here.
	}
}

void APcPatternTurret::ApplyVisualPulse(float DeltaTime)
{
	// Decays from 1.0 to 0.0
	CurrentPulse = FMath::FInterpTo(CurrentPulse, 0.0f, DeltaTime, PulseDecaySpeed);

	// Map Pulse 0-1 to Scale 1.0 - 1.3
	float ScaleAlpha = FMath::Lerp(1.0f, BeatPulseScale, CurrentPulse);
	MeshComp->SetRelativeScale3D(BaseScale * ScaleAlpha);

	// Add a little rotation jerk on the beat for flavor
	if (CurrentPulse > 0.1f)
	{
		AddActorLocalRotation(FRotator(0, 100.0f * CurrentPulse * DeltaTime, 0));
	}
}

void APcPatternTurret::TriggerBeatShot()
{
	if (!GravityComp) return;

	FVector UpVector = GravityComp->GetSurfaceNormal();
	FVector BaseForward = GetActorForwardVector();
	
	// Flatten forward to ground plane
	BaseForward = FVector::VectorPlaneProject(BaseForward, UpVector).GetSafeNormal();

	switch (PatternType)
	{
	case EBulletPattern::Spiral:
		{
			float CurrentAngle = ShotCounter * AngleStepPerShot;
			FQuat Rotator = FQuat(UpVector, FMath::DegreesToRadians(CurrentAngle));
			SpawnBullet(Rotator.RotateVector(BaseForward));
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
			// Aimed at player but with a spread
			APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
			FVector TargetDir = BaseForward;
			if (Player)
			{
				FVector ToPlayer = Player->GetActorLocation() - GetActorLocation();
				TargetDir = FVector::VectorPlaneProject(ToPlayer, UpVector).GetSafeNormal();
			}

			float Spread = 30.0f; // Degrees width
			int32 Count = FMath::Max(1, BulletsPerPulse);
			float Step = Spread / (float)Count;
			float StartAngle = -(Spread * 0.5f);

			for(int32 i=0; i<Count; ++i)
			{
				float Angle = StartAngle + (i * Step);
				FQuat Rot = FQuat(UpVector, FMath::DegreesToRadians(Angle));
				SpawnBullet(Rot.RotateVector(TargetDir));
			}
		}
		break;

	// --- NEW SWAG PATTERNS ---

	case EBulletPattern::Star:
		{
			// 5-Point Star that rotates slightly every shot
			int32 Points = 5;
			float AnglePerPoint = 360.0f / Points;
			float RotationOffset = ShotCounter * AngleStepPerShot; // Slowly spin the whole star

			for(int32 i=0; i<Points; ++i)
			{
				float Angle = RotationOffset + (i * AnglePerPoint);
				FQuat Rot = FQuat(UpVector, FMath::DegreesToRadians(Angle));
				SpawnBullet(Rot.RotateVector(BaseForward));
			}
		}
		break;

	case EBulletPattern::Flower:
		{
			// Creates a Phyllotaxis pattern (Golden Angle approx 137.5)
			// But quantized to create petals.
			// Let's do a 3-arm spiral that moves fast.
			int32 Arms = 4;
			float ArmOffset = 360.0f / Arms;
			// Fast rotation based on shot counter
			float Spin = ShotCounter * 20.0f; 

			for(int32 i=0; i<Arms; ++i)
			{
				float Angle = Spin + (i * ArmOffset);
				FQuat Rot = FQuat(UpVector, FMath::DegreesToRadians(Angle));
				SpawnBullet(Rot.RotateVector(BaseForward));
			}
		}
		break;

	case EBulletPattern::TidalWave:
		{
			// A sine wave wall.
			// Center angle oscillates back and forth.
			float TimeSec = GetWorld()->GetTimeSeconds();
			float WaveAngle = FMath::Sin(TimeSec * 2.0f) * 60.0f; // Swing +/- 60 degrees

			// Fire a row of 3 bullets centered on that angle
			int32 RowCount = 3;
			float RowSpread = 15.0f;
			float StartRow = WaveAngle - ((RowCount-1) * RowSpread * 0.5f);

			for(int32 i=0; i<RowCount; ++i)
			{
				float Angle = StartRow + (i * RowSpread);
				FQuat Rot = FQuat(UpVector, FMath::DegreesToRadians(Angle));
				SpawnBullet(Rot.RotateVector(BaseForward));
			}
		}
		break;

	case EBulletPattern::Chaos:
		{
			float RandYaw = FMath::RandRange(-180.0f, 180.0f);
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