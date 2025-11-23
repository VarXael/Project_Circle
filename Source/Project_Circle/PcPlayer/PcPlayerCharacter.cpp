#include "PcPlayerCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Camera/CameraComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"

APcPlayerCharacter::APcPlayerCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	CapsuleComp = CreateDefaultSubobject<UCapsuleComponent>(TEXT("CapsuleComp"));
	CapsuleComp->InitCapsuleSize(34.0f, 88.0f);
	CapsuleComp->SetCollisionProfileName(TEXT("Pawn"));
	CapsuleComp->SetSimulatePhysics(false);
	RootComponent = CapsuleComp;

	CameraComp = CreateDefaultSubobject<UCameraComponent>(TEXT("CameraComp"));
	CameraComp->SetupAttachment(CapsuleComp);
	CameraComp->SetRelativeLocation(FVector(0, 0, 60.0f));
	CameraComp->bUsePawnControlRotation = false;
}

void APcPlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// 1. Add the Mapping Context (Enable the keys)
	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			// Priority 0 is fine for default movement
			if (DefaultMappingContext)
			{
				Subsystem->AddMappingContext(DefaultMappingContext, 0);
			}
		}
	}

	// 2. Bind the Actions (Connect the logic)
	// We cast to UEnhancedInputComponent to access the new binding functions
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		// Bind MOVE (Triggered = runs every frame key is held; Completed = runs when released)
		if (MoveAction)
		{
			EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &APcPlayerCharacter::Move);
			EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Completed, this, &APcPlayerCharacter::Move);
		}

		// Bind LOOK
		if (LookAction)
		{
			EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &APcPlayerCharacter::Look);
		}
	}
}

void APcPlayerCharacter::Move(const FInputActionValue& Value)
{
	// Value is a Vector2D (X = Forward/Back, Y = Right/Left)
	FVector2D MovementVector = Value.Get<FVector2D>();
	
	// Store it for the Tick function to use
	CurrentInput.X = MovementVector.X;
	CurrentInput.Y = MovementVector.Y;
}

void APcPlayerCharacter::Look(const FInputActionValue& Value)
{
	// Value is a Vector2D (X = Mouse X, Y = Mouse Y)
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	// 1. Yaw (Left/Right) - Rotate the CAPSULE
	if (LookAxisVector.X != 0.0f)
	{
		AddActorLocalRotation(FRotator(0, LookAxisVector.X, 0));
	}

	// 2. Pitch (Up/Down) - Rotate the CAMERA
	if (LookAxisVector.Y != 0.0f)
	{
		if (CameraComp)
		{
			FRotator CurrentRot = CameraComp->GetRelativeRotation();
			// NOTE: We usually invert Y for mouse look, depends on your Input Action modifiers
			float NewPitch = FMath::Clamp(CurrentRot.Pitch + LookAxisVector.Y, -85.0f, 85.0f);
			CameraComp->SetRelativeRotation(FRotator(NewPitch, 0, 0));
		}
	}
}

void APcPlayerCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// --- GRAVITY & ORIENTATION ---
	FVector ActorLoc = GetActorLocation();
	FVector GravityDir;

	if (bIsVoidInside)
		GravityDir = (ActorLoc - SphereCenter).GetSafeNormal();
	else
		GravityDir = (SphereCenter - ActorLoc).GetSafeNormal();

	FVector TargetUp = -GravityDir; 
	
	// Align Capsule
	FQuat CurrentRot = GetActorQuat();
	FQuat TargetRot = FQuat::FindBetweenNormals(GetActorUpVector(), TargetUp) * CurrentRot;
	SetActorRotation(FQuat::Slerp(CurrentRot, TargetRot, 15.0f * DeltaTime));

	// --- MOVEMENT ---
	FVector Forward = GetActorForwardVector();
	FVector Right = GetActorRightVector();
	FVector DesiredMove = (Forward * CurrentInput.X + Right * CurrentInput.Y).GetSafeNormal();

	if (bIsGrounded)
	{
		float TargetSpeed = MoveSpeed;
		FVector CurrentPlaneVel = Velocity - (Velocity | GravityDir) * GravityDir;
		FVector TargetVel = DesiredMove * TargetSpeed;
		
		// Snappy movement on ground
		FVector NewPlaneVel = FMath::VInterpConstantTo(CurrentPlaneVel, TargetVel, DeltaTime, 2000.0f);
		Velocity = NewPlaneVel + (Velocity | GravityDir) * GravityDir;
	}
	else
	{
		// Air Control
		Velocity += DesiredMove * 500.0f * DeltaTime;
	}

	Velocity += GravityDir * GravityStrength * DeltaTime;

	// --- COLLISION ---
	FVector DeltaMove = Velocity * DeltaTime;
	FHitResult Hit;
	AddActorWorldOffset(DeltaMove, true, &Hit);

	bIsGrounded = false;

	if (Hit.IsValidBlockingHit())
	{
		Velocity = SlideAlongSurface(Velocity, Hit.Normal);

		float FloorDot = FVector::DotProduct(Hit.Normal, TargetUp);
		if (FloorDot > 0.7f) bIsGrounded = true;

		if (Hit.PenetrationDepth > 0.0f)
			AddActorWorldOffset(Hit.Normal * Hit.PenetrationDepth, false);
	}
}

FVector APcPlayerCharacter::SlideAlongSurface(const FVector& InVelocity, const FVector& Normal)
{
	return InVelocity - Normal * FVector::DotProduct(InVelocity, Normal);
}