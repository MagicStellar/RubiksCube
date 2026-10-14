#pragma once

#include "CoreMinimal.h"
#include "RubiksCubeTypes.generated.h"

UENUM(BlueprintType)
enum class ERubiksInteractionMode : uint8
{
	Play,
	Inspect
};

/**
 * Standard Rubik's Cube Face Notation (Singmaster notation).
 * U = Up (White)
 * D = Down (Yellow)
 * L = Left (Orange)
 * R = Right (Red)
 * F = Front (Green)
 * B = Back (Blue)
 * Prime (') indicates counter-clockwise rotation when looking directly at the face.
 */
UENUM(BlueprintType)
enum class ERubiksMove : uint8
{
	U       UMETA(DisplayName = "U (Up CW)"),
	UPrime  UMETA(DisplayName = "U' (Up CCW)"),
	D       UMETA(DisplayName = "D (Down CW)"),
	DPrime  UMETA(DisplayName = "D' (Down CCW)"),
	L       UMETA(DisplayName = "L (Left CW)"),
	LPrime  UMETA(DisplayName = "L' (Left CCW)"),
	R       UMETA(DisplayName = "R (Right CW)"),
	RPrime  UMETA(DisplayName = "R' (Right CCW)"),
	F       UMETA(DisplayName = "F (Front CW)"),
	FPrime  UMETA(DisplayName = "F' (Front CCW)"),
	B       UMETA(DisplayName = "B (Back CW)"),
	BPrime  UMETA(DisplayName = "B' (Back CCW)")
};

/**
 * Rubik's Cube face enumeration.
 */
UENUM(BlueprintType)
enum class ERubiksFace : uint8
{
	None,
	Up,     // +Z
	Down,   // -Z
	Left,   // -X
	Right,  // +X
	Front,  // +Y
	Back    // -Y
};

/**
 * Definition of a slice rotation move on the cube.
 * Allows rotating any layer on any axis in either direction.
 */
USTRUCT(BlueprintType)
struct RUBIKSCUBE_API FRubiksMoveDefinition
{
	GENERATED_BODY()

	/** Rotation Axis: X, Y, or Z */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rubiks Move")
	TEnumAsByte<EAxis::Type> Axis = EAxis::Z;

	/** Layer coordinate index: -1, 0, or +1 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rubiks Move")
	int32 Layer = 1;

	/** Rotation direction: +1 (+90 deg), -1 (-90 deg) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rubiks Move")
	int32 Direction = 1;

	/** Associated standard move (if this matches a Singmaster move) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rubiks Move")
	ERubiksMove MoveType = ERubiksMove::U;

	FRubiksMoveDefinition()
		: Axis(EAxis::Z), Layer(1), Direction(1), MoveType(ERubiksMove::U)
	{}

	FRubiksMoveDefinition(EAxis::Type InAxis, int32 InLayer, int32 InDirection, ERubiksMove InType = ERubiksMove::U)
		: Axis(InAxis), Layer(InLayer), Direction(InDirection), MoveType(InType)
	{}

	bool operator==(const FRubiksMoveDefinition& Other) const
	{
		return Axis == Other.Axis && Layer == Other.Layer && Direction == Other.Direction;
	}

	/** Returns the exact opposite/undo move definition */
	FRubiksMoveDefinition GetInverse() const
	{
		FRubiksMoveDefinition Inv = *this;
		Inv.Direction = -Inv.Direction;
		Inv.MoveType = GetInverseMoveType(MoveType);
		return Inv;
	}

	static ERubiksMove GetInverseMoveType(ERubiksMove InMove)
	{
		switch (InMove)
		{
		case ERubiksMove::U:      return ERubiksMove::UPrime;
		case ERubiksMove::UPrime: return ERubiksMove::U;
		case ERubiksMove::D:      return ERubiksMove::DPrime;
		case ERubiksMove::DPrime: return ERubiksMove::D;
		case ERubiksMove::L:      return ERubiksMove::LPrime;
		case ERubiksMove::LPrime: return ERubiksMove::L;
		case ERubiksMove::R:      return ERubiksMove::RPrime;
		case ERubiksMove::RPrime: return ERubiksMove::R;
		case ERubiksMove::F:      return ERubiksMove::FPrime;
		case ERubiksMove::FPrime: return ERubiksMove::F;
		case ERubiksMove::B:      return ERubiksMove::BPrime;
		case ERubiksMove::BPrime: return ERubiksMove::B;
		default:                  return ERubiksMove::U;
		}
	}

	/** Converts a Singmaster move enum to its exact Axis, Layer, and Direction */
	static FRubiksMoveDefinition FromStandardMove(ERubiksMove InMove)
	{
		switch (InMove)
		{
		case ERubiksMove::U:      return FRubiksMoveDefinition(EAxis::Z,  1,  1, ERubiksMove::U);
		case ERubiksMove::UPrime: return FRubiksMoveDefinition(EAxis::Z,  1, -1, ERubiksMove::UPrime);
		case ERubiksMove::D:      return FRubiksMoveDefinition(EAxis::Z, -1, -1, ERubiksMove::D);
		case ERubiksMove::DPrime: return FRubiksMoveDefinition(EAxis::Z, -1,  1, ERubiksMove::DPrime);
		case ERubiksMove::R:      return FRubiksMoveDefinition(EAxis::X,  1,  1, ERubiksMove::R);
		case ERubiksMove::RPrime: return FRubiksMoveDefinition(EAxis::X,  1, -1, ERubiksMove::RPrime);
		case ERubiksMove::L:      return FRubiksMoveDefinition(EAxis::X, -1, -1, ERubiksMove::L);
		case ERubiksMove::LPrime: return FRubiksMoveDefinition(EAxis::X, -1,  1, ERubiksMove::LPrime);
		case ERubiksMove::F:      return FRubiksMoveDefinition(EAxis::Y,  1,  1, ERubiksMove::F);
		case ERubiksMove::FPrime: return FRubiksMoveDefinition(EAxis::Y,  1, -1, ERubiksMove::FPrime);
		case ERubiksMove::B:      return FRubiksMoveDefinition(EAxis::Y, -1, -1, ERubiksMove::B);
		case ERubiksMove::BPrime: return FRubiksMoveDefinition(EAxis::Y, -1,  1, ERubiksMove::BPrime);
		default:                  return FRubiksMoveDefinition();
		}
	}

	/** Converts Axis, Layer, Direction back to Singmaster move if applicable */
	static bool TryToStandardMove(EAxis::Type InAxis, int32 InLayer, int32 InDirection, ERubiksMove& OutMove)
	{
		if (InAxis == EAxis::Z)
		{
			if (InLayer == 1)
			{
				OutMove = (InDirection > 0) ? ERubiksMove::U : ERubiksMove::UPrime;
				return true;
			}
			else if (InLayer == -1)
			{
				OutMove = (InDirection < 0) ? ERubiksMove::D : ERubiksMove::DPrime;
				return true;
			}
		}
		else if (InAxis == EAxis::X)
		{
			if (InLayer == 1)
			{
				OutMove = (InDirection > 0) ? ERubiksMove::R : ERubiksMove::RPrime;
				return true;
			}
			else if (InLayer == -1)
			{
				OutMove = (InDirection < 0) ? ERubiksMove::L : ERubiksMove::LPrime;
				return true;
			}
		}
		else if (InAxis == EAxis::Y)
		{
			if (InLayer == 1)
			{
				OutMove = (InDirection > 0) ? ERubiksMove::F : ERubiksMove::FPrime;
				return true;
			}
			else if (InLayer == -1)
			{
				OutMove = (InDirection < 0) ? ERubiksMove::B : ERubiksMove::BPrime;
				return true;
			}
		}
		return false;
	}

	static FString GetMoveString(ERubiksMove InMove)
	{
		switch (InMove)
		{
		case ERubiksMove::U:      return TEXT("U");
		case ERubiksMove::UPrime: return TEXT("U'");
		case ERubiksMove::D:      return TEXT("D");
		case ERubiksMove::DPrime: return TEXT("D'");
		case ERubiksMove::L:      return TEXT("L");
		case ERubiksMove::LPrime: return TEXT("L'");
		case ERubiksMove::R:      return TEXT("R");
		case ERubiksMove::RPrime: return TEXT("R'");
		case ERubiksMove::F:      return TEXT("F");
		case ERubiksMove::FPrime: return TEXT("F'");
		case ERubiksMove::B:      return TEXT("B");
		case ERubiksMove::BPrime: return TEXT("B'");
		default:                  return TEXT("Unknown");
		}
	}
};
