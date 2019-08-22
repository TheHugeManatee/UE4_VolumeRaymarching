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

  UPROPERTY(BlueprintReadWrite, EditAnywhere)
  UVolumeTexture* Texture;

  UPROPERTY(BlueprintReadWrite, EditAnywhere)
  FVector VolumeSizeInMM;

  UPROPERTY(BlueprintReadWrite, EditAnywhere)
  FVector CenterLocation;

  UPROPERTY(BlueprintReadWrite, EditAnywhere)
  FIntVector Dimensions;
};
