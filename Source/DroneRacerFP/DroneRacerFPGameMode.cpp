// Copyright Epic Games, Inc. All Rights Reserved.

#include "DroneRacerFPGameMode.h"
#include "DroneRacerFPCharacter.h"
#include "UObject/ConstructorHelpers.h"

ADroneRacerFPGameMode::ADroneRacerFPGameMode()
	: Super()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnClassFinder(TEXT("/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter"));
	DefaultPawnClass = PlayerPawnClassFinder.Class;

}
