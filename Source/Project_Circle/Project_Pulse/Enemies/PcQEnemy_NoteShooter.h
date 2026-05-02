#pragma once

#include "CoreMinimal.h"
#include "Project_Circle/Project_Pulse/Enemies/PcQEnemyBase.h"
#include "PcQEnemy_NoteShooter.generated.h"

class UPcMusicAnalysisSubsystem;

UCLASS()
class PROJECT_CIRCLE_API APcQEnemy_NoteShooter : public APcQEnemyBase
{
	GENERATED_BODY()

public:
	APcQEnemy_NoteShooter();

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void HandleNoteHit(int32 TimestampMS, int32 NoteType, int32 HitSound);

	UPROPERTY(EditDefaultsOnly, Category = "Combat")
	float DamagePerShot = 10.f;
};