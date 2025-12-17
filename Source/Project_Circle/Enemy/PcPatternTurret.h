// ==========================================
// FILE: PcPatternTurret.h
// PATH: Source/Project_Circle/Enemy/PcPatternTurret.h
// ==========================================
#pragma once

#include "CoreMinimal.h"
#include "PcEnemyBase.h"
#include "PcPatternTurret.generated.h"

class APcProjectile;

UENUM(BlueprintType)
enum class EBulletPattern : uint8
{
	Spiral      UMETA(DisplayName = "Spiral (Single Stream)"),
	DoubleHelix UMETA(DisplayName = "Double Helix (2 Streams)"),
	Ring        UMETA(DisplayName = "Expanding Ring (Nova)"),
	Shotgun     UMETA(DisplayName = "Aimed Shotgun"),
	Chaos       UMETA(DisplayName = "Procedural Chaos"),
	
	// --- NEW SWAG PATTERNS ---
	Star        UMETA(DisplayName = "5-Point Star"),
	Flower      UMETA(DisplayName = "Spinning Flower"),
	TidalWave   UMETA(DisplayName = "Tidal Wave")
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
	UPROPERTY(VisibleAnywhere, Category = "Components")
	USceneComponent* MuzzleLoc;

	// --- COMBAT CONFIG ---
	UPROPERTY(EditAnywhere, Category = "Combat")
	TSubclassOf<APcProjectile> ProjectileClass;

	// --- RHYTHM SOURCE ---
	UPROPERTY(EditAnywhere, Category = "Combat|Rhythm")
	bool bFireOnMetronome = false;

	UPROPERTY(EditAnywhere, Category = "Combat|Rhythm", meta=(EditCondition="bFireOnMetronome"))
	int32 FireEveryNBeats = 1;

	// --- DEBUG ---
	UPROPERTY(EditAnywhere, Category = "Combat|Debug")
	bool bAutoFireDebug = false;

	UPROPERTY(EditAnywhere, Category = "Combat|Debug", meta=(EditCondition="bAutoFireDebug"))
	float FireRate = 0.2f; 

	// --- PATTERN MATH ---
	UPROPERTY(EditAnywhere, Category = "Pattern")
	EBulletPattern PatternType = EBulletPattern::Spiral;

	UPROPERTY(EditAnywhere, Category = "Pattern")
	float AngleStepPerShot = 15.0f;

	UPROPERTY(EditAnywhere, Category = "Pattern")
	int32 BulletsPerPulse = 1;

	// --- VISUAL SWAG ---
	// How much the mesh scales up on the beat (e.g. 1.2 = 20% bigger)
	UPROPERTY(EditAnywhere, Category = "Visual Swag")
	float BeatPulseScale = 1.3f;

	UPROPERTY(EditAnywhere, Category = "Visual Swag")
	float PulseDecaySpeed = 5.0f;

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
	int32 BeatCounter = 0;
	FTimerHandle TimerHandle_TestFire;
	FVector BaseScale;
	float CurrentPulse = 0.0f;

	void SpawnBullet(FVector Direction);
	void ApplyVisualPulse(float DeltaTime);

	UFUNCTION()
	void OnMusicNoteHit(int32 Timestamp, int32 NoteType, int32 HitSound);

	UFUNCTION()
	void OnBeatTriggered(float BeatTimestamp);
};