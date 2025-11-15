#include "DroneFPCharacter.h"

#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Components/InputComponent.h"
#include "Components/CapsuleComponent.h"

#include "EnhancedInputComponent.h"          // <<---
#include "EnhancedInputSubsystems.h"        // <<---
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Engine/LocalPlayer.h"

ADroneFPCharacter::ADroneFPCharacter()
{
    PrimaryActorTick.bCanEverTick = true;

    FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
    FirstPersonCamera->SetupAttachment(GetCapsuleComponent());
    FirstPersonCamera->SetRelativeLocation(FVector(0.f, 0.f, 64.f));
    FirstPersonCamera->bUsePawnControlRotation = true;

    UCharacterMovementComponent* MoveComp = GetCharacterMovement();
    MoveComp->DefaultLandMovementMode = MOVE_Flying;
    MoveComp->SetMovementMode(MOVE_Flying);
    MoveComp->GravityScale = 0.0f;
    MoveComp->BrakingFrictionFactor = 0.0f;
    MoveComp->bOrientRotationToMovement = false;

    bUseControllerRotationYaw = true;
    bUseControllerRotationPitch = true;

    AutoPossessPlayer = EAutoReceiveInput::Player0;
}

void ADroneFPCharacter::BeginPlay()
{
    Super::BeginPlay();

    UE_LOG(LogTemp, Warning, TEXT("DroneFPCharacter BeginPlay: %s, Controller=%s"),
        *GetName(), *GetNameSafe(Controller));

    // Add the mapping context to the local player subsystem
    if (APlayerController* PC = Cast<APlayerController>(Controller))
    {
        if (ULocalPlayer* LP = PC->GetLocalPlayer())
        {
            if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
                ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LP))
            {
                if (DroneMappingContext)
                {
                    Subsystem->AddMappingContext(DroneMappingContext, 0);
                }
            }
        }
    }
}
void ADroneFPCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent))
    {
        if (IA_Move)
        {
            EIC->BindAction(IA_Move, ETriggerEvent::Triggered, this, &ADroneFPCharacter::Move);
        }

        if (IA_Look)
        {
            EIC->BindAction(IA_Look, ETriggerEvent::Triggered, this, &ADroneFPCharacter::Look);
        }

        if (IA_Throttle)
        {
            EIC->BindAction(IA_Throttle, ETriggerEvent::Triggered, this, &ADroneFPCharacter::Throttle);
        }
    }
}
void ADroneFPCharacter::Move(const FInputActionValue& Value)
{
    const FVector2D Input = Value.Get<FVector2D>(); // X = right, Y = forward

    if (Controller)
    {
        const FRotator ControlRot = Controller->GetControlRotation();
        const FVector Forward = ControlRot.Vector(); // uses pitch & yaw
        const FVector Right = FRotationMatrix(ControlRot).GetScaledAxis(EAxis::Y);

        AddMovementInput(Forward, Input.Y * MoveSpeed);
        AddMovementInput(Right, Input.X * MoveSpeed);
    }
}

void ADroneFPCharacter::Look(const FInputActionValue& Value)
{
    const FVector2D Input = Value.Get<FVector2D>(); // X = yaw, Y = pitch

    AddControllerYawInput(Input.X * LookRateYaw * GetWorld()->GetDeltaSeconds());
    AddControllerPitchInput(Input.Y * LookRatePitch * GetWorld()->GetDeltaSeconds());
}

void ADroneFPCharacter::Throttle(const FInputActionValue& Value)
{
    const float Up = Value.Get<float>(); // -1 .. +1
    if (GEngine)
        GEngine->AddOnScreenDebugMessage(-1, 0.f, FColor::Cyan, FString::Printf(TEXT("Throttle: %.2f"), Up));

    if (FMath::Abs(Up) > KINDA_SMALL_NUMBER)
    {
        AddMovementInput(FVector::UpVector, Up * ThrottleSpeed);
    }
}

void ADroneFPCharacter::Jump()
{
    // No jump in drone racing
}

void ADroneFPCharacter::ApplyDroneIMC()
{
    if (APlayerController* PC = Cast<APlayerController>(Controller))
    {
        if (ULocalPlayer* LP = PC->GetLocalPlayer())
        {
            if (UEnhancedInputLocalPlayerSubsystem* Sub =
                ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LP))
            {
                Sub->ClearAllMappings();  // removes IMC_Default and any template stuff

                if (DroneMappingContext)
                {
                    UE_LOG(LogTemp, Warning, TEXT("Applying Mapping Context: %s"),
                        *DroneMappingContext->GetName());

                    Sub->AddMappingContext(DroneMappingContext, 100); // high priority
                }
                else
                {
                    UE_LOG(LogTemp, Warning, TEXT("DroneMappingContext is NULL!"));
                }
            }
        }
    }
}