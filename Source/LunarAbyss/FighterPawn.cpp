#include "FighterPawn.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"
#include "TimerManager.h"

namespace
{
	constexpr float M = 100.f; // metres -> Unreal cm

	FVector MoveToward(const FVector& Cur, const FVector& Target, const FVector& MaxStep)
	{
		FVector Out;
		for (int32 i = 0; i < 3; ++i)
		{
			Out[i] = Cur[i] + FMath::Clamp(Target[i] - Cur[i], -MaxStep[i], MaxStep[i]);
		}
		return Out;
	}
}

AFighterPawn::AFighterPawn()
{
	PrimaryActorTick.bCanEverTick = true;

	Hull = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Hull"));
	RootComponent = Hull;
	Hull->SetSimulatePhysics(false);
	Hull->SetCollisionProfileName(TEXT("Pawn"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Cube.Succeeded())
	{
		Hull->SetStaticMesh(Cube.Object);
		Hull->SetRelativeScale3D(FVector(6.f, 3.f, 1.2f)); // ~6 x 3 x 1.2 m placeholder
	}

	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(Hull);
	SpringArm->SetUsingAbsoluteScale(true);
	SpringArm->TargetArmLength = 1800.f;
	SpringArm->SocketOffset = FVector(0.f, 0.f, 350.f);
	SpringArm->bEnableCameraLag = true;
	SpringArm->bEnableCameraRotationLag = true;
	SpringArm->CameraLagSpeed = 8.f;
	SpringArm->CameraRotationLagSpeed = 6.f;
	SpringArm->bDoCollisionTest = false;

	ChaseCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("ChaseCamera"));
	ChaseCamera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);

	CockpitCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("CockpitCamera"));
	CockpitCamera->SetupAttachment(Hull);
	CockpitCamera->SetUsingAbsoluteScale(true);
	CockpitCamera->SetRelativeLocation(FVector(30.f, 0.f, 60.f)); // relative to scaled cube (local units)
	CockpitCamera->SetAutoActivate(false);

	AutoPossessPlayer = EAutoReceiveInput::Disabled;
	FighterMeshAsset = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/Ships/FighterC/SM_FighterC_Body.SM_FighterC_Body")));
}

void AFighterPawn::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	if (UStaticMesh* Mesh = FighterMeshAsset.LoadSynchronous())
	{
		// Real blockout is already in cm (imported metres x100): drop the cube scale and fix camera offsets.
		Hull->SetStaticMesh(Mesh);
		Hull->SetRelativeScale3D(FVector::OneVector);
		// Forge FighterC v1.3 sockets (prefix stripped on import): Camera_Chase (-16, 0, 5.2) m, Camera_Cockpit.
		if (Hull->DoesSocketExist(TEXT("Camera_Chase")))
		{
			const FVector C = Hull->GetSocketTransform(TEXT("Camera_Chase"), RTS_Component).GetLocation();
			SpringArm->TargetArmLength = -C.X;
			SpringArm->SocketOffset = FVector(0.f, 0.f, C.Z);
		}
		else
		{
			SpringArm->TargetArmLength = FMath::Max(1800.f, Mesh->GetBounds().BoxExtent.X * 2.f);
		}
		for (const TCHAR* S : { TEXT("Camera_Cockpit"), TEXT("SOCKET_Camera_Cockpit") })
		{
			if (Hull->DoesSocketExist(S))
			{
				CockpitCamera->AttachToComponent(Hull, FAttachmentTransformRules::SnapToTargetNotIncludingScale, S);
				break;
			}
		}
	}
}

void AFighterPawn::UnPossessed()
{
	Super::UnPossessed();
	InThrottle = InStrafe = InLift = InPitch = InYaw = InRoll = 0.f;
	bBoost = false;
}

void AFighterPawn::IgnoreActorForSeconds(AActor* Other, float Seconds)
{
	if (!Other) return;
	Hull->IgnoreActorWhenMoving(Other, true);
	TWeakObjectPtr<AFighterPawn> WeakThis(this);
	TWeakObjectPtr<AActor> WeakOther(Other);
	FTimerHandle H;
	GetWorldTimerManager().SetTimer(H, [WeakThis, WeakOther]()
	{
		if (WeakThis.IsValid() && WeakOther.IsValid()) WeakThis->Hull->IgnoreActorWhenMoving(WeakOther.Get(), false);
	}, Seconds, false);
}

void AFighterPawn::BuildInput()
{
	if (FlightContext) return;

	auto MakeAction = [this](const TCHAR* Name, EInputActionValueType Type)
	{
		UInputAction* A = NewObject<UInputAction>(this, Name);
		A->ValueType = Type;
		return A;
	};
	ThrottleAction = MakeAction(TEXT("IA_Throttle"), EInputActionValueType::Axis1D);
	StrafeAction   = MakeAction(TEXT("IA_Strafe"),   EInputActionValueType::Axis1D);
	LiftAction     = MakeAction(TEXT("IA_Lift"),     EInputActionValueType::Axis1D);
	PitchAction    = MakeAction(TEXT("IA_Pitch"),    EInputActionValueType::Axis1D);
	YawAction      = MakeAction(TEXT("IA_Yaw"),      EInputActionValueType::Axis1D);
	RollAction     = MakeAction(TEXT("IA_Roll"),     EInputActionValueType::Axis1D);
	BoostAction    = MakeAction(TEXT("IA_Boost"),    EInputActionValueType::Boolean);
	AssistAction   = MakeAction(TEXT("IA_FlightAssist"), EInputActionValueType::Boolean);
	CameraAction   = MakeAction(TEXT("IA_ToggleCamera"), EInputActionValueType::Boolean);

	FlightContext = NewObject<UInputMappingContext>(this, TEXT("IMC_Flight"));
	auto Map = [this](UInputAction* A, FKey Key, bool bNegate)
	{
		FEnhancedActionKeyMapping& Mapping = FlightContext->MapKey(A, Key);
		if (bNegate)
		{
			Mapping.Modifiers.Add(NewObject<UInputModifierNegate>(FlightContext));
		}
	};
	Map(ThrottleAction, EKeys::W, false);
	Map(ThrottleAction, EKeys::S, true);
	Map(StrafeAction, EKeys::D, false);
	Map(StrafeAction, EKeys::A, true);
	Map(LiftAction, EKeys::SpaceBar, false);
	Map(LiftAction, EKeys::LeftControl, true);
	Map(YawAction, EKeys::MouseX, false);
	Map(PitchAction, EKeys::MouseY, false);
	Map(RollAction, EKeys::E, false);
	Map(RollAction, EKeys::Q, true);
	Map(BoostAction, EKeys::LeftShift, false);
	Map(AssistAction, EKeys::X, false);
	Map(CameraAction, EKeys::V, false);
}

void AFighterPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	BuildInput();

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Sub = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			Sub->AddMappingContext(FlightContext, 0);
		}
	}

	UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EIC)
	{
		UE_LOG(LogTemp, Error, TEXT("AFighterPawn needs EnhancedInputComponent (check Config/DefaultInput.ini)."));
		return;
	}
	EIC->BindAction(ThrottleAction, ETriggerEvent::Triggered, this, &AFighterPawn::OnThrottle);
	EIC->BindAction(ThrottleAction, ETriggerEvent::Completed, this, &AFighterPawn::OnThrottle);
	EIC->BindAction(StrafeAction, ETriggerEvent::Triggered, this, &AFighterPawn::OnStrafe);
	EIC->BindAction(StrafeAction, ETriggerEvent::Completed, this, &AFighterPawn::OnStrafe);
	EIC->BindAction(LiftAction, ETriggerEvent::Triggered, this, &AFighterPawn::OnLift);
	EIC->BindAction(LiftAction, ETriggerEvent::Completed, this, &AFighterPawn::OnLift);
	EIC->BindAction(PitchAction, ETriggerEvent::Triggered, this, &AFighterPawn::OnPitch);
	EIC->BindAction(YawAction, ETriggerEvent::Triggered, this, &AFighterPawn::OnYaw);
	EIC->BindAction(RollAction, ETriggerEvent::Triggered, this, &AFighterPawn::OnRoll);
	EIC->BindAction(RollAction, ETriggerEvent::Completed, this, &AFighterPawn::OnRoll);
	EIC->BindAction(BoostAction, ETriggerEvent::Started, this, &AFighterPawn::OnBoostStart);
	EIC->BindAction(BoostAction, ETriggerEvent::Completed, this, &AFighterPawn::OnBoostStop);
	EIC->BindAction(AssistAction, ETriggerEvent::Started, this, &AFighterPawn::OnToggleAssist);
	EIC->BindAction(CameraAction, ETriggerEvent::Started, this, &AFighterPawn::OnToggleCamera);
}

void AFighterPawn::OnToggleCamera(const FInputActionValue&)
{
	bCockpitView = !bCockpitView;
	ChaseCamera->SetActive(!bCockpitView);
	CockpitCamera->SetActive(bCockpitView);
}

void AFighterPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bParked) return;
	const float Dt = FMath::Min(DeltaSeconds, 0.1f);

	// --- Rotation (mouse is a per-frame delta, keys are rates) ---
	const float MousePitch = FMath::Clamp(InPitch * MouseSensitivity, -1.f, 1.f);
	const float MouseYaw = FMath::Clamp(InYaw * MouseSensitivity, -1.f, 1.f);
	InPitch = 0.f; InYaw = 0.f;
	const FRotator DeltaRot(MousePitch * PitchRate * Dt, MouseYaw * YawRate * Dt, InRoll * RollRate * Dt);
	AddActorLocalRotation(FQuat(DeltaRot));

	// --- Translation ---
	const FQuat Q = GetActorQuat();
	const float SpeedCap = (bBoost ? BoostSpeed : MaxSpeed) * M;
	const float AccelMul = bBoost ? BoostAccelMultiplier : 1.f;
	const FVector AccelLocal = FVector(ForwardAccel * AccelMul, StrafeAccel, StrafeAccel) * M; // cm/s^2
	const FVector Cmd(InThrottle, InStrafe, InLift);

	if (bFlightAssist)
	{
		// Drive local velocity toward commanded velocity; unwanted components are damped.
		const FVector LocalVel = Q.UnrotateVector(LinearVelocity);
		const FVector Target = Cmd.GetClampedToMaxSize(1.f) * SpeedCap;
		LinearVelocity = Q.RotateVector(MoveToward(LocalVel, Target, AccelLocal * Dt));
	}
	else
	{
		// Newtonian: thrusters add velocity, nothing removes it.
		LinearVelocity += Q.RotateVector(Cmd * AccelLocal * Dt);
	}
	LinearVelocity = LinearVelocity.GetClampedToMaxSize(bFlightAssist ? SpeedCap : BoostSpeed * M);

	FHitResult Hit;
	AddActorWorldOffset(LinearVelocity * Dt, true, &Hit);
	if (Hit.bBlockingHit)
	{
		LinearVelocity = FVector::VectorPlaneProject(LinearVelocity, Hit.Normal) * 0.5f;
	}
}
