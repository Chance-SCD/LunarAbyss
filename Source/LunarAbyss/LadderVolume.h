#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LadderVolume.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
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

	// ---- Bottom pressure hatch DCD-77 (QA-140) ----
	/** Hatch leaf, pivot on the hinge (interior socket Hatch_DCD77). Closed = blocks pawns; opens +90 up into the shaft. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ladder|Hatch") TObjectPtr<UStaticMeshComponent> Hatch;
	/** Trigger at the hatch (interior socket Trig_Hatch_DCD77): overlap opens it. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ladder|Hatch") TObjectPtr<UBoxComponent> HatchTrigger;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ladder|Hatch") bool bHasHatch = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ladder|Hatch") bool bHatchPressureLocked = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ladder|Hatch") float HatchOpenTime = 0.6f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ladder|Hatch") float HatchCloseDelay = 2.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ladder|Hatch") FName HatchSocket = TEXT("Hatch_DCD77");
	/** Authored hinge / trigger centre in ship space (UE cm), Atlas-V: Blender (28.40,-13.50,13.117) / (27.70,-13.50,13.867). Used if the socket is off. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ladder|Hatch") FVector HatchHingeLocal = FVector(2840.f, 1350.f, 1311.7f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ladder|Hatch") FVector HatchTriggerLocal = FVector(2770.f, 1350.f, 1386.7f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ladder|Hatch") FName HatchTriggerSocket = TEXT("Trig_Hatch_DCD77");
	/** Hatch slab: top face Z (world cm) and thickness; set from the hinge socket at BeginPlay. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ladder|Hatch") float HatchTopZ = 1311.7f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ladder|Hatch") float HatchThickness = 25.f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ladder|Hatch") float HatchAlpha = 0.f; // 0 closed, 1 open
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ladder|Hatch") bool bHatchPlaceholder = false;
	FString LastPrompt;

	/** Ask the hatch to open. Returns false (and shows the locked prompt) if it is pressure-locked. */
	bool RequestHatchOpen();
	bool IsHatchFullyOpen() const { return !bHasHatch || HatchAlpha >= 0.999f; }
	virtual void Tick(float Dt) override;
	virtual void OnConstruction(const FTransform& T) override;

	FVector GetShaftCentre() const;
	FVector GetTopExitWorld() const;

private:
	float LastOpenRequest = -100.f;
	FTransform HatchRestRel;
	bool bHatchSnapped = false;
	void SnapHatchToSockets();
	UFUNCTION() void OnHatchTrigger(UPrimitiveComponent* Comp, AActor* Other, UPrimitiveComponent* OtherComp, int32 Idx, bool bSweep, const FHitResult& Hit);
	UFUNCTION() void OnBegin(UPrimitiveComponent* Comp, AActor* Other, UPrimitiveComponent* OtherComp, int32 Idx, bool bSweep, const FHitResult& Hit);
	UFUNCTION() void OnEnd(UPrimitiveComponent* Comp, AActor* Other, UPrimitiveComponent* OtherComp, int32 Idx);
};
