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
#include "SliceTuning.h"
#include "LadderVolume.h"

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
	Move->JumpZVelocity = FMath::Sqrt(2.f * 9.81f * BaseJumpHeightM) * M;
	MoveTuningTable = TSoftObjectPtr<UDataTable>(FSoftObjectPath(TEXT("/Game/Data/DT_Slice_MoveTuning.DT_Slice_MoveTuning")));
	Move->AirControl = 0.2f;
	// Moving-base support: inherit base velocity when leaving a moving floor.
	Move->bImpartBaseVelocityX = Move->bImpartBaseVelocityY = Move->bImpartBaseVelocityZ = true;
	Move->bImpartBaseAngularVelocity = true;
	UpdateSpeeds();
}

void ACrewCharacter::BeginPlay()
{
	Super::BeginPlay();
	if (UDataTable* DT = MoveTuningTable.LoadSynchronous())
	{
		if (const FSliceMoveTuningRow* R = DT->FindRow<FSliceMoveTuningRow>(TEXT("Default"), TEXT("CrewMoveTuning")))
		{
			BaseJumpHeightM = R->BaseJumpHeightM;
			JumpGravityExponent = R->JumpGravityExponent;
			IndoorJumpHeightCapM = R->IndoorJumpHeightCapM;
			MagBootBelowG = R->MagBootBelowG;
			WalkSpeed = R->WalkSpeed; JogSpeed = R->JogSpeed; CrouchSpeed = R->CrouchSpeed; SprintSpeed = R->SprintSpeed;
			UpdateSpeeds();
		}
	}
}

void ACrewCharacter::UpdateSpeeds()
{
	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->MaxWalkSpeed = (bSprinting ? SprintSpeed : (bWalking ? WalkSpeed : JogSpeed)) * M;
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
	SprintAction = MakeAction(TEXT("IA_Sprint"), EInputActionValueType::Boolean);

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
	Map(SprintAction, EKeys::LeftShift, false, false);
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
	EIC->BindAction(SprintAction, ETriggerEvent::Started, this, &ACrewCharacter::OnSprintStart);
	EIC->BindAction(SprintAction, ETriggerEvent::Completed, this, &ACrewCharacter::OnSprintStop);
}

void ACrewCharacter::OnMove(const FInputActionValue& V)
{
	const FVector2D In = V.Get<FVector2D>();
	if (Ladder || LadderCooldown) { LadderInput = In.Y; if (Ladder) return; }
	if (bZeroG)
	{
		// Weak push-off along the full view direction.
		const FRotator View = GetControlRotation();
		AddMovementInput(FRotationMatrix(View).GetUnitAxis(EAxis::X), In.Y);
		AddMovementInput(FRotationMatrix(View).GetUnitAxis(EAxis::Y), In.X);
		return;
	}
	// Sprint only while pushing forward (not strafing/backing), standing and not crouched.
	const bool bWantSprint = bSprintHeld && In.Y > 0.5f && FMath::Abs(In.X) < 0.5f && !bIsCrouched;
	if (bWantSprint != bSprinting) { bSprinting = bWantSprint; UpdateSpeeds(); }
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
	if (Ladder) { LadderInput = V.Get<float>(); return; }
	if (bZeroG) AddMovementInput(FVector::UpVector, V.Get<float>());
}

void ACrewCharacter::OnJump(const FInputActionValue&)
{
	if (!bZeroG && !Ladder) Jump(); // cannot jump off mid-shaft
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
	if (bZeroG || Ladder) return;
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
	bFallStartedLowG = bGravityRestoring;
}

void ACrewCharacter::SetShipGravityScale(float Scale)
{
	ShipGravity = Scale;
	if (Ladder) return; // ladder drives movement itself
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
	if (Ladder) TickLadder(DeltaSeconds);
	else if (LadderCooldown && LadderInput > 0.5f && GetCharacterMovement()->IsMovingOnGround()) EnterLadder(LadderCooldown); // climb back up from the bottom
	if (!Ladder) LadderInput = 0.f;
	if (bTrackingFall) FallApexZ = FMath::Max(FallApexZ, GetActorLocation().Z);
	if (GetCharacterMovement()->IsFalling()) { LastAirVz = GetCharacterMovement()->Velocity.Z; LastAirZ = GetActorLocation().Z; } // last airborne frame (QA-142)
	// QA-140: never stay in Flying after the ladder unless we are really in zero-g.
	if (!Ladder && !bZeroG && ShipGravity > 0.001f && GetCharacterMovement()->MovementMode == MOVE_Flying)
	{
		GetCharacterMovement()->GravityScale = ShipGravity;
		GetCharacterMovement()->SetMovementMode(MOVE_Falling);
	}
	if (bSprinting && GetLastMovementInputVector().IsNearlyZero()) { bSprinting = false; UpdateSpeeds(); }
	// Jump: v0 from BaseJumpHeightM at 1 g, scaled by (g / 9.81)^exp with current gravity; apex capped indoors.
	UCharacterMovementComponent* Move = GetCharacterMovement();
	const float GMs2 = FMath::Max(FMath::Abs(Move->GetGravityZ()) / M, 0.01f);
	float V = FMath::Sqrt(2.f * 9.81f * BaseJumpHeightM) * FMath::Pow(GMs2 / 9.81f, JumpGravityExponent);
	if (bIndoors) V = FMath::Min(V, FMath::Sqrt(2.f * GMs2 * IndoorJumpHeightCapM));
	Move->JumpZVelocity = V * M;
	bMagBootMode = bZeroG || GMs2 < MagBootBelowG * 9.81f;
}

void ACrewCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);
	if (!bTrackingFall) return;
	bTrackingFall = false;
	const float DropM = (FallApexZ - GetActorLocation().Z) / M;
	// QA-142: impact speed = vertical speed on the last frame in the air (landing clears/clips Velocity).
	// Carry the last airborne frame's speed through the remaining distance to contact: v^2 = v_last^2 + 2 g d.
	const float Grav = FMath::Abs(GetCharacterMovement()->GetGravityZ());
	const float Rem = FMath::Max(0.f, LastAirZ - GetActorLocation().Z);
	const float V = (LastAirVz < 0.f ? FMath::Sqrt(LastAirVz * LastAirVz + 2.f * Grav * Rem) : 0.f) / M; // m/s
	LastImpactSpeed = V; LastDropM = DropM;
	const float EquivDropM = V * V / (2.f * 9.81f);
	LastEquivDropM = EquivDropM; LastFallDamage = 0.f;
	const float G = GetCharacterMovement()->GravityScale;
	if (EquivDropM > FallSafeHeight)
	{
		float Dmg = FallDamagePerMetre * (EquivDropM - FallSafeHeight);
		if (bFallStartedLowG) Dmg = FMath::Min(Dmg, MaxHealth * RestoreFallDamageCapPct / 100.f);
		LastFallDamage = Dmg;
		TakeDamage(Dmg, FDamageEvent(), GetController(), this);
		UE_LOG(LogTemp, Log, TEXT("Fall %.2f m, impact %.2f m/s (equiv %.2f m) at %.2f g%s -> %.1f damage (HP %.1f)"), DropM, V, EquivDropM, G, bFallStartedLowG ? TEXT(" (restore cap)") : TEXT(""), Dmg, Health);
	}
}

float ACrewCharacter::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	const float D = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	Health = FMath::Max(0.f, Health - D);
	return D;
}

// ---------------- Ladder ----------------
void ACrewCharacter::LadderOverlapBegin(ALadderVolume* L)
{
	if (!L || Ladder || LadderCooldown == L) return;
	EnterLadder(L);
}

void ACrewCharacter::LadderOverlapEnd(ALadderVolume* L)
{
	if (LadderCooldown == L) LadderCooldown = nullptr;
	if (Ladder == L) ExitLadder(false);
}

void ACrewCharacter::EnterLadder(ALadderVolume* L)
{
	Ladder = L; LadderCooldown = nullptr;
	bZeroG = false;              // the shaft handles zero-g itself
	bTrackingFall = false;       // no free fall in a caged ladder -> no fall damage
	if (bIsCrouched) UnCrouch();
	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->GravityScale = 0.f;
	Move->BrakingDecelerationFlying = 0.f;
	Move->SetMovementMode(MOVE_Flying);
	Move->Velocity = FVector::ZeroVector;
	LadderZeroGVz = 0.f;
	UE_LOG(LogTemp, Log, TEXT("Ladder: on %s"), *L->GetName());
}

void ACrewCharacter::ExitLadder(bool bTop)
{
	ALadderVolume* L = Ladder;
	Ladder = nullptr; LadderCooldown = L;
	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->Velocity = FVector::ZeroVector;
	Move->GravityScale = ShipGravity;
	if (bTop && L)
	{
		const float Half = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		SetActorLocation(L->GetTopExitWorld() + FVector(0, 0, Half + 2.f), false, nullptr, ETeleportType::TeleportPhysics);
	}
	bTrackingFall = false;
	bHatchStopped = bHatchBlocked = false;
	// QA-140: always leave the ladder in a ground/air mode (Walking if on a floor, else Falling); Flying only in zero-g.
	if (ShipGravity > 0.f) { Move->SetMovementMode(MOVE_Walking); if (!Move->IsMovingOnGround()) Move->SetMovementMode(MOVE_Falling); }
	else Move->SetMovementMode(MOVE_Flying);
	if (ShipGravity <= 0.f) { bZeroG = false; EnterZeroG(); }
	UE_LOG(LogTemp, Log, TEXT("Ladder: off at %s"), bTop ? TEXT("top") : TEXT("bottom"));
}

void ACrewCharacter::TickLadder(float Dt)
{
	UCharacterMovementComponent* Move = GetCharacterMovement();
	if (Move->MovementMode != MOVE_Flying) Move->SetMovementMode(MOVE_Flying);
	Move->GravityScale = 0.f;
	const float Half = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const float Feet = GetActorLocation().Z - Half;
	float Vz;
	if (ShipGravity <= 0.001f)
	{
		// Zero-g: float, push slowly along the shaft; keep drift.
		LadderZeroGVz = FMath::Clamp(LadderZeroGVz + LadderInput * 0.6f * M * Dt, -Ladder->ZeroGPushSpeed * M, Ladder->ZeroGPushSpeed * M);
		Vz = LadderZeroGVz;
	}
	else
	{
		LadderZeroGVz = 0.f;
		Vz = LadderInput > 0.1f ? Ladder->ClimbSpeed * M : -Ladder->SlideSpeed * M; // release = slide down 2 m/s
	}
	// QA-140: bottom pressure hatch. Open it on approach; if pressure-locked (or still swinging) stop at it.
	bHatchBlocked = false;
	if (Ladder->bHasHatch)
	{
		const float HTop = Ladder->HatchTopZ, HBot = HTop - Ladder->HatchThickness;
		const float Head = Feet + 2.f * Half;
		const bool bAbove = Feet >= HTop - 5.f, bBelow = Head <= HBot + 5.f;
		const bool bNear = (bAbove && Feet - HTop < 250.f) || (bBelow && HBot - Head < 250.f);
		if (bNear && Ladder->RequestHatchOpen() == false) bHatchBlocked = true;
		if (bNear && GFrameCounter % 30 == 0) UE_LOG(LogTemp, Log, TEXT("Ladder hatch: feet %.1f head %.1f top %.1f alpha %.2f locked %d vz %.0f"), Feet, Head, HTop, Ladder->HatchAlpha, Ladder->bHatchPressureLocked, Vz);
		if (!Ladder->IsHatchFullyOpen())
		{
			if (bAbove && Vz < 0.f && Feet + Vz * Dt <= HTop + 2.f) { Vz = FMath::Max(0.f, (HTop + 2.f - Feet) / Dt); bHatchStopped = true; }
			else if (bBelow && Vz > 0.f && Head + Vz * Dt >= HBot - 2.f) { Vz = FMath::Min(0.f, (HBot - 2.f - Head) / Dt); bHatchStopped = true; }
			else bHatchStopped = false;
		}
		else bHatchStopped = false;
		if (ShipGravity <= 0.001f && bHatchStopped) LadderZeroGVz = 0.f;
	}
	// Hold the capsule on the shaft axis.
	FVector ToAxis = Ladder->GetShaftCentre() - GetActorLocation(); ToAxis.Z = 0.f;
	Move->Velocity = FVector(ToAxis.X, ToAxis.Y, 0.f) * 5.f + FVector(0, 0, Vz);
	LadderInput = 0.f; // input re-triggers every frame while held; release => slide
	if (Vz < 0.f && Feet <= Ladder->BottomFloorZ + 3.f) { ExitLadder(false); return; }
	if (Vz > 0.f && Feet >= Ladder->TopFloorZ + 2.f) { ExitLadder(true); return; }
}
