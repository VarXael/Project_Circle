// ==========================================
// FILE: PcFloatingScore.h
// PATH: Source/Project_Circle/UI/PcFloatingScore.h
// ==========================================
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PcFloatingScore.generated.h"

class UTextRenderComponent;

UCLASS()
class PROJECT_CIRCLE_API APcFloatingScore : public AActor
{
	GENERATED_BODY()
	
public:	
	APcFloatingScore();
	virtual void Tick(float DeltaTime) override;

	void InitializeScore(float ScoreValue, FVector HitLocation);

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	USceneComponent* RootScene;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	UTextRenderComponent* TextComp;

	// --- CONFIGURATION ---
	
	// Initial pop strength
	UPROPERTY(EditAnywhere, Category = "Juice|Physics")
	float MinImpulse = 800.0f; 

	UPROPERTY(EditAnywhere, Category = "Juice|Physics")
	float MaxImpulse = 1200.0f;

	// Pulls the text down to create the ARC
	UPROPERTY(EditAnywhere, Category = "Juice|Physics")
	float SimulatedGravity = 2000.0f;

	// Air resistance (Higher = stops horizontal movement faster)
	// Keeps the text "not too far away"
	UPROPERTY(EditAnywhere, Category = "Juice|Physics")
	float Drag = 3.0f; 

	// Size animation
	UPROPERTY(EditAnywhere, Category = "Juice|Scale")
	FVector StartScale = FVector(0.0f);

	UPROPERTY(EditAnywhere, Category = "Juice|Scale")
	FVector PeakScale = FVector(3.0f); 

	UPROPERTY(EditAnywhere, Category = "Juice|Scale")
	FVector EndScale = FVector(2.0f); 

	UPROPERTY(EditAnywhere, Category = "Juice")
	float LifeTime = 1.2f;

private:
	float TimeAlive = 0.0f;
	FVector CurrentVelocity;
	bool bInitialized = false;
};