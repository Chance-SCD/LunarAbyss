#include "LunarAbyssGameMode.h"
#include "CrewCharacter.h"
#include "LunarAbyssPlayerController.h"

ALunarAbyssGameMode::ALunarAbyssGameMode()
{
	DefaultPawnClass = ACrewCharacter::StaticClass();
	PlayerControllerClass = ALunarAbyssPlayerController::StaticClass();
}
