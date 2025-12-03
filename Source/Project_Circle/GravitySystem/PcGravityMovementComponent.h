#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PcGravityMovementComponent.generated.h"

class APcPlanet;

UENUM(BlueprintType)
enum class EPcMovementMode : uint8
{
	Skater      UMETA(DisplayName = "Skater (Momentum, Carving)"),
	GroundUnit  UMETA(DisplayName = "Ground Unit (High Friction, Snappy)"),
	Projectile  UMETA(DisplayName = "Projectile (No Gravity, Constant Speed)"),
	Hover       UMETA(DisplayName = "Hover/Stationary")
};

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class PROJECT_CIRCLE_API UPcGravityMovementComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UPcGravityMovementComponent();

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

public:
	// --- MOVEMENT CONFIG ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement Config")
	EPcMovementMode MovementMode = EPcMovementMode::GroundUnit;

	/** If true, character rotates to face velocity. If false (Player), they face Mouse/Input. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement Config")
	bool bOrientRotationToMovement = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement Config")
	float MaxSpeed = 1200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement Config")
	float Acceleration = 2000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement Config")
	float Deceleration = 2000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement Config")
	float TurnRate = 360.0f;

	// --- PHYSICS SETTINGS ---
	/** Positive = Pulls down to surface. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
	float GravityScale = 1000.0f;

	/** Distance to snap to floor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
	float SnapDistance = 100.0f;

	// --- ORBITAL LOCK (Projectiles) ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
	bool bUseFixedRadius = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
	float FixedRadius = 0.0f;

	/** Call this on spawn to lock the projectile to its current distance from center. */
	UFUNCTION(BlueprintCallable, Category = "Physics")
	void LockCurrentAltitudeAsOrbit();

	// --- POSITIONING ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Positioning")
	float HoverHeight = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Positioning")
	float PivotOffset = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Positioning")
	float VerticalSmoothing = 10.0f;

	/** If true, bypasses smoothing and sets height instantly (For Jumping). */
	UPROPERTY(BlueprintReadWrite, Category = "Positioning")
	bool bSnapToHoverHeight = false;

	// --- DEBUG ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	bool bDrawDebug = false;

	// --- INPUT INTERFACE ---
	UFUNCTION(BlueprintCallable, Category = "Input")
	void AddInputVector(FVector WorldInputDirection);

	UFUNCTION(BlueprintCallable, Category = "Input")
	void AddImpulse(FVector Impulse);

	UFUNCTION(BlueprintCallable, Category = "Input")
	void SetVelocity(FVector NewVelocity);

	UFUNCTION(BlueprintPure)
	FVector GetCurrentVelocity() const { return Velocity; }

	UFUNCTION(BlueprintPure)
	FVector GetSurfaceNormal() const { return CurrentSurfaceNormal; }

	UFUNCTION(BlueprintPure)
	bool IsFalling() const { return bIsFalling; }

private:
	// --- STATE ---
	FVector Velocity = FVector::ZeroVector;
	FVector CurrentInput = FVector::ZeroVector;
	
	bool bIsFalling = false;
	
	// Surface Data
	UPROPERTY()
	APcPlanet* CurrentPlanet;
	FVector CurrentSurfaceNormal = FVector::UpVector;
	FVector SurfaceHitLocation = FVector::ZeroVector;
	bool bSurfaceFound = false;

	void UpdateSurfaceInfo();
	void ApplyPhysics(float DeltaTime);
};