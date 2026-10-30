#include "CrewCharacter.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/LocalPlayer.h"
#include "Engine/DamageEvents.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"

namespace { constexpr float M = 100.f; }

ACrewCharacter::ACrewCharacter()
{
	Health = MaxHealth;
	PrimaryActorTick.bCanEverTick = true;
	GetCapsuleComponent()->InitCapsuleSize(34.f, 90.f); // r 0.34 m, h 1.8 m (Atlas-V walk-height check)

	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCamera->SetupAttachment(GetCapsuleComponent());
	FirstPersonCamera->SetRelativeLocation(FVector(0.f, 0.f, 70.f)); // eye ~1.6 m above floor
	FirstPersonCamera->bUsePawnControlRotation = true;

	bUseControllerRotationYaw = true;
	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->NavAgentProps.bCanCrouch = true;
	Move->MaxStepHeight = 45.f;
	Move->SetWalkableFloorAngle(44.77f);
	Move->JumpZVelocity = 350.f;
	Move->AirControl = 0.2f;
	// Moving-base support: inherit base velocity when leaving a moving floor.
	Move->bImpartBaseVelocityX = Move->bImpartBaseVelocityY = Move->bImpartBaseVelocityZ = true;
	Move->bImpartBaseAngularVelocity = true;
	UpdateSpeeds();
}

void ACrewCharacter::UpdateSpeeds()
{
	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->MaxWalkSpeed = (bWalking ? WalkSpeed : JogSpeed) * M;
	Move->MaxWalkSpeedCrouched = CrouchSpeed * M;
}

void ACrewCharacter::BuildInput()
{
	if (FootContext) return;
	auto MakeAction = [this](const TCHAR* Name, EInputActionValueType Type)
	{
		UInputAction* A = NewObject<UInputAction>(this, Name);
		A->ValueType = Type;
		return A;
	};
	MoveAction   = MakeAction(TEXT("IA_Move"), EInputActionValueType::Axis2D);
	LookAction   = MakeAction(TEXT("IA_Look"), EInputActionValueType::Axis2D);
	UpDownAction = MakeAction(TEXT("IA_UpDown"), EInputActionValueType::Axis1D);
	JumpAction   = MakeAction(TEXT("IA_Jump"), EInputActionValueType::Boolean);
	CrouchAction = MakeAction(TEXT("IA_Crouch"), EInputActionValueType::Boolean);
	WalkAction   = MakeAction(TEXT("IA_Walk"), EInputActionValueType::Boolean);

	FootContext = NewObject<UInputMappingContext>(this, TEXT("IMC_OnFoot"));
	auto Map = [this](UInputAction* A, FKey Key, bool bNegate, bool bSwizzle)
	{
		FEnhancedActionKeyMapping& Mp = FootContext->MapKey(A, Key);
		if (bSwizzle) Mp.Modifiers.Add(NewObject<UInputModifierSwizzleAxis>(FootContext)); // X -> Y
		if (bNegate) Mp.Modifiers.Add(NewObject<UInputModifierNegate>(FootContext));
	};
	// Move: X = right, Y = forward
	Map(MoveAction, EKeys::W, false, true);
	Map(MoveAction, EKeys::S, true, true);
	Map(MoveAction, EKeys::D, false, false);
	Map(MoveAction, EKeys::A, true, false);
	Map(LookAction, EKeys::Mouse2D, false, false);
	Map(UpDownAction, EKeys::SpaceBar, false, false);
	Map(UpDownAction, EKeys::LeftControl, true, false);
	Map(JumpAction, EKeys::SpaceBar, false, false);
	Map(CrouchAction, EKeys::C, false, false);
	Map(WalkAction, EKeys::LeftAlt, false, false);
}

void ACrewCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	BuildInput();
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Sub = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			Sub->AddMappingContext(FootContext, 0);
		}
	}
	UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EIC) return;
	EIC->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ACrewCharacter::OnMove);
	EIC->BindAction(LookAction, ETriggerEvent::Triggered, this, &ACrewCharacter::OnLook);
	EIC->BindAction(UpDownAction, ETriggerEvent::Triggered, this, &ACrewCharacter::OnUpDown);
	EIC->BindAction(JumpAction, ETriggerEvent::Started, this, &ACrewCharacter::OnJump);
	EIC->BindAction(CrouchAction, ETriggerEvent::Started, this, &ACrewCharacter::OnCrouchToggle);
	EIC->BindAction(WalkAction, ETriggerEvent::Started, this, &ACrewCharacter::OnWalkToggle);
}

void ACrewCharacter::OnMove(const FInputActionValue& V)
{
	const FVector2D In = V.Get<FVector2D>();
	if (bZeroG)
	{
		// Weak push-off along the full view direction.
		const FRotator View = GetControlRotation();
		AddMovementInput(FRotationMatrix(View).GetUnitAxis(EAxis::X), In.Y);
		AddMovementInput(FRotationMatrix(View).GetUnitAxis(EAxis::Y), In.X);
		return;
	}
	const FRotator Yaw(0.f, GetControlRotation().Yaw, 0.f);
	AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::X), In.Y);
	AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y), In.X);
}

void ACrewCharacter::OnLook(const FInputActionValue& V)
{
	const FVector2D In = V.Get<FVector2D>();
	AddControllerYawInput(In.X);
	AddControllerPitchInput(-In.Y);
}

void ACrewCharacter::OnUpDown(const FInputActionValue& V)
{
	if (bZeroG) AddMovementInput(FVector::UpVector, V.Get<float>());
}

void ACrewCharacter::OnJump(const FInputActionValue&)
{
	if (!bZeroG) Jump();
}

void ACrewCharacter::OnCrouchToggle(const FInputActionValue&)
{
	if (bZeroG) return;
	if (bIsCrouched) UnCrouch(); else Crouch();
}

void ACrewCharacter::OnWalkToggle(const FInputActionValue&)
{
	bWalking = !bWalking;
	UpdateSpeeds();
}

void ACrewCharacter::EnterZeroG()
{
	if (bZeroG) return;
	bZeroG = true;
	bTrackingFall = false;
	if (bIsCrouched) UnCrouch();
	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->GravityScale = 0.f;
	Move->MaxFlySpeed = ZeroGMaxSpeed * M;
	Move->MaxAcceleration = ZeroGPushAccel * M;
	Move->BrakingDecelerationFlying = 0.f; // keep drifting
	Move->SetMovementMode(MOVE_Flying);
}

void ACrewCharacter::ExitZeroG(float StartGravityScale)
{
	if (!bZeroG) return;
	bZeroG = false;
	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->MaxAcceleration = 2048.f;
	Move->GravityScale = StartGravityScale;
	Move->SetMovementMode(MOVE_Falling); // becomes MOVE_Walking on landing
	StartFall();
}

void ACrewCharacter::StartFall()
{
	bTrackingFall = true;
	FallApexZ = GetActorLocation().Z; // capsule centre: same delta as feet height
	bFallStartedLowG = GetCharacterMovement()->GravityScale < 0.999f;
}

void ACrewCharacter::SetShipGravityScale(float Scale)
{
	if (!bZeroG) GetCharacterMovement()->GravityScale = Scale;
}

void ACrewCharacter::OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PreviousCustomMode)
{
	Super::OnMovementModeChanged(PrevMovementMode, PreviousCustomMode);
	if (GetCharacterMovement()->MovementMode == MOVE_Falling && !bTrackingFall)
	{
		StartFall();
	}
}

void ACrewCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bTrackingFall) FallApexZ = FMath::Max(FallApexZ, GetActorLocation().Z);
}

void ACrewCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);
	if (!bTrackingFall) return;
	bTrackingFall = false;
	const float DropM = (FallApexZ - GetActorLocation().Z) / M;
	const float G = FMath::Clamp(GetCharacterMovement()->GravityScale, 0.f, 1.f);
	if (DropM > FallSafeHeight && G > 0.f)
	{
		float Dmg = FallDamagePerMetre * (DropM - FallSafeHeight) * G;
		if (bFallStartedLowG) Dmg = FMath::Min(Dmg, MaxHealth * RestoreFallDamageCapPct / 100.f);
		TakeDamage(Dmg, FDamageEvent(), GetController(), this);
		UE_LOG(LogTemp, Log, TEXT("Fall %.2f m at %.2f g%s -> %.1f damage (HP %.1f)"), DropM, G, bFallStartedLowG ? TEXT(" (restore cap)") : TEXT(""), Dmg, Health);
	}
}

float ACrewCharacter::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	const float D = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	Health = FMath::Max(0.f, Health - D);
	return D;
}
