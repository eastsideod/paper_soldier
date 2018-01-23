// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "paper_soldierGameMode.h"
#include "paper_soldierCharacter.h"
#include "UObject/ConstructorHelpers.h"

Apaper_soldierGameMode::Apaper_soldierGameMode()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/ThirdPersonCPP/Blueprints/ThirdPersonCharacter"));
	if (PlayerPawnBPClass.Class != NULL)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}
}
