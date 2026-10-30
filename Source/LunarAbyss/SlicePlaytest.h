#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SlicePlaytest.generated.h"

class ALunarAbyssPlayerController;
class ACrewCharacter;
class AFighterPawn;
class ACorvetteHangar;

/**
 * Scripted gray-box play test (console: RunSlicePlaytest). Input is simulated by calling the same handlers the
 * keys are bound to (PressF, LaunchFighter, ToggleGravity) and AddMovementInput / Jump on the crew.
 * Results: Saved/Playtest/results.txt ([PLAYTEST] lines also in the log); screenshots: Saved/Screenshots/Playtest01/.
 */
UCLASS()
class LUNARABYSS_API ASlicePlaytest : public AActor
{
	GENERATED_BODY()

public:
	ASlicePlaytest();
	void Start(ALunarAbyssPlayerController* InPC);
	virtual void Tick(float DeltaSeconds) override;

private:
	void Log(const FString& S);
	void Result(const FString& Item, bool bPass, const FString& Detail);
	void Shot(const FString& Name, bool bUI = false);
	void Next(float Delay = 0.f) { ++Step; StepTime = -Delay; SubStep = 0; }
	void TeleportCrew(const FVector& ShipM, float YawDeg = 0.f);
	bool WalkTo(const FVector2D& ShipXYm, float Dt);
	FVector ShipToWorld(const FVector& ShipM) const;

	UPROPERTY() TObjectPtr<ALunarAbyssPlayerController> PC;
	UPROPERTY() TObjectPtr<ACrewCharacter> Crew;
	UPROPERTY() TObjectPtr<AFighterPawn> Fighter;
	UPROPERTY() TObjectPtr<ACorvetteHangar> Ship;

	int32 Step = -1, SubStep = 0, Waypoint = 0, FallIndex = 0, Passes = 0, Fails = 0;
	float MinCapClear = 1e9f, MinLadClear = 1e9f, GOffFeet = 0.f, GOffV0 = 0.f, GOffStopT = -1.f, PeakDecel = 0.f, PrevVz = 0.f; int32 SwingFrames = 0, SwingShots = 0; bool bSwingOverlap = false, bLadderBoxesBuilt = false; TArray<FBox> LadderBoxes; TArray<FVector> LadderPts; FString LadderSource; FVector HingeP = FVector::ZeroVector, HingeAxis = FVector::YAxisVector; float NearestLadZ = 0.f;
	void SwingProbe(class ALadderVolume* L);
	float StepTime = 0.f, TotalTime = 0.f, LogTimer = 0.f, Z0 = 0.f, ZMax = 0.f, H0 = 0.f, MinAbsY = 1e9f, FallStartFeet = 0.f, MaxHatchAlpha = 0.f, HatchWaitTime = 0.f, SlideLog = 0.f, HatchCrossT = -1.f;
	bool bLeftGround = false;
	TArray<FString> Lines;
	TArray<FString> Shots;
};
