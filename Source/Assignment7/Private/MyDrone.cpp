#include "MyDrone.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/ArrowComponent.h"
#include "EnhancedInputComponent.h"
#include "MyPlayerController.h"

AMyDrone::AMyDrone()
{
	PrimaryActorTick.bCanEverTick = true;

	CollisionComp = CreateDefaultSubobject<UBoxComponent>(TEXT("BoxCollision"));
	SetRootComponent(CollisionComp);
	CollisionComp->SetBoxExtent(FVector(50.f, 50.f, 20.f));
	CollisionComp->SetSimulatePhysics(false);

	DroneMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("DroneMesh"));
	DroneMesh->SetupAttachment(CollisionComp);
	DroneMesh->SetSimulatePhysics(false);

	ArrowComp = CreateDefaultSubobject<UArrowComponent>(TEXT("Arrow"));
	ArrowComp->SetupAttachment(CollisionComp);

	SpringArmComp = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArmComp->SetupAttachment(CollisionComp);
	SpringArmComp->TargetArmLength = 300.0f;
	SpringArmComp->bUsePawnControlRotation = false;
	SpringArmComp->bInheritPitch = true;
	SpringArmComp->bInheritYaw = true;
	SpringArmComp->bInheritRoll = true;

	SpringArmComp->bEnableCameraLag = false;
	SpringArmComp->bEnableCameraRotationLag = true;
	SpringArmComp->CameraRotationLagSpeed = 5.0f;

	CameraComp = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	CameraComp->SetupAttachment(SpringArmComp, USpringArmComponent::SocketName);
	CameraComp->bUsePawnControlRotation = false;
}

void AMyDrone::BeginPlay()
{
	Super::BeginPlay();
}

void AMyDrone::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	CheckGround();

	if (!bIsGrounded)
	{
		CurrentVerticalVelocity += GravityAcceleration * DeltaTime;
		FVector GravityOffset = FVector(0.0f, 0.0f, CurrentVerticalVelocity * DeltaTime);
		FHitResult HitResult;
		AddActorWorldOffset(GravityOffset, true, &HitResult);

		if (HitResult.IsValidBlockingHit())
		{
			CurrentVerticalVelocity = 0.0f;
		}
	}
	else
	{
		CurrentVerticalVelocity = 0.0f;
		FRotator CurrentRotation = GetActorRotation();
		FRotator TargetRotation = FRotator(0.0f, CurrentRotation.Yaw, 0.0f);
		FRotator NewRotation = FMath::RInterpTo(CurrentRotation, TargetRotation, DeltaTime, 10.0f);
		SetActorRotation(NewRotation);
	}
}

void AMyDrone::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		if (AMyPlayerController* PlayerController = Cast<AMyPlayerController>(GetController()))
		{
			if (PlayerController->MoveAction)
			{
				EnhancedInput->BindAction(
					PlayerController->MoveAction,
					ETriggerEvent::Triggered,
					this,
					&AMyDrone::Move
				);
			}

			if (PlayerController->LookAction)
			{
				EnhancedInput->BindAction(
					PlayerController->LookAction,
					ETriggerEvent::Triggered,
					this,
					&AMyDrone::Look
				);
			}

			if (PlayerController->RollAction)
			{
				EnhancedInput->BindAction(
					PlayerController->RollAction,
					ETriggerEvent::Triggered,
					this,
					&AMyDrone::Roll
				);
			}

			if (PlayerController->FlyAction)
			{
				EnhancedInput->BindAction(
					PlayerController->FlyAction,
					ETriggerEvent::Triggered,
					this,
					&AMyDrone::Fly
				);
			}
		}
	}
}

void AMyDrone::Move(const FInputActionValue& Value)
{
	if (!Controller) return;
	
	FVector2D InputVector = Value.Get<FVector2D>();
	InputVector.Normalize();
	
	float CurrentSpeed = MoveSpeed;
	if (!bIsGrounded)
	{
		CurrentSpeed = MoveSpeed * AirControlRatio;
	}
	
	float MoveDistance = CurrentSpeed * GetWorld()->GetDeltaSeconds();
	FVector MoveOffset;

	if (bIsGrounded)
	{
		FRotator YawRotation(0, GetActorRotation().Yaw, 0);
		FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
		FVector WorldDirection = (ForwardDirection * InputVector.X + RightDirection * InputVector.Y);

		MoveOffset = WorldDirection * MoveDistance;
		AddActorWorldOffset(MoveOffset, true);
	}
	else
	{
		FVector MoveDirection = FVector(InputVector.X, InputVector.Y, 0.0f);
		MoveOffset = MoveDirection * MoveDistance;
		AddActorLocalOffset(MoveOffset, true);
	}
}

void AMyDrone::Fly(const FInputActionValue& Value)
{
	if (!Controller) return;

	float FlyValue = Value.Get<float>();

	if (FlyValue > 0.0f)
	{
		if (CurrentVerticalVelocity < 0.0f)
		{
			CurrentVerticalVelocity = 0.0f;
		}
	}

	FVector FlyDirection = FVector(0.0f, 0.0f, FlyValue);
	float FlyDistance = FlySpeed * GetWorld()->GetDeltaSeconds();
	FVector FlyOffset = FlyDirection * FlyDistance;
	AddActorLocalOffset(FlyOffset, true);
}

void AMyDrone::Roll(const FInputActionValue& Value)
{
	if (!Controller || bIsGrounded) return;

	float InputFloat = Value.Get<float>();
	float RollAmount = InputFloat * RotationSpeed * GetWorld()->GetDeltaSeconds();
	FRotator RollOffset = FRotator(0.0f, 0.0f, RollAmount);

	FHitResult HitResult;
	AddActorLocalRotation(RollOffset, true, &HitResult);
}

void AMyDrone::CheckGround()
{
	FVector Start = GetActorLocation();

	float TraceDistance = CollisionComp->GetScaledBoxExtent().Z + 10.0f;
	FVector End = Start + (FVector::DownVector * TraceDistance);

	FHitResult HitResult;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);

	bool bHit = GetWorld()->LineTraceSingleByChannel(
		HitResult,
		Start,
		End,
		ECC_Visibility,
		QueryParams
	);
	bIsGrounded = bHit;
}

void AMyDrone::Look(const FInputActionValue& Value)
{
	if (!Controller) return;

	FVector2D InputVector = Value.Get<FVector2D>();

	float YawAmount = InputVector.X * RotationSpeed * GetWorld()->GetDeltaSeconds();

	float PitchAmount = 0.0f;
	if (!bIsGrounded)
	{
		PitchAmount = InputVector.Y * RotationSpeed * GetWorld()->GetDeltaSeconds();
	}

	FRotator LookOffset = FRotator(PitchAmount, YawAmount, 0.0f);
	FHitResult HitResult;
	AddActorLocalRotation(LookOffset, true, &HitResult);
}
