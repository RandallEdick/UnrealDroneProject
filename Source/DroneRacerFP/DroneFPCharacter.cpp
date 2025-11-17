#include "DroneFPCharacter.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"

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

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(
            -1, 5.0f, FColor::Green,
            FString::Printf(TEXT("DroneFPCharacter BeginPlay: %s, Controller=%s"),
                *GetName(), *GetNameSafe(Controller))
        );
    }
    ApplyMappingContext();
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

    if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent))
    {


        // (Throttle/Yaw/Pitch/Roll can come later)
    }

    if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent))
    {
        
        //EIC->BindAction(IA_Move, ETriggerEvent::Triggered, this, &ThisClass::Move);
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
}

void ADroneFPCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (DeltaTime <= 0.f)
    {
        return;
    }

    // ===== 1) Update orientation from yaw/pitch/roll inputs (DJI Mode 2) =====

    const float dPitch = PitchInput * PitchRateDeg * DeltaTime; // nose up/down
    const float dYaw = YawInput * YawRateDeg * DeltaTime; // rotate about vertical
    const float dRoll = RollInput * RollRateDeg * DeltaTime; // bank about longitudinal

    const FRotator DeltaRot(dPitch, dYaw, dRoll);
    AddActorLocalRotation(DeltaRot);

    // ===== 2) Compute lift magnitude from throttle input =====

    // ThrottleInput is -1..+1 (Left Stick Y)
    // Map stick to 0..1 for lift (0 = no lift, 1 = MaxLiftForce)
    const float Throttle01 = FMath::Clamp((ThrottleInput + 1.f) * 0.5f, 0.f, 1.f);
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
    }
}

// ===== Input handlers: store latest stick values =====

void ADroneFPCharacter::Throttle(const FInputActionValue& Value)
{
    float Raw = Value.Get<float>();      // -1..+1
    if (FMath::Abs(Raw) < 0.1f)         // deadzone
        Raw = 0.f;

    ThrottleInput = Raw;

    UE_LOG(LogTemp, Warning, TEXT("ThrottleInput = %.3f"), ThrottleInput);
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