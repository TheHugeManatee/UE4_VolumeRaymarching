// (C) Technical University of Munich - Computer Aided Medical Procedures
// Developed by Tomas Bartipan (tomas.bartipan@tum.de)

#pragma once

#include "CoreMinimal.h"
#include "Curves/CurveLinearColor.h"
#include "Engine/VolumeTexture.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "RHI.h"
#include "RHIResources.h"
#include "RaymarchRendering.h"
#include "UObject/ObjectMacros.h"

#include "RaymarchBlueprintLibrary.generated.h"

UCLASS()
class URaymarchBlueprintLibrary : public UBlueprintFunctionLibrary {
  GENERATED_BODY()
public:
  //
  //
  // Functions for working with RGB light volumes follow
  //
  //

  /** Adds a light to light volumes.	 */
  UFUNCTION(BlueprintCallable, Category = "RGBRaymarcher",
            meta = (WorldContext = "WorldContextObject"))

  static void AddDirLightToVolumes(const UObject* WorldContextObject,
                                   const FBasicRaymarchRenderingResources Resources,
                                   const FColorVolumesResources ColorResources,
                                   const FDirLightParameters LightParameters, const bool Added,
                                   const FRaymarchWorldParameters WorldParameters,
                                   bool& LightAdded);

  /** Changes a light in the light volumes.	 */
  UFUNCTION(BlueprintCallable, Category = "RGBRaymarcher",
            meta = (WorldContext = "WorldContextObject"))
  static void ChangeDirLightInLightVolumes(const UObject* WorldContextObject,
                                           const FBasicRaymarchRenderingResources Resources,
                                           const FColorVolumesResources ColorResources,
                                           const FDirLightParameters OldLightParameters,
                                           const FDirLightParameters NewLightParameters,
                                           const FRaymarchWorldParameters WorldParameters,
                                           bool& LightAdded);

  /** Clear all light volumes.	 */
  UFUNCTION(BlueprintCallable, Category = "RGBRaymarcher",
            meta = (WorldContext = "WorldContextObject"))
  static void ClearLightVolumes(const UObject* WorldContextObject, UVolumeTexture* RLightVolume,
                                UVolumeTexture* GLightVolume, UVolumeTexture* BLightVolume,
                                UVolumeTexture* ALightVolume, FVector4 ClearValues);

  /** Sets the provided Volume Texture sizes to "Dimensions" changes them to be Float32, and sets
   * them to all-zeros.*/
  UFUNCTION(BlueprintCallable, Category = "RGBRaymarcher",
            meta = (WorldContext = "WorldContextObject"))
  static void CreateLightVolumes(const UObject* WorldContextObject, FIntVector Dimensions,
                                 UVolumeTexture* inRTexture, UVolumeTexture* inGTexture,
                                 UVolumeTexture* inBTexture, UVolumeTexture* inATexture);

  UFUNCTION(BlueprintCallable, Category = "Raymarcher",
            meta = (WorldContext = "WorldContextObject"))
  static void InitLightVolume(UVolumeTexture* LightVolume, FIntVector Dimensions);

  //
  //
  // Functions for working with a single-channel (just alpha) light volume follow.
  //
  //

  /** Adds a light to light volume.	 */
  UFUNCTION(BlueprintCallable, Category = "RGBRaymarcher",
            meta = (WorldContext = "WorldContextObject"))
  static void AddDirLightToSingleVolume(const UObject* WorldContextObject,
                                        const FBasicRaymarchRenderingResources Resources,
                                        const FDirLightParameters LightParameters, const bool Added,
                                        const FRaymarchWorldParameters WorldParameters,
                                        bool& LightAdded, FVector& LocalLightDir);

  /** Changes a light in the light volume.	 */
  UFUNCTION(BlueprintCallable, Category = "RGBRaymarcher",
            meta = (WorldContext = "WorldContextObject"))
  static void ChangeDirLightInSingleVolume(const UObject* WorldContextObject,
                                           FBasicRaymarchRenderingResources Resources,
                                           const FDirLightParameters OldLightParameters,
                                           const FDirLightParameters NewLightParameters,
                                           const FRaymarchWorldParameters WorldParameters,
                                           bool& LightAdded, FVector& LocalLightDir);

  /** Clears a light volume. */
  UFUNCTION(BlueprintCallable, Category = "RGBRaymarcher",
            meta = (WorldContext = "WorldContextObject"))
  static void ClearSingleLightVolume(const UObject* WorldContextObject,
                                     UVolumeTexture* ALightVolume, float ClearValue);

  /** Clears a light volume. */
  UFUNCTION(BlueprintCallable, Category = "RGBRaymarcher",
            meta = (WorldContext = "WorldContextObject"))
  static void ClearResourceLightVolumes(const UObject* WorldContextObject,
                                        FBasicRaymarchRenderingResources Resources,
                                        float ClearValue);

  /** Creates a Float32 volume texture asset and fills it with all-zeros. If an asset with the same
   * name already exists, overwrites it.*/
  UFUNCTION(BlueprintCallable, Category = "Raymarcher",
            meta = (WorldContext = "WorldContextObject"))
  static void CreateLightVolumeAsset(const UObject* WorldContextObject, FString textureName,
                                     FIntVector Dimensions, UVolumeTexture*& CreatedVolume);

  //
  //
  // Functions for loading RAW and MHD files into textures follow.
  //
  //

  /** Loads a RAW file into a provided Volume Texture. Will output error log messages
   * and return if unsuccessful */
  UFUNCTION(BlueprintCallable, Category = "Raymarcher",
            meta = (WorldContext = "WorldContextObject"))
  static void LoadRawVolumeIntoVolumeTexture(const UObject* WorldContextObject, FString textureName,
                                             FIntVector Dimensions, UVolumeTexture* inTexture);

  /** Loads a RAW file into a newly created Volume Texture Asset. Will output error log messages
   * and return if unsuccessful.
   * @param FileName is supposed to be the absolute path of where the raw file can be found.
   * @param SaveAsset whether to save the asset to disk right away
   */
  UFUNCTION(BlueprintCallable, Category = "Raymarcher",
            meta = (WorldContext = "WorldContextObject"))
  static void LoadRawVolumeIntoVolumeTextureAsset(const UObject* WorldContextObject,
                                                  FString FileName, FIntVector Dimensions,
                                                  FString TextureName, bool SaveAsset,
                                                  UVolumeTexture*& LoadedTexture);
  
  /** Loads a MHD file into a newly created Volume Texture Asset. Returns the loaded texture, it's
  world dimensions and texture dimensions.  **/
  UFUNCTION(BlueprintCallable, Category = "Raymarcher",
            meta = (WorldContext = "WorldContextObject"))
  static void LoadMhdFileIntoVolumeTextureAsset(const UObject* WorldContextObject, FString FileName,
                                                FString TextureName, bool Persistent,
                                                FIntVector& TextureDimensions,
                                                FVector& WorldDimensions,
                                                UVolumeTexture*& LoadedTexture);
  //
  //
  // Functions for handling transfer functions and color curves follow.
  //
  //

  /** Will create a 1D texture asset from a ColorCurve. xDim is the number of samples. */
  UFUNCTION(BlueprintCallable, Category = "Raymarcher",
            meta = (WorldContext = "WorldContextObject"))
  static void ExportColorCurveToTexture(const UObject* WorldContextObject, UCurveLinearColor* Curve,
                                        FString TextureName);

  /** Will create a 1D texture asset from a ColorCurve. xDim is the number of samples. */
  UFUNCTION(BlueprintCallable, Category = "Raymarcher",
            meta = (WorldContext = "WorldContextObject"))
  static void ColorCurveToTextureRanged(const UObject* WorldContextObject, UCurveLinearColor* Curve,
                                        UTexture2D* Texture,
                                        FTransferFunctionRangeParameters parameters);

  //
  //
  // Functions for creating parameter collections follow
  //
  //

  UFUNCTION(BlueprintCallable, Category = "Raymarcher",
            meta = (WorldContext = "WorldContextObject"))
  static void CreateBasicRaymarchingResources(
      const UObject* WorldContextObject, UVolumeTexture* Volume, UVolumeTexture* ALightVolume,
      UTexture2D* TransferFunction, FTransferFunctionRangeParameters TFRangeParams,  bool HalfResolution,
      const bool ColoredLightSupport,
      struct FBasicRaymarchRenderingResources& OutParameters);  //

  /** Loads a RAW 3D texture into this classes FRHITexture3D member. Will output error log messages
   * and return if unsuccessful */
  UFUNCTION(BlueprintCallable, Category = "Raymarcher",
            meta = (WorldContext = "WorldContextObject"))
  static void ReadTransferFunctionFromFile(const UObject* WorldContextObject, FString TextFileName,
                                           UCurveLinearColor*& OutColorCurve);  //

  //
  //
  // Experimental and utility functions follow.
  //
  //

  /**
   * Generates Volume texture higher mipmap levels. Directly uses values transformed by the transfer function and saves max value for each level.
   */
  UFUNCTION(BlueprintCallable, Category = "Raymarcher",
	  meta = (WorldContext = "WorldContextObject"))
	  static void GenerateVolumeTextureMipLevels(const UObject* WorldContextObject, FIntVector Dimensions,
		  UVolumeTexture* inTexture, UTexture2D* TransferFunction, bool& success);

  UFUNCTION(BlueprintCallable, Category = "Raymarcher",
	  meta = (WorldContext = "WorldContextObject"))
	  static void GenerateDistanceField(const UObject* WorldContextObject, FIntVector Dimensions,
		  UVolumeTexture* inTexture, UTexture2D* TransferFunction, UVolumeTexture* SDFTexture, float localSphereDiameter, float threshold, bool& success);


  /** Will write pure white on the first layer (z = 0) of the texture */
  UFUNCTION(BlueprintCallable, Category = "Raymarcher",
            meta = (WorldContext = "WorldContextObject"))
  static void TryVolumeTextureSliceWrite(const UObject* WorldContextObject, FIntVector Dimensions,
                                         UVolumeTexture* inTexture);

  /** Logs a string to the on-screen debug messages */
  UFUNCTION(BlueprintCallable, Category = "Raymarcher",
            meta = (WorldContext = "WorldContextObject"))
  static void CustomLog(const UObject* WorldContextObject, FString LoggedString, float Duration);

  /** Dets volume texture dimension. */
  UFUNCTION(BlueprintCallable, Category = "Raymarcher",
            meta = (WorldContext = "WorldContextObject"))
  static void GetVolumeTextureDimensions(const UObject* WorldContextObject, UVolumeTexture* Texture,
                                         FIntVector& Dimensions);

  /** Transforms a transform to a matrix. */
  UFUNCTION(BlueprintPure, Category = "Raymarcher", meta = (WorldContext = "WorldContextObject"))
  static void TransformToMatrix(const UObject* WorldContextObject, const FTransform Transform,
                                FMatrix& OutMatrix);

  /** Changes the transfer function & parameters in the resources struct. This is needed because in blueprints, you cannot change a single 
	  member of a struct and because of hidden members, creating a new struct also doesn't work (unless you'd recreate all the resources, 
	  which would be a waste). Maybe solve this later by taking the TFRangeParameters out of the BasicRaymarchResources struct.
  */
  UFUNCTION(BlueprintCallable, Category = "Raymarcher", meta = (WorldContext = "WorldContextObject"))
  static void ChangeTFInResources(const UObject* WorldContextObject,  FBasicRaymarchRenderingResources Resources, UTexture2D* TFTexture,  FTransferFunctionRangeParameters TFParameters, FBasicRaymarchRenderingResources& OutResources);

  /** Transforms the first player's viewport size on the screen. All values normalized to 0-1.*/
  UFUNCTION(BlueprintCallable, Category = "Raymarcher", meta = (WorldContext = "WorldContextObject"))
	  static void ChangeViewportProperties(const UObject* WorldContextObject, FVector2D Origin, FVector2D Size);




};
