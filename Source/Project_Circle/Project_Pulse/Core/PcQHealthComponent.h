#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PcQHealthComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnHealthChanged, float, NewHealth);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDied);

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent),Blueprintable, BlueprintType )
class PROJECT_CIRCLE_API UPcQHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UPcQHealthComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health") float MaxHealth = 100.f;
	
	UFUNCTION(BlueprintPure, Category = "Health") float GetHealth() const { return CurrentHealth; }
	UFUNCTION(BlueprintPure, Category = "Health") bool IsDead() const { return CurrentHealth <= 0.f; }

	UPROPERTY(BlueprintAssignable, Category = "Events") FOnHealthChanged OnHealthChanged;
	UPROPERTY(BlueprintAssignable, Category = "Events") FOnDied OnDied;

protected:
	virtual void BeginPlay() override;
	
	UFUNCTION()
	void HandleTakeAnyDamage(AActor* DamagedActor, float Damage, const class UDamageType* DamageType, class AController* InstigatedBy, AActor* DamageCauser);

private:
	float CurrentHealth;
};