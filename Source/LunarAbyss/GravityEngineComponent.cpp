#include "GravityEngineComponent.h"
#include "CrewCharacter.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"

UGravityEngineComponent::UGravityEngineComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UGravityEngineComponent::BeginPlay()
{
	Super::BeginPlay();
	HP = MaxHP;
	GravityAlpha = bOnline ? 1.f : 0.f;
	if (AActor* Owner = GetOwner())
	{
		Owner->OnTakeAnyDamage.AddDynamic(this, &UGravityEngineComponent::HandleAnyDamage);
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

	for (TActorIterator<ACrewCharacter> It(GetWorld()); It; ++It)
	{
		ACrewCharacter* C = *It;
		const bool bInside = IsInside(C);
		if (bInside)
		{
			if (bZeroG) ApplyStateTo(C, false);
			C->SetShipGravityScale(GravityAlpha);
		}
		else if (Floating.Contains(C))
		{
			// Left the ship while floating: hand back to normal (world) gravity.
			Floating.Remove(C);
			C->ExitZeroG(1.f);
		}
	}
}
