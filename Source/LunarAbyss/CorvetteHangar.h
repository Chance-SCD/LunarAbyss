#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CorvetteHangar.generated.h"

class AFighterPawn;
class UStaticMeshComponent;

/** Stub corvette that flies at a constant velocity and can launch a fighter from its hangar. */
UCLASS()
class LUNARABYSS_API ACorvetteHangar : public AActor
{
	GENERATED_BODY()

public:
	ACorvetteHangar();
	virtual void Tick(float DeltaSeconds) override;
	virtual FVector GetVelocity() const override { return Velocity * 100.f; }

	/** Teleports the fighter to LaunchPoint and gives it the corvette's velocity plus LaunchSpeed forward. */
	UFUNCTION(BlueprintCallable, Category = "Hangar")
	void LaunchFighter(AFighterPawn* Fighter);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Hull;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> LaunchPoint;

	/** Constant world velocity in m/s. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	FVector Velocity = FVector(250.f, 0.f, 0.f);

	/** Extra forward push along LaunchPoint's X, m/s. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hangar")
	float LaunchSpeed = 10.f;
};
