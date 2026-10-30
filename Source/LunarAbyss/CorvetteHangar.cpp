#include "CorvetteHangar.h"
#include "FighterPawn.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

ACorvetteHangar::ACorvetteHangar()
{
	PrimaryActorTick.bCanEverTick = true;

	Hull = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Hull"));
	RootComponent = Hull;
	Hull->SetSimulatePhysics(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Cube.Succeeded())
	{
		Hull->SetStaticMesh(Cube.Object);
		Hull->SetRelativeScale3D(FVector(60.f, 15.f, 12.f)); // ~60 x 15 x 12 m placeholder
	}

	LaunchPoint = CreateDefaultSubobject<USceneComponent>(TEXT("LaunchPoint"));
	LaunchPoint->SetupAttachment(Hull);
	LaunchPoint->SetUsingAbsoluteScale(true);
	LaunchPoint->SetRelativeLocation(FVector(40.f, 0.f, -75.f)); // scaled cube units: ~24 m fore, ~9 m below centre
}

void ACorvetteHangar::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	AddActorWorldOffset(Velocity * 100.f * DeltaSeconds, false);
}

void ACorvetteHangar::LaunchFighter(AFighterPawn* Fighter)
{
	if (!Fighter || !LaunchPoint) return;
	const FTransform T = LaunchPoint->GetComponentTransform();
	Fighter->SetActorLocationAndRotation(T.GetLocation(), T.GetRotation(), false, nullptr, ETeleportType::TeleportPhysics);
	Fighter->SetLinearVelocity(Velocity * 100.f + T.GetRotation().GetForwardVector() * LaunchSpeed * 100.f);
}
