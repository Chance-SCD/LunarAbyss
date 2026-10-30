#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LadderVolume.generated.h"

class UBoxComponent;
class ACrewCharacter;

/**
 * Caged slide ladder (QA-136). Box = the shaft (local Z = climb axis). Inside there is no free fall:
 * idle = slide down at SlideSpeed, forward/up input = climb at ClimbSpeed, exit only at top or bottom.
 * Zero-g: float in the shaft, input pushes slowly along it; on restore the slide resumes (no fall, no fall damage).
 * Speeds default from DT_Slice_MoveTuning (LadderSlideSpeed / LadderClimbSpeed / LadderZeroGPushSpeed).
 */
UCLASS()
class LUNARABYSS_API ALadderVolume : public AActor
{
	GENERATED_BODY()

public:
	ALadderVolume();
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ladder") TObjectPtr<UBoxComponent> Shaft;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ladder") float SlideSpeed = 2.0f;       // m/s
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ladder") float ClimbSpeed = 0.6f;       // m/s
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ladder") float ZeroGPushSpeed = 0.3f;   // m/s
	/** Floor height at the bottom exit and the top landing, world Z cm (set by placement). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ladder") float BottomFloorZ = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ladder") float TopFloorZ = 0.f;
	/** Where the crew steps off at the top (world, feet height = TopFloorZ). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ladder", meta = (MakeEditWidget)) FVector TopExitLocal = FVector(0, -100, 0);
	UPROPERTY(EditAnywhere, Category = "Ladder") TSoftObjectPtr<UDataTable> MoveTuningTable;

	FVector GetShaftCentre() const;
	FVector GetTopExitWorld() const;

private:
	UFUNCTION() void OnBegin(UPrimitiveComponent* Comp, AActor* Other, UPrimitiveComponent* OtherComp, int32 Idx, bool bSweep, const FHitResult& Hit);
	UFUNCTION() void OnEnd(UPrimitiveComponent* Comp, AActor* Other, UPrimitiveComponent* OtherComp, int32 Idx);
};
