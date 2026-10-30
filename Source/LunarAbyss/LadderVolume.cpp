#include "LadderVolume.h"
#include "UObject/ConstructorHelpers.h"
#include "CrewCharacter.h"
#include "SliceTuning.h"
#include "Components/BoxComponent.h"
#include "Engine/DataTable.h"
#include "Engine/StaticMesh.h"
#include "Engine/Engine.h"
#include "Components/StaticMeshComponent.h"
#include "CorvetteHangar.h"
#include "EngineUtils.h"

ALadderVolume::ALadderVolume()
{
	Shaft = CreateDefaultSubobject<UBoxComponent>(TEXT("Shaft"));
	RootComponent = Shaft;
	Shaft->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	Shaft->SetGenerateOverlapEvents(true);
	Shaft->SetBoxExtent(FVector(70.f, 45.f, 700.f));
	PrimaryActorTick.bCanEverTick = true;
	Hatch = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Hatch"));
	Hatch->SetupAttachment(Shaft);
	Hatch->SetMobility(EComponentMobility::Movable);
	Hatch->SetCollisionProfileName(TEXT("BlockAll"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> HatchMesh(TEXT("/Game/Blockouts/SM_Hatch_DCD77.SM_Hatch_DCD77"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (HatchMesh.Succeeded()) Hatch->SetStaticMesh(HatchMesh.Object);
	else if (Cube.Succeeded()) { Hatch->SetStaticMesh(Cube.Object); bHatchPlaceholder = true; } // 100 cm cube, scaled below
	HatchTrigger = CreateDefaultSubobject<UBoxComponent>(TEXT("HatchTrigger"));
	HatchTrigger->SetupAttachment(Shaft);
	HatchTrigger->SetBoxExtent(FVector(70.f, 45.f, 75.f));
	HatchTrigger->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	HatchTrigger->SetGenerateOverlapEvents(true);
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
	SnapHatchToSockets();
	HatchTrigger->OnComponentBeginOverlap.AddDynamic(this, &ALadderVolume::OnHatchTrigger);
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

void ALadderVolume::OnConstruction(const FTransform& T) { Super::OnConstruction(T); SnapHatchToSockets(); }

void ALadderVolume::SnapHatchToSockets()
{
	Hatch->SetVisibility(bHasHatch); Hatch->SetCollisionEnabled(bHasHatch ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	HatchTrigger->SetCollisionEnabled(bHasHatch ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	if (!bHasHatch || !GetWorld()) return;
	for (TActorIterator<ACorvetteHangar> It(GetWorld()); It; ++It)
	{
		FTransform H, Tr; bool bTrigFallback = false;
		bool bH = It->GetSocketTransform(HatchSocket, H);
		const FVector C = GetShaftCentre();
		if (bH && FVector2D::Distance(FVector2D(H.GetLocation()), FVector2D(C)) > 150.f)
		{
			// The interior's imported sockets come in displaced (see report); fall back to Atlas-V's authored hinge.
			UE_LOG(LogTemp, Warning, TEXT("%s: socket %s at %s is %.0f cm off the shaft; using authored hinge %s"), *GetName(), *HatchSocket.ToString(), *H.GetLocation().ToString(),
				FVector2D::Distance(FVector2D(H.GetLocation()), FVector2D(C)), *It->GetActorTransform().TransformPosition(HatchHingeLocal).ToString());
			H = FTransform(It->GetActorRotation(), It->GetActorTransform().TransformPosition(HatchHingeLocal));
			Tr = FTransform(It->GetActorRotation(), It->GetActorTransform().TransformPosition(HatchTriggerLocal));
			bTrigFallback = true;
		}
		if (bH)
		{
			// Hinge pivot. The leaf extends along local -X (1.4 m) and -Z (thickness) from it.
			Hatch->SetWorldLocationAndRotation(H.GetLocation(), H.GetRotation());
			HatchTopZ = H.GetLocation().Z;
			bHatchSnapped = true;
		}
		if (bTrigFallback || It->GetSocketTransform(HatchTriggerSocket, Tr)) HatchTrigger->SetWorldLocation(Tr.GetLocation());
		else HatchTrigger->SetWorldLocation(FVector(GetShaftCentre().X, GetShaftCentre().Y, HatchTopZ + 75.f));
		break;
	}
	if (bHatchPlaceholder)
	{
		// Placeholder: engine cube (100 cm, centred) scaled to 140 x 90 x 25 and offset so the hinge is the pivot.
		Hatch->SetWorldScale3D(FVector(1.4f, 0.9f, 0.25f));
		Hatch->AddLocalOffset(FVector(-70.f / 1.4f, 0.f, -12.5f / 0.25f)); // cube is centred: move it so the hinge edge sits on the pivot
	}
	HatchRestRel = Hatch->GetRelativeTransform();
	UE_LOG(LogTemp, Log, TEXT("%s hatch: snapped=%d placeholder=%d topZ=%.1f mesh=%s tick=%d"), *GetName(), bHatchSnapped, bHatchPlaceholder, HatchTopZ, *GetNameSafe(Hatch->GetStaticMesh()), PrimaryActorTick.bCanEverTick);
}

bool ALadderVolume::RequestHatchOpen()
{
	if (!bHasHatch) return true;
	if (bHatchPressureLocked)
	{
		LastPrompt = TEXT("DCD-77 LOCKED - pressure differential. Climb back up.");
		if (GEngine) GEngine->AddOnScreenDebugMessage(0x77, 0.25f, FColor::Red, LastPrompt);
		return false;
	}
	LastOpenRequest = GetWorld()->GetTimeSeconds();
	return true;
}

void ALadderVolume::OnHatchTrigger(UPrimitiveComponent*, AActor* Other, UPrimitiveComponent*, int32, bool, const FHitResult&)
{
	if (Cast<ACrewCharacter>(Other)) RequestHatchOpen();
}

void ALadderVolume::Tick(float Dt)
{
	Super::Tick(Dt);
	if (!bHasHatch) return;
	// Keep the hatch open while any crew is in the trigger.
	TArray<AActor*> In; HatchTrigger->GetOverlappingActors(In, ACrewCharacter::StaticClass());
	if (In.Num() > 0 && !bHatchPressureLocked) LastOpenRequest = GetWorld()->GetTimeSeconds();
	const bool bWantOpen = !bHatchPressureLocked && GetWorld()->GetTimeSeconds() - LastOpenRequest < HatchCloseDelay;
	const float Prev = HatchAlpha;
	HatchAlpha = FMath::Clamp(HatchAlpha + (bWantOpen ? 1.f : -1.f) * Dt / FMath::Max(HatchOpenTime, 0.01f), 0.f, 1.f);
	if (HatchAlpha != Prev)
	{
		// Atlas-V: +90 about local Y in Blender = pitch -90 in UE (Y is flipped), leaf swings up into the shaft.
		FTransform R = HatchRestRel;
		R.SetRotation(HatchRestRel.GetRotation() * FQuat(FRotator(-90.f * HatchAlpha, 0.f, 0.f)));
		Hatch->SetRelativeTransform(R);
		// While moving/open the leaf must not shove the capsule; it blocks pawns only when shut.
		Hatch->SetCollisionResponseToChannel(ECC_Pawn, HatchAlpha <= 0.001f ? ECR_Block : ECR_Ignore);
	}
}
