#include "Components/RubiksCubeletComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Materials/MaterialInterface.h"

URubiksCubeletComponent::URubiksCubeletComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetMobility(EComponentMobility::Movable);
	SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
}

void URubiksCubeletComponent::SetGridPosition(const FIntVector& InPosition)
{
	GridPosition = InPosition;
}

void URubiksCubeletComponent::SetupSelectionHighlight(UMaterialInterface* Material)
{
	if (!Material || !GetStaticMesh()) return;
	// Recover shells from older saved actors and remove duplicates left by transient references.
	const auto Children = GetAttachChildren();
	for (USceneComponent* Child : Children)
	{
		UStaticMeshComponent* Mesh = Cast<UStaticMeshComponent>(Child);
		if (!Mesh || !Mesh->GetMaterial(0) || Mesh->GetMaterial(0)->GetMaterial() != Material->GetMaterial()) continue;
		if (!SelectionHighlight) SelectionHighlight = Mesh;
		else if (SelectionHighlight != Mesh) Mesh->DestroyComponent();
	}
	if (!SelectionHighlight)
	{
		SelectionHighlight = NewObject<UStaticMeshComponent>(GetOwner());
		SelectionHighlight->SetMobility(EComponentMobility::Movable);
		SelectionHighlight->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		SelectionHighlight->SetCastShadow(false);
		SelectionHighlight->SetCanEverAffectNavigation(false);
		SelectionHighlight->AttachToComponent(this, FAttachmentTransformRules::KeepRelativeTransform);
		// Slightly outside the body and stickers; only the material's thin edges are opaque.
		SelectionHighlight->SetRelativeScale3D(FVector(1.035f));
		SelectionHighlight->SetVisibility(false);
		SelectionHighlight->SetStaticMesh(GetStaticMesh());
		SelectionHighlight->RegisterComponent();
	}
	SelectionHighlight->SetMaterial(0, Material);
}

void URubiksCubeletComponent::SetSelectionHighlighted(bool bHighlighted)
{
	if (SelectionHighlight) SelectionHighlight->SetVisibility(bHighlighted);
}

void URubiksCubeletComponent::SetInitialGridPosition(const FIntVector& InPosition)
{
	InitialGridPosition = InPosition;
	GridPosition = InPosition;
}

void URubiksCubeletComponent::SetLogicalRotation(const FQuat& InRotation)
{
	LogicalRotation = InRotation;
	LogicalRotation.Normalize();
}

void URubiksCubeletComponent::SnapTransform(float CubeSpacing)
{
	const FVector TargetLocation = FVector(GridPosition) * CubeSpacing;
	SetRelativeLocation(TargetLocation);
	SetRelativeRotation(LogicalRotation);
}

void URubiksCubeletComponent::ResetToInitial(float CubeSpacing)
{
	GridPosition = InitialGridPosition;
	LogicalRotation = FQuat::Identity;
	SnapTransform(CubeSpacing);
}

bool URubiksCubeletComponent::AreStickersSolved() const
{
	for (const FRubiksStickerInfo& Sticker : Stickers)
	{
		const FVector CurrentNormal = LogicalRotation.RotateVector(Sticker.InitialNormal);
		if (FVector::DotProduct(CurrentNormal, Sticker.InitialNormal) < 0.95f)
		{
			return false;
		}
	}
	return true;
}

void URubiksCubeletComponent::SetupStickers(
	UStaticMesh* StickerMesh,
	const TMap<ERubiksFace, UMaterialInterface*>& FaceMaterials,
	float CubeSize)
{
	if (!StickerMesh)
	{
		return;
	}

	// Clear any existing stickers
	for (FRubiksStickerInfo& Sticker : Stickers)
	{
		if (Sticker.MeshComponent)
		{
			Sticker.MeshComponent->DestroyComponent();
		}
	}
	Stickers.Empty();

	const float HalfSize = CubeSize * 0.5f;
	const float StickerThickness = 2.0f; // in cm
	const float ThicknessScale = StickerThickness / CubeSize;
	const float FaceCoverage = 0.88f;   // Covers 88% of face width/height

	// Check 6 directions against InitialGridPosition
	// Up face (+Z)
	if (InitialGridPosition.Z == 1)
	{
		UMaterialInterface* Mat = FaceMaterials.FindRef(ERubiksFace::Up);
		AddSticker(
			ERubiksFace::Up,
			FVector(0.f, 0.f, HalfSize - (StickerThickness * 0.25f)),
			FVector(FaceCoverage, FaceCoverage, ThicknessScale),
			FVector::UpVector,
			StickerMesh,
			Mat
		);
	}
	// Down face (-Z)
	if (InitialGridPosition.Z == -1)
	{
		UMaterialInterface* Mat = FaceMaterials.FindRef(ERubiksFace::Down);
		AddSticker(
			ERubiksFace::Down,
			FVector(0.f, 0.f, -HalfSize + (StickerThickness * 0.25f)),
			FVector(FaceCoverage, FaceCoverage, ThicknessScale),
			FVector::DownVector,
			StickerMesh,
			Mat
		);
	}
	// Right face (+X)
	if (InitialGridPosition.X == 1)
	{
		UMaterialInterface* Mat = FaceMaterials.FindRef(ERubiksFace::Right);
		AddSticker(
			ERubiksFace::Right,
			FVector(HalfSize - (StickerThickness * 0.25f), 0.f, 0.f),
			FVector(ThicknessScale, FaceCoverage, FaceCoverage),
			FVector::ForwardVector,
			StickerMesh,
			Mat
		);
	}
	// Left face (-X)
	if (InitialGridPosition.X == -1)
	{
		UMaterialInterface* Mat = FaceMaterials.FindRef(ERubiksFace::Left);
		AddSticker(
			ERubiksFace::Left,
			FVector(-HalfSize + (StickerThickness * 0.25f), 0.f, 0.f),
			FVector(ThicknessScale, FaceCoverage, FaceCoverage),
			FVector::BackwardVector,
			StickerMesh,
			Mat
		);
	}
	// Front face (+Y)
	if (InitialGridPosition.Y == 1)
	{
		UMaterialInterface* Mat = FaceMaterials.FindRef(ERubiksFace::Front);
		AddSticker(
			ERubiksFace::Front,
			FVector(0.f, HalfSize - (StickerThickness * 0.25f), 0.f),
			FVector(FaceCoverage, ThicknessScale, FaceCoverage),
			FVector::RightVector,
			StickerMesh,
			Mat
		);
	}
	// Back face (-Y)
	if (InitialGridPosition.Y == -1)
	{
		UMaterialInterface* Mat = FaceMaterials.FindRef(ERubiksFace::Back);
		AddSticker(
			ERubiksFace::Back,
			FVector(0.f, -HalfSize + (StickerThickness * 0.25f), 0.f),
			FVector(FaceCoverage, ThicknessScale, FaceCoverage),
			FVector::LeftVector,
			StickerMesh,
			Mat
		);
	}
}

void URubiksCubeletComponent::AddSticker(
	ERubiksFace Face,
	const FVector& LocalOffset,
	const FVector& LocalScale,
	const FVector& FaceNormal,
	UStaticMesh* Mesh,
	UMaterialInterface* Material)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return;
	}

	UStaticMeshComponent* StickerComp = NewObject<UStaticMeshComponent>(OwnerActor);
	if (!StickerComp)
	{
		return;
	}

	StickerComp->SetStaticMesh(Mesh);
	if (Material)
	{
		StickerComp->SetMaterial(0, Material);
	}
	StickerComp->SetMobility(EComponentMobility::Movable);
	StickerComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	StickerComp->AttachToComponent(this, FAttachmentTransformRules::KeepRelativeTransform);
	StickerComp->SetRelativeLocation(LocalOffset);
	StickerComp->SetRelativeScale3D(LocalScale);
	StickerComp->RegisterComponent();

	FRubiksStickerInfo Info;
	Info.Face = Face;
	Info.InitialNormal = FaceNormal;
	Info.MeshComponent = StickerComp;
	Stickers.Add(Info);
}
