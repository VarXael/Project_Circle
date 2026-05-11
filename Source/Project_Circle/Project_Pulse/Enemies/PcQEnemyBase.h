#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "PcQEnemyBase.generated.h"

class UCapsuleComponent;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;

UCLASS()
class PROJECT_CIRCLE_API APcQEnemyBase : public APawn
{
	GENERATED_BODY()

public:
	APcQEnemyBase();

	virtual void Tick(float DeltaTime) override;
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser) override;

protected:
	virtual void BeginPlay() override;

	// ── Components (Rayman Style) ──
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") UCapsuleComponent* CapsuleComp;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") UStaticMeshComponent* BodyMesh;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") UStaticMeshComponent* HeadMesh;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") UStaticMeshComponent* LeftHandMesh;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") UStaticMeshComponent* RightHandMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats") float MaxHealth = 100.f;

	// ── Visuals & Colors ──
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visuals") FLinearColor IdleColor = FLinearColor(0.05f, 0.8f, 1.0f, 1.0f); // Default Edge Color
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visuals") FLinearColor HitColor  = FLinearColor(1.0f, 0.1f, 0.2f, 1.0f); // Flashes this color when hit

private:
	UFUNCTION() void OnGameplayBeat(float BeatTime);

	float CurrentHealth;
	bool bIsDead;

	// Global Alphas
	float BeatPulseAlpha;
	float DeathScaleAlpha;

	// Independent Hit Alphas (for specific part bouncing)
	float HeadHitAlpha;
	float BodyHitAlpha;
	float LHandHitAlpha;
	float RHandHitAlpha;

	float HeadSpinVelocity;
	float LHandSpinVelocity;
	float RHandSpinVelocity;

	// Base Transforms for procedural animation
	FVector BaseHeadScale;
	FVector BaseBodyScale;
	FVector BaseLHandScale;
	FVector BaseRHandScale;

	FVector BaseLHandLoc;
	FVector BaseRHandLoc;

	FRotator CurrentHeadRotation;
	FRotator CurrentLHandRotation;
	FRotator CurrentRHandRotation;

	// Maps a Mesh to its Dynamic Material so we can color them independently
	UPROPERTY()
	TMap<UStaticMeshComponent*, UMaterialInstanceDynamic*> DynamicMaterials;

	void UpdateMeshVisuals(UStaticMeshComponent* Mesh, float HitAlpha, float DeltaTime);
};