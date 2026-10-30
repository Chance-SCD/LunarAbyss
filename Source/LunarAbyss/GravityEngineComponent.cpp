#include "GravityEngineComponent.h"
#include "CrewCharacter.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Components/StaticMeshComponent.h"
#include "Components/AudioComponent.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

UGravityEngineComponent::UGravityEngineComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SpinDownSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/Audio/Cues/SND_4C_GRV_001.SND_4C_GRV_001")));
	SpinUpSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/Audio/Cues/SND_4C_GRV_002.SND_4C_GRV_002")));
	PAGravityLossSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/Audio/Cues/SND_4C_GRV_003.SND_4C_GRV_003")));
}

void UGravityEngineComponent::PlayGravitySound(const TSoftObjectPtr<USoundBase>& Sound, bool bWithShipWide)
{
	USoundBase* S = Sound.LoadSynchronous();
	if (!S || !GetWorld()) return;
	FVector Loc = GetOwner()->GetActorLocation();
	if (const UStaticMeshComponent* Int = Cast<UStaticMeshComponent>(InteriorBoundsComponent.Get()))
	{
		if (Int->DoesSocketExist(EngineSocket)) Loc = Int->GetSocketLocation(EngineSocket);
	}
	UGameplayStatics::PlaySoundAtLocation(this, S, Loc);
	if (bWithShipWide) UGameplayStatics::PlaySound2D(this, S, ShipWideVolume);
}

void UGravityEngineComponent::SetState(EGravityState NewState)
{
	if (NewState == State) return;
	const EGravityState Old = State;
	State = NewState;
	UE_LOG(LogTemp, Log, TEXT("Gravity state %d -> %d"), (int32)Old, (int32)NewState);
	OnGravityStateChanged.Broadcast(Old, NewState);
}

void UGravityEngineComponent::BeginPlay()
{
	Super::BeginPlay();
	HP = MaxHP;
	GravityAlpha = bOnline ? 1.f : 0.f;
	if (AActor* Owner = GetOwner())
	{
		Owner->OnTakeAnyDamage.AddDynamic(this, &UGravityEngineComponent::HandleAnyDamage);
		OnGravityStateChanged.AddDynamic(this, &UGravityEngineComponent::HandleGravityStateChanged);
		LoadGravityAudioMap();
	}
}

void UGravityEngineComponent::HandleAnyDamage(AActor*, float Damage, const UDamageType*, AController*, AActor*)
{
	ApplyGravityDamage(Damage);
}

float UGravityEngineComponent::ApplyGravityDamage(float Damage)
{
	if (Damage <= 0.f || HP <= 0.f) return 0.f;
	const float Applied = FMath::Min(Damage, HP);
	HP -= Applied;
	if (HP <= 0.f) SetOnline(false);
	return Applied;
}

void UGravityEngineComponent::Repair(float Amount)
{
	HP = FMath::Clamp(HP + Amount, 0.f, MaxHP);
}

void UGravityEngineComponent::SetOnline(bool bNewOnline)
{
	if (bNewOnline && HP <= 0.f)
	{
		UE_LOG(LogTemp, Warning, TEXT("Gravity engine has 0 HP; repair before bringing it online."));
		return;
	}
	if (bNewOnline == bOnline) return;
	bOnline = bNewOnline;
	UE_LOG(LogTemp, Log, TEXT("Gravity engine %s (HP %.0f/%.0f)"), bOnline ? TEXT("ONLINE") : TEXT("OFFLINE"), HP, MaxHP);
	if (bOnline)
	{
		for (TActorIterator<ACrewCharacter> It(GetWorld()); It; ++It)
		{
			if (IsInside(*It)) ApplyStateTo(*It, true);
		}
	}
	OnGravityChanged.Broadcast(bOnline);
}

bool UGravityEngineComponent::IsInside(const AActor* A) const
{
	const UPrimitiveComponent* P = InteriorBoundsComponent ? InteriorBoundsComponent.Get() : Cast<UPrimitiveComponent>(GetOwner()->GetRootComponent());
	if (!A || !P) return false;
	return P->Bounds.GetBox().IsInsideOrOn(A->GetActorLocation());
}

void UGravityEngineComponent::ApplyStateTo(ACrewCharacter* C, bool bJustChanged)
{
	UCharacterMovementComponent* Move = C->GetCharacterMovement();
	if (C->IsOnLadder()) return; // ladder handles zero-g / restore itself (QA-136)
	if (!bOnline)
	{
		if (!Floating.Contains(C))
		{
			Floating.Add(C);
			C->EnterZeroG();
			Move->Velocity += FMath::VRand() * DriftSpeed + FVector(0, 0, DriftSpeed * 0.5f);
		}
	}
	else if (Floating.Contains(C) || bJustChanged)
	{
		Floating.Remove(C);
		C->ExitZeroG(GravityAlpha); // falls, then walks on landing
	}
}

void UGravityEngineComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bOnline && GravityAlpha < 1.f)
	{
		GravityAlpha = RampInTime > 0.f ? FMath::Min(1.f, GravityAlpha + DeltaTime / RampInTime) : 1.f;
	}
	else if (!bOnline && GravityAlpha > 0.f)
	{
		GravityAlpha = RampOutTime > 0.f ? FMath::Max(0.f, GravityAlpha - DeltaTime / RampOutTime) : 0.f;
	}
	const bool bZeroG = !bOnline && GravityAlpha <= 0.f;
	SetState(bOnline ? (GravityAlpha < 1.f ? EGravityState::Restoring : EGravityState::On)
	                 : (GravityAlpha > 0.f ? EGravityState::Failing : EGravityState::Off));

	for (TActorIterator<ACrewCharacter> It(GetWorld()); It; ++It)
	{
		ACrewCharacter* C = *It;
		const bool bInside = IsInside(C);
		C->SetIndoors(bInside);
		if (bInside)
		{
			if (bZeroG) ApplyStateTo(C, false);
			C->SetShipGravityScale(GravityAlpha);
			C->SetGravityRestoring(bOnline && GravityAlpha < 1.f);
		}
		else if (Floating.Contains(C))
		{
			// Left the ship while floating: hand back to normal (world) gravity.
			Floating.Remove(C);
			C->ExitZeroG(1.f);
		}
	}
}

// ---- Echo-S C4C_GravityAudioMap.json (v1.8): state -> cues ----
void UGravityEngineComponent::LoadGravityAudioMap()
{
	StateCues.Reset();
	auto Cue = [](const FString& Id) { return TSoftObjectPtr<USoundBase>(FSoftObjectPath(FString::Printf(TEXT("/Game/Audio/Cues/%s.%s"), *Id, *Id))); };
	FString Text;
	const FString File = FPaths::ProjectDir() / GravityAudioMapFile;
	TSharedPtr<FJsonObject> Root;
	if (FFileHelper::LoadFileToString(Text, *File) && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) && Root.IsValid())
	{
		for (const TSharedPtr<FJsonValue>& V : Root->GetArrayField(TEXT("events")))
		{
			const TSharedPtr<FJsonObject> E = V->AsObject();
			const FString When = E->GetStringField(TEXT("when"));
			if (!When.StartsWith(TEXT("New =="))) continue; // notify-driven rows (footsteps, spray) belong to the crew anim notifies
			const FString St = E->GetStringField(TEXT("state"));
			const EGravityState S = St == TEXT("Off") ? EGravityState::Off : St == TEXT("Restoring") ? EGravityState::Restoring : St == TEXT("Failing") ? EGravityState::Failing : EGravityState::On;
			for (const TSharedPtr<FJsonValue>& C : E->GetArrayField(TEXT("cues"))) StateCues.FindOrAdd(S).Ids.Add(C->AsString());
		}
		UE_LOG(LogTemp, Log, TEXT("Gravity audio map %s: Off %d, Restoring %d, On %d cues"), *File, StateCues.FindOrAdd(EGravityState::Off).Ids.Num(), StateCues.FindOrAdd(EGravityState::Restoring).Ids.Num(), StateCues.FindOrAdd(EGravityState::On).Ids.Num());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Gravity audio map %s not found; using built-in v1.8 defaults"), *File);
		StateCues.FindOrAdd(EGravityState::Off).Ids = { TEXT("SND_4C_GRV_001"), TEXT("SND_4C_GRV_003") };
		StateCues.FindOrAdd(EGravityState::Restoring).Ids = { TEXT("SND_4C_GRV_002") };
	}
	for (auto& P : StateCues) { P.Value.Sounds.Reset(); for (const FString& Id : P.Value.Ids) P.Value.Sounds.Add(Cue(Id)); }
}

void UGravityEngineComponent::HandleGravityStateChanged(EGravityState Old, EGravityState New)
{
	const FGravityStateCues* Cues = StateCues.Find(New);
	if (New == EGravityState::Restoring && SpinDownTail) { SpinDownTail->FadeOut(0.3f, 0.f); SpinDownTail = nullptr; } // stop the spin-down tail
	if (New == EGravityState::On) bPAPlayedThisOutage = false;
	if (!Cues) return;
	for (int32 i = 0; i < Cues->Sounds.Num(); ++i)
	{
		const FString& Id = Cues->Ids[i];
		if (Id == TEXT("SND_4C_GRV_003")) { if (bPAPlayedThisOutage) continue; bPAPlayedThisOutage = true; } // PA once per loss
		USoundBase* S = Cues->Sounds[i].LoadSynchronous();
		if (!S) { UE_LOG(LogTemp, Warning, TEXT("Gravity cue %s missing"), *Id); continue; }
		FVector Loc = GetOwner()->GetActorLocation();
		if (const UStaticMeshComponent* Int = Cast<UStaticMeshComponent>(InteriorBoundsComponent.Get()))
		{
			for (const FName N : { FName(TEXT("GravityEngine")), EngineSocket }) if (Int->DoesSocketExist(N)) { Loc = Int->GetSocketLocation(N); break; }
		}
		UAudioComponent* AC = UGameplayStatics::SpawnSoundAtLocation(this, S, Loc);
		if (Id == TEXT("SND_4C_GRV_001")) SpinDownTail = AC;
		UGameplayStatics::PlaySound2D(this, S, ShipWideVolume);
		LastPlayedCues.Add(FString::Printf(TEXT("%d->%d:%s"), (int32)Old, (int32)New, *Id));
		UE_LOG(LogTemp, Log, TEXT("Gravity %d -> %d: play %s"), (int32)Old, (int32)New, *Id);
	}
}
