#include "PcQEnemy_NoteShooter.h"
#include "Project_Circle/MusicSystem/MusicGameplaySystem/PcMusicAnalysisSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/Character.h"

APcQEnemy_NoteShooter::APcQEnemy_NoteShooter()
{
	PrimaryActorTick.bCanEverTick = false;
}

void APcQEnemy_NoteShooter::BeginPlay()
{
	Super::BeginPlay();

	// Bind to the Micro-Rhythm (Every single note)
	if (UPcMusicAnalysisSubsystem* Sub = GetWorld()->GetSubsystem<UPcMusicAnalysisSubsystem>())
	{
		Sub->OnNoteHit.AddDynamic(this, &APcQEnemy_NoteShooter::HandleNoteHit);
	}
}

void APcQEnemy_NoteShooter::HandleNoteHit(int32 TimestampMS, int32 NoteType, int32 HitSound)
{
	ACharacter* Player = UGameplayStatics::GetPlayerCharacter(this, 0);
	if (!Player) return;

	FVector MuzzleLoc = GetActorLocation() + FVector(0.f, 0.f, 50.f);
	FVector PlayerLoc = Player->GetActorLocation();

	// Raycast to player
	FHitResult Hit;
	FCollisionQueryParams QP; 
	QP.AddIgnoredActor(this);

	bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, MuzzleLoc, PlayerLoc, ECC_Pawn, QP);
	
	if (bHit && Hit.GetActor() == Player)
	{
		UGameplayStatics::ApplyDamage(Player, DamagePerShot, nullptr, this, nullptr);
		// Visual Laser (Magenta) connecting directly to the player
		DrawDebugLine(GetWorld(), MuzzleLoc, PlayerLoc, FColor::Magenta, false, 0.15f, 0, 4.f);
	}
	else
	{
		// Missed or hit geometry
		DrawDebugLine(GetWorld(), MuzzleLoc, PlayerLoc, FColor::Purple, false, 0.15f, 0, 1.f);
	}
}