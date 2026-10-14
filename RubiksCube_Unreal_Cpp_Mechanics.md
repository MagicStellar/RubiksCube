# Unreal Engine C++ — 3×3 Rubik's Cube Mechanics

## Goal

Build a fully playable 3×3 Rubik's Cube in Unreal Engine using **C++ and basic Unreal components/transforms only**.

### Constraints

- No Animation Blueprint
- No Animation Graph
- No skeletal mesh
- No animation montages
- No physics simulation required
- Cubelets are separate `UStaticMeshComponent`s
- Layer rotations are performed by code using a temporary pivot
- Cube state is represented logically with integer grid coordinates
- Rotation visuals are driven by C++ interpolation
- Final transforms are snapped to exact 90° increments

---

# 1. Target Architecture

Start simple, then extract systems as the project becomes stable.

```text
ARubiksCube
│
├── RootComponent
│
├── Cubelets[27]
│   └── URubiksCubeletComponent
│
├── RotationPivot
│
├── UPROPERTY / state
│   ├── CubeSpacing
│   ├── RotationDuration
│   ├── CurrentMove
│   ├── MoveQueue
│   ├── MoveHistory
│   ├── SelectedAxis (EAxis::Type: X, Y, Z)
│   └── SelectedLayerIndex (-1, 0, +1)
│
└── Systems
    ├── Cube State / Coordinate Logic
    ├── Move System
    ├── Rotation System
    ├── Input System
    └── Solved Detection
```

Later, these can become separate classes:

```text
ARubiksCube
    │
    ├── URubiksCubeState
    ├── URubiksCubeMoveSystem
    ├── URubiksCubeRotationSystem
    └── URubiksCubeInputComponent
```

Do **not** over-engineer the first implementation. Get the cube working first.

---

# 2. Recommended Folder Structure

```text
Source/
└── YourProject/
    │
    ├── RubiksCube/
    │   ├── Actors/
    │   │   ├── RubiksCube.h
    │   │   └── RubiksCube.cpp
    │   │
    │   ├── Components/
    │   │   ├── RubiksCubeletComponent.h
    │   │   └── RubiksCubeletComponent.cpp
    │   │
    │   ├── Data/
    │   │   ├── RubiksCubeTypes.h
    │   │   └── RubiksCubeMove.h
    │   │
    │   └── Systems/
    │       ├── RubiksCubeState.h
    │       ├── RubiksCubeState.cpp
    │       ├── RubiksCubeMoveSystem.h
    │       ├── RubiksCubeMoveSystem.cpp
    │       ├── RubiksCubeRotationSystem.h
    │       └── RubiksCubeRotationSystem.cpp
    │
    └── ...
```

For the first milestone, it is acceptable to keep most logic inside `ARubiksCube` and extract it after the mechanics work.

---

# 3. Unreal Assets

Create:

```text
Content/
└── RubiksCube/
    ├── Meshes/
    │   └── SM_RubiksCubelet
    │
    ├── Materials/
    │   ├── M_RubiksWhite
    │   ├── M_RubiksYellow
    │   ├── M_RubiksRed
    │   ├── M_RubiksOrange
    │   ├── M_RubiksBlue
    │   └── M_RubiksGreen
    │
    ├── Input/
    │   ├── IMC_RubiksCube        (Input Mapping Context)
    │   ├── IA_RotateLeft         (Action: Rotate selected layer -90°)
    │   ├── IA_RotateRight        (Action: Rotate selected layer +90°)
    │   ├── IA_SelectNextLayer    (Action: Step to next layer/row +1)
    │   ├── IA_SelectPrevLayer    (Action: Step to prev layer/row -1)
    │   ├── IA_CycleAxis          (Action: Cycle axis X -> Y -> Z)
    │   ├── IA_Undo               (Action: Revert last move)
    │   ├── IA_Reset              (Action: Reset to solved state)
    │   └── IA_Scramble           (Action: Scramble cube)
    │
    └── Blueprints/
        └── BP_RubiksCube
```

`BP_RubiksCube` is used to configure exposed properties, assign meshes/materials, and assign the `IMC_RubiksCube` and `UInputAction` asset references. No keybindings are hardcoded in C++; all mappings remain completely customizable in the Editor.

---

# 4. Milestone 1 — Create One Cubelet

## Objective

Create a single cubelet as a reusable component.

Create:

```cpp
URubiksCubeletComponent
    : public UStaticMeshComponent
```

Responsibilities:

- Store its logical grid position
- Store its current logical orientation
- Hold the cubelet mesh
- Provide helper functions for snapping transforms

Suggested data:

```cpp
UCLASS(ClassGroup=(RubiksCube), meta=(BlueprintSpawnableComponent))
class YOURPROJECT_API URubiksCubeletComponent
    : public UStaticMeshComponent
{
    GENERATED_BODY()

public:

    void SetGridPosition(const FIntVector& InPosition);
    FIntVector GetGridPosition() const;

    void SnapTransform(float CubeSpacing);

private:

    UPROPERTY()
    FIntVector GridPosition;
};
```

Do not implement rotation mechanics yet.

---

# 5. Milestone 2 — Create ARubiksCube

Create:

```cpp
ARubiksCube
    : public AActor
```

Responsibilities:

- Own the 27 cubelets
- Spawn/initialize cubelets
- Own the temporary rotation pivot
- Start moves
- Finish moves
- Maintain move queue
- Maintain cube state

Basic structure:

```cpp
UCLASS()
class YOURPROJECT_API ARubiksCube : public AActor
{
    GENERATED_BODY()

public:

    ARubiksCube();

protected:

    virtual void BeginPlay() override;

public:

    virtual void Tick(float DeltaTime) override;

private:

    UPROPERTY()
    TObjectPtr<USceneComponent> Root;

    UPROPERTY()
    TObjectPtr<USceneComponent> RotationPivot;

    UPROPERTY()
    TArray<TObjectPtr<URubiksCubeletComponent>> Cubelets;
};
```

Enable ticking:

```cpp
PrimaryActorTick.bCanEverTick = true;
```

---

# 6. Milestone 3 — Spawn the 27 Cubelets

Use three nested loops:

```text
X = -1 → 1
Y = -1 → 1
Z = -1 → 1
```

This gives:

```text
3 × 3 × 3 = 27
```

Each cubelet gets:

```cpp
FIntVector(X, Y, Z)
```

and a world/local position:

```cpp
FVector(X, Y, Z) * CubeSpacing
```

Example:

```cpp
for (int32 X = -1; X <= 1; ++X)
{
    for (int32 Y = -1; Y <= 1; ++Y)
    {
        for (int32 Z = -1; Z <= 1; ++Z)
        {
            // Create cubelet
        }
    }
}
```

Use a configurable property:

```cpp
UPROPERTY(EditAnywhere, Category="Rubiks Cube")
float CubeSpacing = 105.f;
```

Adjust this according to the actual mesh dimensions.

---

# 7. Cubelet Coordinate Model

Every cubelet must have a logical coordinate.

```text
             Z+
             ↑

        (-1,?,1) (0,?,1) (1,?,1)

             ...

        (-1,?,-1)       (1,?,-1)

             → X+
```

The third axis is Y.

The important rule:

> Never use floating-point world position as the authoritative cube state.

Use:

```cpp
FIntVector GridPosition;
```

as the authoritative position.

The mesh transform is only the visual representation.

---

# 8. Milestone 4 — Add Cube Materials

For the first version, you can keep the cubelet visually simple.

Recommended approach:

### Option A — Colored face/sticker components

Each cubelet can contain small sticker meshes on exposed faces.

Example corner:

```text
Cubelet
├── Core
├── Sticker_Up
├── Sticker_Front
└── Sticker_Right
```

An edge:

```text
Cubelet
├── Core
├── Sticker_Up
└── Sticker_Right
```

An internal cubelet:

```text
Cubelet
└── Core
```

This is recommended for a proper Rubik's Cube.

### Do not create six visible stickers on every cubelet.

Only external faces need stickers.

---

# 9. Milestone 5 — Define Move Types

Create:

```cpp
enum class ERubiksMove : uint8
{
    U,
    UPrime,

    D,
    DPrime,

    L,
    LPrime,

    R,
    RPrime,

    F,
    FPrime,

    B,
    BPrime
};
```

Later you can add:

```text
X
Y
Z
```

for whole-cube rotations, but don't implement those initially.

---

# 10. Move Definition

Create:

```cpp
USTRUCT()
struct FRubiksMoveDefinition
{
    GENERATED_BODY()

    UPROPERTY()
    EAxis Axis;

    UPROPERTY()
    int32 Layer;

    UPROPERTY()
    int32 Direction;
};
```

Meaning:

```text
Axis
    X / Y / Z

Layer
    -1 / 0 / +1

Direction
    +1 / -1
```

Example:

```text
R
Axis      = X
Layer     = +1
Direction = +1
```

```text
R'
Axis      = X
Layer     = +1
Direction = -1
```

The exact direction convention should be established once and verified visually.

---

# 11. Milestone 6 — Select a Layer

Given a move:

```cpp
FRubiksMoveDefinition Definition;
```

find all cubelets belonging to that layer.

Example:

```text
R → GridPosition.X == +1

L → GridPosition.X == -1

U → GridPosition.Z == +1

D → GridPosition.Z == -1

F → GridPosition.Y == +1

B → GridPosition.Y == -1
```

This always returns:

```text
9 cubelets
```

Create:

```cpp
TArray<TObjectPtr<URubiksCubeletComponent>> GetLayerCubelets(
    const FRubiksMoveDefinition& Move);
```

---

# 12. Milestone 7 — Create the Rotation Pivot

Create one temporary:

```cpp
USceneComponent
```

called:

```text
RotationPivot
```

It should be attached to the RubiksCube root.

When rotating a layer:

```text
ARubiksCube
│
├── Cubelets
│
└── RotationPivot
      │
      ├── Cubelet
      ├── Cubelet
      ├── Cubelet
      ├── ...
      └── Cubelet
```

The pivot's location should be positioned at the center of the layer.

For example:

```text
R layer:

Pivot X = +CubeSpacing
Pivot Y = 0
Pivot Z = 0
```

---

# 13. Important Attachment Rule

Before the move:

```text
Cubelets
    ↓
RubiksCube Root
```

During the move:

```text
Cubelets
    ↓
RotationPivot
    ↓
RubiksCube Root
```

When the move completes:

```text
Cubelets
    ↓
RubiksCube Root
```

Use:

```cpp
AttachToComponent(...)
DetachFromComponent(...)
```

with appropriate transform rules.

Do not let the pivot become the permanent owner of cubelets.

---

# 14. Milestone 8 — Rotate the Pivot in C++

Do not use an Animation Blueprint.

Use:

```cpp
Tick()
```

with a rotation state.

Example state:

```cpp
bool bIsRotating = false;

float RotationElapsed = 0.f;
float RotationDuration = 0.20f;

float RotationStartAngle = 0.f;
float RotationTargetAngle = 90.f;

FVector RotationAxis;
```

During Tick:

```cpp
RotationElapsed += DeltaTime;

const float Alpha =
    FMath::Clamp(
        RotationElapsed / RotationDuration,
        0.f,
        1.f);
```

Use an easing function:

```cpp
const float EasedAlpha =
    FMath::InterpEaseInOut(
        0.f,
        1.f,
        Alpha,
        2.f);
```

Then:

```cpp
float Angle =
    FMath::Lerp(
        RotationStartAngle,
        RotationTargetAngle,
        EasedAlpha);
```

Apply:

```cpp
FQuat Rotation =
    FQuat(
        RotationAxis,
        FMath::DegreesToRadians(Angle));

RotationPivot->SetRelativeRotation(Rotation);
```

---

# 15. Milestone 9 — Finish the Rotation

When:

```cpp
Alpha >= 1.f
```

call:

```cpp
FinishRotation();
```

This function must:

1. Stop animation
2. Detach cubelets
3. Update logical coordinates
4. Update cubelet orientation
5. Snap transforms
6. Reset pivot rotation
7. Process next queued move

Order matters.

---

# 16. Logical Coordinate Rotation

This is the core mathematical part.

For rotation around Z:

### +90°

```cpp
NewX = -OldY;
NewY =  OldX;
NewZ =  OldZ;
```

### -90°

```cpp
NewX =  OldY;
NewY = -OldX;
NewZ =  OldZ;
```

For X and Y rotations, implement equivalent functions.

Create helpers:

```cpp
FIntVector RotateGridPositionX(
    const FIntVector& Position,
    int32 Direction);

FIntVector RotateGridPositionY(
    const FIntVector& Position,
    int32 Direction);

FIntVector RotateGridPositionZ(
    const FIntVector& Position,
    int32 Direction);
```

Only rotate the coordinate components perpendicular to the selected axis.

---

# 17. Why FIntVector Matters

Do not do this:

```cpp
FVector Position;
```

for cube logic.

Use:

```cpp
FIntVector Position;
```

because every cubelet is always exactly one of:

```text
-1
0
+1
```

This eliminates accumulated floating-point errors from the logical state.

---

# 18. Milestone 10 — Snap Cubelets

After the move:

```cpp
Cubelet->SetRelativeLocation(
    FVector(Cubelet->GetGridPosition()) * CubeSpacing);
```

The logical grid position determines the final position.

This means even if the visual interpolation had tiny floating-point errors, the cube returns to an exact grid.

---

# 19. Cubelet Orientation

Position is not enough.

A cubelet's colored stickers must rotate with it.

Therefore the cubelet also needs an orientation.

Initially:

```cpp
FRotator InitialRotation = FRotator::ZeroRotator;
```

After every move, update its orientation according to the layer rotation.

You can represent orientation as:

```cpp
FQuat
```

rather than repeatedly accumulating Euler angles.

Recommended:

```cpp
FQuat LogicalRotation;
```

When a layer rotates:

```cpp
LogicalRotation =
    LayerRotation * LogicalRotation;
```

Then normalize:

```cpp
LogicalRotation.Normalize();
```

This is preferable to repeatedly adding Pitch/Yaw/Roll.

---

# 20. Milestone 11 — Move Queue

Do not allow a new move to corrupt an existing rotation.

Create:

```cpp
TArray<ERubiksMove> MoveQueue;
```

Public API:

```cpp
void ExecuteMove(ERubiksMove Move);
```

Logic:

```text
ExecuteMove()
    │
    ├── Is rotating?
    │      │
    │      ├── YES → Queue move
    │      │
    │      └── NO → Start immediately
    │
    └── StartMove()
```

When a move finishes:

```text
FinishMove()
    ↓
Queue empty?
    ↓
NO
    ↓
Start next move
```

This lets the user rapidly enter:

```text
R U R' U'
```

without breaking the cube.

---

# 21. Milestone 12 — Move History

Create:

```cpp
TArray<ERubiksMove> MoveHistory;
```

After every completed move:

```cpp
MoveHistory.Add(Move);
```

Do not add the move when it starts.

Only add it after the rotation successfully completes.

---

# 22. Undo

Create:

```cpp
ERubiksMove GetInverseMove(ERubiksMove Move);
```

Mappings:

```text
U  ↔ U'
D  ↔ D'
L  ↔ L'
R  ↔ R'
F  ↔ F'
B  ↔ B'
```

Undo:

```text
Last move
    ↓
Get inverse
    ↓
Execute inverse
```

Example:

```text
History:
R U F

Undo

F'
```

---

# 23. Milestone 13 — Enhanced Input: Two-Key Row Rotation & Axis/Layer Selection

Rather than hardcoding keyboard keys in C++ or requiring the player to memorize 12 face notation keys (U, D, L, R, F, B + Shift), use **UE5 Enhanced Input** with a streamlined **Two-Key Row Rotation** scheme.

### Concept: Two Rotation Keys + Slice Selection

The player rotates the cube using two primary rotation actions:
1. **Rotate Left** (Counter-Clockwise / -90° around active axis)
2. **Rotate Right** (Clockwise / +90° around active axis)

Which row/slice turns is determined by the current selection state:
- **Active Axis**: `EAxis::X`, `EAxis::Y`, or `EAxis::Z` (default `EAxis::Z`)
- **Active Layer**: `-1` (Bottom / Left / Back), `0` (Middle), or `+1` (Top / Right / Front)

The player adjusts what row they want to manipulate before rotating:
- **Cycle Axis**: Toggles through axes: `Z -> X -> Y -> Z`
- **Select Next Layer**: Moves selection up/forward (`-1 -> 0 -> +1`)
- **Select Prev Layer**: Moves selection down/backward (`+1 -> 0 -> -1`)

```text
Current Selection: [ Axis: Z | Layer: +1 (Top Row) ]
Press "Rotate Left"  → Rotates Top Layer CCW (equivalent to U')
Press "Rotate Right" → Rotates Top Layer CW  (equivalent to U)

Change Layer to -1:
Current Selection: [ Axis: Z | Layer: -1 (Bottom Row) ]
Press "Rotate Right" → Rotates Bottom Layer CW (equivalent to D)

Change Axis to X:
Current Selection: [ Axis: X | Layer: +1 (Right Row) ]
Press "Rotate Right" → Rotates Right Layer CW (equivalent to R)
```

### No Hardcoded Keys in C++ (Enhanced Input Assets)

All bindings are defined in Unreal assets using **`UInputMappingContext`** and **`UInputAction`**. This keeps input completely remappable without touching C++.

Create the following Input Action assets in `Content/RubiksCube/Input/`:
- `IA_RotateLeft` (Digital Bool)
- `IA_RotateRight` (Digital Bool)
- `IA_SelectNextLayer` (Digital Bool)
- `IA_SelectPrevLayer` (Digital Bool)
- `IA_CycleAxis` (Digital Bool)
- `IA_Undo` (Digital Bool)
- `IA_Reset` (Digital Bool)
- `IA_Scramble` (Digital Bool)

Inside `IMC_RubiksCube`, assign your desired keys (e.g., A/D or Left/Right Arrow for rotate, W/S or Up/Down Arrow for layers, Tab/Space for axis toggle, Backspace for Undo, Enter for Reset).

### C++ Interface & Binding

Expose the input actions and mapping context as properties on `ARubiksCube` (or `URubiksCubeInputComponent`):

```cpp
UPROPERTY(EditDefaultsOnly, Category="Input|Context")
TObjectPtr<UInputMappingContext> RubiksMappingContext;

UPROPERTY(EditDefaultsOnly, Category="Input|Actions")
TObjectPtr<UInputAction> IA_RotateLeft;

UPROPERTY(EditDefaultsOnly, Category="Input|Actions")
TObjectPtr<UInputAction> IA_RotateRight;

UPROPERTY(EditDefaultsOnly, Category="Input|Actions")
TObjectPtr<UInputAction> IA_SelectNextLayer;

UPROPERTY(EditDefaultsOnly, Category="Input|Actions")
TObjectPtr<UInputAction> IA_SelectPrevLayer;

UPROPERTY(EditDefaultsOnly, Category="Input|Actions")
TObjectPtr<UInputAction> IA_CycleAxis;

UPROPERTY(EditDefaultsOnly, Category="Input|Actions")
TObjectPtr<UInputAction> IA_Undo;

UPROPERTY(EditDefaultsOnly, Category="Input|Actions")
TObjectPtr<UInputAction> IA_Reset;

UPROPERTY(EditDefaultsOnly, Category="Input|Actions")
TObjectPtr<UInputAction> IA_Scramble;
```

In `SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)`:

```cpp
if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent))
{
    EnhancedInput->BindAction(IA_RotateLeft, ETriggerEvent::Started, this, &ARubiksCube::OnRotateLeft);
    EnhancedInput->BindAction(IA_RotateRight, ETriggerEvent::Started, this, &ARubiksCube::OnRotateRight);
    EnhancedInput->BindAction(IA_SelectNextLayer, ETriggerEvent::Started, this, &ARubiksCube::OnSelectNextLayer);
    EnhancedInput->BindAction(IA_SelectPrevLayer, ETriggerEvent::Started, this, &ARubiksCube::OnSelectPrevLayer);
    EnhancedInput->BindAction(IA_CycleAxis, ETriggerEvent::Started, this, &ARubiksCube::OnCycleAxis);
    EnhancedInput->BindAction(IA_Undo, ETriggerEvent::Started, this, &ARubiksCube::Undo);
    EnhancedInput->BindAction(IA_Reset, ETriggerEvent::Started, this, &ARubiksCube::ResetCube);
    EnhancedInput->BindAction(IA_Scramble, ETriggerEvent::Started, this, &ARubiksCube::OnScrambleInput);
}

// Add IMC to player subsystem
if (APlayerController* PC = Cast<APlayerController>(GetWorld()->GetFirstPlayerController()))
{
    if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
        ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
    {
        Subsystem->AddMappingContext(RubiksMappingContext, 0);
    }
}
```

### Action Handlers

```cpp
void ARubiksCube::OnRotateLeft()
{
    FRubiksMoveDefinition Move;
    Move.Axis = SelectedAxis;
    Move.Layer = SelectedLayer;
    Move.Direction = -1; // -90 deg
    ExecuteMove(Move);
}

void ARubiksCube::OnRotateRight()
{
    FRubiksMoveDefinition Move;
    Move.Axis = SelectedAxis;
    Move.Layer = SelectedLayer;
    Move.Direction = +1; // +90 deg
    ExecuteMove(Move);
}

void ARubiksCube::OnCycleAxis()
{
    switch (SelectedAxis)
    {
    case EAxis::X: SelectedAxis = EAxis::Y; break;
    case EAxis::Y: SelectedAxis = EAxis::Z; break;
    case EAxis::Z: SelectedAxis = EAxis::X; break;
    default:       SelectedAxis = EAxis::Z; break;
    }
    UpdateSelectionHighlight();
}

void ARubiksCube::OnSelectNextLayer()
{
    SelectedLayer = FMath::Clamp(SelectedLayer + 1, -1, 1);
    UpdateSelectionHighlight();
}

void ARubiksCube::OnSelectPrevLayer()
{
    SelectedLayer = FMath::Clamp(SelectedLayer - 1, -1, 1);
    UpdateSelectionHighlight();
}
```

### Visual Slice Highlight

To make the controls intuitive, provide instant visual feedback on which row is selected:
- Draw a subtle debug bounding box or ring around the currently selected 9 cubelets:
```cpp
void ARubiksCube::UpdateSelectionHighlight()
{
    // Draw visual outline or pass parameter to cubelet materials
    // to highlight the active axis and layer
}
```
- This ensures the player always knows exactly which row will turn when pressing Rotate Left or Rotate Right.

---

# 24. Milestone 14 — Scramble

Create:

```cpp
void Scramble(int32 NumberOfMoves);
```

Start with:

```cpp
Scramble(20);
```

Generate random moves.

Improve later by preventing:

```text
R R'
```

and repeated same-face moves:

```text
R R R
```

A better scramble generator should avoid immediately selecting the same face or an incompatible redundant move.

---

# 25. Milestone 15 — Reset

Create:

```cpp
void ResetCube();
```

Reset:

```text
Grid positions
Cubelet rotations
Materials
Move history
Move queue
Pivot
Rotation state
```

The resulting state must be exactly:

```text
Solved
```

Do not rebuild the entire actor unless necessary.

Reset existing cubelets.

---

# 26. Milestone 16 — Solved Detection

Create:

```cpp
bool IsSolved() const;
```

Simple implementation:

For each face:

```text
Get center sticker color
        ↓
Check all 9 stickers
        ↓
All equal?
```

If all six faces pass:

```cpp
return true;
```

Later you can implement a more formal cubie-state comparison.

---

# 27. Milestone 17 — Mouse Interaction

Only implement this after keyboard moves are completely reliable.

Workflow:

```text
Mouse Down
    ↓
Line Trace
    ↓
Hit cubelet
    ↓
Get Hit.ImpactNormal
    ↓
Determine selected face
    ↓
Record mouse position
```

On mouse release:

```text
Mouse movement
    ↓
Horizontal or vertical?
    ↓
Compare against face basis
    ↓
Determine move direction
    ↓
ExecuteMove()
```

For example:

```text
Click Front face
Drag Right

        ↓

F
```

or:

```text
Click Front face
Drag Left

        ↓

F'
```

The exact mapping should be tested for each face.

---

# 28. Face Identification

Use the hit normal.

```text
Normal                 Face

(+1, 0, 0)             Right
(-1, 0, 0)             Left

(0, +1, 0)             Front
(0, -1, 0)             Back

(0, 0, +1)             Up
(0, 0, -1)             Down
```

This assumes the project uses the same coordinate convention.

If you choose another convention, change the mapping once and keep it consistent.

---

# 29. Camera

Create a simple camera around the cube.

For the first version:

```text
PlayerController
      ↓
Camera
      ↓
ARubiksCube
```

You can implement:

- Orbit left/right
- Orbit up/down
- Zoom
- Reset camera

Do not mix camera code into the Rubik's Cube move system.

---

# 30. Recommended Final Class Architecture

Once the first prototype works, refactor toward:

```text
ARubiksCube
│
├── Owns visual cubelets
│
├── URubiksCubeState
│
├── URubiksCubeMoveSystem
│
├── URubiksCubeRotationSystem
│
└── URubiksCubeInputComponent
```

---

## ARubiksCube

Responsible for:

```text
Actor lifecycle
Cubelet creation
Visual representation
High-level API
```

Public API:

```cpp
void ExecuteMove(ERubiksMove Move);
void Scramble(int32 MoveCount);
void ResetCube();
void Undo();
bool IsSolved() const;
```

---

# 31. URubiksCubeState

Responsible for:

```text
Logical cube state
Grid positions
Cubelet orientations
Solved detection
```

It should NOT know about:

```text
Unreal Tick
Input
Mouse
Camera
Animation
```

---

# 32. URubiksCubeMoveSystem

Responsible for:

```text
Move definitions
Layer selection
Move validation
Move inversion
Move history
Move queue
```

Example:

```cpp
void ExecuteMove(ERubiksMove Move);
```

---

# 33. URubiksCubeRotationSystem

Responsible for:

```text
Temporary pivot
Attaching cubelets
Detaching cubelets
Interpolating rotation
Final snapping
```

It should know nothing about keyboard input.

---

# 34. URubiksCubeInputComponent

Responsible for:

```text
Enhanced Input handling
Input Mapping Context (IMC) registration
Layer and Axis selection state tracking
Rotate Left / Rotate Right event dispatching
Optional mouse drag / face detection
```

It converts player input actions into:

```cpp
FRubiksMoveDefinition MoveDef(SelectedAxis, SelectedLayer, Direction);
ExecuteMove(MoveDef);
```

It does not directly manipulate cubelet transforms or bypass the move queue.

---

# 35. Data Flow

The final architecture should look like this:

```text
PLAYER
  │
  │ Enhanced Input (IMC_RubiksCube & Actions)
  ↓
Input Component
  │
  ├── Cycle Axis (X ↔ Y ↔ Z)
  ├── Change Layer (-1 ↔ 0 ↔ +1)
  ├── Rotate Left (-90°) / Rotate Right (+90°)
  │
  │ Construct FRubiksMoveDefinition { Axis, Layer, Direction }
  ↓
Move System
  │
  ├── Validate move
  ├── Add to MoveQueue (if busy)
  └── Start rotation
  │
  ↓
Rotation System
  │
  ├── Select 9 cubelets matching (Axis, Layer)
  ├── Attach to RotationPivot
  ├── Interpolate pivot rotation
  ├── Detach cubelets
  └── Snap to exact integer coordinates
  │
  ↓
Cube State
  │
  ├── Update FIntVector GridPositions
  ├── Update FQuat LogicalRotations
  ├── Append to MoveHistory
  └── Check IsSolved()
```

---

# 36. Critical Rule

The architecture must maintain:

```text
        LOGIC
          │
          ↓
     Cube State
          │
          ↓
       Visuals
```

Never:

```text
Visual Transform
      ↓
Game Logic
```

The logical state is authoritative.

The mesh is only a representation of the state.

---

# 37. Development Order

Follow this exact order.

## Phase 1 — Visual Prototype

- [ ] Create cube mesh
- [ ] Create `ARubiksCube`
- [ ] Create one cubelet
- [ ] Spawn 27 cubelets
- [ ] Arrange them in 3×3×3
- [ ] Add colored stickers
- [ ] Add camera

## Phase 2 — Cube Mathematics

- [ ] Add `FIntVector GridPosition`
- [ ] Implement X rotation
- [ ] Implement Y rotation
- [ ] Implement Z rotation
- [ ] Test coordinate transformations
- [ ] Add cubelet orientation

## Phase 3 — First Move

Implement ONLY:

```text
R
```

Verify:

- [ ] Correct 9 cubelets selected
- [ ] Correct pivot position
- [ ] Correct 90° rotation
- [ ] Correct final coordinates
- [ ] Correct sticker orientation
- [ ] Correct snapping

Do not implement all six faces until R is perfect.

## Phase 4 — All Moves

- [ ] R
- [ ] R'
- [ ] L
- [ ] L'
- [ ] U
- [ ] U'
- [ ] D
- [ ] D'
- [ ] F
- [ ] F'
- [ ] B
- [ ] B'

## Phase 5 — Gameplay

- [ ] Move queue
- [ ] Move history
- [ ] Undo
- [ ] Reset
- [ ] Scramble
- [ ] Solved detection
- [ ] Move counter

## Phase 6 — Input

- [ ] Create Input Actions (`IA_RotateLeft`, `IA_RotateRight`, `IA_SelectNextLayer`, `IA_SelectPrevLayer`, `IA_CycleAxis`, etc.)
- [ ] Create & configure `IMC_RubiksCube` mapping context (assignable in Editor/BP)
- [ ] Bind actions in C++ via `UEnhancedInputComponent`
- [ ] Implement layer selection (`-1`, `0`, `+1`) & axis cycling (`X`, `Y`, `Z`)
- [ ] Implement two-key Left/Right layer rotation
- [ ] Add visual highlight indicator for active row/axis
- [ ] Optional: Mouse click & drag face detection

## Phase 7 — Polish

- [ ] Camera orbit
- [ ] Zoom
- [ ] Face highlight
- [ ] Move UI
- [ ] Scramble UI
- [ ] Sound effects
- [ ] Completion effect

---

# 38. First Implementation Milestone

Do NOT start by implementing the entire cube.

Your first goal should be:

```text
Launch Unreal
    ↓
Spawn ARubiksCube
    ↓
See 27 cubelets
    ↓
See a recognizable 3×3×3 Rubik's Cube
```

Then:

```text
Trigger Rotate Right (Axis: X, Layer: +1)
    ↓
Right 9 cubelets rotate 90°
    ↓
They snap perfectly into position
```

Then:

```text
Trigger Rotate Left (Axis: X, Layer: +1)
    ↓
Cube returns exactly to solved state
```

If that works, the foundation is correct.

---

# 39. Debugging Tools

Add debug information early.

Useful functions:

```cpp
void DebugPrintCubeState() const;
void DrawDebugCubeGrid();
void DrawDebugLayer(const FRubiksMoveDefinition& Move);
```

During development, draw:

```text
Cubelet coordinate
Current move
Rotation axis
Rotation direction
Current rotation alpha
```

For example:

```text
Move: R
Axis: X
Layer: +1
Direction: +90
Alpha: 0.54
```

This will make coordinate bugs much easier to find.

---

# 40. Unit Tests

Before adding mouse interaction, test the mathematics.

Important tests:

```text
R then R' → solved

U then U' → solved

F then F' → solved

R R R R → solved

U U U U → solved

F F F F → solved
```

Also test:

```text
R U R' U'
```

and verify that the cube does not lose or duplicate cubelets.

Eventually test:

```text
Scramble
    ↓
Record moves
    ↓
Apply inverse moves in reverse order
    ↓
Solved
```

This is an excellent validation of your architecture.

---

# 41. Performance

A 3×3 cube contains only:

```text
27 cubelets
```

Even if every cubelet has several components, this is trivial for Unreal.

Do not optimize prematurely.

Focus on:

```text
Correct state
Correct transforms
Correct interaction
```

---

# 42. Things NOT to Do

Avoid these approaches:

### Don't create 27 Actors initially

Prefer:

```cpp
ARubiksCube
    └── 27 components
```

rather than:

```text
ARubiksCubelet × 27 Actors
```

Components are simpler for this use case.

### Don't physically simulate the cube

You don't need:

```text
Physics
Constraints
Torque
Collision forces
```

A Rubik's Cube is deterministic.

### Don't use Animation Blueprints

The entire layer rotation can be done with:

```cpp
FQuat
FMath::Lerp
Tick
```

### Don't use world transforms as state

Use:

```cpp
FIntVector
```

for logical coordinates.

### Don't hard-code 54 sticker movements initially

Derive movement from:

```text
Axis
Layer
Direction
```

---

# 43. End Goal

The finished system should support:

```text
                 ┌──────────────┐
                 │ Player Input │
                 └──────┬───────┘
                        │
                        ↓
                ┌───────────────┐
                │ Move System   │
                └───────┬───────┘
                        │
                        ↓
                ┌───────────────┐
                │ Cube State    │
                └───────┬───────┘
                        │
                        ↓
              ┌───────────────────┐
              │ Rotation System   │
              └─────────┬─────────┘
                        │
                        ↓
                 ┌─────────────┐
                 │ 27 Cubelets │
                 └─────────────┘
```

The cube should be completely playable using only:

```text
C++
UStaticMeshComponent
USceneComponent
FVector
FQuat
FIntVector
Tick()
Input
```

No animation system is required.

---

# 44. Suggested First Coding Session

For the first session, implement only these files:

```text
RubiksCubeTypes.h
RubiksCubeletComponent.h
RubiksCubeletComponent.cpp
RubiksCube.h
RubiksCube.cpp
```

And get this working:

```cpp
ARubiksCube
    ↓
CreateCubelets()
    ↓
27 cubelets
    ↓
FIntVector positions
    ↓
3×3×3 visual cube
```

Do not implement moves yet.

Once the cube looks correct, implement **R only**.

Once R and R' are mathematically correct, generalize the system to all six faces.

That progression will keep the project manageable and make debugging much easier.
