# RubiksCube

An interactive 3×3×3 Rubik's Cube built with Unreal Engine 5.8 and C++.

The project includes animated slice turns, direct mouse manipulation, layer highlights, a curved move-preview arrow, orbit inspection, scrambling, reset, move history, and undo. Integer grid coordinates keep puzzle mechanics independent of camera and whole-cube presentation.

## Getting started

1. Install Unreal Engine **5.8** and Visual Studio with the **Game development with C++** workload and a Windows SDK. `.vsconfig` lists the development components used by this project.
2. Clone this repository.
3. Right-click `RubiksCube.uproject` and generate Visual Studio project files.
4. Build **RubiksCubeEditor / Development Editor / Win64**, then open the project.
5. Open `Content/Maps/CubeStudio` and press **Play**.

The project descriptor currently enables `ModelContextProtocol`, `EditorToolset`, and `Terminal` editor plugins. Install these plugins if your engine distribution does not include them, or disable their entries in `RubiksCube.uproject` if you only need to run the cube. They are not included in this repository.

Unreal regenerates `Binaries`, `Intermediate`, `DerivedDataCache`, and `Saved` locally. These folders and generated IDE solution files are excluded from version control. The checked-in assets are small and stored directly in Git; Git LFS is not required for this snapshot.

## Controls

| Input | Action |
| --- | --- |
| Left-click a sticker | Select and highlight that face's outer layer |
| Left-drag a sticker, then release | Preview and turn a layer by 90° |
| Right-click and drag | Orbit around the cube |
| Mouse wheel | Zoom |
| Ctrl+Z or Z | Undo |
| Space | Scramble |
| R | Reset the puzzle |
| F | Restore the selected-face camera |
| V | Toggle Play / Inspect |
| Tab | Cycle the selected axis |
| Up / Down | Select the next / previous layer |
| Left / Right | Turn the selected layer |
| Escape | Cancel a move preview; in the editor it can also stop Play |

A short drag or dragging back to the starting point cancels the turn. The highlighted layer and cyan arrow show the pending move before release. Scrambling temporarily moves the camera to a wider view, then restores the previous camera angle and distance.

## Project layout

- `Source/RubiksCube/Actors`: cube interaction, presentation, camera, and gameplay.
- `Source/RubiksCube/Components`: cubelet geometry, stickers, and highlights.
- `Source/RubiksCube/Data`: move definitions and enums.
- `Source/RubiksCube/Tests`: automation regression coverage.
- `Content/CubeStudio`: Blueprint, input actions, materials, and UI assets.
- `Content/Maps/CubeStudio.umap`: demonstration scene.

See [TIMELINE_AND_INPUT_SETUP.md](TIMELINE_AND_INPUT_SETUP.md) for implementation and input configuration details, and [RubiksCube_Unreal_Cpp_Mechanics.md](RubiksCube_Unreal_Cpp_Mechanics.md) for mechanics notes.

## Verification

Build target: `RubiksCubeEditor Win64 Development`.

Run `RubiksCube.Stages1To5.MathematicsAndMechanics` from the Unreal Session Frontend automation panel. Tests cover coordinate rotation, animated turns, queueing, undo, reset, face presentation, selected-layer highlights, camera restoration, scramble behavior, drag selection and cancellation, and click selection.

Local device credentials, including the Android file-server security token, belong in ignored local settings and are not published.
