#include "LunarAbyssPlayerController.h"
#include "CrewCharacter.h"
#include "FighterPawn.h"
#include "CorvetteHangar.h"
#include "GravityEngineComponent.h"
#include "EngineUtils.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputSubsystems.h"
#include "SlicePlaytest.h"

void ALunarAbyssPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	InputComponent->BindKey(EKeys::F, IE_Pressed, this, &ALunarAbyssPlayerController::OnInteract);
	InputComponent->BindKey(EKeys::L, IE_Pressed, this, &ALunarAbyssPlayerController::LaunchFighter);
	InputComponent->BindKey(EKeys::G, IE_Pressed, this, &ALunarAbyssPlayerController::ToggleGravity);
	InputComponent->BindKey(EKeys::H, IE_Pressed, this, &ALunarAbyssPlayerController::ToggleHangarDoors);
}

void ALunarAbyssPlayerController::OnPossess(APawn* InPawn)
{
	// Each pawn adds its own mapping context in SetupPlayerInputComponent; drop the previous pawn's.
	if (UEnhancedInputLocalPlayerSubsystem* Sub = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		Sub->ClearAllMappings();
	}
	Super::OnPossess(InPawn);
	if (ACrewCharacter* C = Cast<ACrewCharacter>(InPawn)) Crew = C;
}

ACorvetteHangar* ALunarAbyssPlayerController::FindCorvette() const
{
	for (TActorIterator<ACorvetteHangar> It(GetWorld()); It; ++It) return *It;
	return nullptr;
}

AFighterPawn* ALunarAbyssPlayerController::FindFighter() const
{
	if (ACorvetteHangar* V = FindCorvette())
	{
		if (AFighterPawn* P = V->GetParkedFighter()) return P;
	}
	for (TActorIterator<AFighterPawn> It(GetWorld()); It; ++It) return *It;
	return nullptr;
}

void ALunarAbyssPlayerController::PossessFighter(AFighterPawn* F)
{
	if (!F) { UE_LOG(LogTemp, Warning, TEXT("No fighter in the level.")); return; }
	Possess(F);
}

void ALunarAbyssPlayerController::OnInteract()
{
	APawn* P = GetPawn();
	if (Cast<AFighterPawn>(P)) { SwitchToCrew(); return; }
	if (!P) return;

	const FVector Loc = P->GetActorLocation();
	if (ACorvetteHangar* V = FindCorvette())
	{
		for (const FName S : { FName(TEXT("Seat_Pilot")), FName(TEXT("Seat_CoPilot")), FName(TEXT("Camera_Cockpit")) })
		{
			if (V->DistToSocket(S, Loc) <= InteractRange * 100.f + 90.f)
			{
				// Pilot seat: take the fighter straight out of the bay.
				AFighterPawn* F = FindFighter();
				if (F && F == V->GetParkedFighter()) V->RequestLaunch(F);
				PossessFighter(F);
				return;
			}
		}
	}
	if (AFighterPawn* F = FindFighter())
	{
		if (FVector::Dist(F->GetActorLocation(), Loc) <= FighterInteractRange * 100.f) { PossessFighter(F); return; }
	}
	UE_LOG(LogTemp, Log, TEXT("F: not near a pilot seat or the fighter (use console SwitchToFighter)."));
}

void ALunarAbyssPlayerController::SwitchToFighter() { PossessFighter(FindFighter()); }

void ALunarAbyssPlayerController::SwitchToCrew()
{
	if (!Crew) { for (TActorIterator<ACrewCharacter> It(GetWorld()); It; ++It) { Crew = *It; break; } }
	if (Crew) Possess(Crew);
}

void ALunarAbyssPlayerController::LaunchFighter()
{
	ACorvetteHangar* V = FindCorvette();
	AFighterPawn* F = FindFighter();
	if (!V || !F) return;
	V->RequestLaunch(F); // opens doors first (horn 1 s + 5.4 s), launches, closes 8 s later
	if (GetPawn() != F) PossessFighter(F);
}

void ALunarAbyssPlayerController::ToggleGravity()
{
	if (ACorvetteHangar* V = FindCorvette()) V->GravityEngine->Toggle();
}

void ALunarAbyssPlayerController::RunSlicePlaytest()
{
	if (ASlicePlaytest* T = GetWorld()->SpawnActor<ASlicePlaytest>()) T->Start(this);
}

void ALunarAbyssPlayerController::ToggleHangarDoors()
{
	if (ACorvetteHangar* V = FindCorvette()) V->ToggleHangarDoors();
}

void ALunarAbyssPlayerController::DamageGravity(float Amount)
{
	if (ACorvetteHangar* V = FindCorvette()) V->GravityEngine->ApplyGravityDamage(Amount);
}

void ALunarAbyssPlayerController::SetCorvetteSpeed(float MetresPerSecond)
{
	if (ACorvetteHangar* V = FindCorvette()) V->TargetSpeed = MetresPerSecond;
}
