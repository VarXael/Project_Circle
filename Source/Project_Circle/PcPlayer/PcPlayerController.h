#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "PcPlayerController.generated.h"

class UInputMappingContext;

UCLASS()
class PROJECT_CIRCLE_API APcPlayerController : public APlayerController
{
	GENERATED_BODY()
	
protected:
	virtual void BeginPlay() override;

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputMappingContext* DefaultMappingContext;
};