#include "PcQHealthComponent.h"

UPcQHealthComponent::UPcQHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UPcQHealthComponent::BeginPlay()
{
	Super::BeginPlay();
	CurrentHealth = MaxHealth;

	if (AActor* Owner = GetOwner()) {
		Owner->OnTakeAnyDamage.AddDynamic(this, &UPcQHealthComponent::HandleTakeAnyDamage);
	}
}

void UPcQHealthComponent::HandleTakeAnyDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType, AController* InstigatedBy, AActor* DamageCauser)
{
	if (Damage <= 0.f || IsDead()) return;

	CurrentHealth = FMath::Clamp(CurrentHealth - Damage, 0.f, MaxHealth);
	OnHealthChanged.Broadcast(CurrentHealth);

	if (IsDead()) {
		OnDied.Broadcast();
	}
}