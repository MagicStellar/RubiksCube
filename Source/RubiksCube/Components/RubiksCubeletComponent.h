#pragma once

#include "CoreMinimal.h"
#include "Components/StaticMeshComponent.h"
#include "Data/RubiksCubeTypes.h"
#include "RubiksCubeletComponent.generated.h"

USTRUCT()
struct FRubiksStickerInfo
{
	GENERATED_BODY()

	UPROPERTY()
	ERubiksFace Face = ERubiksFace::None;

	UPROPERTY()
	FVector InitialNormal = FVector::ZeroVector;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> MeshComponent = nullptr;
};

/**
 * Component representing a single 1x1x1 cubelet in the 3x3x3 Rubik's Cube.
 * Maintains both authoritative logical grid coordinates / rotation and visual transform.
 */
UCLASS(ClassGroup = (RubiksCube), meta = (BlueprintSpawnableComponent))
class RUBIKSCUBE_API URubiksCubeletComponent : public UStaticMeshComponent
{
	GENERATED_BODY()

public:
	URubiksCubeletComponent();

	/** Set authoritative logical grid position (coordinates -1, 0, 1) */
	UFUNCTION(BlueprintCallable, Category = "Rubiks Cubelet")
	void SetGridPosition(const FIntVector& InPosition);

	/** Get current authoritative logical grid position */
	UFUNCTION(BlueprintPure, Category = "Rubiks Cubelet")
	FIntVector GetGridPosition() const { return GridPosition; }

	/** Get initial starting grid position */
	UFUNCTION(BlueprintPure, Category = "Rubiks Cubelet")
	FIntVector GetInitialGridPosition() const { return InitialGridPosition; }

	/** Set initial starting position */
	void SetInitialGridPosition(const FIntVector& InPosition);

	/** Set logical orientation quaternion */
	UFUNCTION(BlueprintCallable, Category = "Rubiks Cubelet")
	void SetLogicalRotation(const FQuat& InRotation);

	/** Get logical orientation quaternion */
	UFUNCTION(BlueprintPure, Category = "Rubiks Cubelet")
	FQuat GetLogicalRotation() const { return LogicalRotation; }

	/** Snaps component relative location and rotation to exact grid coordinates */
	UFUNCTION(BlueprintCallable, Category = "Rubiks Cubelet")
	void SnapTransform(float CubeSpacing);

	/** Sets up stickers on exposed outer faces based on InitialGridPosition */
	void SetupStickers(
		UStaticMesh* StickerMesh,
		const TMap<ERubiksFace, UMaterialInterface*>& FaceMaterials,
		float CubeSize = 100.f
	);

	/** Resets the cubelet back to solved state (initial position and identity rotation) */
	UFUNCTION(BlueprintCallable, Category = "Rubiks Cubelet")
	void ResetToInitial(float CubeSpacing);

	/** Returns true if this cubelet is at its initial grid position */
	UFUNCTION(BlueprintPure, Category = "Rubiks Cubelet")
	bool IsInInitialGridPosition() const { return GridPosition == InitialGridPosition; }

	/** Returns true if all stickers on this cubelet are aligned with their initial face normals */
	UFUNCTION(BlueprintPure, Category = "Rubiks Cubelet")
	bool AreStickersSolved() const;

	/** Number of stickers attached to this cubelet (0=internal, 1=center, 2=edge, 3=corner) */
	UFUNCTION(BlueprintPure, Category = "Rubiks Cubelet")
	int32 GetStickerCount() const { return Stickers.Num(); }

	const TArray<FRubiksStickerInfo>& GetStickers() const { return Stickers; }

	void SetupSelectionHighlight(UMaterialInterface* Material);
	void SetSelectionHighlighted(bool bHighlighted);
	bool IsSelectionHighlighted() const { return SelectionHighlight && SelectionHighlight->IsVisible(); }

	// Retain the component reference when the editor actor is duplicated into PIE.
	// A transient reference leaves the old visible shell behind and creates a second shell.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rubiks Cubelet")
	TObjectPtr<UStaticMeshComponent> SelectionHighlight;

private:
	/** Current logical coordinate (-1..1, -1..1, -1..1) */
	UPROPERTY(VisibleAnywhere, Category = "Rubiks Cubelet")
	FIntVector GridPosition = FIntVector::ZeroValue;

	/** Solved/initial logical coordinate */
	UPROPERTY(VisibleAnywhere, Category = "Rubiks Cubelet")
	FIntVector InitialGridPosition = FIntVector::ZeroValue;

	/** Authoritative logical orientation quaternion */
	UPROPERTY(VisibleAnywhere, Category = "Rubiks Cubelet")
	FQuat LogicalRotation = FQuat::Identity;

	/** Registered stickers for exposed faces */
	UPROPERTY()
	TArray<FRubiksStickerInfo> Stickers;

	/** Helper to spawn a single face sticker */
	void AddSticker(
		ERubiksFace Face,
		const FVector& LocalOffset,
		const FVector& LocalScale,
		const FVector& FaceNormal,
		UStaticMesh* Mesh,
		UMaterialInterface* Material
	);
};
