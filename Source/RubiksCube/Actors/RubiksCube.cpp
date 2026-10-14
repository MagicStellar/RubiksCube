#include "Actors/RubiksCube.h"
#include "Components/SceneComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "Curves/CurveFloat.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/PlayerController.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Math/RotationMatrix.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SplineMeshComponent.h"

ARubiksCube::ARubiksCube()
{
	PrimaryActorTick.bCanEverTick = true;

	CubeRoot = CreateDefaultSubobject<USceneComponent>(TEXT("CubeRoot"));
	SetRootComponent(CubeRoot);
	CubePresentation = CreateDefaultSubobject<USceneComponent>(TEXT("CubePresentation"));
	CubePresentation->SetupAttachment(CubeRoot);

	RotationPivot = CreateDefaultSubobject<USceneComponent>(TEXT("RotationPivot"));
	RotationPivot->SetupAttachment(CubePresentation);

	MovePreviewShaft = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MovePreviewShaft"));
	MovePreviewTip = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MovePreviewTip"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ShaftMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> TipMesh(TEXT("/Engine/BasicShapes/Cone.Cone"));
	MovePreviewShaft->SetStaticMesh(ShaftMesh.Object);
	MovePreviewTip->SetStaticMesh(TipMesh.Object);
	for (UStaticMeshComponent* Part : {MovePreviewShaft.Get(), MovePreviewTip.Get()})
	{
		Part->SetupAttachment(CubeRoot);
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Part->SetCastShadow(false);
		Part->SetCanEverAffectNavigation(false);
		Part->SetVisibility(false);
	}

	for (int32 Segment = 0; Segment < 3; ++Segment)
	{
		USplineMeshComponent* Arc = CreateDefaultSubobject<USplineMeshComponent>(*FString::Printf(TEXT("MovePreviewArc%d"), Segment));
		Arc->SetMobility(EComponentMobility::Movable);
		Arc->SetupAttachment(CubePresentation);
		Arc->SetStaticMesh(ShaftMesh.Object);
		Arc->SetForwardAxis(ESplineMeshAxis::Z, false);
		Arc->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Arc->SetCastShadow(false);
		Arc->SetCanEverAffectNavigation(false);
		Arc->SetVisibility(false);
		MovePreviewArc.Add(Arc);
	}

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(CubeRoot);
	CameraBoom->TargetArmLength = ViewingDistance;
	CameraBoom->SetRelativeRotation(FRotator(-20.f, 0.f, 0.f));
	CameraBoom->bDoCollisionTest = false;
	CameraBoom->bEnableCameraLag = false;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);

	// Load Engine basic cube as default mesh
	static ConstructorHelpers::FObjectFinder<UStaticMesh> DefaultCubeFinder(
		TEXT("/Engine/BasicShapes/Cube.Cube")
	);
	if (DefaultCubeFinder.Succeeded())
	{
		CubeletMesh = DefaultCubeFinder.Object;
		StickerMesh = DefaultCubeFinder.Object;
	}

	// Load Engine basic shape material for default fallback
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> DefaultMatFinder(
		TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")
	);
	if (DefaultMatFinder.Succeeded())
	{
		MaterialCore = DefaultMatFinder.Object;
	}
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> HighlightFinder(
		TEXT("/Game/CubeStudio/Materials/M_SelectedLayerHighlight.M_SelectedLayerHighlight"));
	SelectionHighlightMaterial = HighlightFinder.Object;
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ArrowFinder(TEXT("/Game/CubeStudio/Materials/M_MovePreviewArrow.M_MovePreviewArrow"));
	MovePreviewMaterial = ArrowFinder.Object;
}

void ARubiksCube::BeginPlay()
{
	Super::BeginPlay();

	if (Cubelets.Num() != 27)
	{
		SetupDefaultMaterials();
		SpawnCubelets();
	}
	// Existing Blueprint components may retain their previously serialized attachment.
	RotationPivot->AttachToComponent(CubePresentation, FAttachmentTransformRules::KeepRelativeTransform);
	for (URubiksCubeletComponent* Cubelet : Cubelets)
	{
		if (Cubelet) Cubelet->AttachToComponent(CubePresentation, FAttachmentTransformRules::KeepRelativeTransform);
	}
	if (UMaterialInterface* ArrowMaterial = MovePreviewMaterial)
	{
		MovePreviewTip->SetMaterial(0, ArrowMaterial);
		for (USplineMeshComponent* Arc : MovePreviewArc) Arc->SetMaterial(0, ArrowMaterial);
	}
	SetPreviewArrowVisible(false);
	for (USplineMeshComponent* Arc : MovePreviewArc)
	{
		// Recover attachments on Blueprint instances saved before the mobility update.
		Arc->SetMobility(EComponentMobility::Movable);
		Arc->AttachToComponent(CubePresentation, FAttachmentTransformRules::KeepRelativeTransform);
		Arc->SetRelativeTransform(FTransform::Identity);
	}
	BindPlayerInput();
	SetupSelectionHighlight();
	UpdateSelectionHighlight();
	// Keep the camera stationary while the presentation component turns the puzzle.
	ResetCamera();
	FaceSelectedAxis(true);
	ActivateCamera();
	UpdateSelectionHighlight();
}

void ARubiksCube::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	FinishStickerGesture(true);
	EndInspectDrag();
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		if (PC->GetViewTarget() == this) PC->bShowMouseCursor = false;
	}
	Super::EndPlay(EndPlayReason);
}

void ARubiksCube::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	if (Cubelets.Num() != 27)
	{
		SetupDefaultMaterials();
		SpawnCubelets();
	}
	SetupSelectionHighlight();
	UpdateSelectionHighlight();
}

void ARubiksCube::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (const APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		if (bSelectionWasActive != (PC->GetViewTarget() == this)) UpdateSelectionHighlight();
	}
	UpdateInspectCamera();
	UpdateStickerPointer();
	UpdateCameraReturn(DeltaTime);
	UpdateFaceTransition(DeltaTime);
	if (SelectionHighlightDynamic && HighlightPulseRemaining > 0.f)
	{
		HighlightPulseRemaining = FMath::Max(0.f, HighlightPulseRemaining - DeltaTime);
		SelectionHighlightDynamic->SetScalarParameterValue(TEXT("GlowStrength"), 1.5f + 2.f * HighlightPulseRemaining / .3f);
	}

	if (bIsRotating)
	{
		RotationTimeline.TickTimeline(DeltaTime);
		// Finish outside the timeline callback, so queued moves can safely restart it.
		if (bIsRotating && (!RotationTimeline.IsPlaying() || RotationTimeline.GetPlaybackPosition() >= 1.f))
		{
			FinishRotation();
		}
	}
}

void ARubiksCube::SetupDefaultMaterials()
{
	UMaterialInterface* BaseMat = MaterialCore;
	if (!BaseMat)
	{
		BaseMat = UMaterial::GetDefaultMaterial(MD_Surface);
	}

	auto CreateColoredMat = [this, BaseMat](const FName& Name, const FLinearColor& Color) -> UMaterialInterface*
	{
		UMaterialInstanceDynamic* DynMat = UMaterialInstanceDynamic::Create(BaseMat, this, Name);
		if (DynMat)
		{
			DynMat->SetVectorParameterValue(TEXT("Color"), Color);
		}
		return DynMat;
	};

	// Create dynamic instances if not explicitly assigned
	if (!MaterialCore || MaterialCore == BaseMat)
	{
		MaterialCore = CreateColoredMat(TEXT("DynMat_Core"), FLinearColor(0.04f, 0.04f, 0.04f));
	}
	if (!MaterialUp)
	{
		MaterialUp = CreateColoredMat(TEXT("DynMat_Up"), FLinearColor(0.95f, 0.95f, 0.95f)); // White
	}
	if (!MaterialDown)
	{
		MaterialDown = CreateColoredMat(TEXT("DynMat_Down"), FLinearColor(1.0f, 0.85f, 0.0f)); // Yellow
	}
	if (!MaterialLeft)
	{
		MaterialLeft = CreateColoredMat(TEXT("DynMat_Left"), FLinearColor(1.0f, 0.35f, 0.0f)); // Orange
	}
	if (!MaterialRight)
	{
		MaterialRight = CreateColoredMat(TEXT("DynMat_Right"), FLinearColor(0.85f, 0.05f, 0.05f)); // Red
	}
	if (!MaterialFront)
	{
		MaterialFront = CreateColoredMat(TEXT("DynMat_Front"), FLinearColor(0.0f, 0.65f, 0.15f)); // Green
	}
	if (!MaterialBack)
	{
		MaterialBack = CreateColoredMat(TEXT("DynMat_Back"), FLinearColor(0.0f, 0.3f, 0.85f)); // Blue
	}

	FaceMaterialsMap.Empty();
	FaceMaterialsMap.Add(ERubiksFace::Up, MaterialUp);
	FaceMaterialsMap.Add(ERubiksFace::Down, MaterialDown);
	FaceMaterialsMap.Add(ERubiksFace::Left, MaterialLeft);
	FaceMaterialsMap.Add(ERubiksFace::Right, MaterialRight);
	FaceMaterialsMap.Add(ERubiksFace::Front, MaterialFront);
	FaceMaterialsMap.Add(ERubiksFace::Back, MaterialBack);
}

void ARubiksCube::SpawnCubelets()
{
	// Clean up existing cubelets if any
	for (URubiksCubeletComponent* Existing : Cubelets)
	{
		if (Existing)
		{
			Existing->DestroyComponent();
		}
	}
	Cubelets.Empty();
	Cubelets.Reserve(27);

	// Spawn 3x3x3 cubelets
	for (int32 X = -1; X <= 1; ++X)
	{
		for (int32 Y = -1; Y <= 1; ++Y)
		{
			for (int32 Z = -1; Z <= 1; ++Z)
			{
				const FString CompName = FString::Printf(TEXT("Cubelet_%d_%d_%d"), X, Y, Z);
				URubiksCubeletComponent* Cubelet = NewObject<URubiksCubeletComponent>(this, *CompName);
				if (Cubelet)
				{
					Cubelet->SetStaticMesh(CubeletMesh);
					if (MaterialCore)
					{
						Cubelet->SetMaterial(0, MaterialCore);
					}
					Cubelet->AttachToComponent(CubePresentation, FAttachmentTransformRules::KeepRelativeTransform);
					Cubelet->SetInitialGridPosition(FIntVector(X, Y, Z));
					Cubelet->SetRelativeScale3D(FVector(0.98f, 0.98f, 0.98f));
					Cubelet->SetupStickers(StickerMesh, FaceMaterialsMap, 100.f);
					Cubelet->SnapTransform(CubeSpacing);
					Cubelet->RegisterComponent();

					Cubelets.Add(Cubelet);
				}
			}
		}
	}
}

// =========================================================================
// STAGE 2: Cube Mathematics
// =========================================================================

FIntVector ARubiksCube::RotateGridPosition(const FIntVector& Position, EAxis::Type Axis, int32 Direction)
{
	switch (Axis)
	{
	case EAxis::X: return RotateGridPositionX(Position, Direction);
	case EAxis::Y: return RotateGridPositionY(Position, Direction);
	case EAxis::Z: return RotateGridPositionZ(Position, Direction);
	default:       return Position;
	}
}

FIntVector ARubiksCube::RotateGridPositionX(const FIntVector& Position, int32 Direction)
{
	if (Direction > 0)
	{
		return FIntVector(Position.X, -Position.Z, Position.Y);
	}
	else
	{
		return FIntVector(Position.X, Position.Z, -Position.Y);
	}
}

FIntVector ARubiksCube::RotateGridPositionY(const FIntVector& Position, int32 Direction)
{
	if (Direction > 0)
	{
		return FIntVector(Position.Z, Position.Y, -Position.X);
	}
	else
	{
		return FIntVector(-Position.Z, Position.Y, Position.X);
	}
}

FIntVector ARubiksCube::RotateGridPositionZ(const FIntVector& Position, int32 Direction)
{
	if (Direction > 0)
	{
		return FIntVector(-Position.Y, Position.X, Position.Z);
	}
	else
	{
		return FIntVector(Position.Y, -Position.X, Position.Z);
	}
}

TArray<URubiksCubeletComponent*> ARubiksCube::GetLayerCubelets(EAxis::Type Axis, int32 Layer) const
{
	TArray<URubiksCubeletComponent*> Result;
	Result.Reserve(9);

	for (URubiksCubeletComponent* Cubelet : Cubelets)
	{
		if (!Cubelet)
		{
			continue;
		}

		const FIntVector Pos = Cubelet->GetGridPosition();
		int32 Coord = 0;
		switch (Axis)
		{
		case EAxis::X: Coord = Pos.X; break;
		case EAxis::Y: Coord = Pos.Y; break;
		case EAxis::Z: Coord = Pos.Z; break;
		default:       Coord = 0;     break;
		}

		if (Coord == Layer)
		{
			Result.Add(Cubelet);
		}
	}

	return Result;
}

// =========================================================================
// STAGE 3 & 4: Move Execution & Rotation Animation
// =========================================================================

void ARubiksCube::ExecuteStandardMove(ERubiksMove Move)
{
	ExecuteMove(FRubiksMoveDefinition::FromStandardMove(Move));
}

void ARubiksCube::ExecuteMove(const FRubiksMoveDefinition& MoveDef)
{
	if ((MoveDef.Axis != EAxis::X && MoveDef.Axis != EAxis::Y && MoveDef.Axis != EAxis::Z)
		|| MoveDef.Layer < -1 || MoveDef.Layer > 1 || FMath::Abs(MoveDef.Direction) != 1
		|| GetLayerCubelets(MoveDef.Axis, MoveDef.Layer).Num() != 9)
	{
		UE_LOG(LogTemp, Warning, TEXT("Rejected invalid cube move"));
		return;
	}
	if (bIsRotating)
	{
		MoveQueue.Add(MoveDef);
		return;
	}

	StartRotation(MoveDef);
}

void ARubiksCube::StartRotation(const FRubiksMoveDefinition& MoveDef)
{
	ActiveMove = MoveDef;
	bIsRotating = true;
	ActiveRotationDuration = bIsScrambling ? ScrambleRotationDuration : RotationDuration;

	ActiveLayerCubelets = GetLayerCubelets(MoveDef.Axis, MoveDef.Layer);

	FVector PivotLocation = FVector::ZeroVector;
	switch (MoveDef.Axis)
	{
	case EAxis::X:
		ActiveRotationAxis = FVector::ForwardVector;
		PivotLocation = FVector(MoveDef.Layer * CubeSpacing, 0.f, 0.f);
		break;
	case EAxis::Y:
		ActiveRotationAxis = FVector::RightVector;
		PivotLocation = FVector(0.f, MoveDef.Layer * CubeSpacing, 0.f);
		break;
	case EAxis::Z:
		ActiveRotationAxis = FVector::UpVector;
		PivotLocation = FVector(0.f, 0.f, MoveDef.Layer * CubeSpacing);
		break;
	default:
		ActiveRotationAxis = FVector::UpVector;
		break;
	}

	TargetAngleDegrees = 90.f * MoveDef.Direction;

	// Position and reset pivot
	RotationPivot->SetRelativeLocation(PivotLocation);
	RotationPivot->SetRelativeRotation(FRotator::ZeroRotator);

	// Attach 9 layer cubelets to pivot
	for (URubiksCubeletComponent* Cubelet : ActiveLayerCubelets)
	{
		if (Cubelet)
		{
			Cubelet->AttachToComponent(RotationPivot, FAttachmentTransformRules::KeepWorldTransform);
		}
	}

	// Rebuild per move to allow changing the curve between turns in Blueprint.
	RotationTimeline = FTimeline();
	ActiveRotationCurve = RotationCurve;
	bUseDefaultRotationEase = !ActiveRotationCurve;
	if (!ActiveRotationCurve)
	{
		ActiveRotationCurve = NewObject<UCurveFloat>(this);
		ActiveRotationCurve->FloatCurve.AddKey(0.f, 0.f);
		ActiveRotationCurve->FloatCurve.AddKey(1.f, 1.f);
	}
	FOnTimelineFloatStatic UpdateDelegate;
	UpdateDelegate.BindUObject(this, &ARubiksCube::UpdateRotationTimeline);
	RotationTimeline.AddInterpFloat(ActiveRotationCurve, UpdateDelegate);
	RotationTimeline.SetTimelineLengthMode(ETimelineLengthMode::TL_TimelineLength);
	RotationTimeline.SetTimelineLength(1.f);
	RotationTimeline.SetLooping(false);
	RotationTimeline.SetPlayRate(1.f / FMath::Max(ActiveRotationDuration, 0.1f));
	RotationTimeline.PlayFromStart();
	HighlightPulseRemaining = .3f;
	if (SelectionHighlightDynamic) SelectionHighlightDynamic->SetScalarParameterValue(TEXT("GlowStrength"), 3.5f);
	OnMoveStarted.Broadcast(ActiveMove);
}

void ARubiksCube::UpdateRotationTimeline(float Alpha)
{
	Alpha = FMath::Clamp(Alpha, 0.f, 1.f);
	if (bUseDefaultRotationEase)
	{
		Alpha = FMath::InterpEaseInOut(0.f, 1.f, Alpha, 2.f);
	}
	RotationPivot->SetRelativeRotation(FQuat(ActiveRotationAxis,
		FMath::DegreesToRadians(TargetAngleDegrees * Alpha)));
}

void ARubiksCube::FinishRotation()
{
	RotationTimeline.Stop();
	RotationPivot->SetRelativeRotation(FQuat(ActiveRotationAxis, FMath::DegreesToRadians(TargetAngleDegrees)));
	bIsRotating = false;

	// Detach cubelets from pivot, restore attachment to the presentation component.
	for (URubiksCubeletComponent* Cubelet : ActiveLayerCubelets)
	{
		if (Cubelet)
		{
			Cubelet->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
			Cubelet->AttachToComponent(CubePresentation, FAttachmentTransformRules::KeepWorldTransform);
		}
	}

	// Reset pivot transform
	RotationPivot->SetRelativeRotation(FRotator::ZeroRotator);
	RotationPivot->SetRelativeLocation(FVector::ZeroVector);

	// Mathematical rotation quaternion
	const FQuat StepQuat(ActiveRotationAxis, FMath::DegreesToRadians(TargetAngleDegrees));

	// Update logical coordinates, logical orientation, and snap visual transform
	for (URubiksCubeletComponent* Cubelet : ActiveLayerCubelets)
	{
		if (Cubelet)
		{
			const FIntVector OldPos = Cubelet->GetGridPosition();
			const FIntVector NewPos = RotateGridPosition(OldPos, ActiveMove.Axis, ActiveMove.Direction);
			Cubelet->SetGridPosition(NewPos);

			const FQuat OldRot = Cubelet->GetLogicalRotation();
			const FQuat NewRot = StepQuat * OldRot;
			Cubelet->SetLogicalRotation(NewRot);

			Cubelet->SnapTransform(CubeSpacing);
		}
	}

	ActiveLayerCubelets.Empty();

	// Record move in history and update counter
	if (bUndoInProgress)
	{
		MoveHistory.Pop();
		MoveCounter = FMath::Max(0, MoveCounter - 1);
		bUndoInProgress = false;
	}
	else
	{
		MoveHistory.Add(ActiveMove);
		MoveCounter++;
	}
	UpdateSelectionHighlight();
	OnStateChanged.Broadcast();

	OnMoveCompleted.Broadcast(ActiveMove);

	// Check solved state
	if (IsSolved())
	{
		OnCubeSolved.Broadcast();
	}

	// Process queue
	if (MoveQueue.Num() > 0)
	{
		const FRubiksMoveDefinition NextMove = MoveQueue[0];
		MoveQueue.RemoveAt(0);
		StartRotation(NextMove);
	}
	else
	{
		bIsScrambling = false;
		EndScrambleOverview();
	}
}

void ARubiksCube::ApplyMoveInstant(const FRubiksMoveDefinition& MoveDef)
{
	TArray<URubiksCubeletComponent*> LayerCubelets = GetLayerCubelets(MoveDef.Axis, MoveDef.Layer);

	FVector RotationAxisVec = FVector::UpVector;
	switch (MoveDef.Axis)
	{
	case EAxis::X: RotationAxisVec = FVector::ForwardVector; break;
	case EAxis::Y: RotationAxisVec = FVector::RightVector;   break;
	case EAxis::Z: RotationAxisVec = FVector::UpVector;      break;
	default: break;
	}

	const float AngleDeg = 90.f * MoveDef.Direction;
	const FQuat StepQuat(RotationAxisVec, FMath::DegreesToRadians(AngleDeg));

	for (URubiksCubeletComponent* Cubelet : LayerCubelets)
	{
		if (Cubelet)
		{
			const FIntVector OldPos = Cubelet->GetGridPosition();
			const FIntVector NewPos = RotateGridPosition(OldPos, MoveDef.Axis, MoveDef.Direction);
			Cubelet->SetGridPosition(NewPos);

			const FQuat OldRot = Cubelet->GetLogicalRotation();
			const FQuat NewRot = StepQuat * OldRot;
			Cubelet->SetLogicalRotation(NewRot);

			Cubelet->SnapTransform(CubeSpacing);
		}
	}
}

// =========================================================================
// STAGE 5: Gameplay Systems
// =========================================================================

bool ARubiksCube::Undo()
{
	if (MoveHistory.Num() == 0 || bIsRotating || MoveQueue.Num() > 0)
	{
		return false;
	}

	const FRubiksMoveDefinition LastMove = MoveHistory.Last();
	const FRubiksMoveDefinition InverseMove = LastMove.GetInverse();

	// Remove history only after the inverse animation completes.
	bUndoInProgress = true;

	ExecuteMove(InverseMove);
	return true;
}

void ARubiksCube::ResetCube()
{
	FinishStickerGesture(true);
	EndInspectDrag();
	RotationTimeline.Stop();
	bUndoInProgress = false;
	ActiveLayerCubelets.Empty();
	bIsRotating = false;
	bIsScrambling = false;
	EndScrambleOverview();
	MoveQueue.Empty();
	MoveHistory.Empty();
	MoveCounter = 0;

	if (Cubelets.Num() != 27)
	{
		SetupDefaultMaterials();
		SpawnCubelets();
	}

	if (RotationPivot)
	{
		RotationPivot->SetRelativeRotation(FRotator::ZeroRotator);
		RotationPivot->SetRelativeLocation(FVector::ZeroVector);
	}

	for (URubiksCubeletComponent* Cubelet : Cubelets)
	{
		if (Cubelet)
		{
			Cubelet->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
			Cubelet->AttachToComponent(CubePresentation, FAttachmentTransformRules::KeepWorldTransform);
			Cubelet->ResetToInitial(CubeSpacing);
		}
	}
	UpdateSelectionHighlight();
	OnStateChanged.Broadcast();
}

TArray<FRubiksMoveDefinition> ARubiksCube::GenerateScrambleSequence(int32 MoveCount) const
{
	TArray<FRubiksMoveDefinition> Moves;
	if (MoveCount <= 0)
	{
		return Moves;
	}

	static const ERubiksMove AllStandardMoves[] = {
		ERubiksMove::U, ERubiksMove::UPrime,
		ERubiksMove::D, ERubiksMove::DPrime,
		ERubiksMove::L, ERubiksMove::LPrime,
		ERubiksMove::R, ERubiksMove::RPrime,
		ERubiksMove::F, ERubiksMove::FPrime,
		ERubiksMove::B, ERubiksMove::BPrime
	};
	constexpr int32 NumMoves = UE_ARRAY_COUNT(AllStandardMoves);

	ERubiksMove LastMove = ERubiksMove::U;
	int32 SameFaceStreak = 0;
	EAxis::Type LastAxis = EAxis::None;
	int32 LastLayer = 0;

	for (int32 i = 0; i < MoveCount; ++i)
	{
		ERubiksMove PickedMove;
		FRubiksMoveDefinition MoveDef;
		int32 Attempts = 0;

		do
		{
			PickedMove = AllStandardMoves[FMath::RandRange(0, NumMoves - 1)];
			MoveDef = FRubiksMoveDefinition::FromStandardMove(PickedMove);
			Attempts++;

			// Avoid immediate inverse (e.g., R then R')
			if (i > 0 && PickedMove == FRubiksMoveDefinition::GetInverseMoveType(LastMove))
			{
				continue;
			}

			// Avoid 3 consecutive moves on the same face (e.g., R R R)
			if (i > 0 && MoveDef.Axis == LastAxis && MoveDef.Layer == LastLayer && SameFaceStreak >= 2)
			{
				continue;
			}

			break;
		} while (Attempts < 20);

		if (MoveDef.Axis == LastAxis && MoveDef.Layer == LastLayer)
		{
			SameFaceStreak++;
		}
		else
		{
			SameFaceStreak = 1;
		}

		LastMove = PickedMove;
		LastAxis = (EAxis::Type)MoveDef.Axis;
		LastLayer = MoveDef.Layer;

		Moves.Add(MoveDef);
	}

	return Moves;
}

void ARubiksCube::Scramble(int32 MoveCount, bool bInstant)
{
	if (MoveCount <= 0 || bIsScrambling)
	{
		return;
	}

	const TArray<FRubiksMoveDefinition> Moves = GenerateScrambleSequence(MoveCount);

	if (bInstant)
	{
		ResetCube();
		for (const FRubiksMoveDefinition& MoveDef : Moves)
		{
			ApplyMoveInstant(MoveDef);
		}
		MoveHistory = Moves;
		MoveCounter = Moves.Num();
	}
	else
	{
		bIsScrambling = true;
		BeginScrambleOverview();
		for (const FRubiksMoveDefinition& MoveDef : Moves)
		{
			ExecuteMove(MoveDef);
		}
	}
}

bool ARubiksCube::IsSolved() const
{
	if (bIsRotating)
	{
		return false;
	}

	if (Cubelets.Num() != 27)
	{
		return false;
	}

	// Compare sticker colors on each current face, allowing middle-slice moves
	// and a uniformly solved cube in any overall orientation.
	TMap<FIntVector, ERubiksFace> FaceColors;
	TMap<FIntVector, int32> FaceCounts;
	for (const URubiksCubeletComponent* Cubelet : Cubelets)
	{
		if (!Cubelet) return false;
		for (const FRubiksStickerInfo& Sticker : Cubelet->GetStickers())
		{
			const FVector Normal = Cubelet->GetLogicalRotation().RotateVector(Sticker.InitialNormal);
			const FIntVector Face(FMath::RoundToInt(Normal.X), FMath::RoundToInt(Normal.Y), FMath::RoundToInt(Normal.Z));
			if (FMath::Abs(Face.X) + FMath::Abs(Face.Y) + FMath::Abs(Face.Z) != 1) return false;
			const FIntVector Position = Cubelet->GetGridPosition();
			if (Position.X * Face.X + Position.Y * Face.Y + Position.Z * Face.Z != 1) return false;
			if (const ERubiksFace* Color = FaceColors.Find(Face))
			{
				if (*Color != Sticker.Face) return false;
			}
			else FaceColors.Add(Face, Sticker.Face);
			FaceCounts.FindOrAdd(Face)++;
		}
	}
	if (FaceColors.Num() != 6) return false;
	for (const auto& Entry : FaceCounts) if (Entry.Value != 9) return false;

	return true;
}

void ARubiksCube::DebugPrintCubeState() const
{
	UE_LOG(LogTemp, Log, TEXT("=== ARubiksCube State ==="));
	UE_LOG(LogTemp, Log, TEXT("Cubelets count: %d, IsRotating: %s, MoveCounter: %d, IsSolved: %s"),
		Cubelets.Num(),
		bIsRotating ? TEXT("True") : TEXT("False"),
		MoveCounter,
		IsSolved() ? TEXT("True") : TEXT("False")
	);

	for (int32 i = 0; i < Cubelets.Num(); ++i)
	{
		if (const URubiksCubeletComponent* Cubelet = Cubelets[i])
		{
			const FIntVector Pos = Cubelet->GetGridPosition();
			const FIntVector InitPos = Cubelet->GetInitialGridPosition();
			const FRotator Rot = Cubelet->GetLogicalRotation().Rotator();
			UE_LOG(LogTemp, Log, TEXT("[%02d] Pos: (%2d,%2d,%2d) | Init: (%2d,%2d,%2d) | Rot: (P=%.0f, Y=%.0f, R=%.0f) | StickersSolved: %s"),
				i, Pos.X, Pos.Y, Pos.Z, InitPos.X, InitPos.Y, InitPos.Z,
				Rot.Pitch, Rot.Yaw, Rot.Roll,
				Cubelet->AreStickersSolved() ? TEXT("Yes") : TEXT("No")
			);
		}
	}
}

void ARubiksCube::RunSelfTest()
{
	UE_LOG(LogTemp, Log, TEXT("========================================"));
	UE_LOG(LogTemp, Log, TEXT("RUNNING RUBIK'S CUBE SELF-TEST (STAGES 1-5)"));
	UE_LOG(LogTemp, Log, TEXT("========================================"));

	// Test 1: Spawning & Initial Solved State
	ResetCube();
	check(Cubelets.Num() == 27);
	const bool bInitialSolved = IsSolved();
	UE_LOG(LogTemp, Log, TEXT("Test 1: Initial state is solved: %s"), bInitialSolved ? TEXT("PASSED") : TEXT("FAILED"));

	// Test 2: R then R' returns to solved
	ApplyMoveInstant(FRubiksMoveDefinition::FromStandardMove(ERubiksMove::R));
	const bool bAfterRNotSolved = !IsSolved();
	ApplyMoveInstant(FRubiksMoveDefinition::FromStandardMove(ERubiksMove::RPrime));
	const bool bAfterRPrimeSolved = IsSolved();
	UE_LOG(LogTemp, Log, TEXT("Test 2: R disrupts solved (%s), R' restores solved (%s)"),
		bAfterRNotSolved ? TEXT("PASSED") : TEXT("FAILED"),
		bAfterRPrimeSolved ? TEXT("PASSED") : TEXT("FAILED")
	);

	// Test 3: U 4 times returns to solved
	for (int32 i = 0; i < 4; ++i)
	{
		ApplyMoveInstant(FRubiksMoveDefinition::FromStandardMove(ERubiksMove::U));
	}
	const bool bAfterU4Solved = IsSolved();
	UE_LOG(LogTemp, Log, TEXT("Test 3: U x 4 returns to solved: %s"), bAfterU4Solved ? TEXT("PASSED") : TEXT("FAILED"));

	// Test 4: Sexy move (R U R' U') x 6 returns to solved (mathematical theorem)
	for (int32 Cycle = 0; Cycle < 6; ++Cycle)
	{
		ApplyMoveInstant(FRubiksMoveDefinition::FromStandardMove(ERubiksMove::R));
		ApplyMoveInstant(FRubiksMoveDefinition::FromStandardMove(ERubiksMove::U));
		ApplyMoveInstant(FRubiksMoveDefinition::FromStandardMove(ERubiksMove::RPrime));
		ApplyMoveInstant(FRubiksMoveDefinition::FromStandardMove(ERubiksMove::UPrime));
	}
	const bool bAfterSexy6Solved = IsSolved();
	UE_LOG(LogTemp, Log, TEXT("Test 4: (R U R' U') x 6 returns to solved: %s"), bAfterSexy6Solved ? TEXT("PASSED") : TEXT("FAILED"));

	// Test 5: Instant scramble of 30 moves + applying inverse moves in reverse order returns to solved
	const int32 ScrambleMovesCount = 30;
	const TArray<FRubiksMoveDefinition> ScrambleSeq = GenerateScrambleSequence(ScrambleMovesCount);
	for (const FRubiksMoveDefinition& MoveDef : ScrambleSeq)
	{
		ApplyMoveInstant(MoveDef);
	}
	const bool bScrambledNotSolved = !IsSolved();

	// Reverse solve
	for (int32 i = ScrambleSeq.Num() - 1; i >= 0; --i)
	{
		ApplyMoveInstant(ScrambleSeq[i].GetInverse());
	}
	const bool bReverseSolveSuccess = IsSolved();
	UE_LOG(LogTemp, Log, TEXT("Test 5: Scramble %d moves disrupts (%s), reverse inverse moves solves (%s)"),
		ScrambleMovesCount,
		bScrambledNotSolved ? TEXT("PASSED") : TEXT("FAILED"),
		bReverseSolveSuccess ? TEXT("PASSED") : TEXT("FAILED")
	);

	// Test 6: Layer cubelet queries
	bool bLayerCountPassed = true;
	for (EAxis::Type Axis : { EAxis::X, EAxis::Y, EAxis::Z })
	{
		for (int32 Layer : { -1, 0, 1 })
		{
			if (GetLayerCubelets(Axis, Layer).Num() != 9)
			{
				bLayerCountPassed = false;
			}
		}
	}
	UE_LOG(LogTemp, Log, TEXT("Test 6: All 9 slices return exactly 9 cubelets: %s"), bLayerCountPassed ? TEXT("PASSED") : TEXT("FAILED"));

	ResetCube();
	UE_LOG(LogTemp, Log, TEXT("========================================"));
	UE_LOG(LogTemp, Log, TEXT("SELF-TESTS FINISHED: %s"), (bInitialSolved && bAfterRNotSolved && bAfterRPrimeSolved && bAfterU4Solved && bAfterSexy6Solved && bScrambledNotSolved && bReverseSolveSuccess && bLayerCountPassed) ? TEXT("PASSED") : TEXT("FAILED"));
	UE_LOG(LogTemp, Log, TEXT("========================================"));
}

void ARubiksCube::RotateLeft() { if (!bStickerDragging && !bIsScrambling) ExecuteMove(FRubiksMoveDefinition(SelectedAxis, SelectedLayer, -1)); }
void ARubiksCube::RotateRight() { if (!bStickerDragging && !bIsScrambling) ExecuteMove(FRubiksMoveDefinition(SelectedAxis, SelectedLayer, 1)); }
void ARubiksCube::SelectNextLayer() { SetSelection(SelectedAxis, FMath::Min(SelectedLayer + 1, 1)); }
void ARubiksCube::SelectPrevLayer() { SetSelection(SelectedAxis, FMath::Max(SelectedLayer - 1, -1)); }
void ARubiksCube::CycleAxis()
{
	SetSelection(SelectedAxis == EAxis::Z ? EAxis::X : SelectedAxis == EAxis::X ? EAxis::Y : EAxis::Z, SelectedLayer);
}
void ARubiksCube::SetSelection(EAxis::Type Axis, int32 Layer)
{
	if ((Axis != EAxis::X && Axis != EAxis::Y && Axis != EAxis::Z) || Layer < -1 || Layer > 1) return;
	SelectedAxis = Axis;
	SelectedLayer = Layer;
	FaceSelectedAxis();
	UpdateSelectionHighlight();
	OnStateChanged.Broadcast();
}
void ARubiksCube::UpdateSelectionHighlight()
{
	const APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	bSelectionWasActive = !PC || !GetWorld()->IsGameWorld() || PC->GetViewTarget() == this;
	const TArray<URubiksCubeletComponent*> Selected = GetLayerCubelets(SelectedAxis, SelectedLayer);
	for (URubiksCubeletComponent* Cubelet : Cubelets)
	{
		if (Cubelet)
		{
			Cubelet->SetSelectionHighlighted(bSelectionWasActive && Selected.Contains(Cubelet));
			Cubelet->SetRenderCustomDepth(false);
		}
	}
}
void ARubiksCube::OrbitCamera(float YawDelta, float PitchDelta)
{
	if (bScrambleOverviewActive) return;
	bIsCameraReturning = false;
	FRotator Rotation = CameraBoom->GetRelativeRotation();
	Rotation.Yaw += YawDelta;
	Rotation.Pitch = FMath::Clamp(Rotation.Pitch + PitchDelta, -85.f, 85.f);
	CameraBoom->SetRelativeRotation(Rotation);
	if (InteractionMode == ERubiksInteractionMode::Play) FaceSelectedAxis();
}
void ARubiksCube::ZoomCamera(float DistanceDelta)
{
	if (bScrambleOverviewActive || bStickerDragging) return;
	bIsCameraReturning = false;
	CameraBoom->TargetArmLength = FMath::Clamp(CameraBoom->TargetArmLength + DistanceDelta, 400.f, 2000.f);
}
void ARubiksCube::ResetCamera()
{
	if (bScrambleOverviewActive || bStickerDragging) return;
	bIsCameraReturning = false;
	CameraBoom->TargetArmLength = FMath::Clamp(ViewingDistance, 400.f, 2000.f);
	CameraBoom->SetRelativeRotation(FRotator(-20.f, 0.f, 0.f));
	FollowCamera->SetRelativeRotation(FRotator::ZeroRotator);
	FaceSelectedAxis();
}
void ARubiksCube::ActivateCamera()
{
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		PC->bAutoManageActiveCameraTarget = false;
		PC->SetViewTarget(this);
		RefreshPointerInput();
	}
}

void ARubiksCube::FaceSelectedAxis(bool bInstant)
{
	if (InteractionMode == ERubiksInteractionMode::Inspect || bScrambleOverviewActive || bStickerDragging) return;
	FVector Normal = SelectedAxis == EAxis::X ? FVector::ForwardVector
		: SelectedAxis == EAxis::Y ? FVector::RightVector : FVector::UpVector;
	if (SelectedLayer < 0) Normal *= -1.f;
	const FVector LocalUp = SelectedAxis == EAxis::Z ? FVector::RightVector : FVector::UpVector;
	const FQuat LocalBasis = FRotationMatrix::MakeFromXZ(Normal, LocalUp).ToQuat();
	// Spring arm socket direction is known even before its first component tick.
	const FQuat CameraRotation = bIsCameraReturning ? CameraTransitionTarget * FollowCamera->GetRelativeRotation().Quaternion()
		: CubeRoot->GetComponentQuat().Inverse() * CameraBoom->GetComponentQuat() * FollowCamera->GetRelativeRotation().Quaternion();
	const float ViewPitch = SelectedLayer == 0 ? FMath::Max(30.f, FaceViewPitch) : FaceViewPitch;
	const FVector ViewNormal = -CameraRotation.RotateVector(FRotator(
		FMath::Clamp(ViewPitch, 0.f, 45.f), FMath::Clamp(FaceViewYaw, -45.f, 45.f), 0.f).Vector());
	const FQuat ViewBasis = FRotationMatrix::MakeFromXZ(ViewNormal, CameraRotation.GetUpVector()).ToQuat();
	FaceTransitionStart = CubePresentation->GetRelativeRotation().Quaternion();
	FaceTransitionTarget = (ViewBasis * LocalBasis.Inverse()).GetNormalized();
	FaceTransitionElapsed = 0.f;
	bIsFacingTransition = !bInstant && !FaceTransitionStart.Equals(FaceTransitionTarget, 0.0001f);
	if (!bIsFacingTransition) CubePresentation->SetRelativeRotation(FaceTransitionTarget);
}

void ARubiksCube::UpdateFaceTransition(float DeltaTime)
{
	if (!bIsFacingTransition) return;
	FaceTransitionElapsed += FMath::Max(0.f, DeltaTime);
	const float Alpha = FMath::Clamp(FaceTransitionElapsed / FMath::Max(0.1f, FaceTransitionDuration), 0.f, 1.f);
	const float Eased = FMath::InterpEaseInOut(0.f, 1.f, Alpha, 2.f);
	CubePresentation->SetRelativeRotation(FQuat::Slerp(FaceTransitionStart, FaceTransitionTarget, Eased).GetNormalized());
	if (Alpha >= 1.f)
	{
		CubePresentation->SetRelativeRotation(FaceTransitionTarget);
		bIsFacingTransition = false;
	}
}
void ARubiksCube::HandleUndoInput() { FinishStickerGesture(true); Undo(); }
void ARubiksCube::HandleScrambleInput() { Scramble(20, false); }
void ARubiksCube::HandleResetInput() { ResetCube(); }
void ARubiksCube::BindPlayerInput()
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC) return;
	EnableInput(PC);
	InputComponent->BindKey(EKeys::V, IE_Pressed, this, &ARubiksCube::ToggleInteractionMode);
	InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &ARubiksCube::BeginStickerPointer);
	InputComponent->BindKey(EKeys::LeftMouseButton, IE_Released, this, &ARubiksCube::ReleaseStickerPointer);
	InputComponent->BindKey(EKeys::RightMouseButton, IE_Pressed, this, &ARubiksCube::BeginInspectDrag);
	InputComponent->BindKey(EKeys::RightMouseButton, IE_Released, this, &ARubiksCube::EndInspectDrag);
	InputComponent->BindKey(EKeys::F, IE_Pressed, this, &ARubiksCube::ReturnToFaceView);
	InputComponent->BindKey(FInputChord(EKeys::Z, false, true, false, false), IE_Pressed, this, &ARubiksCube::HandleUndoInput);
	InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &ARubiksCube::CancelStickerPointer);

	InputComponent->BindKey(EKeys::MouseScrollUp, IE_Pressed, this, &ARubiksCube::InspectZoomIn);
	InputComponent->BindKey(EKeys::MouseScrollDown, IE_Pressed, this, &ARubiksCube::InspectZoomOut);
	if (ULocalPlayer* Player = PC->GetLocalPlayer())
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = Player->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
		{
			if (RubiksMappingContext) Subsystem->AddMappingContext(RubiksMappingContext, 0);
		}
	}
	if (UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(InputComponent))
	{
		if (IA_RotateLeft) Input->BindAction(IA_RotateLeft, ETriggerEvent::Started, this, &ARubiksCube::RotateLeft);
		if (IA_RotateRight) Input->BindAction(IA_RotateRight, ETriggerEvent::Started, this, &ARubiksCube::RotateRight);
		if (IA_SelectNextLayer) Input->BindAction(IA_SelectNextLayer, ETriggerEvent::Started, this, &ARubiksCube::SelectNextLayer);
		if (IA_SelectPrevLayer) Input->BindAction(IA_SelectPrevLayer, ETriggerEvent::Started, this, &ARubiksCube::SelectPrevLayer);
		if (IA_CycleAxis) Input->BindAction(IA_CycleAxis, ETriggerEvent::Started, this, &ARubiksCube::CycleAxis);
		if (IA_Undo) Input->BindAction(IA_Undo, ETriggerEvent::Started, this, &ARubiksCube::HandleUndoInput);
		if (IA_Reset) Input->BindAction(IA_Reset, ETriggerEvent::Started, this, &ARubiksCube::HandleResetInput);
		if (IA_Scramble) Input->BindAction(IA_Scramble, ETriggerEvent::Started, this, &ARubiksCube::HandleScrambleInput);
	}
}

void ARubiksCube::SetupSelectionHighlight()
{
    if (!SelectionHighlightMaterial) return;
    if (!SelectionHighlightDynamic || SelectionHighlightDynamic->Parent != SelectionHighlightMaterial)
        SelectionHighlightDynamic = UMaterialInstanceDynamic::Create(SelectionHighlightMaterial, this);
    for (URubiksCubeletComponent* Cubelet : Cubelets)
        if (Cubelet) Cubelet->SetupSelectionHighlight(SelectionHighlightDynamic);
}

void ARubiksCube::ToggleInteractionMode()
{
    SetInteractionMode(InteractionMode == ERubiksInteractionMode::Play ? ERubiksInteractionMode::Inspect : ERubiksInteractionMode::Play);
}

void ARubiksCube::SetInteractionMode(ERubiksInteractionMode Mode)
{
    if (bScrambleOverviewActive || Mode == InteractionMode || (Mode != ERubiksInteractionMode::Play && Mode != ERubiksInteractionMode::Inspect)) return;
    FinishStickerGesture(true);
    EndInspectDrag();
    InteractionMode = Mode;
    if (Mode == ERubiksInteractionMode::Inspect)
    {
        // Freeze the presentation at the current visual pose; only the camera orbits.
        bIsFacingTransition = false;
        FRotator InspectView = CameraBoom->GetRelativeRotation();
        InspectView.Yaw += 12.f;
        InspectView.Pitch = FMath::Clamp(InspectView.Pitch - 5.f, -85.f, 85.f);
        StartCameraTransition(InspectView.Quaternion(), CameraBoom->TargetArmLength + 60.f);
    }
    else
    {
        StartCameraTransition(FRotator(-20.f, 0.f, 0.f).Quaternion(), ViewingDistance);
        FaceSelectedAxis();
    }
    RefreshPointerInput();
    OnStateChanged.Broadcast();
}

void ARubiksCube::UpdateCameraReturn(float DeltaTime)
{
    if (!bIsCameraReturning) return;
    CameraReturnElapsed += FMath::Max(0.f, DeltaTime);
    const float Alpha = FMath::Clamp(CameraReturnElapsed / FMath::Max(.1f, FaceTransitionDuration), 0.f, 1.f);
    const float Eased = FMath::InterpEaseInOut(0.f, 1.f, Alpha, 2.f);
    CameraBoom->SetRelativeRotation(FQuat::Slerp(CameraReturnStart, CameraTransitionTarget, Eased).GetNormalized());
    CameraBoom->TargetArmLength = FMath::Lerp(CameraReturnDistance, CameraTransitionDistance, Eased);
    if (Alpha >= 1.f) bIsCameraReturning = false;
}

void ARubiksCube::RefreshPointerInput()
{
    if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
    {
        if (PC->GetViewTarget() != this) return;
        PC->bShowMouseCursor = !bInspectDragging;
        if (bInspectDragging) PC->SetInputMode(FInputModeGameOnly());
        else
        {
            FInputModeGameAndUI PointerMode;
            PointerMode.SetHideCursorDuringCapture(false);
            PointerMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
            PC->SetInputMode(PointerMode);
        }
    }
}

void ARubiksCube::BeginInspectDrag()
{
    if (bScrambleOverviewActive) return;
    if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
    {
        if (PC->GetViewTarget() != this) return;
        FinishStickerGesture(true);
        // Direct orbit enters Inspect without a separate key or entry animation.
        InteractionMode = ERubiksInteractionMode::Inspect;
        bIsFacingTransition = false;
        bIsCameraReturning = false;
        bInspectDragging = true;
        RefreshPointerInput();
        OnStateChanged.Broadcast();
    }
}

void ARubiksCube::EndInspectDrag()
{
    if (!bInspectDragging) return;
    bInspectDragging = false;
    RefreshPointerInput();
}

void ARubiksCube::UpdateInspectCamera()
{
    if (!bInspectDragging) return;
    if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
    {
        if (PC->GetViewTarget() != this || !PC->IsInputKeyDown(EKeys::RightMouseButton)) { EndInspectDrag(); return; }
        float X = 0.f, Y = 0.f;
        PC->GetInputMouseDelta(X, Y);
        OrbitCamera(-X * OrbitSensitivity, Y * OrbitSensitivity);
    }
}

void ARubiksCube::InspectZoomIn() { ZoomCamera(-ZoomStep); }
void ARubiksCube::InspectZoomOut() { ZoomCamera(ZoomStep); }
void ARubiksCube::ReturnToFaceView()
{
    if (bScrambleOverviewActive) return;
    FinishStickerGesture(true);
    EndInspectDrag();
    if (InteractionMode != ERubiksInteractionMode::Play) SetInteractionMode(ERubiksInteractionMode::Play);
    else { StartCameraTransition(FRotator(-20.f, 0.f, 0.f).Quaternion(), ViewingDistance); FaceSelectedAxis(); }
}

bool ARubiksCube::ResolveStickerDrag(const FIntVector& Position, const FVector& FaceNormal, const FQuat& Presentation, const FQuat& Camera, const FVector2D& Drag, FRubiksMoveDefinition& OutMove)
{
    if (Drag.IsNearlyZero()) return false;
    const FVector2D DragDirection = Drag.GetSafeNormal();
    float Best = 0.f, Second = 0.f;
    for (EAxis::Type Axis : {EAxis::X, EAxis::Y, EAxis::Z})
    {
        const FVector Basis = Axis == EAxis::X ? FVector::ForwardVector : Axis == EAxis::Y ? FVector::RightVector : FVector::UpVector;
        if (FMath::Abs(FVector::DotProduct(Basis, FaceNormal)) > .5f) continue;
        const FVector Tangent = Camera.UnrotateVector(Presentation.RotateVector(FVector::CrossProduct(Basis, FVector(Position) + FaceNormal * .5f)));
        const FVector2D ScreenTangent(Tangent.Y, -Tangent.Z);
        if (ScreenTangent.SizeSquared() < .01f) continue;
        const float Signed = FVector2D::DotProduct(DragDirection, ScreenTangent.GetSafeNormal());
        const float Score = FMath::Abs(Signed);
        if (Score > Best)
        {
            Second = Best;
            Best = Score;
            const int32 Layer = Axis == EAxis::X ? Position.X : Axis == EAxis::Y ? Position.Y : Position.Z;
            OutMove = FRubiksMoveDefinition(Axis, Layer, Signed > 0.f ? 1 : -1);
        }
        else Second = FMath::Max(Second, Score);
    }
    // Wait for a deliberate direction rather than guessing on diagonal gestures.
    return Best >= .8f && Best - Second >= .12f;
}

bool ARubiksCube::BeginStickerGesture(URubiksCubeletComponent* Cubelet, const FVector& WorldNormal)
{
    if (bStickerDragging || bInspectDragging || bIsRotating || bIsScrambling || bIsCameraReturning || bIsFacingTransition || !Cubelet || !Cubelets.Contains(Cubelet)) return false;
    const FVector Normal = CubePresentation->GetComponentQuat().UnrotateVector(WorldNormal);
    const FVector Abs = Normal.GetAbs();
    StickerFaceNormal = Abs.X >= Abs.Y && Abs.X >= Abs.Z ? FVector(FMath::Sign(Normal.X), 0, 0)
        : Abs.Y >= Abs.Z ? FVector(0, FMath::Sign(Normal.Y), 0) : FVector(0, 0, FMath::Sign(Normal.Z));
    const FIntVector Position = Cubelet->GetGridPosition();
    if (FVector::DotProduct(FVector(Position), StickerFaceNormal) < .9f) return false;
    DraggedCubelet = Cubelet;
    PreDragAxis = SelectedAxis;
    PreDragLayer = SelectedLayer;
    StickerDrag = FVector2D::ZeroVector;
    StickerMaxTravel = 0.f;
    bStickerDragging = true;
    bHasStickerPreview = false;
    return true;
}

void ARubiksCube::UpdateStickerGesture(const FVector2D& Drag)
{
    if (!bStickerDragging || !DraggedCubelet.IsValid()) return;
    StickerDrag = Drag;
    StickerMaxTravel = FMath::Max(StickerMaxTravel, static_cast<float>(Drag.Size()));
    bHasStickerPreview = Drag.Size() >= 10.f && ResolveStickerDrag(DraggedCubelet->GetGridPosition(), StickerFaceNormal,
        CubePresentation->GetComponentQuat(), CameraBoom->GetComponentQuat() * FollowCamera->GetRelativeRotation().Quaternion(), Drag, PreviewMove);
    if (bHasStickerPreview)
    {
        SelectedAxis = PreviewMove.Axis;
        SelectedLayer = PreviewMove.Layer;
        const FVector Axis = PreviewMove.Axis == EAxis::X ? FVector::ForwardVector : PreviewMove.Axis == EAxis::Y ? FVector::RightVector : FVector::UpVector;
        const FVector Point = FVector(DraggedCubelet->GetGridPosition()) + StickerFaceNormal * .5f;
        // Place a tapered arc just outside the selected slice's silhouette.
        const FVector Center = Axis * PreviewMove.Layer * CubeSpacing;
        const FVector Radial = Point * CubeSpacing - Center;
        const FVector Radius = Radial.GetSafeNormal() * FMath::Max(CubeSpacing * 2.35f, static_cast<float>(Radial.Size()) + 25.f);
        const float Sweep = FMath::DegreesToRadians(32.f) * PreviewMove.Direction;
        const float StartAngle = FMath::DegreesToRadians(-12.f) * PreviewMove.Direction;
        for (int32 Index = 0; Index < MovePreviewArc.Num(); ++Index)
        {
            const float StartAlpha = static_cast<float>(Index) / MovePreviewArc.Num();
            const float EndAlpha = static_cast<float>(Index + 1) / MovePreviewArc.Num();
            const FVector StartRadius = FQuat(Axis, StartAngle + Sweep * StartAlpha).RotateVector(Radius);
            const FVector EndRadius = FQuat(Axis, StartAngle + Sweep * EndAlpha).RotateVector(Radius);
            const float SegmentAngle = Sweep / MovePreviewArc.Num();
            USplineMeshComponent* Arc = MovePreviewArc[Index];
            Arc->SetStartScale(FVector2D(FMath::Lerp(.025f, .065f, StartAlpha)), false);
            Arc->SetEndScale(FVector2D(FMath::Lerp(.025f, .065f, EndAlpha)), false);
            Arc->SetSplineUpDir(Axis, false);
            Arc->SetStartAndEnd(Center + StartRadius, FVector::CrossProduct(Axis, StartRadius) * SegmentAngle,
                Center + EndRadius, FVector::CrossProduct(Axis, EndRadius) * SegmentAngle, true);
        }
        const FVector EndRadius = FQuat(Axis, StartAngle + Sweep).RotateVector(Radius);
        const FVector Direction = CubePresentation->GetComponentQuat().RotateVector(FVector::CrossProduct(Axis, EndRadius)).GetSafeNormal() * PreviewMove.Direction;
        const FVector End = CubePresentation->GetComponentTransform().TransformPosition(Center + EndRadius);
        MovePreviewTip->SetWorldLocationAndRotation(End + Direction * 10.f, FRotationMatrix::MakeFromZ(Direction).Rotator());
        MovePreviewTip->SetWorldScale3D(FVector(.18f, .18f, .2f));
    }
    else { SelectedAxis = PreDragAxis; SelectedLayer = PreDragLayer; }
    SetPreviewArrowVisible(bHasStickerPreview);
    UpdateSelectionHighlight();
}

bool ARubiksCube::FinishStickerGesture(bool bCancel)
{
    if (!bStickerDragging) return false;
    const bool bCommit = !bCancel && bHasStickerPreview && StickerDrag.Size() >= FMath::Max(12.f, TurnDragThreshold) && !bIsRotating && !bIsScrambling;
    const bool bSelectFace = !bCancel && StickerMaxTravel < 10.f && !bIsRotating && !bIsScrambling;
    bStickerDragging = false;
    bStickerPointerActive = false;
    bHasStickerPreview = false;
    DraggedCubelet.Reset();
    SetPreviewArrowVisible(false);
    if (bSelectFace)
    {
        SelectedAxis = FMath::Abs(StickerFaceNormal.X) > .5f ? EAxis::X : FMath::Abs(StickerFaceNormal.Y) > .5f ? EAxis::Y : EAxis::Z;
        SelectedLayer = FMath::RoundToInt(SelectedAxis == EAxis::X ? StickerFaceNormal.X : SelectedAxis == EAxis::Y ? StickerFaceNormal.Y : StickerFaceNormal.Z);
    }
    else if (!bCommit) { SelectedAxis = PreDragAxis; SelectedLayer = PreDragLayer; }
    UpdateSelectionHighlight();
    OnStateChanged.Broadcast();
    if (bCommit) ExecuteMove(PreviewMove);
    return bCommit;
}

void ARubiksCube::CancelStickerPointer() { FinishStickerGesture(true); }

void ARubiksCube::BeginStickerPointer()
{
    APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
    if (!PC || PC->GetViewTarget() != this || !PC->GetMousePosition(StickerPointerStart.X, StickerPointerStart.Y)) return;
    FHitResult Hit;
    if (!PC->GetHitResultUnderCursor(ECC_Visibility, false, Hit) || Hit.GetActor() != this) return;
    for (USceneComponent* Component = Hit.GetComponent(); Component; Component = Component->GetAttachParent())
        if (URubiksCubeletComponent* Cubelet = Cast<URubiksCubeletComponent>(Component))
        { bStickerPointerActive = BeginStickerGesture(Cubelet, Hit.ImpactNormal); break; }
}

void ARubiksCube::UpdateStickerPointer()
{
    if (!bStickerDragging || !bStickerPointerActive) return;
    APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
    FVector2D Mouse;
    if (!PC || PC->GetViewTarget() != this || !PC->IsInputKeyDown(EKeys::LeftMouseButton) || !PC->GetMousePosition(Mouse.X, Mouse.Y))
    { FinishStickerGesture(true); return; }
    UpdateStickerGesture(Mouse - StickerPointerStart);
}

void ARubiksCube::ReleaseStickerPointer()
{
    if (!bStickerDragging) return;
    APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
    FVector2D Mouse;
    if (!PC || PC->GetViewTarget() != this || !PC->GetMousePosition(Mouse.X, Mouse.Y)) { FinishStickerGesture(true); return; }
    UpdateStickerGesture(Mouse - StickerPointerStart);
    FinishStickerGesture();
}

void ARubiksCube::StartCameraTransition(const FQuat& Rotation, float Distance)
{
    CameraReturnStart = CameraBoom->GetRelativeRotation().Quaternion();
    CameraReturnDistance = CameraBoom->TargetArmLength;
    CameraTransitionTarget = Rotation.GetNormalized();
    CameraTransitionDistance = FMath::Clamp(Distance, 400.f, 2000.f);
    CameraReturnElapsed = 0.f;
    bIsCameraReturning = true;
}

void ARubiksCube::BeginScrambleOverview()
{
    FinishStickerGesture(true);
    EndInspectDrag();
    // Preserve the intended destination if scramble interrupts a mode transition.
    PreScrambleCameraRotation = bIsCameraReturning ? CameraTransitionTarget : CameraBoom->GetRelativeRotation().Quaternion();
    PreScrambleCameraDistance = bIsCameraReturning ? CameraTransitionDistance : CameraBoom->TargetArmLength;
    bScrambleOverviewActive = true;
    bIsFacingTransition = false;
    FRotator Overview = CameraBoom->GetRelativeRotation();
    Overview.Yaw += 30.f;
    Overview.Pitch = FMath::Clamp(Overview.Pitch - 12.f, -65.f, 65.f);
    Overview.Roll = 0.f;
    StartCameraTransition(Overview.Quaternion(), FMath::Max(PreScrambleCameraDistance, ViewingDistance) + 220.f);
}

void ARubiksCube::EndScrambleOverview()
{
    if (!bScrambleOverviewActive) return;
    bScrambleOverviewActive = false;
    StartCameraTransition(PreScrambleCameraRotation, PreScrambleCameraDistance);
    // Inspect retains its orbit and presentation; Play follows the latest selection.
    if (InteractionMode == ERubiksInteractionMode::Play) FaceSelectedAxis();
}

void ARubiksCube::SetPreviewArrowVisible(bool bVisible)
{
    // Keep the old straight shaft hidden for existing Blueprint compatibility.
    MovePreviewShaft->SetVisibility(false);
    MovePreviewTip->SetVisibility(bVisible);
    for (USplineMeshComponent* Arc : MovePreviewArc) Arc->SetVisibility(bVisible);
}
