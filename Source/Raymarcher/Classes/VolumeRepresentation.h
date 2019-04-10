// (c) 2019 Technical University of Munich
// Jakob Weiss <jakob.weiss@tum.de>, Tomas Bartipan <tomas.bartipan@tum.de>

#pragma once

#include "CoreMinimal.h"

#include "VolumeRepresentation.generated.h"

/**
 *
 */
USTRUCT(BlueprintType)
struct RAYMARCHER_API FVolumeRepresentation {
  GENERATED_BODY()
public:
  FVolumeRepresentation();
  ~FVolumeRepresentation();

  UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "VolumeRepresentation")
  UVolumeTexture* Texture;

  UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "VolumeRepresentation")
  FVector VolumeSizeInMM;

  UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "VolumeRepresentation")
  FVector CenterLocation;

  UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "VolumeRepresentation")
  FIntVector Dimensions;
};
