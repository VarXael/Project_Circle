#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PcGravityMovementComponent.generated.h"

class APcGravityZone;

UENUM(BlueprintType)
enum class EPcMovementMode : uint8
{
	Skater      UMETA(DisplayName = "Skater (Momentum, Carving)"),
	GroundUnit  UMETA(DisplayName = "Ground Unit (High Friction, Snappy)"),
	Projectile  UMETA(DisplayName = "Projectile (Gravity Enabled, No Friction)"), 
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
	// --- CONFIG ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Project Circle | Gravity Config")
	EPcMovementMode MovementMode = EPcMovementMode::GroundUnit;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Project Circle | Gravity Config")
	bool bOrientRotationToMovement = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Project Circle | Gravity Config")
	float RotationInterpSpeed = 50.0f; 

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Project Circle | Gravity Config")
	float MaxSpeed = 1200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Project Circle | Gravity Config")
	float Acceleration = 2000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Project Circle | Gravity Config")
	float Deceleration = 2000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Project Circle | Gravity Config")
	float TurnRate = 360.0f;

	// --- PHYSICS ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Project Circle | Gravity Physics")
	float GravityScale = 1000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Project Circle | Gravity Physics")
	float SnapDistance = 100.0f;

	/** If true, forces the object to stay at 'FixedRadius' distance from the zone center. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Project Circle | Gravity Physics")
	bool bUseFixedRadius = false;

	/** The distance from the center of the Gravity Zone to lock to (if bUseFixedRadius is true). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Project Circle | Gravity Physics")
	float FixedRadius = 0.0f;

	// --- POSITIONING ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Project Circle | Gravity Positioning")
	float HoverHeight = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Project Circle | Gravity Positioning")
	float PivotOffset = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Project Circle | Gravity Positioning")
	float VerticalSmoothing = 10.0f;

	UPROPERTY(BlueprintReadWrite, Category = "Project Circle | Gravity Positioning")
	bool bSnapToHoverHeight = false;

	// --- DEBUG ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Project Circle | Debug")
	bool bDrawDebug = false;

	// --- API ---
	UFUNCTION(BlueprintCallable, Category = "Input")
	void AddInputVector(FVector WorldInputDirection);

	UFUNCTION(BlueprintCallable, Category = "Input")
	void AddImpulse(FVector Impulse);

	UFUNCTION(BlueprintCallable, Category = "Input")
	void SetVelocity(FVector NewVelocity);

	/** Automatically calculates PivotOffset based on Capsule or Mesh bounds. */
	UFUNCTION(BlueprintCallable, Category = "Setup")
	void AutoCalibratePivot();

	UFUNCTION(BlueprintPure)
	FVector GetCurrentVelocity() const { return Velocity; }

	UFUNCTION(BlueprintPure)
	FVector GetSurfaceNormal() const { return CurrentSurfaceNormal; }

	UFUNCTION(BlueprintPure)
	bool IsFalling() const { return bIsFalling; }

private:
	FVector Velocity = FVector::ZeroVector;
	FVector CurrentInput = FVector::ZeroVector;
	bool bIsFalling = false;
	bool bInZeroG = false;
	
	UPROPERTY()
	APcGravityZone* CurrentZone;

	FVector CurrentSurfaceNormal = FVector::UpVector;
	FVector SurfaceHitLocation = FVector::ZeroVector;
	bool bSurfaceFound = false;

	void UpdateSurfaceInfo();
	void ApplyPhysics(float DeltaTime);
};