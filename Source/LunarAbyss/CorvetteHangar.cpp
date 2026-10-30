#include "CorvetteHangar.h"
#include "FighterPawn.h"
#include "GravityEngineComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

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

	HangarDoorL = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HangarDoorL"));
	HangarDoorL->SetupAttachment(Hull, TEXT("Mount_HangarDoor_L"));
	HangarDoorR = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HangarDoorR"));
	HangarDoorR->SetupAttachment(Hull, TEXT("Mount_HangarDoor_R"));
	for (UStaticMeshComponent* D : { HangarDoorL.Get(), HangarDoorR.Get() })
	{
		D->SetMobility(EComponentMobility::Movable);
		D->SetCollisionProfileName(TEXT("ShipHull"));
	}
	HangarDoorLAsset = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/Ships/Corvette4C/Parts/SM_Corvette4C_HangarDoor_L.SM_Corvette4C_HangarDoor_L")));
	HangarDoorRAsset = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/Ships/Corvette4C/Parts/SM_Corvette4C_HangarDoor_R.SM_Corvette4C_HangarDoor_R")));
	DoorHornSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/Audio/Cues/SND_HGR_DOOR_001.SND_HGR_DOOR_001")));
	DoorOpenSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/Audio/Cues/SND_HGR_DOOR_002.SND_HGR_DOOR_002")));
	DoorCloseSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/Audio/Cues/SND_HGR_DOOR_003.SND_HGR_DOOR_003")));

	GravityEngine = CreateDefaultSubobject<UGravityEngineComponent>(TEXT("GravityEngine"));

	// Crew walk on the interior mesh; the hull's UCX hulls would enclose the decks, so pawns ignore the hull.
	Hull->SetCollisionProfileName(TEXT("ShipHull")); // blocks Spacecraft (fighter), ignores Pawn (crew)
	HullMeshAsset = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/Ships/Corvette4C/SM_Corvette4C_Body.SM_Corvette4C_Body")));
	InteriorMeshAsset = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/Blockouts/SM_Corvette4C_Interior_Blockout.SM_Corvette4C_Interior_Blockout")));
	FighterClass = AFighterPawn::StaticClass();
}

void ACorvetteHangar::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	if (!Hull->GetStaticMesh() && !HullMeshAsset.IsNull()) Hull->SetStaticMesh(HullMeshAsset.LoadSynchronous());
	if (!Interior->GetStaticMesh() && !InteriorMeshAsset.IsNull()) Interior->SetStaticMesh(InteriorMeshAsset.LoadSynchronous());
	if (!HangarDoorL->GetStaticMesh() && !HangarDoorLAsset.IsNull()) HangarDoorL->SetStaticMesh(HangarDoorLAsset.LoadSynchronous());
	if (!HangarDoorR->GetStaticMesh() && !HangarDoorRAsset.IsNull()) HangarDoorR->SetStaticMesh(HangarDoorRAsset.LoadSynchronous());
	AttachDoors();
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

float ACorvetteHangar::DistToSocket(FName Socket, const FVector& World) const
{
	// Nearest instance over interior and hull (both meshes may carry a socket of the same name).
	FString S = Socket.ToString();
	const FName Alt = S.StartsWith(TEXT("SOCKET_")) ? FName(*S.RightChop(7)) : FName(*(TEXT("SOCKET_") + S));
	float Best = TNumericLimits<float>::Max();
	for (const UStaticMeshComponent* C : { Interior.Get(), Hull.Get() })
		for (const FName N : { Socket, Alt })
			if (C && C->DoesSocketExist(N)) Best = FMath::Min(Best, (float)FVector::Dist(C->GetSocketLocation(N), World));
	return Best;
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
	AttachDoors(); // rest pose is runtime state (not serialized), capture it here too
	DoorState = EHangarDoorState::Closed;

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

void ACorvetteHangar::AttachDoors()
{
	// Re-attach at the mount sockets (sockets come from the mesh, which may be set after construction).
	HangarDoorL->AttachToComponent(Hull, FAttachmentTransformRules::SnapToTargetNotIncludingScale, TEXT("Mount_HangarDoor_L"));
	HangarDoorR->AttachToComponent(Hull, FAttachmentTransformRules::SnapToTargetNotIncludingScale, TEXT("Mount_HangarDoor_R"));
	// Part FBX origin = pivot = socket; parts are modelled in ship orientation, so cancel the socket rotation.
	for (UStaticMeshComponent* D : { HangarDoorL.Get(), HangarDoorR.Get() })
	{
		D->SetWorldRotation(Hull->GetComponentQuat());
	}
	DoorLRest = HangarDoorL->GetRelativeTransform();
	DoorRRest = HangarDoorR->GetRelativeTransform();
}

void ACorvetteHangar::PlayDoorSound(const TSoftObjectPtr<USoundBase>& S)
{
	if (USoundBase* Snd = S.LoadSynchronous())
	{
		const FVector Seam = (HangarDoorL->GetComponentLocation() + HangarDoorR->GetComponentLocation()) * 0.5f;
		UGameplayStatics::PlaySoundAtLocation(this, Snd, Seam);
	}
}

void ACorvetteHangar::ApplyDoorPose(float Push01, float Swing01)
{
	// Blender: push +Y (outboard, port), L rotates +90 about Z, R -90. UE flips Y: push -Y, L yaw -90, R yaw +90.
	const FVector Push(0.f, -DoorPushDistance * Push01, 0.f);
	const float Yaw = DoorSwingDeg * Swing01;
	FTransform L = DoorLRest; L.AddToTranslation(Push); L.ConcatenateRotation(FQuat(FRotator(0.f, -Yaw, 0.f)));
	FTransform R = DoorRRest; R.AddToTranslation(Push); R.ConcatenateRotation(FQuat(FRotator(0.f, Yaw, 0.f)));
	HangarDoorL->SetRelativeTransform(L);
	HangarDoorR->SetRelativeTransform(R);
}

void ACorvetteHangar::OpenHangarDoors()
{
	if (DoorState == EHangarDoorState::Open || DoorState == EHangarDoorState::Opening) return;
	DoorState = EHangarDoorState::Opening; DoorTime = -DoorHornLeadIn; bDoorSoundPlayed = false;
	PlayDoorSound(DoorHornSound);
	UE_LOG(LogTemp, Log, TEXT("Hangar doors opening"));
}

void ACorvetteHangar::CloseHangarDoors()
{
	if (DoorState == EHangarDoorState::Closed || DoorState == EHangarDoorState::Closing) return;
	DoorState = EHangarDoorState::Closing; DoorTime = -DoorHornLeadIn; bDoorSoundPlayed = false; CloseTimer = -1.f;
	PlayDoorSound(DoorHornSound);
	UE_LOG(LogTemp, Log, TEXT("Hangar doors closing"));
}

void ACorvetteHangar::ToggleHangarDoors()
{
	if (DoorState == EHangarDoorState::Open || DoorState == EHangarDoorState::Opening) CloseHangarDoors(); else OpenHangarDoors();
}

void ACorvetteHangar::RequestLaunch(AFighterPawn* Fighter)
{
	if (!Fighter) Fighter = ParkedFighter;
	if (!Fighter) return;
	if (DoorState == EHangarDoorState::Open) { LaunchFighter(Fighter); CloseTimer = CloseAfterLaunchDelay; return; }
	PendingLaunch = Fighter;
	if (DoorState == EHangarDoorState::Closing) { DoorState = EHangarDoorState::Closed; } // reverse: restart open cycle
	OpenHangarDoors();
}

void ACorvetteHangar::TickDoors(float Dt)
{
	if (CloseTimer > 0.f && (CloseTimer -= Dt) <= 0.f) CloseHangarDoors();
	if (DoorState != EHangarDoorState::Opening && DoorState != EHangarDoorState::Closing) return;
	DoorTime += Dt;
	if (DoorTime < 0.f) return; // horn lead-in
	auto Ramp = [](float T, float A, float B) { return B > A ? FMath::SmoothStep(A, B, T) : (T >= B ? 1.f : 0.f); };
	if (DoorState == EHangarDoorState::Opening)
	{
		if (!bDoorSoundPlayed) { bDoorSoundPlayed = true; PlayDoorSound(DoorOpenSound); }
		ApplyDoorPose(Ramp(DoorTime, DoorOpenPushStart, DoorOpenSwingStart), Ramp(DoorTime, DoorOpenSwingStart, DoorOpenSettle));
		if (DoorTime >= DoorOpenSettle)
		{
			DoorState = EHangarDoorState::Open;
			UE_LOG(LogTemp, Log, TEXT("Hangar doors open"));
			if (PendingLaunch) { LaunchFighter(PendingLaunch); PendingLaunch = nullptr; CloseTimer = CloseAfterLaunchDelay; }
		}
	}
	else
	{
		if (!bDoorSoundPlayed) { bDoorSoundPlayed = true; PlayDoorSound(DoorCloseSound); }
		ApplyDoorPose(1.f - Ramp(DoorTime, DoorCloseSealStart, DoorCloseSealed), 1.f - Ramp(DoorTime, DoorCloseSwingStart, DoorCloseSealStart));
		if (DoorTime >= DoorCloseSealed) { DoorState = EHangarDoorState::Closed; UE_LOG(LogTemp, Log, TEXT("Hangar doors closed")); }
	}
}

void ACorvetteHangar::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	TickDoors(DeltaSeconds);
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
	Fighter->IgnoreActorForSeconds(this, 4.f);
	Fighter->SuspendAssist(LaunchCoastTime); // coast out of the bay; assist would otherwise brake to 0 at once // clear the bay without snagging on hull collision
}
