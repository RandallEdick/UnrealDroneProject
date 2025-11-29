#include "DroneFPCharacter.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"

ADroneFPCharacter::ADroneFPCharacter()
{
    PrimaryActorTick.bCanEverTick = true;

    // First-person camera attached to capsule
    FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
    FirstPersonCamera->SetupAttachment(GetCapsuleComponent());
    FirstPersonCamera->SetRelativeLocation(FVector(0.f, 0.f, 64.f));
    FirstPersonCamera->bUsePawnControlRotation = false; // we rotate the whole actor


    //get the arms mesh
    Mesh1P = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("CharacterMesh1P"));

    //resize the collision capsule
    GetCapsuleComponent()->InitCapsuleSize(12.0f, 7.0f);

    // Character movement component: let us handle physics
    UCharacterMovementComponent* MoveComp = GetCharacterMovement();
    if (MoveComp)
    {
        MoveComp->SetMovementMode(MOVE_Flying);
        MoveComp->GravityScale = 0.f;      // We'll apply gravity ourselves
        MoveComp->Velocity = FVector::ZeroVector;
        MoveComp->bOrientRotationToMovement = false;
        MoveComp->bUseControllerDesiredRotation = false;
        MoveComp->Deactivate();            // We drive movement via AddActorWorldOffset
    }

    // We control rotation directly on the actor, not via controller yaw/pitch/roll flags
    bUseControllerRotationYaw = false;
    bUseControllerRotationPitch = false;
    bUseControllerRotationRoll = false;

    AutoPossessPlayer = EAutoReceiveInput::Player0;
}

void ADroneFPCharacter::BeginPlay()
{
    Super::BeginPlay();

    Health = MaxHealth;
    if (Mesh1P)
    {
        Mesh1P->SetHiddenInGame(true);
        Mesh1P->SetVisibility(false, true);
        Mesh1P->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Mesh1P->Deactivate();
        Mesh1P->DestroyComponent();   // fully removes it at runtime
    }

    //ApplyMappingContext();
    UE_LOG(LogTemp, Warning, TEXT("ADroneFPCharacter::BeginPlay"));

    if (APlayerController* PC = Cast<APlayerController>(GetController()))
    {
        if (ULocalPlayer* LP = PC->GetLocalPlayer())
        {
            if (UEnhancedInputLocalPlayerSubsystem* Subsys =
                ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LP))
            {
                if (IMC_Default)
                {
                    Subsys->AddMappingContext(IMC_Default, 0);
                    UE_LOG(LogTemp, Warning, TEXT("Added IMC_Default to EnhancedInput subsystem"));
                }
                else
                {
                    UE_LOG(LogTemp, Error, TEXT("IMC_Default is NULL on DroneFPCharacter!"));
                }
            }
        }
    }
}
void ADroneFPCharacter::ApplyMappingContext()
{
    APlayerController* PC = Cast<APlayerController>(Controller);
    if (!PC) return;

    ULocalPlayer* LP = PC->GetLocalPlayer();
    if (!LP) return;

    UEnhancedInputLocalPlayerSubsystem* Subsys =
        ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LP);

    if (!Subsys) return;

    if (DefaultMappingContext)
    {
        // Optional: clear others first
        Subsys->ClearAllMappings();
        Subsys->AddMappingContext(DefaultMappingContext, /*Priority*/ 0);

        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(
                -1, 5.0f, FColor::Cyan,
                FString::Printf(TEXT("Applied Mapping Context: %s"),
                    *DefaultMappingContext->GetName()));
        }
    }
}

void ADroneFPCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);


    UE_LOG(LogTemp, Warning, TEXT("ADroneFPCharacter::SetupPlayerInputComponent called"));
    if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent))
    {
        

        if (IA_Throttle)
        {
            EIC->BindAction(IA_Throttle, ETriggerEvent::Triggered,
                this, &ADroneFPCharacter::Throttle);
        }

        if (IA_Yaw)
        {
            EIC->BindAction(IA_Yaw, ETriggerEvent::Triggered,
                this, &ADroneFPCharacter::Yaw);
        }

        if (IA_Pitch)
        {
            EIC->BindAction(IA_Pitch, ETriggerEvent::Triggered,
                this, &ADroneFPCharacter::Pitch);
        }

        if (IA_Roll)
        {
            EIC->BindAction(IA_Roll, ETriggerEvent::Triggered,
                this, &ADroneFPCharacter::Roll);
        }

    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("PlayerInputComponent is NOT an EnhancedInputComponent!"));
    }
}

void ADroneFPCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (!bThrottleArmed)return;

    if (DeltaTime <= 0.f)return;


    // ===== 1) Update orientation from yaw/pitch/roll inputs (DJI Mode 2) =====

    const float dPitch = PitchInput * PitchRateDeg * DeltaTime; // nose up/down
    const float dYaw = YawInput * YawRateDeg * DeltaTime; // rotate about vertical
    const float dRoll = RollInput * RollRateDeg * DeltaTime; // bank about longitudinal

    const FRotator DeltaRot(dPitch, dYaw, dRoll);
    AddActorLocalRotation(DeltaRot);

    // ===== 2) Compute lift magnitude from throttle input =====

    // ThrottleInput is -1..+1 (Left Stick Y)
    // Map stick to 0..1 for lift (0 = no lift, 1 = MaxLiftForce)
    const float LiftMag = Throttle01 * MaxLiftForce; // Newtons

    // ===== 3) Compute forces in world space =====

    // Drone local up (lift direction), and world up
    const FVector UpLocal = GetActorUpVector();
    const FVector WorldUp = FVector::UpVector;

    // Lift along local up
    const FVector Lift = UpLocal * LiftMag;

    // Gravity from world settings (gravity Z is negative)
    const float GravityZ = GetWorld() ? GetWorld()->GetGravityZ() : -980.f;
    const FVector Gravity = FVector(0.f, 0.f, GravityZ * Mass);

    // Simple linear drag opposite to velocity
    const FVector Drag = -DragCoeff * Velocity;

    // Total acceleration
    const FVector Accel = (Lift + Gravity + Drag) / FMath::Max(Mass, KINDA_SMALL_NUMBER);

    // ===== 4) Integrate velocity and position =====

    Velocity += Accel * DeltaTime;

    const FVector Delta = Velocity * DeltaTime;

    UE_LOG(LogTemp, Warning, TEXT("Throttle01=%.3f  Lift=%s  Velocity=%s  LiftMag=%.3f" ),
        Throttle01,
        *Lift.ToString(),
        *Velocity.ToString(),
        LiftMag
        );
    // Use sweep so we still get collision
    FHitResult Hit;
    AddActorWorldOffset(Delta, true, &Hit);

    if (Hit.IsValidBlockingHit())
    {
        // Simple slide along surface: remove component of velocity into the normal
        const FVector Normal = Hit.Normal.GetSafeNormal();
        const float Vn = FVector::DotProduct(Velocity, Normal);
        if (Vn < 0.f)
        {
            Velocity -= Normal * Vn;
        }

        HandleImpactDamage(Hit);
    }
}
void ADroneFPCharacter::HandleImpactDamage(const FHitResult& Hit)
{
    if (!Hit.IsValidBlockingHit()) return;
    if (Health <= 0.f) return;

    const FVector Normal = Hit.Normal.GetSafeNormal();

    // Velocity is in cm/s. We want component INTO the surface.
    const float Vn = FVector::DotProduct(Velocity, Normal);
    const float ImpactSpeedCm = FMath::Max(-Vn, 0.f); // only if moving into the surface

    if (ImpactSpeedCm <= KINDA_SMALL_NUMBER)
    {
        return; // grazing / sliding, no real impact
    }

    // Convert to m/s for energy calculation
    const float ImpactSpeedM = ImpactSpeedCm / 100.f;

    // Kinetic energy-ish: 0.5 * m * v^2
    const float ImpactEnergy = 0.5f * Mass * ImpactSpeedM * ImpactSpeedM;

    // Hardness multiplier based on what we hit (1.0 = neutral)
    const float Hardness = GetSurfaceHardness(Hit);

    // Map energy range to 0..1 damage factor
    const float Damage01 = FMath::GetMappedRangeValueClamped(
        FVector2D(MinEnergyForDamage, MaxEnergyForMaxDamage),
        FVector2D(0.f, 1.f),
        ImpactEnergy
    );

    const float Damage = Damage01 * MaxDamagePerImpact * Hardness;

    if (Damage > 0.f)
    {
        ApplyDamageToDrone(Damage);

        UE_LOG(LogTemp, Warning,
            TEXT("Impact: Speed=%.1f cm/s (%.2f m/s), Energy=%.2f, Hardness=%.2f, Damage=%.2f, Health=%.1f/%.1f"),
            ImpactSpeedCm, ImpactSpeedM, ImpactEnergy, Hardness, Damage, Health, MaxHealth);
    }
}

float ADroneFPCharacter::GetSurfaceHardness(const FHitResult& Hit) const
{
    // Default if nothing special
    float Hardness = 1.0f;

    if (UPhysicalMaterial* PhysMat = Hit.PhysMaterial.Get())
    {
        EPhysicalSurface SurfaceType = UPhysicalMaterial::DetermineSurfaceType(PhysMat);

         //You can customize these in Project Settings -> Physics -> Physical Surfaces
        switch (SurfaceType)
        {
        case SurfaceType1: // FleshDefault
            Hardness = 0.3f;
            break;

        case SurfaceType2: // Wood
            Hardness = 0.7f;
            break;

        case SurfaceType3: // Metal
        case SurfaceType4: // Concrete
            Hardness = 1.5f;
            break;

        default:
            Hardness = 1.0f;
            break;
        }
    }

    return Hardness;
}

void ADroneFPCharacter::ApplyDamageToDrone(float DamageAmount)
{
    if (DamageAmount <= 0.f || Health <= 0.f) return;

    Health = FMath::Clamp(Health - DamageAmount, 0.f, MaxHealth);

    if (Health <= 0.f)
    {
        OnDroneDestroyed();
    }
}

void ADroneFPCharacter::OnDroneDestroyed()
{
    UE_LOG(LogTemp, Warning, TEXT("Drone destroyed!"));

    // Simple behavior: disarm and stop
    bThrottleArmed = false;
    Velocity = FVector::ZeroVector;

    // You could also:
    // - Enable SimulatePhysics on mesh and let it ragdoll
    // - Trigger explosion FX
    // - Restart level, etc.
}

// ===== Input handlers: store latest stick values =====

void ADroneFPCharacter::Throttle(const FInputActionValue& Value)
{
    float Raw = Value.Get<float>();      // -1..+1
    if (FMath::Abs(Raw) < 0.1f)         // deadzone
        Raw = 0.f;

    Throttle01 = FMath::Clamp((Raw + 1.0f) * .5, 0.f, 1.f);

    if (!bThrottleArmed)
    {
        if (Throttle01 <= 0.01f)
        {
            bThrottleArmed = true;
            UE_LOG(LogTemp, Warning, TEXT("Throttle armed!"));
        }
        else
        {
            return;
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("Throttle01= %.3f"), Throttle01);
}


void ADroneFPCharacter::Yaw(const FInputActionValue& Value)
{
    YawInput = Value.Get<float>();
    UE_LOG(LogTemp, Warning, TEXT("YawInput = %.3f"), YawInput);
}

void ADroneFPCharacter::Pitch(const FInputActionValue& Value)
{
    PitchInput = Value.Get<float>();
    UE_LOG(LogTemp,Warning, TEXT("PitchInput = %.3f"), PitchInput);
}

void ADroneFPCharacter::Roll(const FInputActionValue& Value)
{
    RollInput = Value.Get<float>();
   UE_LOG(LogTemp, Warning, TEXT("RollInput = %.3f"), RollInput);
}
static float Deadzone1D(float v, float dz = 0.1f)
{
    return (FMath::Abs(v) < dz) ? 0.f : v;
}

void ADroneFPCharacter::Move(const FInputActionValue& Value)
{
    const FVector2D Input = Value.Get<FVector2D>();
    const float X = Deadzone1D(Input.X);
    const float Y = Deadzone1D(Input.Y);

    if (Controller && (FMath::Abs(X) > 0.f || FMath::Abs(Y) > 0.f))
    {
        const FRotator YawRot(0.f, Controller->GetControlRotation().Yaw, 0.f);
        const FVector Forward = FRotationMatrix(YawRot).GetUnitAxis(EAxis::X);
        const FVector Right = FRotationMatrix(YawRot).GetUnitAxis(EAxis::Y);

        AddMovementInput(Forward, Y);
        AddMovementInput(Right, X);
    }

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(
            -1, 0.f, FColor::Green,
            FString::Printf(TEXT("Move: X=%.2f Y=%.2f"), X, Y));
    }
}

void ADroneFPCharacter::Look(const FInputActionValue& Value)
{
    const FVector2D Input = Value.Get<FVector2D>();
    const float X = Deadzone1D(Input.X);
    const float Y = Deadzone1D(Input.Y);

    AddControllerYawInput(X);
    AddControllerPitchInput(Y);

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(
            -1, 0.f, FColor::Cyan,
            FString::Printf(TEXT("Look: X=%.2f Y=%.2f"), X, Y));
    }
}