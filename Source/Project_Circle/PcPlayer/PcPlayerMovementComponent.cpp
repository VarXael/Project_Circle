#include "PcPlayerMovementComponent.h"
#include "GameFramework/Character.h"

UPcPlayerMovementComponent::UPcPlayerMovementComponent()
{
    // Let's make sure our component can tick, so our physics code runs every frame.
    PrimaryComponentTick.bCanEverTick = true;
}

void UPcPlayerMovementComponent::PhysWalking(float deltaTime, int32 Iterations)
{
    if (deltaTime < MIN_TICK_TIME)
    {
        return;
    }

    // First, apply friction. This slows us down if we're not trying to move.
    ApplyGroundFriction(deltaTime);

    // Get the direction the player wants to move.
    FVector WishDirection = Acceleration.GetSafeNormal();
    float WishSpeed = Acceleration.Size();
    
    // If the player isn't trying to move, Acceleration will be zero.
    if (WishSpeed <= 0.0f)
    {
        // Let the base class handle the rest.
        Super::PhysWalking(deltaTime, Iterations);
        return;
    }

    // Now, accelerate the character based on their input.
    AccelerateGround(WishDirection, WishSpeed, deltaTime);

    // After our custom acceleration, let the base walking physics do its job.
    Super::PhysWalking(deltaTime, Iterations);
}

void UPcPlayerMovementComponent::PhysFalling(float deltaTime, int32 Iterations)
{
    // Get player's desired movement direction.
    FVector WishDirection = Acceleration.GetSafeNormal();
    float WishSpeed = Acceleration.Size();

    // Apply our new, simpler air control logic.
    AccelerateAir(WishDirection, WishSpeed, deltaTime);

    // Let the base class handle gravity and the rest.
    Super::PhysFalling(deltaTime, Iterations);
}

void UPcPlayerMovementComponent::ApplyGroundFriction(float DeltaTime)
{
    float Speed = Velocity.Size2D();
    if (Speed <= 0.0f) return;

    // We need to make sure we don't apply so much friction that we reverse direction.
    float Drop = Speed * GroundDecelRate * DeltaTime;
    float NewSpeed = FMath::Max(0.0f, Speed - Drop);

    if (NewSpeed != Speed)
    {
        // Apply the new speed, keeping the direction.
        Velocity *= NewSpeed / Speed;
    }
}

void UPcPlayerMovementComponent::AccelerateGround(const FVector& WishDirection, float WishSpeed, float DeltaTime)
{
    // This projects our current velocity onto the direction we want to go.
    float CurrentSpeed = FVector::DotProduct(Velocity, WishDirection);
    
    // This is how much speed we need to add to reach our max speed.
    float AddSpeed = MaxWalkSpeed - CurrentSpeed;

    // If we are already at or above max speed, we don't need to add more.
    if (AddSpeed <= 0)
    {
        return;
    }

    // Calculate the acceleration for this frame, making sure not to overshoot.
    float AccelSpeed = GroundAccel * MaxWalkSpeed * DeltaTime;
    AccelSpeed = FMath::Min(AccelSpeed, AddSpeed);

    // Add the new velocity.
    Velocity += AccelSpeed * WishDirection;
}

void UPcPlayerMovementComponent::AccelerateAir(const FVector& WishDirection, float WishSpeed, float DeltaTime)
{
    // If player isn't trying to move, do nothing.
    if (WishSpeed <= 0.0f)
    {
        return;
    }

    // Get current velocity on the horizontal plane.
    FVector PlayerVel = FVector(Velocity.X, Velocity.Y, 0);

    // This calculates how much we need to accelerate to match the player's input.
    FVector Accel = WishDirection * AirControlStrength * DeltaTime;

    // Add the new acceleration to our velocity!
    Velocity += Accel;

    // Finally, we clamp the horizontal speed to make sure we don't go too fast.
    FVector NewVel2D = FVector(Velocity.X, Velocity.Y, 0);
    NewVel2D = NewVel2D.GetClampedToMaxSize(GetMaxSpeed());
    Velocity.X = NewVel2D.X;
    Velocity.Y = NewVel2D.Y;
}