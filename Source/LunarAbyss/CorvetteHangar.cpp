#include "CorvetteHangar.h"
#include "FighterPawn.h"
#include "GravityEngineComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "TimerManager.h"

static void SnapToSocketIfPresent(ACorvetteHangar* V, USceneComponent* C, FName Socket);

ACorvetteHangar::ACorvetteHangar()
{
	PrimaryActorTick.bCanEverTick = true;

	Hull = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Hull"));
	RootComponent = Hull;
	Hull->SetSimulatePhysics(false);
	Hull->SetMobility(EComponentMobility::Movable);

	Interior = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Interior"));
	Interior->SetupAttachment(Hull);
	Interior->SetSimulatePhysics(false);
	Interior->SetCollisionProfileName(TEXT("BlockAll"));
	Interior->SetCanEverAffectNavigation(false);

	LaunchPoint = CreateDefaultSubobject<USceneComponent>(TEXT("LaunchPoint"));
	LaunchPoint->SetupAttachment(Hull);
	// Fallback = SOCKET_Hangar (6.675, 1.0, 9.667) m, yaw 90 in Blender -> UE (667.5, -100, 966.7) cm, yaw -90
	LaunchPoint->SetRelativeLocationAndRotation(FVector(-382.5f, -350.f, 966.7f), FRotator(0.f, -90.f, 0.f)); // Hangar_Spot_Fighter fallback

	DockPortPoint = CreateDefaultSubobject<USceneComponent>(TEXT("DockPortPoint"));
	DockPortPoint->SetupAttachment(Hull);
	// Fallback = Forge SOCKET_DockCollar_Port (39.4, 14.05, 14.87) m, rot Z 90 Blender -> UE (3940, -1405, 1487) cm, yaw -90
	DockPortPoint->SetRelativeLocationAndRotation(FVector(3940.f, -1405.f, 1487.f), FRotator(0.f, -90.f, 0.f));

	GravityEngine = CreateDefaultSubobject<UGravityEngineComponent>(TEXT("GravityEngine"));

	// Crew walk on the interior mesh; the hull's UCX hulls would enclose the decks, so pawns ignore the hull.
	Hull->SetCollisionProfileName(TEXT("BlockAll"));
	Hull->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	HullMeshAsset = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/Ships/Corvette4C/SM_Corvette4C_Body.SM_Corvette4C_Body")));
	InteriorMeshAsset = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/Blockouts/SM_Corvette4C_Interior_Blockout.SM_Corvette4C_Interior_Blockout")));
	FighterClass = AFighterPawn::StaticClass();
}

void ACorvetteHangar::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	if (!Hull->GetStaticMesh() && !HullMeshAsset.IsNull()) Hull->SetStaticMesh(HullMeshAsset.LoadSynchronous());
	if (!Interior->GetStaticMesh() && !InteriorMeshAsset.IsNull()) Interior->SetStaticMesh(InteriorMeshAsset.LoadSynchronous());
	SnapLaunchPointToSocket();
	SnapToSocketIfPresent(this, DockPortPoint, DockPortSocket);
}

bool ACorvetteHangar::GetSocketTransform(FName Socket, FTransform& Out) const
{
	// Accept both "Name" and "SOCKET_Name" (UE's FBX importer strips the prefix; Atlas-V's json keeps it).
	FString S = Socket.ToString();
	const FName Alt = S.StartsWith(TEXT("SOCKET_")) ? FName(*S.RightChop(7)) : FName(*(TEXT("SOCKET_") + S));
	for (const UStaticMeshComponent* C : { Interior.Get(), Hull.Get() })
	{
		for (const FName N : { Socket, Alt })
		{
			if (C && C->DoesSocketExist(N))
			{
				Out = C->GetSocketTransform(N, RTS_World);
				return true;
			}
		}
	}
	return false;
}

bool ACorvetteHangar::SnapLaunchPointToSocket()
{
	for (const FName& Name : LaunchSocketNames)
	{
		FTransform T;
		if (GetSocketTransform(Name, T))
		{
			T.SetScale3D(FVector::OneVector);
			const FVector Loc = T.TransformPosition(LaunchOffset);
			FQuat Rot = T.GetRotation();
			FTransform A, B;
			if (GetSocketTransform(LaunchThresholdSocket, A) && GetSocketTransform(LaunchClearSocket, B))
			{
				Rot = (B.GetLocation() - A.GetLocation()).GetSafeNormal().ToOrientationQuat(); // nose out of the bay
			}
			LaunchPoint->SetWorldLocationAndRotation(Loc, Rot);
			UE_LOG(LogTemp, Log, TEXT("LaunchPoint snapped to %s at %s"), *Name.ToString(), *Loc.ToString());
			return true;
		}
	}
	UE_LOG(LogTemp, Warning, TEXT("No launch socket found on %s; using default LaunchPoint."), *GetName());
	return false;
}

static void SnapToSocketIfPresent(ACorvetteHangar* V, USceneComponent* C, FName Socket)
{
	FTransform T;
	if (C && V->GetSocketTransform(Socket, T)) C->SetWorldLocationAndRotation(T.GetLocation(), T.GetRotation());
}

void ACorvetteHangar::BeginPlay()
{
	Super::BeginPlay();
	GravityEngine->InteriorBoundsComponent = Interior->GetStaticMesh() ? Interior.Get() : Hull.Get();
	SnapLaunchPointToSocket();

	if (bSpawnParkedFighter && FighterClass)
	{
		FActorSpawnParameters P;
		P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ParkedFighter = GetWorld()->SpawnActor<AFighterPawn>(FighterClass, LaunchPoint->GetComponentTransform(), P);
		if (ParkedFighter)
		{
			ParkedFighter->SetParked(true);
			ParkedFighter->AttachToComponent(LaunchPoint, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		}
	}
}

void ACorvetteHangar::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Cap = bBoost ? BoostSpeed : MaxSpeed;
	const FVector Target = GetActorForwardVector() * FMath::Clamp(TargetSpeed, -Cap, Cap);
	const FVector Delta = Target - CurrentVelocity;
	const float Step = Acceleration * DeltaSeconds;
	CurrentVelocity = Delta.SizeSquared() <= Step * Step ? Target : CurrentVelocity + Delta.GetSafeNormal() * Step;
	if (bMoveInWorld && !CurrentVelocity.IsNearlyZero())
	{
		AddActorWorldOffset(CurrentVelocity * 100.f * DeltaSeconds, false);
	}
}

void ACorvetteHangar::LaunchFighter(AFighterPawn* Fighter)
{
	if (!Fighter) Fighter = ParkedFighter;
	if (!Fighter || !LaunchPoint) return;
	SnapLaunchPointToSocket();
	Fighter->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	Fighter->SetParked(false);
	if (Fighter == ParkedFighter) ParkedFighter = nullptr;

	FTransform T = LaunchPoint->GetComponentTransform();
	FTransform A, B;
	if (GetSocketTransform(LaunchThresholdSocket, A) && GetSocketTransform(LaunchClearSocket, B))
	{
		// Start at the bay threshold, nose toward the clear point.
		T = FTransform((B.GetLocation() - A.GetLocation()).GetSafeNormal().ToOrientationQuat(), A.GetLocation());
	}
	Fighter->SetActorLocationAndRotation(T.GetLocation(), T.GetRotation(), false, nullptr, ETeleportType::TeleportPhysics);
	const FVector ShipVel = bMoveInWorld ? CurrentVelocity * 100.f : FVector::ZeroVector;
	Fighter->SetLinearVelocity(ShipVel + T.GetRotation().GetForwardVector() * LaunchSpeed * 100.f);
	Fighter->IgnoreActorForSeconds(this, 4.f); // clear the bay without snagging on hull collision
}
