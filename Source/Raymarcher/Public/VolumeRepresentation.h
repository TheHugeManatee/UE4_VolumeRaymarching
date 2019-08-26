// (c) 2019 Technical University of Munich
// Jakob Weiss <jakob.weiss@tum.de>, Tomas Bartipan <tomas.bartipan@tum.de>

#pragma once

#include "CoreMinimal.h"

#include "VolumeRepresentation.generated.h"

/**
 *
 */
UCLASS(BlueprintType)
class RAYMARCHER_API UVolumeRepresentation : public UObject {
  GENERATED_BODY()
public:
  UVolumeRepresentation();
  ~UVolumeRepresentation();

  /**
   * The volume texture that contains the actual data.
   */
  UPROPERTY(BlueprintReadWrite, EditAnywhere)
  UVolumeTexture* Texture;

  /**
   * The volume size in MM.
   */
  UPROPERTY(BlueprintReadWrite, EditAnywhere)
  FVector VolumeSizeInMM;

  /**
   * The location of the volume center in the world.
   */
  UPROPERTY(BlueprintReadWrite, EditAnywhere)
  FVector CenterLocation;

  /**
   * The voxel dimensions of the volume.
   */
  UPROPERTY(BlueprintReadWrite, EditAnywhere)
  FIntVector Dimensions;

  /**
   * Tags associated with this volume.
   * This can be used to indicate modality, which post-processing operations were applied etc..
   * Common tags are:
   *
   *
   */
  UPROPERTY(BlueprintReadWrite, EditAnywhere)
  TMap<FString, FString> Tags;
};
