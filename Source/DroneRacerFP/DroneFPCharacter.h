#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"   // <--- NEW
#include "DroneFPCharacter.generated.h"

// ------------------------------------------------------
// Forward declarations go RIGHT HERE
// ------------------------------------------------------
class UCameraComponent;
class UInputMappingContext;
class UInputAction;

UCLASS()
class DRONERACERFP_API ADroneFPCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    ADroneFPCharacter();


protected:
    virtual void BeginPlay() override;
    virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
    void ApplyDroneIMC();

    // First person camera
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UCameraComponent* FirstPersonCamera;

    // Enhanced Input

    // Mapping context that holds all our bindings
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    UInputMappingContext* DroneMappingContext;

    // Move: left stick 2D
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    UInputAction* IA_Move;

    // Look: right stick 2D
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    UInputAction* IA_Look;

    // Throttle: triggers (Axis1D)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    UInputAction* IA_Throttle;

    // Tuning
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Flight")
    float MoveSpeed = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Flight")
    float ThrottleSpeed = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Flight")
    float LookRateYaw = 120.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Flight")
    float LookRatePitch = 120.0f;

    // Enhanced Input handlers
    void Move(const FInputActionValue& Value);
    void Look(const FInputActionValue& Value);
    void Throttle(const FInputActionValue& Value);

    // Disable jump (even if something calls it)
    virtual void Jump() override;
};
