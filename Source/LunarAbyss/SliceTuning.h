#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "SliceTuning.generated.h"

/** Row of DT_Slice_MoveTuning (Data/DT_Slice_MoveTuning.csv). Row "Default" is used. TENTATIVE values. */
USTRUCT(BlueprintType)
struct FSliceMoveTuningRow : public FTableRowBase
{
	GENERATED_BODY()

	/** Jump apex height at 1 g, m. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float BaseJumpHeightM = 0.6f;
	/** Jump velocity scale = (g / 9.81)^JumpGravityExponent. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float JumpGravityExponent = 0.25f;
	/** Max jump apex height when indoors (inside a ship interior), m. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float IndoorJumpHeightCapM = 1.4f;
	/** Below this gravity (in g) the crew is in mag-boot mode (stub flag only). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float MagBootBelowG = 0.1f;
};
