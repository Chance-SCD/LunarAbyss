#include "LadderVolume.h"
#include "CrewCharacter.h"
#include "SliceTuning.h"
#include "Components/BoxComponent.h"
#include "Engine/DataTable.h"

ALadderVolume::ALadderVolume()
{
	Shaft = CreateDefaultSubobject<UBoxComponent>(TEXT("Shaft"));
	RootComponent = Shaft;
	Shaft->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	Shaft->SetGenerateOverlapEvents(true);
	Shaft->SetBoxExtent(FVector(70.f, 45.f, 700.f));
	MoveTuningTable = TSoftObjectPtr<UDataTable>(FSoftObjectPath(TEXT("/Game/Data/DT_Slice_MoveTuning.DT_Slice_MoveTuning")));
}

void ALadderVolume::BeginPlay()
{
	Super::BeginPlay();
	if (UDataTable* DT = MoveTuningTable.LoadSynchronous())
	{
		if (const FSliceMoveTuningRow* R = DT->FindRow<FSliceMoveTuningRow>(TEXT("Default"), TEXT("Ladder")))
		{
			SlideSpeed = R->LadderSlideSpeed; ClimbSpeed = R->LadderClimbSpeed; ZeroGPushSpeed = R->LadderZeroGPushSpeed;
		}
	}
	Shaft->OnComponentBeginOverlap.AddDynamic(this, &ALadderVolume::OnBegin);
	Shaft->OnComponentEndOverlap.AddDynamic(this, &ALadderVolume::OnEnd);
}

FVector ALadderVolume::GetShaftCentre() const { return Shaft->GetComponentLocation(); }
FVector ALadderVolume::GetTopExitWorld() const { FVector P = GetActorTransform().TransformPosition(TopExitLocal); P.Z = TopFloorZ; return P; }

void ALadderVolume::OnBegin(UPrimitiveComponent*, AActor* Other, UPrimitiveComponent* OtherComp, int32, bool, const FHitResult&)
{
	if (ACrewCharacter* C = Cast<ACrewCharacter>(Other)) C->LadderOverlapBegin(this);
}

void ALadderVolume::OnEnd(UPrimitiveComponent*, AActor* Other, UPrimitiveComponent*, int32)
{
	if (ACrewCharacter* C = Cast<ACrewCharacter>(Other)) C->LadderOverlapEnd(this);
}
