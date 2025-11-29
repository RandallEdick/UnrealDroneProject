#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "DroneFPCharacter.generated.h"

class UCameraComponent;
class UInputAction;

/**
 * Physics-based first-person drone character, DJI Mode 2 controls.
 *
 * Left Stick:
 *   Y: Throttle (up/down)
 *   X: Yaw (rotate around vertical axis)
 *
 * Right Stick:
 *   Y: Pitch (tilt nose up/down)
 *   X: Roll (bank left/right, rotation about longitudinal axis)
 */
UCLASS()
class DRONERACERFP_API ADroneFPCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    ADroneFPCharacter();

    virtual void Tick(float DeltaTime) override;

protected:
    virtual void BeginPlay() override;
    virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

    /** First person camera */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UCameraComponent* FirstPersonCamera;

    // ===== Enhanced Input Actions (set in BP_DroneFPCharacter) =====
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    UInputMappingContext* IMC_Default;

    /** Left Stick Y: Throttle */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    UInputAction* IA_Throttle;

    /** Left Stick X: Yaw */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    UInputAction* IA_Yaw;

    /** Right Stick Y: Pitch */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    UInputAction* IA_Pitch;

    /** Right Stick X: Roll */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    UInputAction* IA_Roll;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    UInputMappingContext* DefaultMappingContext;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    UInputAction* IA_Move;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    UInputAction* IA_Look;

    // ===== Raw input state (normalized stick values) =====

    /** Left Stick Y (-1..+1) */
    float ThrottleInput = 0.f;

    /** Left Stick X (-1..+1) */
    float YawInput = 0.f;

    /** Right Stick Y (-1..+1) */
    float PitchInput = 0.f;

    /** Right Stick X (-1..+1) */
    float RollInput = 0.f;

    // ===== Physical parameters =====

    /** Drone mass in kg */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Flight|Physics")
    float Mass = .7f;

    /** Maximum lift force at full throttle (Newtons) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Flight|Physics")
    float MaxLiftForce = 2800.0f;  //4 x lift force per motor of Kgf per motor.

    /** Linear drag coefficient */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Flight|Physics")
    float DragCoeff = 1.0f;

    /** Pitch rate (deg/s) at full stick */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Flight|Physics")
    float PitchRateDeg = 120.0f;

    /** Roll rate (deg/s) at full stick */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Flight|Physics")
    float RollRateDeg = 120.0f;

    /** Yaw rate (deg/s) at full stick */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Flight|Physics")
    float YawRateDeg = 90.0f;

    /** Current world-space velocity of the drone (m/s) */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Flight|Physics")
    FVector Velocity = FVector::ZeroVector;

    // Health / damage
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flight|Health")
    float MaxHealth = 100.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Flight|Health")
    float Health = 100.f;

    // Max damage a *single* impact can do (before hardness multiplier)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flight|Health")
    float MaxDamagePerImpact = 50.f;

    // Minimum & maximum impact energy for mapping to [0..1] damage
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flight|Health")
    float MinEnergyForDamage = 5.f;      // J-ish
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flight|Health")
    float MaxEnergyForMaxDamage = 100.f; // J-ish

    // ===== Input handlers (Enhanced Input) =====

    void Throttle(const FInputActionValue& Value);
    void Yaw(const FInputActionValue& Value);
    void Pitch(const FInputActionValue& Value);
    void Roll(const FInputActionValue& Value);

    void Move(const FInputActionValue& Value);
    void Look(const FInputActionValue& Value);

    void HandleImpactDamage(const FHitResult& Hit);
    float GetSurfaceHardness(const FHitResult& Hit) const;
    void ApplyDamageToDrone(float DamageAmount);
    void OnDroneDestroyed();

private:
    void ApplyMappingContext();
    float Throttle01;
    bool bThrottleArmed = false;

    UPROPERTY(VisibleDefaultsOnly, Category = Mesh)
    USkeletalMeshComponent* Mesh1P;
};
