// ==========================================
// FILE: PcProjectile.h
// PATH: Source/Project_Circle/PcPlayer/PcProjectile.h
// ==========================================
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PcProjectile.generated.h"

class USphereComponent;
class UPcGravityMovementComponent; 
class APcPlanet; 
class APcGravityZone; 
class UPcMusicAnalysisSubsystem;

UCLASS()
class PROJECT_CIRCLE_API APcProjectile : public AActor
{
	GENERATED_BODY()
	
public:	
	APcProjectile();

	/** 
	 * Fired by Weapon or Enemy Turret.
	 * @param InRingSpacing: If > 0, enables Rhythm Mode.
	 * @param InMaxRings: If > 0, projectile destroys itself after passing this ring index.
	 */
	void InitializeProjectile(FVector ShootDirection, APcPlanet* InPlanet, bool bIsPlayerOwned, float InRingSpacing = 0.0f, int32 InMaxRings = 0);

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	USphereComponent* CollisionComp;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	UStaticMeshComponent* MeshComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UPcGravityMovementComponent* MovementComp;

	UPROPERTY(EditAnywhere, Category = "Projectile")
	float Speed = 2000.0f;

	UPROPERTY(EditAnywhere, Category = "Projectile")
	float LifeSpan = 10.0f; 

	UPROPERTY(EditAnywhere, Category = "Projectile")
	float HoverHeight = 20.0f; 

private:
	float TimeAlive = 0.0f;
	bool bIsPlayerProjectile = true;

	UFUNCTION()
	void OnOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	// --- RHYTHM LOGIC ---
	bool bIsRhythmic = false;
	
	UPROPERTY()
	APcGravityZone* CurrentZone; 

	FVector OriginLocation;     
	FVector StartGravityNormal; 
	FVector FireTangent;        
	
	FVector PlanetCenter = FVector::ZeroVector;
	float ReferenceRadius = 0.0f;

	int32 CurrentRingIndex = 0; 
	int32 MaxRingIndex = 0; // Added for life limit
	float BaseRingSpacing = 0.0f; 
	
	float TimeSinceLastBeat = 0.0f;
	float CurrentBeatDuration = 0.5f; 
	float CurrentBPM = 120.0f;

	const float ReferenceBPM = 120.0f;
	const float FrenzyThresholdBPM = 150.0f;
	
	float RingOffset = 0.0f;

	UFUNCTION()
	void OnBeatTriggered(float BeatTimestamp);
};