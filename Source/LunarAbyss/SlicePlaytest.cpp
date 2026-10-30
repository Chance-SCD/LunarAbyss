#include "SlicePlaytest.h"
#include "LunarAbyssPlayerController.h"
#include "CrewCharacter.h"
#include "FighterPawn.h"
#include "CorvetteHangar.h"
#include "GravityEngineComponent.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "Kismet/KismetSystemLibrary.h"
#include "LadderVolume.h"

namespace
{
	constexpr float M = 100.f;
	// ST-F switchback (Atlas-V blend, ship metres): aft landing X 30.8, forward mid-landing X 32.9; lane A Y 6.4, lane B Y 7.8.
	const FVector2D DownLoop[4] = { {30.8, 6.4}, {32.9, 6.4}, {32.9, 7.8}, {30.8, 7.8} };
	const FVector2D UpLoop[4]   = { {30.8, 7.8}, {32.9, 7.8}, {32.9, 6.4}, {30.8, 6.4} };
	const float FallHeights[4] = { 2.f, 4.f, 6.f, 9.f };
}

ASlicePlaytest::ASlicePlaytest() { PrimaryActorTick.bCanEverTick = true; PrimaryActorTick.TickGroup = TG_PrePhysics; }

FVector ASlicePlaytest::ShipToWorld(const FVector& S) const
{
	const FVector Local(S.X * M, -S.Y * M, S.Z * M); // Blender ship metres -> UE cm (Y flipped)
	return Ship ? Ship->GetActorTransform().TransformPosition(Local) : Local;
}

void ASlicePlaytest::Log(const FString& S)
{
	const FString L = FString::Printf(TEXT("[%7.2f] %s"), TotalTime, *S);
	UE_LOG(LogTemp, Display, TEXT("[PLAYTEST] %s"), *L);
	Lines.Add(L);
}

void ASlicePlaytest::Result(const FString& Item, bool bPass, const FString& Detail)
{
	(bPass ? Passes : Fails)++;
	Log(FString::Printf(TEXT("RESULT | %s | %s | %s"), *Item, bPass ? TEXT("PASS") : TEXT("FAIL"), *Detail));
}

void ASlicePlaytest::Shot(const FString& Name)
{
	const FString File = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Screenshots/Playtest01") / (Name + TEXT(".png")));
	FScreenshotRequest::RequestScreenshot(File, false, false);
	Shots.Add(Name + TEXT(".png"));
	Log(TEXT("SHOT ") + Name);
}

void ASlicePlaytest::TeleportCrew(const FVector& ShipM, float YawDeg)
{
	const float Half = Crew->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	Crew->SetActorLocation(ShipToWorld(ShipM) + FVector(0, 0, Half + 2.f), false, nullptr, ETeleportType::TeleportPhysics);
	Crew->GetCharacterMovement()->Velocity = FVector::ZeroVector;
	if (PC) PC->SetControlRotation(FRotator(0.f, YawDeg, 0.f));
}

bool ASlicePlaytest::WalkTo(const FVector2D& T, float Dt)
{
	const FVector Goal = ShipToWorld(FVector(T.X, T.Y, 0));
	FVector D = Goal - Crew->GetActorLocation(); D.Z = 0;
	if (D.Size() < 25.f) return true;
	Crew->AddMovementInput(D.GetSafeNormal(), 1.f);
	return false;
}

void ASlicePlaytest::Start(ALunarAbyssPlayerController* InPC)
{
	PC = InPC;
	Crew = Cast<ACrewCharacter>(PC->GetPawn());
	for (TActorIterator<ACorvetteHangar> It(GetWorld()); It; ++It) { Ship = *It; break; }
	Fighter = Ship ? Ship->GetParkedFighter() : nullptr;
	Log(FString::Printf(TEXT("Start: crew=%s ship=%s fighter=%s"), *GetNameSafe(Crew), *GetNameSafe(Ship), *GetNameSafe(Fighter)));
	if (!Crew || !Ship) { Result(TEXT("Setup"), false, TEXT("missing crew or ship")); Step = 99; return; }
	Step = 0; StepTime = -2.f; // let the level settle
}

void ASlicePlaytest::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (Step < 0) return;
	TotalTime += DeltaSeconds; StepTime += DeltaSeconds;
	if (StepTime < 0.f) return;
	UCharacterMovementComponent* Move = Crew ? Crew->GetCharacterMovement() : nullptr;
	const float Z = Crew ? Crew->GetActorLocation().Z : 0.f;
	const float FeetZm = Crew ? (Z - Crew->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()) / M : 0.f;

	switch (Step)
	{
	case 0: // Jump at 1 g on the bridge
		if (SubStep == 0) { TeleportCrew(FVector(35.7, 0, 22.97), 0.f); SubStep = 1; StepTime = -1.f; break; }
		if (SubStep == 1) { Shot(TEXT("01_firstperson_bridge")); Z0 = Z; ZMax = Z; Crew->Jump(); SubStep = 2; bLeftGround = false; break; }
		ZMax = FMath::Max(ZMax, Z);
		if (Move->IsFalling()) bLeftGround = true;
		if ((bLeftGround && Move->IsMovingOnGround()) || StepTime > 4.f)
		{
			const float H = (ZMax - Z0) / M;
			Result(TEXT("Jump height at 1 g (target 0.60 m)"), FMath::Abs(H - 0.6f) < 0.06f, FString::Printf(TEXT("apex %.3f m, JumpZ %.1f cm/s"), H, Move->JumpZVelocity));
			Next(0.5f);
		}
		break;

	case 1: // Stairs down D5 -> D2
		if (SubStep == 0) { TeleportCrew(FVector(30.8, 6.4, 22.97), 0.f); Waypoint = 0; SubStep = 1; LogTimer = 0; Z0 = FeetZm; StepTime = -0.5f; break; }
		if ((LogTimer += DeltaSeconds) > 1.f) { LogTimer = 0; Log(FString::Printf(TEXT("stairs down: feet Z %.2f m, wp %d"), FeetZm, Waypoint)); }
		if (Waypoint == 6 && !Shots.Contains(TEXT("02_stairs_down.png"))) Shot(TEXT("02_stairs_down"));
		if (WalkTo(DownLoop[Waypoint % 4], DeltaSeconds)) Waypoint++;
		if (FeetZm < 9.67f + 0.15f || StepTime > 90.f)
		{
			Result(TEXT("Stairs ST-F down D5->D2"), FeetZm < 9.67f + 0.15f, FString::Printf(TEXT("feet %.2f -> %.2f m in %.1f s, %d waypoints"), Z0, FeetZm, StepTime, Waypoint));
			Next(0.5f);
		}
		break;

	case 2: // Stairs up D2 -> D5
		if (SubStep == 0) { TeleportCrew(FVector(30.8, 7.8, 9.67), 0.f); Waypoint = 0; SubStep = 1; LogTimer = 0; Z0 = FeetZm; StepTime = -0.5f; break; }
		if ((LogTimer += DeltaSeconds) > 1.f) { LogTimer = 0; Log(FString::Printf(TEXT("stairs up: feet Z %.2f m, wp %d"), FeetZm, Waypoint)); }
		if (WalkTo(UpLoop[Waypoint % 4], DeltaSeconds)) Waypoint++;
		if (FeetZm > 22.97f - 0.15f || StepTime > 120.f)
		{
			Result(TEXT("Stairs ST-F up D2->D5"), FeetZm > 22.97f - 0.15f, FString::Printf(TEXT("feet %.2f -> %.2f m in %.1f s, %d waypoints"), Z0, FeetZm, StepTime, Waypoint));
			Next(0.5f);
		}
		break;

	case 3: // Gravity off then on (G handler)
		if (SubStep == 0) { TeleportCrew(FVector(35.7, 0, 22.97), 180.f); SubStep = 1; StepTime = -1.f; break; }
		if (SubStep == 1) { Z0 = FeetZm; PC->ToggleGravity(); Log(TEXT("G: gravity engine toggled OFF")); SubStep = 2; break; }
		if (SubStep == 2 && StepTime > 3.f)
		{
			const bool bFloat = Crew->IsInZeroG() && Move->MovementMode == MOVE_Flying;
			Result(TEXT("Gravity off -> float"), bFloat, FString::Printf(TEXT("mode %d, zeroG %d, state %d, feet %.2f -> %.2f m, vel %s"), (int)Move->MovementMode, Crew->IsInZeroG(), (int)Ship->GravityEngine->GetGravityState(), Z0, FeetZm, *Move->Velocity.ToString()));
			Crew->AddMovementInput(FVector::UpVector, 1.f); // push-off
			Shot(TEXT("03_zero_g_bridge"));
			SubStep = 3; break;
		}
		if (SubStep == 3) { Crew->AddMovementInput(FVector::UpVector, 1.f); if (StepTime > 5.f) { Log(FString::Printf(TEXT("after 2 s push-off up: feet %.2f m, vel %s"), FeetZm, *Move->Velocity.ToString())); PC->ToggleGravity(); Log(TEXT("G: gravity engine toggled ON")); SubStep = 4; } break; }
		if (SubStep == 4 && StepTime > 10.f)
		{
			const bool bLanded = Move->MovementMode == MOVE_Walking;
			Result(TEXT("Gravity on -> land and walk"), bLanded, FString::Printf(TEXT("mode %d, gravity %.2f, state %d, feet %.2f m, HP %.1f"), (int)Move->MovementMode, Move->GravityScale, (int)Ship->GravityEngine->GetGravityState(), FeetZm, Crew->Health));
			Next(0.5f);
		}
		break;

	case 4: // Falls in the hangar at Hangar_Spot_Dropship
	{
		if (FallIndex >= 4) { Next(0.5f); break; }
		const float Hm = FallHeights[FallIndex];
		if (SubStep == 0)
		{
			Crew->Health = Crew->MaxHealth;
			// Find the deck under the spot (the hangar floor is not at the D2 level).
			FHitResult Hit; FCollisionQueryParams Q; Q.AddIgnoredActor(Crew); if (Fighter) Q.AddIgnoredActor(Fighter);
			float FloorM = 9.667f;
			if (GetWorld()->LineTraceSingleByChannel(Hit, ShipToWorld(FVector(16.175, 2.25, 20.0)), ShipToWorld(FVector(16.175, 2.25, 0.0)), ECC_Pawn, Q))
				FloorM = Ship->GetActorTransform().InverseTransformPosition(Hit.ImpactPoint).Z / M;
			TeleportCrew(FVector(16.175, 2.25, FloorM + Hm), 90.f);
			FallStartFeet = FloorM + Hm;
			H0 = Crew->Health; bLeftGround = false; SubStep = 1; break;
		}
		if (Move->IsFalling()) bLeftGround = true;
		if ((bLeftGround && Move->IsMovingOnGround()) || StepTime > 6.f)
		{
			const float Dmg = H0 - Crew->Health;
			const float Hd = FallStartFeet - FeetZm; // actual drop of the feet
			const float Expect = Hd > 3.f ? 15.f * (Hd - 3.f) : 0.f; // v^2/2g = h for a free fall from rest
			Result(FString::Printf(TEXT("Fall %.0f m (expect %.1f HP)"), Hm, Expect), FMath::Abs(Dmg - Expect) <= FMath::Max(1.5f, Expect * 0.06f),
				FString::Printf(TEXT("drop %.2f m, damage %.1f HP, landed feet %.2f m"), Hd, Dmg, FeetZm));
			if (FallIndex == 3) Shot(TEXT("04_hangar_after_falls"));
			++FallIndex; SubStep = 0; StepTime = -0.5f;
		}
		break;
	}

	case 5: // F at the pilot seat -> launch + possess fighter
		if (SubStep == 0) { Crew->Health = Crew->MaxHealth; TeleportCrew(FVector(43.8, 1.0, 22.97), 0.f); SubStep = 1; StepTime = -1.f; break; }
		if (SubStep == 1)
		{
			PC->PressF();
			Fighter = Cast<AFighterPawn>(PC->GetPawn());
			Result(TEXT("F at pilot seat possesses fighter"), Fighter != nullptr, FString::Printf(TEXT("pawn %s, doors state %d"), *GetNameSafe(PC->GetPawn()), (int)Ship->GetHangarDoorState()));
			if (!Fighter) { PC->LaunchFighter(); PC->SwitchToFighter(); Fighter = Cast<AFighterPawn>(PC->GetPawn()); Log(FString::Printf(TEXT("fallback: console LaunchFighter + SwitchToFighter -> %s"), *GetNameSafe(Fighter))); }
			if (!Fighter) { Next(); break; }
			Fighter->SetCockpitView(false);
			SubStep = 2; LogTimer = 0; break;
		}
		if (SubStep == 2 && StepTime > 3.f) { Shot(TEXT("05_chase_doors_opening")); SubStep = 3; break; }
		if (SubStep == 3 && Ship->GetHangarDoorState() == EHangarDoorState::Open) { Log(TEXT("doors OPEN")); Shot(TEXT("06_chase_doors_open_launch")); Fighter->NumBlockingHits = 0; SubStep = 4; StepTime = 0; break; }
		if (SubStep == 3 && StepTime > 12.f) { Result(TEXT("Hangar doors open before launch"), false, TEXT("doors never reached Open")); Next(); break; }
		if (SubStep == 4)
		{
			if ((LogTimer += DeltaSeconds) > 0.5f) { LogTimer = 0; Log(FString::Printf(TEXT("fighter at %s vel %.1f m/s hits %d"), *Fighter->GetActorLocation().ToString(), Fighter->GetVelocity().Size() / M, Fighter->NumBlockingHits)); }
			if (StepTime > 2.f && SubStep == 4 && !Shots.Contains(TEXT("07_chase_clearing_bay.png"))) Shot(TEXT("07_chase_clearing_bay"));
			if (StepTime > 6.f)
			{
				FTransform Clear; const bool bC = Ship->GetSocketTransform(TEXT("HangarLaunch_Clear"), Clear);
				FTransform Thr; Ship->GetSocketTransform(TEXT("HangarLaunch_Threshold"), Thr);
				const FVector Lane = (Clear.GetLocation() - Thr.GetLocation()).GetSafeNormal();
				const float Along = FVector::DotProduct(Fighter->GetActorLocation() - Thr.GetLocation(), Lane) / M;
				Result(TEXT("Launch clears bay (no blocking hits, past Threshold)"), bC && Fighter->NumBlockingHits == 0 && Along > 13.5f,
					FString::Printf(TEXT("%.1f m along lane (Clear = 13.5 m), hits %d (%s)"), Along, Fighter->NumBlockingHits, *Fighter->LastHitComponent));
				Fighter->SetCockpitView(true); SubStep = 5; StepTime = 0;
			}
			break;
		}
		if (SubStep == 5 && StepTime > 0.5f) { Shot(TEXT("08_cockpit_view")); Fighter->SetCockpitView(false); Next(); }
		break;

	case 6: // Hull collision: fly at the port side after the 4 s ignore window
		if (!Fighter) { Result(TEXT("Fighter blocked by 4C hull"), false, TEXT("no fighter pawn")); Next(); break; }
		if (SubStep == 0)
		{
			Fighter->SetActorLocationAndRotation(ShipToWorld(FVector(0, 60, 18)), FRotator(0, 90, 0), false, nullptr, ETeleportType::TeleportPhysics);
			Fighter->SuspendAssist(10.f);
			Fighter->SetLinearVelocity(Ship->GetActorRightVector() * 50.f * M); // UE +Y = ship starboard: toward the hull
			Fighter->NumBlockingHits = 0; MinAbsY = 1e9f; SubStep = 1; break;
		}
		MinAbsY = FMath::Min(MinAbsY, FMath::Abs(Ship->GetActorTransform().InverseTransformPosition(Fighter->GetActorLocation()).Y) / M);
		if (StepTime > 3.f)
		{
			Result(TEXT("Fighter blocked by 4C hull exterior"), Fighter->NumBlockingHits > 0 && MinAbsY > 5.f,
				FString::Printf(TEXT("closest %.1f m from centreline, hits %d (%s)"), MinAbsY, Fighter->NumBlockingHits, *Fighter->LastHitComponent));
			Shot(TEXT("09_chase_against_hull"));
			Next(0.5f);
		}
		break;

	case 7: // F in fighter -> back to crew
		if (SubStep == 0) { PC->PressF(); SubStep = 1; StepTime = 0; break; }
		if (StepTime > 1.f)
		{
			Result(TEXT("F in fighter returns to crew"), PC->GetPawn() == Crew, FString::Printf(TEXT("pawn %s"), *GetNameSafe(PC->GetPawn())));
			Shot(TEXT("10_firstperson_after_return"));
			SubStep = 0; Next(); StepTime = -9.f;
		}
		break;

	case 8: // doors closed again after launch
		Result(TEXT("Hangar doors close after launch"), Ship->GetHangarDoorState() == EHangarDoorState::Closed || Ship->GetHangarDoorState() == EHangarDoorState::Closing,
			FString::Printf(TEXT("state %d (0 Closed, 3 Closing)"), (int)Ship->GetHangarDoorState()));
		Next(1.f);
		break;

	case 9: // LT-H caged slide ladder (QA-136)
	{
		if (SubStep == 0)
		{
			if (PC->GetPawn() != Crew) PC->SwitchToCrew();
			Crew->Health = Crew->MaxHealth; TeleportCrew(FVector(27.7, -13.5, 20.0), 0.f); SubStep = 1; StepTime = -0.5f; break;
		}
		if (SubStep == 1) { Z0 = FeetZm; StepTime = 0; SubStep = 2; Log(FString::Printf(TEXT("ladder: on=%d feet %.2f"), Crew->IsOnLadder(), FeetZm)); break; }
		if (SubStep == 2 && StepTime > 1.5f)
		{
			const float V = (Z0 - FeetZm) / StepTime;
			Result(TEXT("Ladder release = slide 2.0 m/s"), Crew->IsOnLadder() && FMath::Abs(V - 2.f) < 0.15f, FString::Printf(TEXT("on=%d, %.2f m/s"), Crew->IsOnLadder(), V));
			PC->ToggleGravity(); Log(TEXT("G: off in shaft")); SubStep = 3; StepTime = 0; break;
		}
		if (SubStep == 3 && StepTime > 2.5f) { Z0 = FeetZm; SubStep = 4; StepTime = 0; break; }
		if (SubStep == 4 && StepTime > 1.f)
		{
			const float V = FMath::Abs(FeetZm - Z0) / StepTime;
			Result(TEXT("Ladder zero-g: float in shaft (<= 0.3 m/s, still on ladder)"), Crew->IsOnLadder() && V <= 0.32f, FString::Printf(TEXT("on=%d, %.2f m/s, mode %d"), Crew->IsOnLadder(), V, (int)Move->MovementMode));
			PC->ToggleGravity(); Log(TEXT("G: on in shaft")); SubStep = 5; StepTime = 0; break;
		}
		if (SubStep == 5 && (!Crew->IsOnLadder() || StepTime > 12.f))
		{
			const float Dmg = Crew->MaxHealth - Crew->Health;
			Result(TEXT("Ladder: restore resumes slide, exit at bottom, no fall damage"), !Crew->IsOnLadder() && FMath::Abs(FeetZm - 9.667f) < 0.2f && Dmg < 0.01f,
				FString::Printf(TEXT("feet %.2f m, mode %d, damage %.1f"), FeetZm, (int)Move->MovementMode, Dmg));
			Shot(TEXT("11_ladder_bottom_C2SP")); SubStep = 6; StepTime = -0.5f; break;
		}
		if (SubStep == 6) { Crew->SetLadderInput(1.f); if (Crew->IsOnLadder()) { Z0 = FeetZm; SubStep = 7; StepTime = 0; } else if (StepTime > 2.f) { Result(TEXT("Ladder climb 0.6 m/s"), false, TEXT("could not re-grab at bottom")); Next(); } break; }
		if (SubStep == 7) { Crew->SetLadderInput(1.f); if (StepTime > 3.f) { const float V = (FeetZm - Z0) / StepTime; Result(TEXT("Ladder climb 0.6 m/s"), FMath::Abs(V - 0.6f) < 0.08f, FString::Printf(TEXT("%.2f m/s"), V)); Next(); } break; }
		break;
	}

	default:
	{
		Log(FString::Printf(TEXT("DONE: %d pass, %d fail. Screenshots: %s"), Passes, Fails, *FString::Join(Shots, TEXT(", "))));
		const FString Path = FPaths::ProjectSavedDir() / TEXT("Playtest/results.txt");
		FFileHelper::SaveStringToFile(FString::Join(Lines, TEXT("\n")), *Path);
		Step = -1;
		UKismetSystemLibrary::QuitGame(this, PC, EQuitPreference::Quit, false);
	}
	}
}
