#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Project_Circle/Project_Pulse/Core/PcQHealthComponent.h"
#include "PcQEnemyBase.generated.h"

class USphereComponent;
class UStaticMeshComponent;

UCLASS()
class PROJECT_CIRCLE_API APcQEnemyBase : public AActor
{
	GENERATED_BODY()
	
public:	
	APcQEnemyBase();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<USphereComponent> CollisionComp;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<UStaticMeshComponent> MeshComp;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<UPcQHealthComponent> HealthComp;

protected:
	virtual void BeginPlay() override;

	UFUNCTION() void OnDeath();
};