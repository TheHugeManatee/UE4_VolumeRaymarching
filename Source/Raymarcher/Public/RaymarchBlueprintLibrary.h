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
  // Functions for working with a single-channel (just alpha) light volume follow.
  //
  //

  UFUNCTION(BlueprintCallable, Category = "RGBRaymarcher")
  static void InitLightVolume(UVolumeTexture* LightVolume, FIntVector Dimensions);

  /** Adds a light to light volume.	 */
  UFUNCTION(BlueprintCallable, Category = "Raymarcher")
  static void AddDirLightToSingleVolume(const FBasicRaymarchRenderingResources Resources,
                                        const FDirLightParameters LightParameters, const bool Added,
                                        const FRaymarchWorldParameters WorldParameters,
                                        bool& LightAdded, FVector& LocalLightDir);

  /** Adds a light to light volume.	 */
  UFUNCTION(BlueprintCallable, Category = "Raymarcher")
  static void CreateLightVolumeUAV(FBasicRaymarchRenderingResources Resources,
                                   FBasicRaymarchRenderingResources& OutResources, bool& Success);

  /** Changes a light in the light volume.	 */
  UFUNCTION(BlueprintCallable, Category = "Raymarcher")
  static void ChangeDirLightInSingleVolume(FBasicRaymarchRenderingResources Resources,
                                           const FDirLightParameters OldLightParameters,
                                           const FDirLightParameters NewLightParameters,
                                           const FRaymarchWorldParameters WorldParameters,
                                           bool& LightAdded, FVector& LocalLightDir);

  /** Clears a light volume. */
  UFUNCTION(BlueprintCallable, Category = "Raymarcher")
  static void ClearSingleLightVolume(UVolumeTexture* ALightVolume, float ClearValue);

  /** Clears a light volume in provided raymarch resources. */
  UFUNCTION(BlueprintCallable, Category = "Raymarcher")
  static void ClearResourceLightVolumes(FBasicRaymarchRenderingResources Resources,
                                        float ClearValue);

  /** Creates a G8 volume texture asset and fills it with all-zeros. If an asset with the same
   * name already exists, overwrites it.*/
  UFUNCTION(BlueprintCallable, Category = "Raymarcher")
  static void CreateLightVolumeAsset(FString textureName, FIntVector Dimensions,
                                     UVolumeTexture*& CreatedVolume);

  //
  //
  // Functions for loading RAW and MHD files into textures follow.
  //
  //

  /** Loads a RAW file into a provided Volume Texture. Will output error log messages
   * and return if unsuccessful */
  UFUNCTION(BlueprintCallable, Category = "Raymarcher")
  static void LoadRawIntoVolumeTextureAsset(FString RawFileName, UVolumeTexture* inTexture,
                                            FIntVector Dimensions, bool Persistent);

  /** Loads a RAW file into a newly created Volume Texture Asset. Will output error log messages
   * and return if unsuccessful.
   * @param FileName is supposed to be the absolute path of where the raw file can be found.
   * @param SaveAsset whether to save the asset to disk right away
   */
  UFUNCTION(BlueprintCallable, Category = "Raymarcher")
  static void LoadRawIntoNewVolumeTextureAsset(FString RawFileName, FString TextureName,
                                               FIntVector Dimensions, bool Persistent,
                                               UVolumeTexture*& LoadedTexture);

  /** Loads a MHD file into a newly created Volume Texture Asset. Returns the loaded texture, it's
  world dimensions and texture dimensions.  **/
  UFUNCTION(BlueprintCallable, Category = "Raymarcher")
  static void LoadMhdIntoNewVolumeTextureAsset(FString FileName, FString TextureName,
                                               bool Persistent, FIntVector& TextureDimensions,
                                               FVector& WorldDimensions,
                                               UVolumeTexture*& LoadedTexture);

  /** Loads a MHD file into a newly created Volume Texture Asset. Returns the loaded texture, it's
  world dimensions and texture dimensions.  **/
  UFUNCTION(BlueprintCallable, Category = "Raymarcher")
  static void LoadMhdIntoVolumeTextureAsset(FString FileName, UVolumeTexture* VolumeAsset,
                                            bool Persistent, FIntVector& TextureDimensions,
                                            FVector& WorldDimensions,
                                            UVolumeTexture*& LoadedTexture);

  //
  //
  // Functions for handling transfer functions and color curves follow.
  //
  //

  /** Will create a 1D texture asset from a ColorCurve. xDim is the number of samples. */
  UFUNCTION(BlueprintCallable, Category = "Raymarcher")
  static void ColorCurveToTexture(UCurveLinearColor* Curve, UTexture2D* Texture);

  /** Will create a 1D texture asset from a ColorCurve. xDim is the number of samples. */
  UFUNCTION(BlueprintCallable, Category = "Raymarcher")
  static void ColorCurveToTextureRanged(UCurveLinearColor* Curve, UTexture2D* Texture,
                                        FTransferFunctionRangeParameters parameters);

  //
  //
  // Functions for creating parameter collections follow
  //
  //

  UFUNCTION(BlueprintCallable, Category = "Raymarcher")
  static void CreateBasicRaymarchingResources(UVolumeTexture* Volume, UVolumeTexture* ALightVolume,
                                              UTexture2D* TransferFunction,
                                              FTransferFunctionRangeParameters TFRangeParams,
                                              bool HalfResolution, const bool ColoredLightSupport,
                                              FBasicRaymarchRenderingResources& OutParameters);  //

  //

  UFUNCTION(BlueprintCallable, Category = "Raymarcher")
  static void CheckBasicRaymarchingResources(FBasicRaymarchRenderingResources OutParameters);  //

  /** Loads a RAW 3D texture into this classes FRHITexture3D member. Will output error log messages
   * and return if unsuccessful */
  UFUNCTION(BlueprintCallable, Category = "Raymarcher")
  static void ReadTransferFunctionFromFile(FString TextFileName,
                                           UCurveLinearColor*& OutColorCurve);  //

  //
  //
  // Experimental and utility functions follow.
  //
  //

  /**
   * Generates Volume texture higher mipmap levels. Directly uses values transformed by the transfer
   * function and saves max value for each level.
   */
  UFUNCTION(BlueprintCallable, Category = "Raymarcher")
  static void GenerateVolumeTextureMipLevels(FIntVector Dimensions, UVolumeTexture* inTexture,
                                             UTexture2D* TransferFunction, bool& success);

  UFUNCTION(BlueprintCallable, Category = "Raymarcher")
  static void GenerateDistanceField(FIntVector Dimensions, UVolumeTexture* inTexture,
                                    UTexture2D* TransferFunction, UVolumeTexture* SDFTexture,
                                    float localSphereDiameter, float threshold, bool& success);

  /** Will write pure white on the first layer (z = 0) of the texture */
  UFUNCTION(BlueprintCallable, Category = "Raymarcher")
  static void TryVolumeTextureSliceWrite(FIntVector Dimensions, UVolumeTexture* inTexture);

  /** Logs a string to the on-screen debug messages */
  UFUNCTION(BlueprintCallable, Category = "Raymarcher")
  static void CustomLog(FString LoggedString, float Duration);

  /** Gets volume texture dimension. */
  UFUNCTION(BlueprintPure, Category = "Raymarcher")
  static void GetVolumeTextureDimensions(UVolumeTexture* Texture, FIntVector& Dimensions);

  /** Transforms a transform to a matrix. */
  UFUNCTION(BlueprintPure, Category = "Raymarcher")
  static void TransformToMatrix(const FTransform Transform, FMatrix& OutMatrix, bool WithScaling);

  /** Changes the transfer function & parameters in the resources struct. This is needed because in
    blueprints, you cannot change a single member of a struct and because of hidden members,
    creating a new struct also doesn't work (unless you'd recreate all the resources, which would be
    a waste). Maybe solve this later by taking the TFRangeParameters out of the
    BasicRaymarchResources struct. Or doing stuff in C++...
  */
  UFUNCTION(BlueprintCallable, Category = "Raymarcher")
  static void ChangeTFInResources(FBasicRaymarchRenderingResources Resources, UTexture2D* TFTexture,
                                  FTransferFunctionRangeParameters TFParameters,
                                  FBasicRaymarchRenderingResources& OutResources);

  /** Transforms the first player's viewport size on the screen. All values normalized to 0-1.*/
  UFUNCTION(BlueprintCallable, Category = "Raymarcher")
  static void ChangeViewportProperties(FVector2D Origin, FVector2D Size);

  /** Transforms the first player's viewport size on the screen. All values normalized to 0-1.*/
  UFUNCTION(BlueprintPure, Category = "Raymarcher")
  static void GetCutplaneMaterialParams(FCubeFace DominantFace, FVector& Origin,
                                        FVector& UMultiplier, FVector& SliceMultiplier);

  /** Returns the face whose inverted normal is closest to the provided vector (in local space).
    E.G - local vector (0,0,1) will return Bottom (as that side's normal is (0,0,-1)).
  */
  UFUNCTION(BlueprintPure, Category = "Raymarcher")
  static void GetDominantFace(FVector LocalDirectionVector, FCubeFace& DominantFace);

  /** Returns the face whose inverted normal is closest to the provided vector (in local space).
    E.G - local vector (0,0,1) will return Bottom (as that side's normal is (0,0,-1)).
    Will only return faces that are not +-X
  */
  UFUNCTION(BlueprintPure, Category = "Raymarcher")
  static void GetDominantFaceNotX(FVector LocalDirectionVector, FCubeFace& DominantFace);

  /**
    Returns a face normal in local space.
   */
  UFUNCTION(BlueprintPure, Category = "Raymarcher")
  static void GetFaceNormal(FCubeFace CubeFace, FVector& FaceNormalLocal);

  /**
  Returns a face that is to the right of the input face, when the cube is set with negative X
  facing upwards and we're looking from the front.
  */
  UFUNCTION(BlueprintPure, Category = "Raymarcher")
  static void GetRightFaceAlongNegX(FCubeFace CubeFace, FCubeFace& RightCubeFace);

  /**
  Initializes the provided 2D texture to be compatible with writing a slice of the provided
  Volume Texture along X.
  */
  UFUNCTION(BlueprintCallable, Category = "Raymarcher")
  static void Initialize2DTextureForSliceWrite(UVolumeTexture* VolumeTexture,
                                               UTexture2D* WrittenSliceTexture);
  /**
  Writes the specified layer of the	volume texture into the provided 2D Texture.
  Will fail if VolumeTexture doesn't have that many layers or if WrittenSliceTexture is
  not UAV compatible.
  */
  UFUNCTION(BlueprintCallable, Category = "Raymarcher")
  static void WriteVolumeTextureSlice(UVolumeTexture* VolumeTexture,
                                      UTexture2D* WrittenSliceTexture, int Layer);

  /**
   Given a volume texture, this will copy the current texture content into the texture source.   This causes the current contents of the texture to be saved when you hit the save button,   instead of the source content this texture was created with.  */
  // UFUNCTION(BlueprintCallable, Category = "Raymarcher")
  // static void UpdateVolumeTextureSource(UVolumeTexture* VolumeTexture);
};
