# Smooth turns and input setup

Each normal move now uses a C++ FTimeline and rotates the temporary pivot continuously. Exact grid snapping happens only after the animation completes. No Blueprint Timeline node is required.

## Blueprint configuration

1. Create a Blueprint derived from RubiksCube and place it in your level.
2. Set RotationDuration to 0.6 seconds and ScrambleRotationDuration to 0.3 seconds. Existing Blueprint instances may retain old overrides: reset these properties or edit them explicitly.
3. Leave RotationCurve unset for smooth ease-in/ease-out. Alternatively assign a CurveFloat with time 0..1 and value 0..1, starting at (0,0) and ending at (1,1).
4. Call RotateLeft/RotateRight or ExecuteStandardMove from Blueprint buttons. Each call animates and queues safely; do not call cubelet SnapTransform or SetGridPosition during playback.
5. Scramble(20, false) animates. InstantScramble, Scramble(..., true), ResetCube and RunSelfTest intentionally apply immediate state changes.
6. Editor preview requires viewport Realtime enabled. Play In Editor is the recommended way to preview turns.

## Enhanced Input

`/Game/CubeStudio/Blueprints/BP_RubiksCube` derives from the C++ RubiksCube actor and is placed in `/Game/Maps/CubeStudio`. It replaces the native cube at the same transform and preserves its face materials and animation durations.

Eight Digital/Bool Input Actions and `IMC_RubiksCube` are saved under `/Game/CubeStudio/Input`. The context and all eight IA_* fields are assigned in the Blueprint defaults. BeginPlay inherits the parent's context registration and Started bindings; keys are defined only in the mapping context.

| Action | Key |
| --- | --- |
| IA_RotateLeft | Left Arrow |
| IA_RotateRight | Right Arrow |
| IA_SelectNextLayer | Up Arrow |
| IA_SelectPrevLayer | Down Arrow |
| IA_CycleAxis | Tab |
| IA_Undo | Z |
| IA_Reset | R |
| IA_Scramble | Space |

Change keys in `IMC_RubiksCube` without editing C++ or the Blueprint graph.

SelectedAxis and SelectedLayer are exposed to Blueprint. SetSelection validates their values. `/Game/CubeStudio/Materials/M_SelectedLayerHighlight` was authored through Unreal MCP. It is a masked, unlit material with a thin cyan edge mask and configurable HighlightColor/GlowStrength parameters. Nine slightly expanded cubelet shells use the material; centers are transparent so sticker colors remain readable. Shells follow the physical cubelets during turns, pulse briefly when a move starts, and refresh from logical coordinates at completion. No post-process or custom-depth configuration is required.

## Direct mouse controls

The cursor is visible in Play and Inspect. No mode switch is needed for normal mouse interaction.

| Action | Control |
| --- | --- |
| Select a face layer | Single left-click on a sticker |
| Turn a slice | Left-click a sticker and drag, then release |
| Orbit the cube | Hold right mouse and drag anywhere |
| Zoom | Mouse wheel |
| Undo | Ctrl+Z (Z also retained) |
| Scramble | Space |
| Reset puzzle | R |
| Restore selected-face camera | F |
| Optional Play/Inspect toggle | V |
| Optional axis/layer selection | Tab / Up / Down |
| Optional selected slice turn | Left / Right arrows |

A sticker drag infers the axis, row and turn direction from the grabbed face and camera orientation. At 10 pixels of clear motion it previews the affected nine cubelets using M_SelectedLayerHighlight and shows a cyan 3D direction arrow. Releasing after TurnDragThreshold (default 28 pixels) commits one smooth 90-degree turn. A short drag, returning to the starting point, an ambiguous diagonal, or Escape cancels without changing history and restores the previous highlight. Arrow meshes have no collision. Cube geometry and logical coordinates stay unchanged during preview.

Sticker gestures work in either mode, including middle layers and scrambled cubes. Background clicks do not turn anything. Right-drag cancels any sticker preview and enters Inspect directly, freezing presentation while orbiting the camera; release restores the cursor. The wheel works in either mode with 400–2000 cm limits. Inputs during scramble cannot start mouse gestures. F smoothly restores the configured 1050 cm, 20-degree overhead view and the currently selected face. V retains the small Inspect entry camera cue for optional keyboard use. Keyboard turns, undo and reset remain available in Inspect. Old corner-click turns have been removed.

Play initially activates the cube camera. CubePresentation rotates the puzzle for keyboard face selection, independently of puzzle mathematics; FaceTransitionDuration defaults to 0.6 seconds. Negative outer selection faces the opposite side; middle selection uses at least 30 degrees of overhead angle. FaceViewPitch and FaceViewYaw configure the view. OrbitSensitivity and ZoomStep configure mouse controls.

Space smoothly pulls back 220 cm and orbits 30 degrees sideways / 12 degrees upward during animated scramble. On completion it restores the previous angle and distance in the original mode. Repeated Space and mode toggles are ignored during playback; ResetCube cancels playback and restores the view. Instant scramble does not animate the camera.

The native camera and pointer controls require no project widgets. OnStateChanged, OnMoveStarted, OnMoveCompleted and OnCubeSolved remain available for gameplay hooks. Turns use the existing timeline, integer mathematics, move counter, queue and undo history.

## Verification

Build target: RubiksCubeEditor Win64 Development. Automation test: RubiksCube.Stages1To5.MathematicsAndMechanics. Regression coverage includes six faces, all face rows, both drag directions, arbitrary whole-cube presentation, inferred middle layers, unchanged puzzle state during preview, short-release/origin cancellation, reset cancellation, a committed quarter turn, undo, camera restoration and scramble behavior.

Single clicks (under 10 pixels of total displacement from the gesture origin) select the signed outer layer of the hit face and highlight its nine cubelets, without changing the camera or puzzle state. A gesture that previously exceeded the preview threshold remains a cancelled drag when returned to its origin. The move arrow is a tapered 32-degree arc with a compact arrowhead outside the slice silhouette, using the unlit cyan M_MovePreviewArrow material authored through Unreal MCP.
