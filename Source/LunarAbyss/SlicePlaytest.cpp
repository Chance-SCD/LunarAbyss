#include "SlicePlaytest.h"
#include "LunarAbyssPlayerController.h"
#include "CrewCharacter.h"
#include "FighterPawn.h"
#include "CorvetteHangar.h"
#include "GravityEngineComponent.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "Kismet/KismetSystemLibrary.h"
#include "LadderVolume.h"
#include "Components/BoxComponent.h"
#include "AudioDevice.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshSocket.h"
#include "PhysicsEngine/BodySetup.h"
#include "Misc/CommandLine.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"

namespace
{
	constexpr float M = 100.f;
	// ST-F switchback (Atlas-V blend, ship metres): aft landing X 30.8, forward mid-landing X 32.9; lane A Y 6.4, lane B Y 7.8.
	// QA-141: ST-F from Atlas-V's UCX (Blender m). Deck landings at the fore end X 30.41..31.70, mid landings at the aft end
	// X 34.70..36.01; flights run aft in lane A (Y 5.80..7.05) and fore in lane B (Y 7.15..8.40) going down. 8 flights D5->D2.
	const FVector2D DownLoop[4] = { {31.05, 6.43}, {35.35, 6.43}, {35.35, 7.78}, {31.05, 7.78} };
	const FVector2D UpLoop[4]   = { {31.05, 7.78}, {35.35, 7.78}, {35.35, 6.43}, {31.05, 6.43} };
	const float FallHeights[5] = { 2.f, 4.f, 6.f, 9.f, 11.f };
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

void ASlicePlaytest::Shot(const FString& Name, bool bUI)
{
	const FString File = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Screenshots/Playtest01") / (Name + TEXT(".png")));
	FScreenshotRequest::RequestScreenshot(File, bUI, false);
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
	if (D.Size() < 30.f) return true;
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
	// Audio state of this run (Rex: explicit line).
	{
		FAudioDeviceHandle AD = GetWorld()->GetAudioDevice();
		const bool bNoSound = FParse::Param(FCommandLine::Get(), TEXT("nosound"));
		Log(FString::Printf(TEXT("AUDIO: sound %s (audio device %s, -nosound %d)"), (AD.IsValid() && !bNoSound) ? TEXT("ON") : TEXT("OFF"), AD.IsValid() ? TEXT("valid") : TEXT("none"), bNoSound));
	}
	// QA-139: collision counts of the placed meshes + interior socket check (mesh-local cm).
	{
		TMap<FString, int32> Convex;
		TMap<FString, UStaticMesh*> Meshes;
		for (TActorIterator<AActor> It(GetWorld()); It; ++It)
		{
			TArray<UStaticMeshComponent*> Comps; It->GetComponents(Comps);
			for (UStaticMeshComponent* C : Comps)
				if (UStaticMesh* SM = C ? C->GetStaticMesh() : nullptr)
					if (!Convex.Contains(SM->GetName())) { Convex.Add(SM->GetName(), SM->GetBodySetup() ? SM->GetBodySetup()->AggGeom.ConvexElems.Num() : -1); Meshes.Add(SM->GetName(), SM); }
		}
		const TPair<const TCHAR*, int32> Want[] = { {TEXT("SM_Corvette4C_Body"), 140}, {TEXT("SM_FighterC_Body"), 52}, {TEXT("SM_Hatch_DCD77"), 1} };
		for (const auto& W : Want)
		{
			const int32* N = Convex.Find(W.Key);
			Result(FString::Printf(TEXT("QA-139 collision %s = %d convex"), W.Key, W.Value), N && *N == W.Value, FString::Printf(TEXT("%s: %d convex in the running level"), W.Key, N ? *N : -1));
		}
		struct FS { const TCHAR* N; FVector E; };
		const FS Sock[] = { {TEXT("Hatch_DCD77"), FVector(2840, 1350, 1311.7)}, {TEXT("Seat_Pilot"), FVector(4497.5, -100, 2431.7)}, {TEXT("DC_DCD-77"), FVector(2770, 1350, 1311.7)}, {TEXT("Trig_Hatch_DCD77"), FVector(2770, 1350, 1386.7)} };
		UStaticMesh** Int = Meshes.Find(TEXT("SM_Corvette4C_Interior_Blockout"));
		for (const FS& K : Sock)
		{
			const UStaticMeshSocket* S = (Int && *Int) ? (*Int)->FindSocket(K.N) : nullptr;
			const FVector P = S ? S->RelativeLocation : FVector(NAN);
			Result(FString::Printf(TEXT("QA-139 socket %s at (%.1f, %.1f, %.1f) cm"), K.N, K.E.X, K.E.Y, K.E.Z), S && P.Equals(K.E, 0.5f),
				S ? FString::Printf(TEXT("found (%.2f, %.2f, %.2f) cm, error %.2f cm"), P.X, P.Y, P.Z, (P - K.E).Size()) : FString(TEXT("socket missing")));
		}
	}
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
		if (SubStep == 0) { TeleportCrew(FVector(31.05, 6.43, 22.97), 0.f); Waypoint = 0; SubStep = 1; LogTimer = 0; Z0 = FeetZm; StepTime = -0.5f; break; }
		if ((LogTimer += DeltaSeconds) > 1.f) { LogTimer = 0; Log(FString::Printf(TEXT("stairs down: feet Z %.2f m, wp %d"), FeetZm, Waypoint)); }
		if (Waypoint == 3 && !Shots.Contains(TEXT("02_stairs_down.png"))) Shot(TEXT("02_stairs_down"));
		if (WalkTo(DownLoop[Waypoint % 4], DeltaSeconds)) Waypoint++;
		if (FeetZm < 9.67f + 0.15f || StepTime > 90.f)
		{
			Result(TEXT("Stairs ST-F down D5->D2"), FeetZm < 9.67f + 0.15f, FString::Printf(TEXT("feet %.2f -> %.2f m in %.1f s, %d waypoints"), Z0, FeetZm, StepTime, Waypoint));
			Next(0.5f);
		}
		break;

	case 2: // Stairs up D2 -> D5
		if (SubStep == 0) { TeleportCrew(FVector(31.05, 7.78, 9.67), 0.f); Waypoint = 0; SubStep = 1; LogTimer = 0; Z0 = FeetZm; StepTime = -0.5f; break; }
		if ((LogTimer += DeltaSeconds) > 1.f) { LogTimer = 0; Log(FString::Printf(TEXT("stairs up: feet Z %.2f m, wp %d"), FeetZm, Waypoint)); }
		if (Waypoint == 5 && !Shots.Contains(TEXT("02b_stairs_up.png"))) Shot(TEXT("02b_stairs_up"));
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
		if (FallIndex >= 5) { Next(0.5f); break; }
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
			// QA-142: impact speed from the last airborne frame -> equivalent drop v^2/2g must match the real drop within 0.1 m;
			// damage must equal 15 x (equiv - 3) for equiv > 3.
			const float Eq = Crew->LastEquivDropM;
			const float Expect = Eq > 3.f ? 15.f * (Eq - 3.f) : 0.f;
			const float ExpectFromDrop = Hd > 3.f ? 15.f * (Hd - 3.f) : 0.f;
			Result(FString::Printf(TEXT("Fall %.0f m (drop vs v^2/2g within 0.1 m)"), Hm), FMath::Abs(Eq - Hd) <= 0.1f && FMath::Abs(Dmg - Expect) <= 0.05f,
				FString::Printf(TEXT("drop %.3f m, impact %.3f m/s -> v^2/2g %.3f m (err %+.3f m), damage %.2f HP (formula on drop %.2f)"), Hd, Crew->LastImpactSpeed, Eq, Eq - Hd, Dmg, ExpectFromDrop));
			if (Hm > 10.f) Result(TEXT("QA-142 fall 11 m = 120 +/-1 HP"), FMath::Abs(Dmg - 120.f) <= 1.f, FString::Printf(TEXT("drop %.3f m, impact %.3f m/s, damage %.2f HP"), Hd, Crew->LastImpactSpeed, Dmg));
			if (FallIndex == 4) Shot(TEXT("04_hangar_after_falls"));
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
			Crew->Health = Crew->MaxHealth; TeleportCrew(FVector(27.7, -13.5, 20.0), 0.f); HatchCrossT = -1.f; SubStep = 1; StepTime = -0.5f; break;
		}
		if (SubStep == 1) { Z0 = FeetZm; StepTime = 0; SubStep = 2; Log(FString::Printf(TEXT("ladder: on=%d feet %.2f"), Crew->IsOnLadder(), FeetZm)); break; }
		if (SubStep == 2 && StepTime > 1.5f)
		{
			const float V = (Z0 - FeetZm) / StepTime;
			Result(TEXT("Ladder release = slide 2.0 m/s"), Crew->IsOnLadder() && FMath::Abs(V - 2.f) < 0.15f, FString::Printf(TEXT("on=%d, %.2f m/s"), Crew->IsOnLadder(), V));
			PC->ToggleGravity(); Log(TEXT("G: off in shaft")); GOffFeet = FeetZm; PrevVz = GOffV0 = Crew->GetVelocity().Z / M; GOffStopT = -1.f; PeakDecel = 0.f; SubStep = 3; StepTime = 0; break;
		}
		if (SubStep >= 3 && SubStep <= 5) if (ALadderVolume* LS = Crew->GetLadder()) SwingProbe(LS);
		if (SubStep == 3 && DeltaSeconds > 0.f)
		{
			// QA-157: deceleration of the slide when gravity goes off.
			const float Vz = Crew->GetVelocity().Z / M;
			PeakDecel = FMath::Max(PeakDecel, FMath::Abs(Vz - PrevVz) / DeltaSeconds); PrevVz = Vz;
			if (GOffStopT < 0.f && FMath::Abs(Vz) < 0.01f)
			{
				GOffStopT = StepTime;
				Log(FString::Printf(TEXT("QA-157 zero-g slide stop: %.2f -> 0 m/s in %.2f s, average decel %.2f m/s^2, peak %.2f m/s^2 (per frame), stopping distance %.2f m (feet %.2f -> %.2f m)"),
					GOffV0, GOffStopT, GOffStopT > 0.f ? FMath::Abs(GOffV0) / GOffStopT : 0.f, PeakDecel, GOffFeet - FeetZm, GOffFeet, FeetZm));
			}
		}
		if ((SubStep == 3 || SubStep == 4) && (SlideLog += DeltaSeconds) >= 0.2f)
		{
			SlideLog = 0; ALadderVolume* L = Crew->GetLadder();
			Log(FString::Printf(TEXT("QA-143 slide gravity OFF t=%.2f feet %.2f m vz %.2f m/s on=%d hatch %.2f"), (SubStep == 4 ? 2.5f : 0.f) + StepTime, FeetZm, Crew->GetVelocity().Z / M, Crew->IsOnLadder(), L ? L->HatchAlpha : -1.f));
		}
		if (SubStep == 3 && StepTime > 2.5f) { Z0 = FeetZm; SubStep = 4; StepTime = 0; break; }
		if (SubStep == 4 && StepTime > 1.f)
		{
			const float V = FMath::Abs(FeetZm - Z0) / StepTime;
			Result(TEXT("Ladder zero-g: float in shaft (<= 0.3 m/s, still on ladder)"), Crew->IsOnLadder() && V <= 0.32f, FString::Printf(TEXT("on=%d, %.2f m/s, mode %d"), Crew->IsOnLadder(), V, (int)Move->MovementMode));
			PC->ToggleGravity(); Log(TEXT("G: on in shaft")); SubStep = 5; StepTime = 0; break;
		}
		if (SubStep == 5 && Crew->GetLadder())
		{
			ALadderVolume* L = Crew->GetLadder();
			MaxHatchAlpha = FMath::Max(MaxHatchAlpha, L->HatchAlpha);
			if (Crew->bHatchStopped) HatchWaitTime += DeltaSeconds;
			if ((SlideLog += DeltaSeconds) >= 0.2f) { SlideLog = 0; Log(FString::Printf(TEXT("QA-143 slide gravity ON t=%.2f feet %.2f m hatch %.2f"), StepTime, FeetZm, L->HatchAlpha)); }
			if (FeetZm <= (L->HatchTrigger->GetComponentLocation().Z + 75.f) / M && HatchCrossT < 0) HatchCrossT = StepTime; // entering the trigger (top 14.617)
			PC->SetControlRotation(FRotator(-75.f, 0.f, 0.f)); // look down the shaft at the hatch
			// Shot 12 later: once the leaf is fully open and the feet are ~0.6 m below the leaf top (inside the open hatchway).
			if (FeetZm < L->HatchTopZ / M - 0.6f && L->HatchAlpha >= 0.999f && !Shots.Contains(TEXT("12_hatch_DCD77_open.png")))
			{
				Log(FString::Printf(TEXT("SHOT 12 state: hatch open %.3f, feet %.2f m (leaf top %.3f m), t=%.2f s"), L->HatchAlpha, FeetZm, L->HatchTopZ / M, StepTime));
				Shot(TEXT("12_hatch_DCD77_open"));
			}
		}
		if (SubStep == 5 && (!Crew->IsOnLadder() || StepTime > 12.f))
		{
			const float Dmg = Crew->MaxHealth - Crew->Health;
			Result(TEXT("QA-143 descent trigger top (14.62 m) -> D2 floor ~2.2-2.5 s at 2 m/s"), HatchCrossT >= 0 && (StepTime - HatchCrossT) > 2.0f && (StepTime - HatchCrossT) < 2.7f,
				FString::Printf(TEXT("%.2f s from 14.62 m to exit at %.2f m (pure 2 m/s: %.2f s)"), StepTime - HatchCrossT, FeetZm, (14.617f - 9.667f) / 2.f));
			Result(TEXT("QA-140 hatch DCD-77 auto-opens on the slide (0.6 s), no stop"), MaxHatchAlpha >= 0.999f && HatchWaitTime < 0.05f,
				FString::Printf(TEXT("max hatch open %.2f, held at hatch %.2f s"), MaxHatchAlpha, HatchWaitTime));
			Result(TEXT("Ladder: restore resumes slide, exit at bottom (Walking), no fall damage"), !Crew->IsOnLadder() && FMath::Abs(FeetZm - 9.667f) < 0.2f && Dmg < 0.01f && Move->MovementMode == MOVE_Walking,
				FString::Printf(TEXT("feet %.2f m, mode %d (1 = Walking), damage %.1f"), FeetZm, (int)Move->MovementMode, Dmg));
			Result(TEXT("QA-157 hatch swing: leaf never overlaps the ladder or the player capsule"), SwingFrames > 0 && !bSwingOverlap,
				FString::Printf(TEXT("min clearance capsule %.1f cm, ladder %.1f cm (ladder = %d interior convex pieces in the shaft, AABB = lower bound), %d swing frames"), MinCapClear, MinLadClear, LadderBoxes.Num(), SwingFrames));
			Shot(TEXT("11_ladder_bottom_C2SP")); SubStep = 6; StepTime = -0.5f; break;
		}
		if (SubStep == 6) { Crew->SetLadderInput(1.f); if (Crew->IsOnLadder()) { Z0 = FeetZm; SubStep = 7; StepTime = 0; } else if (StepTime > 2.f) { Result(TEXT("Ladder climb 0.6 m/s"), false, TEXT("could not re-grab at bottom")); Next(); } break; }
		if (SubStep == 7) { Crew->SetLadderInput(1.f); if (StepTime > 3.f) { const float V = (FeetZm - Z0) / StepTime; Result(TEXT("Ladder climb 0.6 m/s"), FMath::Abs(V - 0.6f) < 0.08f, FString::Printf(TEXT("%.2f m/s"), V)); Next(); } break; }
		break;
	}

	case 10: // QA-140 pressure-locked hatch: stop on it, prompt, climb back up; then unlock -> slide through, Walking at the bottom
	{
		ALadderVolume* L = nullptr; for (TActorIterator<ALadderVolume> It(GetWorld()); It; ++It) { L = *It; break; }
		if (!L) { Result(TEXT("QA-140 locked hatch"), false, TEXT("no ladder")); Next(); break; }
		if (SubStep == 0) { L->bHatchPressureLocked = true; PC->SetControlRotation(FRotator(-75.f, 0.f, 0.f)); Crew->Health = Crew->MaxHealth; TeleportCrew(FVector(27.7, -13.5, 17.0), 0.f); SubStep = 1; StepTime = -0.5f; break; }
		if (SubStep == 1 && StepTime > 3.5f)
		{
			Result(TEXT("QA-140 locked: slide stops on the hatch + LOCKED prompt"), Crew->IsOnLadder() && Crew->bHatchStopped && FMath::Abs(FeetZm - (L->HatchTopZ / M + 0.02f)) < 0.1f && !L->LastPrompt.IsEmpty() && L->HatchAlpha < 0.001f,
				FString::Printf(TEXT("on=%d stopped=%d feet %.2f m (leaf top %.3f m), hatch %.2f, prompt '%s'"), Crew->IsOnLadder(), Crew->bHatchStopped, FeetZm, L->HatchTopZ / M, L->HatchAlpha, *L->LastPrompt));
			PC->SetControlRotation(FRotator(-75.f, 0.f, 0.f)); L->RequestHatchOpen(); Shot(TEXT("13_hatch_locked_prompt"), true); Z0 = FeetZm; SubStep = 2; StepTime = 0; break;
		}
		if (SubStep == 2) { Crew->SetLadderInput(1.f); if (StepTime > 2.f) { Result(TEXT("QA-140 locked: can climb back up"), Crew->IsOnLadder() && FeetZm - Z0 > 1.0f, FString::Printf(TEXT("%.2f -> %.2f m"), Z0, FeetZm)); L->bHatchPressureLocked = false; SubStep = 3; StepTime = 0; } break; }
		if (SubStep == 3 && (!Crew->IsOnLadder() || StepTime > 10.f))
		{
			Result(TEXT("QA-140 unlocked: slide through, Walking after exit"), !Crew->IsOnLadder() && Move->MovementMode == MOVE_Walking && FMath::Abs(FeetZm - 9.667f) < 0.2f,
				FString::Printf(TEXT("feet %.2f m, mode %d"), FeetZm, (int)Move->MovementMode));
			Next();
		}
		break;
	}

	default:
	{
		if (IConsoleVariable* CV = IConsoleManager::Get().FindConsoleVariable(TEXT("r.MegaLights.Allow"))) Log(FString::Printf(TEXT("r.MegaLights.Allow = %d"), CV->GetInt()));
		if (Ship && Ship->GravityEngine) Log(TEXT("gravity cues played: ") + FString::Join(Ship->GravityEngine->LastPlayedCues, TEXT(", ")));
		Log(FString::Printf(TEXT("DONE: %d pass, %d fail. Screenshots: %s"), Passes, Fails, *FString::Join(Shots, TEXT(", "))));
		const FString Path = FPaths::ProjectSavedDir() / TEXT("Playtest/results.txt");
		FFileHelper::SaveStringToFile(FString::Join(Lines, TEXT("\n")), *Path);
		Step = -1;
		UKismetSystemLibrary::QuitGame(this, PC, EQuitPreference::Quit, false);
	}
	}
}

void ASlicePlaytest::SwingProbe(ALadderVolume* L)
{
	UStaticMeshComponent* H = L ? L->Hatch.Get() : nullptr;
	if (!H || !H->GetBodySetup() || H->GetBodySetup()->AggGeom.ConvexElems.Num() == 0) return;
	if (!bLadderBoxesBuilt)
	{
		bLadderBoxesBuilt = true;
		const FVector S = L->GetShaftCentre(); const FVector Ext = L->Shaft->GetScaledBoxExtent();
		FBox All(ForceInit);
		for (TActorIterator<AActor> It(GetWorld()); It; ++It)
		{
			TArray<UStaticMeshComponent*> Comps; It->GetComponents(Comps);
			for (UStaticMeshComponent* C : Comps)
			{
				if (!C || !C->GetStaticMesh() || C->GetStaticMesh()->GetName() != TEXT("SM_Corvette4C_Interior_Blockout") || !C->GetBodySetup()) continue;
				const FTransform T = C->GetComponentTransform();
				for (const FKConvexElem& E : C->GetBodySetup()->AggGeom.ConvexElems)
				{
					FBox B(ForceInit); for (const FVector& V : E.VertexData) B += T.TransformPosition(V);
					const FVector Ce = B.GetCenter(), Sz = B.GetSize();
					// Ladder pieces: centred inside the shaft footprint and no wider than the shaft (excludes deck slabs and walls).
					if (FMath::Abs(Ce.X - S.X) <= Ext.X + 10.f && FMath::Abs(Ce.Y - S.Y) <= Ext.Y + 10.f && Sz.X <= 2.f * Ext.X + 20.f && Sz.Y <= 2.f * Ext.Y + 20.f)
					{ LadderBoxes.Add(B); All += B; }
				}
			}
		}
		Log(FString::Printf(TEXT("QA-157 ladder geometry: %d interior convex pieces in shaft footprint, union box %s"), LadderBoxes.Num(), *All.ToString()));
	}
	const float A = L->HatchAlpha;
	if (A <= 0.001f || A >= 0.999f) return;
	++SwingFrames;
	// Leaf samples: convex vertices + midpoints of every vertex pair (edges, faces, interior).
	const FTransform HT = H->GetComponentTransform();
	TArray<FVector> P;
	for (const FKConvexElem& E : H->GetBodySetup()->AggGeom.ConvexElems) for (const FVector& V : E.VertexData) P.Add(HT.TransformPosition(V));
	const int32 N = P.Num();
	for (int32 i = 0; i < N; ++i) for (int32 j = i + 1; j < N; ++j) P.Add((P[i] + P[j]) * 0.5f);
	float Lad = 1e9f;
	for (const FBox& B : LadderBoxes) for (const FVector& X : P) Lad = FMath::Min(Lad, FMath::Sqrt(B.ComputeSquaredDistanceToPoint(X)));
	// Capsule: exact distance from the leaf's simple collision to the capsule axis, minus the radius.
	UCapsuleComponent* Cap = Crew->GetCapsuleComponent();
	const float R = Cap->GetScaledCapsuleRadius(), HH = Cap->GetScaledCapsuleHalfHeight();
	const FVector C = Cap->GetComponentLocation();
	float CapD = 1e9f;
	for (int32 k = 0; k <= 8; ++k)
	{
		const FVector Q = C + FVector(0, 0, FMath::Lerp(-(HH - R), HH - R, k / 8.f)); FVector Closest;
		const float D = H->GetDistanceToCollision(Q, Closest);
		if (D >= 0.f) CapD = FMath::Min(CapD, D - R);
	}
	MinCapClear = FMath::Min(MinCapClear, CapD); MinLadClear = FMath::Min(MinLadClear, Lad);
	if (CapD <= 0.f || Lad <= 0.f) bSwingOverlap = true;
	Log(FString::Printf(TEXT("QA-157 swing alpha %.3f: capsule clearance %.1f cm, ladder clearance %.1f cm, feet %.2f m"), A, CapD, Lad, (C.Z - HH) / M));
	const float Marks[3] = { 0.25f, 0.5f, 0.75f }; const TCHAR* Names[3] = { TEXT("14a_hatch_swing_25"), TEXT("14b_hatch_swing_50"), TEXT("14c_hatch_swing_75") };
	if (SwingShots < 3 && A >= Marks[SwingShots]) { Log(FString::Printf(TEXT("SHOT %s state: hatch %.3f"), Names[SwingShots], A)); Shot(Names[SwingShots]); ++SwingShots; }
}
