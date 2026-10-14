#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Actors/RubiksCube.h"
#include "Components/RubiksCubeletComponent.h"
#include "Data/RubiksCubeTypes.h"
#include "Engine/World.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Math/RotationMatrix.h"
#include "Components/StaticMeshComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRubiksCubeMathematicsTest, "RubiksCube.Stages1To5.MathematicsAndMechanics",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FRubiksCubeMathematicsTest::RunTest(const FString& Parameters)
{
	// -------------------------------------------------------------
	// STAGE 2: Coordinate Mathematics Tests (Pure Math)
	// -------------------------------------------------------------
	{
		// Test pure X rotation (+90 and -90)
		const FIntVector PosTop(0, 0, 1);
		const FIntVector PosRotatedX = ARubiksCube::RotateGridPosition(PosTop, EAxis::X, 1);
		TestEqual(TEXT("Rotate (0,0,1) around X by +90 should be (0,-1,0)"), PosRotatedX, FIntVector(0, -1, 0));

		const FIntVector PosRevertedX = ARubiksCube::RotateGridPosition(PosRotatedX, EAxis::X, -1);
		TestEqual(TEXT("Rotate back around X by -90 should restore (0,0,1)"), PosRevertedX, PosTop);

		// Test pure Y rotation (+90 and -90)
		const FIntVector PosRotatedY = ARubiksCube::RotateGridPosition(PosTop, EAxis::Y, 1);
		TestEqual(TEXT("Rotate (0,0,1) around Y by +90 should be (1,0,0)"), PosRotatedY, FIntVector(1, 0, 0));

		const FIntVector PosRevertedY = ARubiksCube::RotateGridPosition(PosRotatedY, EAxis::Y, -1);
		TestEqual(TEXT("Rotate back around Y by -90 should restore (0,0,1)"), PosRevertedY, PosTop);

		// Test pure Z rotation (+90 and -90)
		const FIntVector PosFront(0, 1, 0);
		const FIntVector PosRotatedZ = ARubiksCube::RotateGridPosition(PosFront, EAxis::Z, 1);
		TestEqual(TEXT("Rotate (0,1,0) around Z by +90 should be (-1,0,0)"), PosRotatedZ, FIntVector(-1, 0, 0));

		const FIntVector PosRevertedZ = ARubiksCube::RotateGridPosition(PosRotatedZ, EAxis::Z, -1);
		TestEqual(TEXT("Rotate back around Z by -90 should restore (0,1,0)"), PosRevertedZ, PosFront);

		// 4x rotation around any axis must be identity
		FIntVector Arbitrary(1, -1, 1);
		FIntVector Cur = Arbitrary;
		for (int32 i = 0; i < 4; ++i)
		{
			Cur = ARubiksCube::RotateGridPosition(Cur, EAxis::X, 1);
		}
		TestEqual(TEXT("4x X rotations return to identity"), Cur, Arbitrary);

		Cur = Arbitrary;
		for (int32 i = 0; i < 4; ++i)
		{
			Cur = ARubiksCube::RotateGridPosition(Cur, EAxis::Y, 1);
		}
		TestEqual(TEXT("4x Y rotations return to identity"), Cur, Arbitrary);

		Cur = Arbitrary;
		for (int32 i = 0; i < 4; ++i)
		{
			Cur = ARubiksCube::RotateGridPosition(Cur, EAxis::Z, 1);
		}
		TestEqual(TEXT("4x Z rotations return to identity"), Cur, Arbitrary);
	}

	// -------------------------------------------------------------
	// STAGE 1, 3, 4, 5: Actor & Cubelet Full System Integration
	// -------------------------------------------------------------
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Created test game world"), World))
	{
		return false;
	}

	FActorSpawnParameters SpawnParams;
	ARubiksCube* Cube = World->SpawnActor<ARubiksCube>(ARubiksCube::StaticClass(), SpawnParams);
	if (!TestNotNull(TEXT("Spawned ARubiksCube actor"), Cube))
	{
		World->DestroyWorld(false);
		return false;
	}

	// 1. Validate Cubelet counts & Sticker counts
	TestEqual(TEXT("Cube must spawn exactly 27 cubelets"), Cube->Cubelets.Num(), 27);

	int32 TotalStickers = 0;
	int32 CornersCount = 0;
	int32 EdgesCount = 0;
	int32 CentersCount = 0;
	int32 CoreCount = 0;

	for (const URubiksCubeletComponent* Cubelet : Cube->Cubelets)
	{
		const int32 Count = Cubelet->GetStickerCount();
		TotalStickers += Count;
		if (Count == 3) CornersCount++;
		else if (Count == 2) EdgesCount++;
		else if (Count == 1) CentersCount++;
		else if (Count == 0) CoreCount++;
	}

	TestEqual(TEXT("Total stickers across 6 faces must be 54"), TotalStickers, 54);
	TestEqual(TEXT("Corner cubelets count (3 stickers each)"), CornersCount, 8);
	TestEqual(TEXT("Edge cubelets count (2 stickers each)"), EdgesCount, 12);
	TestEqual(TEXT("Center cubelets count (1 sticker each)"), CentersCount, 6);
	TestEqual(TEXT("Core internal cubelet count (0 stickers)"), CoreCount, 1);

	// 2. Validate Initial Solved State
	TestTrue(TEXT("Cube is initially solved"), Cube->IsSolved());

	// 3. Stage 3 (First Move): R move and R' move
	Cube->ExecuteStandardMove(ERubiksMove::R);
	// In the test, run self-test verification via the internal instant applicator or tick
	Cube->RunSelfTest();
	TestTrue(TEXT("Self test completed successfully"), Cube->IsSolved());

	// 4. Test Reset
	Cube->Scramble(20, true);
	TestFalse(TEXT("Scrambled cube is not solved"), Cube->IsSolved());
	Cube->ResetCube();
	TestTrue(TEXT("ResetCube restores 100% solved state"), Cube->IsSolved());
	TestEqual(TEXT("ResetCube clears MoveCounter"), Cube->MoveCounter, 0);

	// Timeline must move visuals before committing integer state.
	Cube->RotationDuration = 1.f;
	for (EAxis::Type Axis : {EAxis::X, EAxis::Y, EAxis::Z})
	{
		Cube->ResetCube();
		const FIntVector Initial(1, 1, 1);
		URubiksCubeletComponent* Corner = nullptr;
		for (URubiksCubeletComponent* Candidate : Cube->Cubelets)
			if (Candidate->GetGridPosition() == Initial) Corner = Candidate;
		if (!TestNotNull(TEXT("Corner exists"), Corner)) continue;
		const FVector Start = Corner->GetComponentLocation();
		Cube->ExecuteMove(FRubiksMoveDefinition(Axis, 1, 1));
		Cube->Tick(0.5f);
		TestTrue(TEXT("Half turn still animating"), Cube->IsRotating());
		TestEqual(TEXT("Logical position is unchanged until completion"), Corner->GetGridPosition(), Initial);
		TestFalse(TEXT("Visual position moves at midpoint"), Corner->GetComponentLocation().Equals(Start));
		Cube->Tick(0.5f);
		TestFalse(TEXT("Full turn completes"), Cube->IsRotating());
		TestEqual(TEXT("Final integer position"), Corner->GetGridPosition(), ARubiksCube::RotateGridPosition(Initial, Axis, 1));
		TestEqual(TEXT("One completed move recorded"), Cube->MoveHistory.Num(), 1);
	}
	Cube->ResetCube();
	Cube->Move_R();
	Cube->Move_U();
	TestEqual(TEXT("Second move queues"), Cube->MoveQueue.Num(), 1);
	Cube->Tick(1.f);
	TestTrue(TEXT("Second move starts its own timeline"), Cube->IsRotating());
	TestEqual(TEXT("Only first move committed"), Cube->MoveHistory.Num(), 1);
	Cube->Tick(1.f);
	TestTrue(TEXT("First undo accepted"), Cube->Undo());
	Cube->Tick(1.f);
	TestEqual(TEXT("Undo removes latest history entry"), Cube->MoveHistory.Num(), 1);
	TestTrue(TEXT("Second undo accepted"), Cube->Undo());
	Cube->Tick(1.f);
	TestTrue(TEXT("Repeated undo restores solved cube"), Cube->IsSolved());
	TestEqual(TEXT("Undo counter returns to zero"), Cube->MoveCounter, 0);
	TestFalse(TEXT("Empty history cannot undo"), Cube->Undo());
	Cube->Move_R();
	Cube->Move_U();
	Cube->Tick(0.5f);
	Cube->ResetCube();
	Cube->Tick(2.f);
	TestTrue(TEXT("Reset cancels active timeline and queue"), Cube->IsSolved());
	TestEqual(TEXT("Cancelled moves never enter history"), Cube->MoveHistory.Num(), 0);
	Cube->ExecuteMove(FRubiksMoveDefinition(EAxis::Z, 1, 2));
	TestFalse(TEXT("Invalid move rejected"), Cube->IsRotating());

	// Whole-cube presentation must face the camera without applying a puzzle move.
	Cube->FaceTransitionDuration = 1.f;
	Cube->ResetCamera();
	Cube->Tick(1.f);
	const FTransform CameraStart = Cube->CameraBoom->GetComponentTransform();
	Cube->SetActorRotation(FRotator(0.f, 31.f, 0.f));
	for (EAxis::Type Axis : {EAxis::X, EAxis::Y, EAxis::Z})
	{
		for (int32 Layer : {-1, 1})
		{
			const FQuat Start = Cube->CubePresentation->GetComponentQuat();
			Cube->SetSelection(Axis, Layer);
			TestTrue(TEXT("Selecting a face does not snap its visual rotation"), Cube->CubePresentation->GetComponentQuat().Equals(Start));
			Cube->Tick(.5f);
			TestFalse(TEXT("Face rotation moves smoothly at midpoint"), Cube->CubePresentation->GetComponentQuat().Equals(Start));
			Cube->Tick(.5f);
			const FVector Normal = (Axis == EAxis::X ? FVector::ForwardVector : Axis == EAxis::Y ? FVector::RightVector : FVector::UpVector) * Layer;
			const FVector WorldNormal = Cube->CubePresentation->GetComponentQuat().RotateVector(Normal);
			const FVector ExpectedNormal = -Cube->FollowCamera->GetComponentQuat().RotateVector(FRotator(Cube->FaceViewPitch, Cube->FaceViewYaw, 0.f).Vector());
			TestTrue(TEXT("Selected signed axis face keeps the configured viewing angle"), FVector::DotProduct(WorldNormal, ExpectedNormal) > .9999f);
			TestTrue(TEXT("Selected face remains prominent toward camera"), FVector::DotProduct(WorldNormal, -Cube->FollowCamera->GetForwardVector()) > .85f);
			TestTrue(TEXT("Presentation rotation preserves solved state"), Cube->IsSolved());
			TestEqual(TEXT("Presentation rotation adds no moves"), Cube->MoveCounter, 0);
		}
	}
	Cube->SetActorRotation(FRotator::ZeroRotator);
	TestTrue(TEXT("Camera stays fixed relative to actor"), Cube->CameraBoom->GetComponentTransform().Equals(CameraStart));
	Cube->SetSelection(EAxis::X, 1);
	Cube->Tick(.25f);
	const FQuat Interrupted = Cube->CubePresentation->GetRelativeRotation().Quaternion();
	Cube->SetSelection(EAxis::Y, 1);
	TestTrue(TEXT("Rapid axis changes continue from current orientation"), Cube->CubePresentation->GetRelativeRotation().Quaternion().Equals(Interrupted));
	Cube->ExecuteMove(FRubiksMoveDefinition(EAxis::Y, 1, 1));
	Cube->Tick(1.f);
	TestEqual(TEXT("Slice turn during face transition records one move"), Cube->MoveCounter, 1);
	TestTrue(TEXT("Undo works after presentation rotation"), Cube->Undo());
	Cube->Tick(1.f);
	TestTrue(TEXT("Undo restores solved state after concurrent animations"), Cube->IsSolved());

	for (EAxis::Type Axis : {EAxis::X, EAxis::Y, EAxis::Z})
	{
		for (int32 Layer : {-1, 0, 1})
		{
			Cube->SetSelection(Axis, Layer);
			const TArray<URubiksCubeletComponent*> Selected = Cube->GetLayerCubelets(Axis, Layer);
			int32 HighlightCount = 0;
			for (URubiksCubeletComponent* Cubelet : Cube->Cubelets)
			{
				HighlightCount += Cubelet->IsSelectionHighlighted() ? 1 : 0;
				TestEqual(TEXT("Highlight follows logical selected slice"), Cubelet->IsSelectionHighlighted(), Selected.Contains(Cubelet));
			}
			TestEqual(TEXT("Exactly nine cubelets use the selection material"), HighlightCount, 9);
		}
	}
	Cube->SetSelection(EAxis::Z, 1);
	Cube->Tick(1.f);
	const FQuat PresentationBeforeInspect = Cube->CubePresentation->GetComponentQuat();
	const FQuat CameraBeforeInspect = Cube->CameraBoom->GetRelativeRotation().Quaternion();
	Cube->ToggleInteractionMode();
	TestTrue(TEXT("Toggle enters Inspect mode"), Cube->InteractionMode == ERubiksInteractionMode::Inspect);
	TestTrue(TEXT("Inspect entry starts without snapping the camera"), Cube->CameraBoom->GetRelativeRotation().Quaternion().Equals(CameraBeforeInspect));
	Cube->Tick(.5f);
	TestFalse(TEXT("Inspect entry provides a visible smooth orbit cue"), Cube->CameraBoom->GetRelativeRotation().Quaternion().Equals(CameraBeforeInspect));
	TestTrue(TEXT("Inspect cue remains in progress at midpoint"), Cube->IsViewTransitioning());
	Cube->OrbitCamera(65.f, 18.f);
	const FQuat ManualOrbit = Cube->CameraBoom->GetRelativeRotation().Quaternion();
	Cube->SetSelection(EAxis::X, -1);
	Cube->Tick(1.f);
	TestTrue(TEXT("Manual orbit cancels Inspect entry motion"), Cube->CameraBoom->GetRelativeRotation().Quaternion().Equals(ManualOrbit));
	TestTrue(TEXT("Inspect orbit and selection do not turn the puzzle presentation"), Cube->CubePresentation->GetComponentQuat().Equals(PresentationBeforeInspect));
	Cube->RotateRight();
	TestTrue(TEXT("Keyboard turns remain available in Inspect"), Cube->IsRotating());
	Cube->Tick(1.f);
	Cube->Undo();
	Cube->Tick(1.f);
	Cube->ZoomCamera(10000.f);
	TestEqual(TEXT("Zoom has an outer distance limit"), Cube->CameraBoom->TargetArmLength, 2000.f);
	Cube->ZoomCamera(-10000.f);
	TestEqual(TEXT("Zoom cannot enter the cube"), Cube->CameraBoom->TargetArmLength, 400.f);
	const FQuat CameraBeforeReturn = Cube->CameraBoom->GetRelativeRotation().Quaternion();
	Cube->ToggleInteractionMode();
	TestTrue(TEXT("Return to Play does not snap the orbit camera"), Cube->CameraBoom->GetRelativeRotation().Quaternion().Equals(CameraBeforeReturn));
	Cube->Tick(.5f);
	TestTrue(TEXT("Camera return is still animating at midpoint"), Cube->IsViewTransitioning());
	TestFalse(TEXT("Camera return moves smoothly"), Cube->CameraBoom->GetRelativeRotation().Quaternion().Equals(CameraBeforeReturn));
	Cube->Tick(.5f);
	TestFalse(TEXT("Camera settles when returning to Play"), Cube->IsViewTransitioning());
	TestEqual(TEXT("Play restores configured camera distance"), Cube->CameraBoom->TargetArmLength, Cube->ViewingDistance);
	TestTrue(TEXT("Inspect and return preserve solved state"), Cube->IsSolved());
	TestEqual(TEXT("Inspect never changes move history"), Cube->MoveHistory.Num(), 0);
	const FVector ReturnedNormal = Cube->CubePresentation->GetComponentQuat().RotateVector(-FVector::ForwardVector);
	TestTrue(TEXT("Play return faces the latest selected negative axis"), FVector::DotProduct(ReturnedNormal, -Cube->FollowCamera->GetForwardVector()) > .9f);

	// Scramble overview preserves both camera modes, including custom Inspect views.
	for (ERubiksInteractionMode Mode : {ERubiksInteractionMode::Play, ERubiksInteractionMode::Inspect})
	{
		Cube->ResetCube();
		Cube->SetInteractionMode(Mode);
		Cube->Tick(1.f);
		if (Mode == ERubiksInteractionMode::Inspect) { Cube->OrbitCamera(23.f, 8.f); Cube->ZoomCamera(100.f); }
		const FQuat BeforeScramble = Cube->CameraBoom->GetRelativeRotation().Quaternion();
		const FQuat BeforePresentation = Cube->CubePresentation->GetRelativeRotation().Quaternion();
		const float BeforeDistance = Cube->CameraBoom->TargetArmLength;
		Cube->ScrambleRotationDuration = .5f;
		Cube->Scramble(5, false);
		TestTrue(TEXT("Scramble camera starts without snapping"), Cube->CameraBoom->GetRelativeRotation().Quaternion().Equals(BeforeScramble));
		Cube->Tick(.25f);
		TestTrue(TEXT("Scramble smoothly pulls back"), Cube->CameraBoom->TargetArmLength > BeforeDistance);
		TestFalse(TEXT("Scramble adds a three-dimensional orbit angle"), Cube->CameraBoom->GetRelativeRotation().Quaternion().Equals(BeforeScramble));
		TestTrue(TEXT("Scramble does not reorient the puzzle presentation"), Cube->CubePresentation->GetRelativeRotation().Quaternion().Equals(BeforePresentation));
		const int32 Pending = Cube->MoveQueue.Num();
		Cube->Scramble(20, false);
		Cube->RotateRight();
		TestEqual(TEXT("Repeated scramble and turn input do not extend playback"), Cube->MoveQueue.Num(), Pending);
		Cube->ToggleInteractionMode();
		TestTrue(TEXT("Scramble retains the original mode"), Cube->InteractionMode == Mode);
		for (int32 Step = 0; Step < 40; ++Step) Cube->Tick(.1f);
		TestFalse(TEXT("Scramble and camera restoration complete"), Cube->IsRotating() || Cube->IsViewTransitioning());
		TestTrue(TEXT("Scramble restores the prior camera angle"), Cube->CameraBoom->GetRelativeRotation().Quaternion().Equals(BeforeScramble, .0001f));
		TestEqual(TEXT("Scramble restores the prior distance"), Cube->CameraBoom->TargetArmLength, BeforeDistance);
		TestEqual(TEXT("Only scramble moves enter history"), Cube->MoveHistory.Num(), 5);
		Cube->Scramble(5, false);
		Cube->Tick(.25f);
		Cube->ResetCube();
		Cube->Tick(1.f);
		TestTrue(TEXT("Reset cancels scramble and restores its camera"), Cube->IsSolved() && Cube->CameraBoom->GetRelativeRotation().Quaternion().Equals(BeforeScramble, .0001f));
		TestEqual(TEXT("Cancelled scramble restores distance"), Cube->CameraBoom->TargetArmLength, BeforeDistance);
		Cube->Scramble(5, true);
		TestFalse(TEXT("Instant scramble does not trigger camera motion"), Cube->IsViewTransitioning());
	}

	// Drag directions are derived from the grabbed face, even when the cube is arbitrarily oriented.
	for (const FQuat Presentation : {FQuat::Identity, FRotator(19.f, 37.f, -12.f).Quaternion()})
	{
		for (EAxis::Type FaceAxis : {EAxis::X, EAxis::Y, EAxis::Z})
		{
			for (int32 Side : {-1, 1})
			{
				const FVector Normal = (FaceAxis == EAxis::X ? FVector::ForwardVector : FaceAxis == EAxis::Y ? FVector::RightVector : FVector::UpVector) * Side;
				const FVector Up = FaceAxis == EAxis::Z ? FVector::RightVector : FVector::UpVector;
				const FQuat Camera = Presentation * FRotationMatrix::MakeFromXZ(-Normal, Up).ToQuat();
				for (int32 Row = -1; Row <= 1; ++Row)
				for (int32 Column = -1; Column <= 1; ++Column)
				{
					const FIntVector Position = FaceAxis == EAxis::X ? FIntVector(Side, Row, Column) : FaceAxis == EAxis::Y ? FIntVector(Row, Side, Column) : FIntVector(Row, Column, Side);
					for (EAxis::Type MoveAxis : {EAxis::X, EAxis::Y, EAxis::Z})
					{
						if (MoveAxis == FaceAxis) continue;
						const FVector Axis = MoveAxis == EAxis::X ? FVector::ForwardVector : MoveAxis == EAxis::Y ? FVector::RightVector : FVector::UpVector;
						const FVector Tangent = Camera.UnrotateVector(Presentation.RotateVector(FVector::CrossProduct(Axis, FVector(Position) + Normal * .5f)));
						for (int32 Direction : {-1, 1})
						{
							FRubiksMoveDefinition Move;
							const FVector2D Drag = FVector2D(Tangent.Y, -Tangent.Z).GetSafeNormal() * Direction * 80.f;
							TestTrue(TEXT("A face-aligned drag resolves a move"), ARubiksCube::ResolveStickerDrag(Position, Normal, Presentation, Camera, Drag, Move));
							TestEqual(TEXT("Drag chooses the correct axis"), Move.Axis.GetValue(), MoveAxis);
							TestEqual(TEXT("Drag chooses the grabbed row including middle slices"), Move.Layer, MoveAxis == EAxis::X ? Position.X : MoveAxis == EAxis::Y ? Position.Y : Position.Z);
							TestEqual(TEXT("Reversing the drag reverses the turn"), Move.Direction, Direction);
						}
					}
				}
			}
		}
	}
	Cube->ResetCube();
	Cube->SetInteractionMode(ERubiksInteractionMode::Play);
	Cube->SetSelection(EAxis::X, 1);
	Cube->Tick(1.f);
	URubiksCubeletComponent* Grabbed = nullptr;
	for (URubiksCubeletComponent* Cubelet : Cube->Cubelets)
		if (Cubelet->GetGridPosition() == FIntVector(1, 0, 0)) Grabbed = Cubelet;
	const FQuat Presentation = Cube->CubePresentation->GetComponentQuat();
	const FVector WorldNormal = Presentation.RotateVector(FVector::ForwardVector);
	const FVector Tangent = Cube->CameraBoom->GetComponentQuat().UnrotateVector(Presentation.RotateVector(FVector::CrossProduct(FVector::RightVector, FVector(1.5f, 0, 0))));
	const FVector2D DragDirection = FVector2D(Tangent.Y, -Tangent.Z).GetSafeNormal();
	TestTrue(TEXT("A visible sticker accepts a gesture in Play"), Cube->BeginStickerGesture(Grabbed, WorldNormal));
	Cube->UpdateStickerGesture(DragDirection * 14.f);
	TestTrue(TEXT("Short intentional drag shows its direction arrow"), Cube->MovePreviewTip->IsVisible());
	TestEqual(TEXT("Preview highlights the inferred middle row"), Cube->SelectedLayer, 0);
	TestEqual(TEXT("Preview does not add moves"), Cube->MoveCounter, 0);
	TestTrue(TEXT("Preview does not alter puzzle mathematics"), Cube->IsSolved());
	TestFalse(TEXT("A short release cancels rather than turning"), Cube->FinishStickerGesture());
	TestEqual(TEXT("Cancellation restores the previous selection"), Cube->SelectedLayer, 1);
	TestFalse(TEXT("Cancellation hides the arrow"), Cube->MovePreviewTip->IsVisible());
	TestTrue(TEXT("Next gesture starts normally"), Cube->BeginStickerGesture(Grabbed, WorldNormal));
	Cube->UpdateStickerGesture(DragDirection * 80.f);
	TestTrue(TEXT("Deliberate release commits a quarter turn"), Cube->FinishStickerGesture());
	TestFalse(TEXT("Gestures cannot begin during a turn"), Cube->BeginStickerGesture(Grabbed, WorldNormal));
	Cube->Tick(1.f);
	TestEqual(TEXT("Drag move records one history entry"), Cube->MoveHistory.Num(), 1);
	TestTrue(TEXT("Direct manipulation leaves the view fixed"), Cube->CubePresentation->GetComponentQuat().Equals(Presentation));
	TestTrue(TEXT("Drag move is undoable"), Cube->Undo());
	Cube->Tick(1.f);
	TestTrue(TEXT("Undo restores a drag turn"), Cube->IsSolved());
	Cube->SetInteractionMode(ERubiksInteractionMode::Inspect);
	Cube->Tick(1.f);
	TestTrue(TEXT("Sticker turns are also available in Inspect"), Cube->BeginStickerGesture(Grabbed, WorldNormal));
	Cube->UpdateStickerGesture(DragDirection * 80.f);
	Cube->UpdateStickerGesture(FVector2D::ZeroVector);
	TestFalse(TEXT("Returning to the gesture origin cancels a prepared turn"), Cube->FinishStickerGesture());
	TestTrue(TEXT("Reset test starts an active preview"), Cube->BeginStickerGesture(Grabbed, WorldNormal));
	Cube->UpdateStickerGesture(DragDirection * 80.f);
	Cube->ResetCube();
	TestFalse(TEXT("Reset cancels gesture and arrow"), Cube->FinishStickerGesture() || Cube->MovePreviewTip->IsVisible());

	// A stationary click selects the grabbed face, without turning or moving the view.
	for (EAxis::Type FaceAxis : {EAxis::X, EAxis::Y, EAxis::Z})
	for (int32 Side : {-1, 1})
	{
		Cube->SetSelection(EAxis::Z, 0);
		const FVector Normal = (FaceAxis == EAxis::X ? FVector::ForwardVector : FaceAxis == EAxis::Y ? FVector::RightVector : FVector::UpVector) * Side;
		URubiksCubeletComponent* FaceCenter = nullptr;
		for (URubiksCubeletComponent* Cubelet : Cube->Cubelets)
			if (Cubelet->GetGridPosition() == FIntVector(Normal)) FaceCenter = Cubelet;
		const FQuat BeforeClick = Cube->CubePresentation->GetComponentQuat();
		TestTrue(TEXT("Face click begins on a sticker"), Cube->BeginStickerGesture(FaceCenter, BeforeClick.RotateVector(Normal)));
		Cube->UpdateStickerGesture(FVector2D(2.f, 1.f));
		TestFalse(TEXT("Click selection does not execute a move"), Cube->FinishStickerGesture());
		TestEqual(TEXT("Click selects the hit face axis"), Cube->SelectedAxis.GetValue(), FaceAxis);
		TestEqual(TEXT("Click selects the signed outer layer"), Cube->SelectedLayer, Side);
		TestTrue(TEXT("Click keeps inspection pose fixed"), Cube->CubePresentation->GetComponentQuat().Equals(BeforeClick));
		TestTrue(TEXT("Click keeps puzzle solved"), Cube->IsSolved());
		TestEqual(TEXT("Click does not record history"), Cube->MoveHistory.Num(), 0);
		int32 Count = 0;
		for (URubiksCubeletComponent* Cubelet : Cube->Cubelets) Count += Cubelet->IsSelectionHighlighted() ? 1 : 0;
		TestEqual(TEXT("Click highlights exactly one nine-cubelet layer"), Count, 9);
	}
	Cube->SetSelection(EAxis::Z, 0);
	TestTrue(TEXT("Cancellation test starts on a face"), Cube->BeginStickerGesture(Grabbed, WorldNormal));
	Cube->UpdateStickerGesture(DragDirection * 80.f);
	Cube->UpdateStickerGesture(FVector2D::ZeroVector);
	Cube->FinishStickerGesture();
	TestEqual(TEXT("Dragging out and back never becomes a click selection"), Cube->SelectedAxis.GetValue(), EAxis::Z);
	TestEqual(TEXT("Dragging out and back restores the prior layer"), Cube->SelectedLayer, 0);
	TestTrue(TEXT("Escape test starts a gesture"), Cube->BeginStickerGesture(Grabbed, WorldNormal));
	Cube->FinishStickerGesture(true);
	TestEqual(TEXT("Explicit cancellation does not select a face"), Cube->SelectedLayer, 0);

	// Clean up
	World->DestroyWorld(false);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
