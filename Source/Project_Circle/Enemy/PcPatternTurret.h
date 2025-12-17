// ==========================================
// FILE: PcPatternTurret.h
// PATH: Source/Project_Circle/Enemy/PcPatternTurret.h
// ==========================================
#pragma once

#include "CoreMinimal.h"
#include "PcEnemyBase.h" // Inherit from Base
#include "PcPatternTurret.generated.h"

class APcProjectile;

UENUM(BlueprintType)
enum class EBulletPattern : uint8
{
	Spiral      UMETA(DisplayName = "Spiral (Single Stream)"),
	DoubleHelix UMETA(DisplayName = "Double Helix (2 Streams)"),
	Ring        UMETA(DisplayName = "Expanding Ring (Nova)"),
	Shotgun     UMETA(DisplayName = "Aimed Shotgun"),
	Chaos       UMETA(DisplayName = "Procedural Chaos")
};

UCLASS()
class PROJECT_CIRCLE_API APcPatternTurret : public APcEnemyBase
{
	GENERATED_BODY()
	
public:	
	APcPatternTurret();
	virtual void Tick(float DeltaTime) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override; 

public:
	// --- COMPONENTS (Mesh, HitBox, Gravity are now in Base) ---

	UPROPERTY(VisibleAnywhere, Category = "Components")
	USceneComponent* MuzzleLoc;

	// --- COMBAT CONFIG ---
	UPROPERTY(EditAnywhere, Category = "Combat")
	TSubclassOf<APcProjectile> ProjectileClass;

	UPROPERTY(EditAnywhere, Category = "Combat")
	bool bAutoFireDebug = false;

	UPROPERTY(EditAnywhere, Category = "Combat", meta=(EditCondition="bAutoFireDebug"))
	float FireRate = 0.2f; 

	// --- PATTERN MATH ---
	UPROPERTY(EditAnywhere, Category = "Pattern")
	EBulletPattern PatternType = EBulletPattern::Spiral;

	UPROPERTY(EditAnywhere, Category = "Pattern")
	float AngleStepPerShot = 15.0f;

	UPROPERTY(EditAnywhere, Category = "Pattern")
	int32 BulletsPerPulse = 1;

	// --- RHYTHM ARENA CONFIG ---
	UPROPERTY(EditAnywhere, Category = "Rhythm Arena")
	float RingSpacing = 600.0f; 

	UPROPERTY(EditAnywhere, Category = "Rhythm Arena")
	int32 ArenaRingCount = 5; 

	UPROPERTY(EditAnywhere, Category = "Rhythm Arena")
	bool bDrawDebugArena = true; 

	// --- API ---
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void TriggerBeatShot();

private:
	int32 ShotCounter = 0; 
	FTimerHandle TimerHandle_TestFire;

	void SpawnBullet(FVector Direction);
	
	UFUNCTION()
	void OnMusicNoteHit(int32 Timestamp, int32 NoteType, int32 HitSound);
};