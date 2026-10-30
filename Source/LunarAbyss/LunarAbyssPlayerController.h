#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "LunarAbyssPlayerController.generated.h"

class ACrewCharacter;
class AFighterPawn;
class ACorvetteHangar;

/** Slice controller: F = switch crew <-> fighter (near pilot seat / fighter), L = launch fighter, G = toggle gravity engine. */
UCLASS()
class LUNARABYSS_API ALunarAbyssPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	virtual void SetupInputComponent() override;
	virtual void OnPossess(APawn* InPawn) override;

	// Console commands
	UFUNCTION(Exec) void SwitchToFighter();
	UFUNCTION(Exec) void SwitchToCrew();
	UFUNCTION(Exec) void LaunchFighter();
	UFUNCTION(Exec) void ToggleGravity();
	UFUNCTION(Exec) void ToggleHangarDoors();
	/** Same handler as the F key (used by the automated play test). */
	UFUNCTION(Exec) void PressF() { OnInteract(); }
	/** Runs the scripted slice play test (ASlicePlaytest), writes Saved/Playtest/results.txt, then quits. */
	UFUNCTION(Exec) void RunSlicePlaytest();
	UFUNCTION(Exec) void DamageGravity(float Amount);
	UFUNCTION(Exec) void SetCorvetteSpeed(float MetresPerSecond);

	/** Max distance (m) from SOCKET_Seat_Pilot / SOCKET_Camera_Cockpit, or from the parked fighter, for F to work. */
	UPROPERTY(EditAnywhere, Category = "Slice") float InteractRange = 2.5f;
	UPROPERTY(EditAnywhere, Category = "Slice") float FighterInteractRange = 8.f;

private:
	void OnInteract();
	ACorvetteHangar* FindCorvette() const;
	AFighterPawn* FindFighter() const;
	void PossessFighter(AFighterPawn* F);

	UPROPERTY() TObjectPtr<ACrewCharacter> Crew;
};
