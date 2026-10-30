#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "SliceTuning.generated.h"

/** Row of DT_Slice_MoveTuning (Data/DT_Slice_MoveTuning.csv). Row "Default" is used. TENTATIVE values. */
USTRUCT(BlueprintType)
struct FSliceMoveTuningRow : public FTableRowBase
{
	GENERATED_BODY()

	/** On-foot speeds, m/s. Sprint = Left Shift, forward only. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float WalkSpeed = 1.25f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float JogSpeed = 3.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float CrouchSpeed = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float SprintSpeed = 5.5f;
	/** Caged slide ladder (ALadderVolume), m/s. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float LadderSlideSpeed = 2.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float LadderClimbSpeed = 0.6f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float LadderZeroGPushSpeed = 0.3f;
	/** Jump apex height at 1 g, m. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float BaseJumpHeightM = 0.6f;
	/** Jump velocity scale = (g / 9.81)^JumpGravityExponent. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float JumpGravityExponent = 0.25f;
	/** Max jump apex height when indoors (inside a ship interior), m. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float IndoorJumpHeightCapM = 1.4f;
	/** Below this gravity (in g) the crew is in mag-boot mode (stub flag only). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float MagBootBelowG = 0.1f;
};
