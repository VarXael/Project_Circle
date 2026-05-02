#pragma once

#include "CoreMinimal.h"
#include "Project_Circle/Project_Pulse/Enemies/PcQEnemyBase.h"
#include "PcQEnemy_FlamePillar.generated.h"

class UPcMusicAnalysisSubsystem;

UCLASS()
class PROJECT_CIRCLE_API APcQEnemy_FlamePillar : public APcQEnemyBase
{
	GENERATED_BODY()

public:
	APcQEnemy_FlamePillar();

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void HandleNoteHit(int32 TimestampMS, int32 NoteType, int32 HitSound);

	UFUNCTION()
	void HandleGameplayBeat(float BeatTimestamp);

	UPROPERTY(EditDefaultsOnly, Category = "Combat")
	float PillarRadius = 300.f;

	UPROPERTY(EditDefaultsOnly, Category = "Combat")
	float PillarDamage = 40.f;

	// Stores locations painted during micro-notes, waiting for the macro beat to detonate
	TArray<FVector> PendingPillars;
};