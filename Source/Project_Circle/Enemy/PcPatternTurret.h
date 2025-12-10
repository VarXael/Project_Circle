// ==========================================
// FILE: PcPatternTurret.h
// PATH: Source/Project_Circle/Enemy/PcPatternTurret.h
// ==========================================
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PcPatternTurret.generated.h"

class APcProjectile;
class UPcGravityMovementComponent; 

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
class PROJECT_CIRCLE_API APcPatternTurret : public AActor
{
	GENERATED_BODY()
	
public:	
	APcPatternTurret();

protected:
	virtual void BeginPlay() override;

public:
	// --- COMPONENTS ---
	UPROPERTY(VisibleAnywhere, Category = "Components")
	UStaticMeshComponent* MeshComp;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	USceneComponent* MuzzleLoc;

	// Used only for Surface Alignment (Finding "Up"), not moving.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UPcGravityMovementComponent* GravityComp;

	// --- COMBAT CONFIG ---
	UPROPERTY(EditAnywhere, Category = "Combat")
	TSubclassOf<APcProjectile> ProjectileClass;

	// If true, fires automatically on a timer. 
	// If false, waits for an external event (Music Subsystem).
	UPROPERTY(EditAnywhere, Category = "Combat")
	bool bAutoFireTest = true;

	UPROPERTY(EditAnywhere, Category = "Combat", meta=(EditCondition="bAutoFireTest"))
	float FireRate = 0.2f; // Fast for bullet hell

	// --- PATTERN MATH ---
	UPROPERTY(EditAnywhere, Category = "Pattern")
	EBulletPattern PatternType = EBulletPattern::Spiral;

	// How many degrees the pattern rotates PER SHOT.
	// e.g. 10.0 = A tight spiral. 137.5 = Golden Ratio (No gaps).
	UPROPERTY(EditAnywhere, Category = "Pattern")
	float AngleStepPerShot = 15.0f;

	// For Ring/Shotgun: How many bullets per pulse?
	UPROPERTY(EditAnywhere, Category = "Pattern")
	int32 BulletsPerPulse = 1;

	// --- API ---
	// Call this every Beat/Rhythm tick
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void TriggerBeatShot();

private:
	int32 ShotCounter = 0; // The "Index" of the pattern
	FTimerHandle TimerHandle_TestFire;

	void SpawnBullet(FVector Direction);
};