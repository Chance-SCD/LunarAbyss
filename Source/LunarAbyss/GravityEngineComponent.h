#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GravityEngineComponent.generated.h"

class ACrewCharacter;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGravityStateChanged, bool, bNowOnline);

/**
 * Ship gravity engine. Online: crew inside walk normally. Offline (HP 0 or toggled): crew inside float
 * (MOVE_Flying, no gravity, slight drift, weak push-off). Coming back online ramps gravity in over RampInTime.
 * "Inside" = within the owner's interior mesh bounds (InteriorBoundsComponent), falling back to owner bounds.
 */
UCLASS(ClassGroup = (Ship), meta = (BlueprintSpawnableComponent))
class LUNARABYSS_API UGravityEngineComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGravityEngineComponent();
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category = "Gravity") void SetOnline(bool bNewOnline);
	UFUNCTION(BlueprintCallable, Category = "Gravity") void Toggle() { SetOnline(!bOnline); }
	/** Reduces HP; at 0 the engine goes offline. Returns damage applied. */
	UFUNCTION(BlueprintCallable, Category = "Gravity") float ApplyGravityDamage(float Damage);
	UFUNCTION(BlueprintCallable, Category = "Gravity") void Repair(float Amount);
	UFUNCTION(BlueprintPure, Category = "Gravity") bool IsOnline() const { return bOnline; }
	/** Current gravity scale applied to crew inside (0..1; ramps after restore). */
	UFUNCTION(BlueprintPure, Category = "Gravity") float GetGravityAlpha() const { return GravityAlpha; }

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gravity") bool bOnline = true;
	/** TENTATIVE (Orion, DT_Slice_ShipStats Corvette4C.GravityEngineHP). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gravity") float MaxHP = 220.f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gravity") float HP = 220.f;
	/** DT_Slice_GravityTuning (Orion, TENTATIVE): GravityRestoreRampSec, seconds 0 -> 1 when restored. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gravity") float RampInTime = 1.5f;
	/** DT_Slice_GravityTuning GravityLossRampSec: seconds 1 -> 0 when the engine goes offline. Crew float at 0. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gravity") float RampOutTime = 1.0f;
	/** Random drift given to each floating crew member when gravity drops, cm/s. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gravity") float DriftSpeed = 15.f;
	/** Component whose bounds define "inside the ship". Set by the owner. */
	UPROPERTY(Transient) TObjectPtr<UPrimitiveComponent> InteriorBoundsComponent;

	UPROPERTY(BlueprintAssignable, Category = "Gravity") FGravityStateChanged OnGravityChanged;

private:
	UFUNCTION() void HandleAnyDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType, AController* InstigatedBy, AActor* DamageCauser);
	bool IsInside(const AActor* A) const;
	void ApplyStateTo(ACrewCharacter* C, bool bJustChanged);

	float GravityAlpha = 1.f;
	TSet<TWeakObjectPtr<ACrewCharacter>> Floating; // crew currently in zero-g from us
};
