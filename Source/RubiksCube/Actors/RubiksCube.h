#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/TimelineComponent.h"
#include "Data/RubiksCubeTypes.h"
#include "Components/RubiksCubeletComponent.h"
#include "RubiksCube.generated.h"

class USceneComponent;
class USpringArmComponent;
class UCameraComponent;
class UMaterialInstanceDynamic;
class UCurveFloat;
class UStaticMeshComponent;
class USplineMeshComponent;
class UInputMappingContext;
class UInputAction;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnRubiksCubeSolved);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRubiksMoveStarted, const FRubiksMoveDefinition&, MoveDef);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRubiksMoveCompleted, const FRubiksMoveDefinition&, MoveDef);

/**
 * 3x3x3 Rubik's Cube Actor.
 * Implements Phases 1-5: Visual prototype, pure integer cube mathematics,
 * temporary pivot rotation mechanics, all 12+ slice moves, and full gameplay systems
 * (move queue, history, undo, reset, scramble, solved detection, and move counter).
 */
UCLASS()
class RUBIKSCUBE_API ARubiksCube : public AActor
{
	GENERATED_BODY()

public:
	ARubiksCube();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual bool ShouldTickIfViewportsOnly() const override { return true; }

public:
	virtual void Tick(float DeltaTime) override;

	// =========================================================================
	// STAGE 1: Visual Prototype & Components
	// =========================================================================

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rubiks Cube|Components")
	TObjectPtr<USceneComponent> CubeRoot;

	/** Rotates the entire puzzle for viewing without changing its logical coordinates. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rubiks Cube|Components")
	TObjectPtr<USceneComponent> CubePresentation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rubiks Cube|Camera", meta=(ClampMin="0.1"))
	float FaceTransitionDuration = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rubiks Cube|Camera", meta=(ClampMin="400", ClampMax="2000"))
	float ViewingDistance = 1050.f;

	/** The overhead offset shows depth while keeping the selected face upright. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rubiks Cube|Camera", meta=(ClampMin="0", ClampMax="45"))
	float FaceViewPitch = 20.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rubiks Cube|Camera", meta=(ClampMin="-45", ClampMax="45"))
	float FaceViewYaw = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rubiks Cube|Camera")
	ERubiksInteractionMode InteractionMode = ERubiksInteractionMode::Play;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rubiks Cube|Camera", meta=(ClampMin="0.05", ClampMax="2"))
	float OrbitSensitivity = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rubiks Cube|Camera", meta=(ClampMin="10", ClampMax="200"))
	float ZoomStep = 80.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rubiks Cube|Materials")
	TObjectPtr<UMaterialInterface> SelectionHighlightMaterial;

	UFUNCTION(BlueprintCallable, Category="Rubiks Cube|Camera") void SetInteractionMode(ERubiksInteractionMode Mode);
	UFUNCTION(BlueprintCallable, Category="Rubiks Cube|Camera") void ToggleInteractionMode();
	/** Resolve screen-space sticker motion independently of the keyboard selection. */
	static bool ResolveStickerDrag(const FIntVector& Position, const FVector& FaceNormal, const FQuat& Presentation, const FQuat& Camera, const FVector2D& Drag, FRubiksMoveDefinition& OutMove);
	UFUNCTION(BlueprintCallable, Category="Rubiks Cube|Input")
	bool BeginStickerGesture(URubiksCubeletComponent* Cubelet, const FVector& WorldNormal);
	UFUNCTION(BlueprintCallable, Category="Rubiks Cube|Input")
	void UpdateStickerGesture(const FVector2D& Drag);
	UFUNCTION(BlueprintCallable, Category="Rubiks Cube|Input")
	bool FinishStickerGesture(bool bCancel = false);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rubiks Cube|Input", meta=(ClampMin="12", ClampMax="100"))
	float TurnDragThreshold = 28.f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Rubiks Cube|Components")
	TObjectPtr<UStaticMeshComponent> MovePreviewShaft;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Rubiks Cube|Components")
	TObjectPtr<UStaticMeshComponent> MovePreviewTip;
	UFUNCTION(BlueprintPure, Category="Rubiks Cube|Camera") bool IsViewTransitioning() const { return bIsFacingTransition || bIsCameraReturning; }

	/** Temporary pivot used strictly during slice rotations */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rubiks Cube|Components")
	TObjectPtr<USceneComponent> RotationPivot;

	/** Camera spring arm for viewing the cube */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rubiks Cube|Components")
	TObjectPtr<USpringArmComponent> CameraBoom;

	/** Fixed camera viewing the selected face; supports optional orbit controls. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rubiks Cube|Components")
	TObjectPtr<UCameraComponent> FollowCamera;

	/** Spacing between cubelet centers (in cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rubiks Cube|Visuals")
	float CubeSpacing = 105.f;

	/** Base cubelet mesh (defaults to Engine /BasicShapes/Cube) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rubiks Cube|Visuals")
	TObjectPtr<UStaticMesh> CubeletMesh;

	/** Sticker mesh (defaults to Engine /BasicShapes/Cube scaled flat) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rubiks Cube|Visuals")
	TObjectPtr<UStaticMesh> StickerMesh;

	/** Materials for core body and the 6 faces */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rubiks Cube|Materials")
	TObjectPtr<UMaterialInterface> MaterialCore;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rubiks Cube|Materials")
	TObjectPtr<UMaterialInterface> MaterialUp;     // White (+Z)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rubiks Cube|Materials")
	TObjectPtr<UMaterialInterface> MaterialDown;   // Yellow (-Z)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rubiks Cube|Materials")
	TObjectPtr<UMaterialInterface> MaterialLeft;   // Orange (-X)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rubiks Cube|Materials")
	TObjectPtr<UMaterialInterface> MaterialRight;  // Red (+X)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rubiks Cube|Materials")
	TObjectPtr<UMaterialInterface> MaterialFront;  // Green (+Y)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rubiks Cube|Materials")
	TObjectPtr<UMaterialInterface> MaterialBack;   // Blue (-Y)

	/** Array of all 27 spawned cubelets */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Rubiks Cube|State")
	TArray<TObjectPtr<URubiksCubeletComponent>> Cubelets;

	// =========================================================================
	// STAGE 2: Cube Mathematics
	// =========================================================================

	/** Pure integer coordinate rotation for arbitrary axis and direction (+1 or -1) */
	UFUNCTION(BlueprintPure, Category = "Rubiks Cube|Math")
	static FIntVector RotateGridPosition(const FIntVector& Position, EAxis::Type Axis, int32 Direction);

	static FIntVector RotateGridPositionX(const FIntVector& Position, int32 Direction);
	static FIntVector RotateGridPositionY(const FIntVector& Position, int32 Direction);
	static FIntVector RotateGridPositionZ(const FIntVector& Position, int32 Direction);

	/** Returns the 9 cubelets belonging to a specific axis and layer (-1, 0, +1) */
	UFUNCTION(BlueprintCallable, Category = "Rubiks Cube|Math")
	TArray<URubiksCubeletComponent*> GetLayerCubelets(EAxis::Type Axis, int32 Layer) const;

	// =========================================================================
	// STAGE 3 & 4: Move Execution & Rotation Animation
	// =========================================================================

	/** Duration of a single 90° slice rotation in seconds */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rubiks Cube|Animation")
	float RotationDuration = 0.6f;

	/** Duration used during scramble playback */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rubiks Cube|Animation")
	float ScrambleRotationDuration = 0.3f;


	/** Optional normalized CurveFloat: time 0..1, value 0..1. Uses smooth easing when unset. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rubiks Cube|Animation")
	TObjectPtr<UCurveFloat> RotationCurve;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Rubiks Cube|Input")
	TEnumAsByte<EAxis::Type> SelectedAxis = EAxis::Z;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Rubiks Cube|Input")
	int32 SelectedLayer = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rubiks Cube|Input")
	TObjectPtr<UInputMappingContext> RubiksMappingContext;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rubiks Cube|Input")
	TObjectPtr<UInputAction> IA_RotateLeft;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rubiks Cube|Input")
	TObjectPtr<UInputAction> IA_RotateRight;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rubiks Cube|Input")
	TObjectPtr<UInputAction> IA_SelectNextLayer;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rubiks Cube|Input")
	TObjectPtr<UInputAction> IA_SelectPrevLayer;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rubiks Cube|Input")
	TObjectPtr<UInputAction> IA_CycleAxis;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rubiks Cube|Input")
	TObjectPtr<UInputAction> IA_Undo;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rubiks Cube|Input")
	TObjectPtr<UInputAction> IA_Reset;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rubiks Cube|Input")
	TObjectPtr<UInputAction> IA_Scramble;

	UFUNCTION(BlueprintCallable, Category="Rubiks Cube|Input") void RotateLeft();
	UFUNCTION(BlueprintCallable, Category="Rubiks Cube|Input") void RotateRight();
	UFUNCTION(BlueprintCallable, Category="Rubiks Cube|Input") void SelectNextLayer();
	UFUNCTION(BlueprintCallable, Category="Rubiks Cube|Input") void SelectPrevLayer();
	UFUNCTION(BlueprintCallable, Category="Rubiks Cube|Input") void CycleAxis();
	UFUNCTION(BlueprintCallable, Category="Rubiks Cube|Input") void SetSelection(EAxis::Type Axis, int32 Layer);
	UFUNCTION(BlueprintCallable, Category="Rubiks Cube|Camera") void OrbitCamera(float YawDelta, float PitchDelta);
	UFUNCTION(BlueprintCallable, Category="Rubiks Cube|Camera") void ZoomCamera(float DistanceDelta);
	UFUNCTION(BlueprintCallable, Category="Rubiks Cube|Camera") void ResetCamera();
	UFUNCTION(BlueprintCallable, Category="Rubiks Cube|Camera") void ActivateCamera();
	/** Broadcast on selection/reset/move completion. */
	UPROPERTY(BlueprintAssignable, Category="Rubiks Cube|Events")
	FOnRubiksCubeSolved OnStateChanged;

	/** Executes a Singmaster standard move (U, U', D, D', L, L', R, R', F, F', B, B') */
	UFUNCTION(BlueprintCallable, Category = "Rubiks Cube|Moves")
	void ExecuteStandardMove(ERubiksMove Move);

	/** Executes an arbitrary slice move definition */
	UFUNCTION(BlueprintCallable, Category = "Rubiks Cube|Moves")
	void ExecuteMove(const FRubiksMoveDefinition& MoveDef);

	/** Convenience functions for each of the 12 classic moves */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Rubiks Cube|Moves") void Move_U() { ExecuteStandardMove(ERubiksMove::U); }
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Rubiks Cube|Moves") void Move_UPrime() { ExecuteStandardMove(ERubiksMove::UPrime); }
	UFUNCTION(BlueprintCallable, Category = "Rubiks Cube|Moves") void Move_D() { ExecuteStandardMove(ERubiksMove::D); }
	UFUNCTION(BlueprintCallable, Category = "Rubiks Cube|Moves") void Move_DPrime() { ExecuteStandardMove(ERubiksMove::DPrime); }
	UFUNCTION(BlueprintCallable, Category = "Rubiks Cube|Moves") void Move_L() { ExecuteStandardMove(ERubiksMove::L); }
	UFUNCTION(BlueprintCallable, Category = "Rubiks Cube|Moves") void Move_LPrime() { ExecuteStandardMove(ERubiksMove::LPrime); }
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Rubiks Cube|Moves") void Move_R() { ExecuteStandardMove(ERubiksMove::R); }
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Rubiks Cube|Moves") void Move_RPrime() { ExecuteStandardMove(ERubiksMove::RPrime); }
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Rubiks Cube|Moves") void Move_F() { ExecuteStandardMove(ERubiksMove::F); }
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Rubiks Cube|Moves") void Move_FPrime() { ExecuteStandardMove(ERubiksMove::FPrime); }
	UFUNCTION(BlueprintCallable, Category = "Rubiks Cube|Moves") void Move_B() { ExecuteStandardMove(ERubiksMove::B); }
	UFUNCTION(BlueprintCallable, Category = "Rubiks Cube|Moves") void Move_BPrime() { ExecuteStandardMove(ERubiksMove::BPrime); }

	/** Is a slice rotation currently animating */
	UFUNCTION(BlueprintPure, Category = "Rubiks Cube|State")
	bool IsRotating() const { return bIsRotating; }

	// =========================================================================
	// STAGE 5: Gameplay Systems
	// =========================================================================

	/** Move counter tracking number of player moves made */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Rubiks Cube|Gameplay")
	int32 MoveCounter = 0;

	/** Reverts the most recent move in history */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Rubiks Cube|Gameplay")
	bool Undo();

	/** Resets the cube to its solved state immediately */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Rubiks Cube|Gameplay")
	void ResetCube();

	/** Scrambles the cube with N random moves adhering to WCA anti-redundancy */
	UFUNCTION(BlueprintCallable, Category = "Rubiks Cube|Gameplay")
	void Scramble(int32 MoveCount = 20, bool bInstant = false);

	/** One-click animated scramble (20 moves) in editor */
	UFUNCTION(CallInEditor, Category = "Rubiks Cube|Gameplay")
	void ScrambleCube() { Scramble(20, false); }

	/** One-click instant scramble (20 moves) in editor */
	UFUNCTION(CallInEditor, Category = "Rubiks Cube|Gameplay")
	void InstantScramble() { Scramble(20, true); }

	/** Returns true if the entire cube is mathematically solved */
	UFUNCTION(BlueprintPure, Category = "Rubiks Cube|Gameplay")
	bool IsSolved() const;

	/** Move queue containing pending moves */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Rubiks Cube|Gameplay")
	TArray<FRubiksMoveDefinition> MoveQueue;

	/** Move history for undo and replay */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Rubiks Cube|Gameplay")
	TArray<FRubiksMoveDefinition> MoveHistory;

	// =========================================================================
	// Events & Delegates
	// =========================================================================

	UPROPERTY(BlueprintAssignable, Category = "Rubiks Cube|Events")
	FOnRubiksCubeSolved OnCubeSolved;

	UPROPERTY(BlueprintAssignable, Category = "Rubiks Cube|Events")
	FOnRubiksMoveStarted OnMoveStarted;

	UPROPERTY(BlueprintAssignable, Category = "Rubiks Cube|Events")
	FOnRubiksMoveCompleted OnMoveCompleted;

	// =========================================================================
	// Debugging & Testing
	// =========================================================================

	/** Prints debug info of all 27 cubelet coordinates to Log */
	UFUNCTION(CallInEditor, Category = "Rubiks Cube|Debug")
	void DebugPrintCubeState() const;

	/** Runs mathematical self-test verifying Stages 1-5 logic */
	UFUNCTION(CallInEditor, Category = "Rubiks Cube|Debug")
	void RunSelfTest();

private:
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> SelectionHighlightDynamic;
	float HighlightPulseRemaining = 0.f;
	bool bSelectionWasActive = false;
	bool bInspectDragging = false;
	bool bStickerDragging = false;
	bool bStickerPointerActive = false;
	bool bHasStickerPreview = false;
	TWeakObjectPtr<URubiksCubeletComponent> DraggedCubelet;
	FVector StickerFaceNormal = FVector::UpVector;
	FVector2D StickerPointerStart = FVector2D::ZeroVector;
	FVector2D StickerDrag = FVector2D::ZeroVector;
	float StickerMaxTravel = 0.f;
	UPROPERTY(VisibleAnywhere, Category="Rubiks Cube|Components")
	TArray<TObjectPtr<USplineMeshComponent>> MovePreviewArc;
	UPROPERTY() TObjectPtr<UMaterialInterface> MovePreviewMaterial;
	void SetPreviewArrowVisible(bool bVisible);
	EAxis::Type PreDragAxis = EAxis::Z;
	int32 PreDragLayer = 1;
	FRubiksMoveDefinition PreviewMove;
	void CancelStickerPointer();
	void BeginStickerPointer();
	void ReleaseStickerPointer();
	void UpdateStickerPointer();
	void RefreshPointerInput();
	void ReturnToFaceView();
	bool bIsCameraReturning = false;
	FQuat CameraReturnStart = FQuat::Identity;
	float CameraReturnDistance = 1050.f;
	float CameraReturnElapsed = 0.f;
	FQuat CameraTransitionTarget = FQuat::Identity;
	float CameraTransitionDistance = 1050.f;
	bool bScrambleOverviewActive = false;
	FQuat PreScrambleCameraRotation = FQuat::Identity;
	float PreScrambleCameraDistance = 1050.f;
	void StartCameraTransition(const FQuat& Rotation, float Distance);
	void BeginScrambleOverview();
	void EndScrambleOverview();
	void SetupSelectionHighlight();
	void BeginInspectDrag();
	void EndInspectDrag();
	void InspectZoomIn();
	void InspectZoomOut();
	void HandleResetInput();
	void UpdateInspectCamera();
	void UpdateCameraReturn(float DeltaTime);
	FQuat FaceTransitionStart = FQuat::Identity;
	FQuat FaceTransitionTarget = FQuat::Identity;
	float FaceTransitionElapsed = 0.f;
	bool bIsFacingTransition = false;
	void FaceSelectedAxis(bool bInstant = false);
	void UpdateFaceTransition(float DeltaTime);
	/** Creates the 27 cubelets in a 3x3x3 grid */
	void SpawnCubelets();

	/** Creates dynamic fallback materials if none were assigned */
	void SetupDefaultMaterials();

	/** Map of face materials for sticker application */
	TMap<ERubiksFace, UMaterialInterface*> FaceMaterialsMap;

	// --- Rotation State ---
	UPROPERTY()
	bool bIsRotating = false;

	UPROPERTY()
	bool bIsScrambling = false;

	UPROPERTY()
	FRubiksMoveDefinition ActiveMove;

	UPROPERTY()
	float ActiveRotationDuration = 0.22f;

	UPROPERTY()
	FVector ActiveRotationAxis = FVector::UpVector;

	UPROPERTY()
	float TargetAngleDegrees = 90.f;

	UPROPERTY()
	TArray<TObjectPtr<URubiksCubeletComponent>> ActiveLayerCubelets;

	void StartRotation(const FRubiksMoveDefinition& MoveDef);
	FTimeline RotationTimeline;
	UPROPERTY(Transient) TObjectPtr<UCurveFloat> ActiveRotationCurve;
	bool bUndoInProgress = false;
	bool bUseDefaultRotationEase = true;
	UFUNCTION() void UpdateRotationTimeline(float Alpha);
	void UpdateSelectionHighlight();
	void BindPlayerInput();
	void HandleUndoInput();
	void HandleScrambleInput();
	void FinishRotation();

	/** Applies move mathematically and snaps without visual animation */
	void ApplyMoveInstant(const FRubiksMoveDefinition& MoveDef);

	/** Generates a list of scramble moves without redundant inverses or repeats */
	TArray<FRubiksMoveDefinition> GenerateScrambleSequence(int32 MoveCount) const;
};
