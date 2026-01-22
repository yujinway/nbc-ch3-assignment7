#include "MyGameModeBase.h"
#include "MyDrone.h"
#include "MyPlayerController.h"

AMyGameModeBase::AMyGameModeBase()
{
	DefaultPawnClass = AMyDrone::StaticClass();
	PlayerControllerClass = AMyPlayerController::StaticClass();
}