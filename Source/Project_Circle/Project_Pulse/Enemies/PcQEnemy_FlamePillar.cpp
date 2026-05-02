#include "PcQEnemy_FlamePillar.h"
#include "Project_Circle/MusicSystem/MusicGameplaySystem/PcMusicAnalysisSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/Character.h"

APcQEnemy_FlamePillar::APcQEnemy_FlamePillar()
{
	PrimaryActorTick.bCanEverTick = false;
}

void APcQEnemy_FlamePillar::BeginPlay()
{
	Super::BeginPlay();

	if (UPcMusicAnalysisSubsystem* Sub = GetWorld()->GetSubsystem<UPcMusicAnalysisSubsystem>())
	{
		// Micro-Rhythm paints the floor
		Sub->OnNoteHit.AddDynamic(this, &APcQEnemy_FlamePillar::HandleNoteHit);
		
		// Macro-Rhythm detonates
		Sub->OnGameplayBeatTriggered.AddDynamic(this, &APcQEnemy_FlamePillar::HandleGameplayBeat);
	}
}

void APcQEnemy_FlamePillar::HandleNoteHit(int32 TimestampMS, int32 NoteType, int32 HitSound)
{
	ACharacter* Player = UGameplayStatics::GetPlayerCharacter(this, 0);
	if (!Player) return;

	// Only spawn a new warning zone if we haven't flooded the arena yet this beat
	if (PendingPillars.Num() < 5)
	{
		// Paint a warning zone exactly where the player is currently standing
		FVector FloorLoc = Player->GetActorLocation() - FVector(0.f, 0.f, 90.f); // approximate feet
		PendingPillars.Add(FloorLoc);

		// Draw a Yellow warning circle on the floor
		DrawDebugCylinder(GetWorld(), FloorLoc, FloorLoc + FVector(0.f, 0.f, 10.f), PillarRadius, 16, FColor::Yellow, false, 1.0f, 0, 5.f);
	}
}

void APcQEnemy_FlamePillar::HandleGameplayBeat(float BeatTimestamp)
{
	if (PendingPillars.IsEmpty()) return;

	for (const FVector& PillarLoc : PendingPillars)
	{
		FVector PillarTop = PillarLoc + FVector(0.f, 0.f, 2000.f);

		// Detonate! Massive Red Cylinder
		DrawDebugCylinder(GetWorld(), PillarLoc, PillarTop, PillarRadius, 16, FColor::Red, false, 0.5f, 0, 15.f);

		// Apply Radial Damage to anything caught inside
		TArray<AActor*> IgnoredActors;
		IgnoredActors.Add(this);
		
		UGameplayStatics::ApplyRadialDamage(
			this, 
			PillarDamage, 
			PillarLoc, 
			PillarRadius, 
			nullptr, 
			IgnoredActors, 
			this, 
			GetInstigatorController(), 
			true
		);
	}

	PendingPillars.Empty();
}