// (C) Technical University of Munich - Computer Aided Medical Procedures
// Developed by Tomas Bartipan (tomas.bartipan@tum.de)

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Classes/Kismet/BlueprintFunctionLibrary.h"
#include "Classes/Engine/VolumeTexture.h"
#include "Classes/Curves/CurveLinearColor.h"
#include "RHI.h"
#include "RHIResources.h"
#include "RaymarchRendering.h"
#include "RaymarchBlueprintLibrary.generated.h"

UCLASS()
class URaymarchBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	/** Adds a light to light volumes.	 */
	UFUNCTION(BlueprintCallable, Category = "Raymarcher", meta = (WorldContext = "WorldContextObject"))
		static void AddDirLightToVolumes(
			const UObject* WorldContextObject,
			UVolumeTexture* RLightVolume,
			UVolumeTexture* GLightVolume,
			UVolumeTexture* BLightVolume,
			UVolumeTexture* ALightVolume,
			const UVolumeTexture* DataVolume,
			const UTexture2D* TransferFunction,
			const FDirLightParameters LightParameters,
			const bool Added,
			bool& LightAdded);

	/** Changes a light in the light volumes.	 */
	UFUNCTION(BlueprintCallable, Category = "Raymarcher", meta = (WorldContext = "WorldContextObject"))
		static void ChangeDirLightInLightVolumes(
			const UObject* WorldContextObject,
			UVolumeTexture* RLightVolume,
			UVolumeTexture* GLightVolume,
			UVolumeTexture* BLightVolume,
			UVolumeTexture* ALightVolume,
			const UVolumeTexture* DataVolume,
			const UTexture2D* TransferFunction,
			FDirLightParameters OldLightParameters,
			FDirLightParameters NewLightParameters,
			bool& LightAdded);


	/** Changes a light in the light volumes.	 */
	UFUNCTION(BlueprintCallable, Category = "Raymarcher", meta = (WorldContext = "WorldContextObject"))
		static void ClearLightVolumes(
			const UObject* WorldContextObject,
			UVolumeTexture* RLightVolume,
			UVolumeTexture* GLightVolume,
			UVolumeTexture* BLightVolume,
			UVolumeTexture* ALightVolume,
			FVector4 ClearValues);

	/** Loads a RAW 3D texture into this classes FRHITexture3D member. Will output error log messages and return if unsuccessful */
	UFUNCTION(BlueprintCallable, Category = "Raymarcher", meta = (WorldContext = "WorldContextObject"))
		static void LoadRawVolumeIntoVolumeTexture(
			const UObject* WorldContextObject,
			FString textureName, 
			FIntVector Dimensions, 
			UVolumeTexture* inTexture);

	/** Loads a RAW 3D texture into this classes FRHITexture3D member. Will output error log messages and return if unsuccessful */
	UFUNCTION(BlueprintCallable, Category = "Raymarcher", meta = (WorldContext = "WorldContextObject"))
		static void CreateLightVolumes(
			const UObject* WorldContextObject,
			FIntVector Dimensions, 
			UVolumeTexture* inRTexture, 
			UVolumeTexture* inGTexture, 
			UVolumeTexture* inBTexture, 
			UVolumeTexture* inATexture);
	
	/** Loads a RAW 3D texture into this classes FRHITexture3D member. Will output error log messages and return if unsuccessful */
	UFUNCTION(BlueprintCallable, Category = "Raymarcher", meta = (WorldContext = "WorldContextObject"))
		static void LoadRawVolumeIntoVolumeTextureAsset(
			const UObject* WorldContextObject, 
			FString FileName, 
			FIntVector Dimensions, 
			FString TextureName,
			UVolumeTexture*& LoadedTexture);

	/** Loads a RAW 3D texture into this classes FRHITexture3D member. Will output error log messages and return if unsuccessful */
	UFUNCTION(BlueprintCallable, Category = "Raymarcher", meta = (WorldContext = "WorldContextObject"))
		static void LoadMhdFileIntoVolumeTextureAsset(
			const UObject* WorldContextObject,
			FString FileName, 
			FString TextureName,
			FIntVector& TextureDimensions,
			FVector& WorldDimensions,
			UVolumeTexture*& LoadedTexture);
	
	/** Will write pure white on the first layer (z = 0) of the texture */
	UFUNCTION(BlueprintCallable, Category = "Raymarcher", meta = (WorldContext = "WorldContextObject"))
		static void TryVolumeTextureSliceWrite(
			const UObject* WorldContextObject,
			FIntVector Dimensions, 
			UVolumeTexture* inTexture);
	
	/** Will create a 1D texture asset from a ColorCurve. xDim is the number of samples. */
	UFUNCTION(BlueprintCallable, Category = "Raymarcher", meta = (WorldContext = "WorldContextObject"))
		static void ExportColorCurveToTexture(
			const UObject* WorldContextObject,
			UCurveLinearColor* Curve,
			int xDim, 
			FString TextureName);
	
	/** Loads a RAW 3D texture into this classes FRHITexture3D member. Will output error log messages and return if unsuccessful */
	UFUNCTION(BlueprintCallable, Category = "Raymarcher", meta = (WorldContext = "WorldContextObject"))
		static void CreateLightVolumeAsset(
			const UObject* WorldContextObject,
			FString textureName, 
			FIntVector Dimensions, 
			UVolumeTexture*& CreatedVolume);
	
	/** Loads a RAW 3D texture into this classes FRHITexture3D member. Will output error log messages and return if unsuccessful */
	UFUNCTION(BlueprintCallable, Category = "Raymarcher", meta = (WorldContext = "WorldContextObject"))
		static void ReadTransferFunctionFromFile(
			const UObject* WorldContextObject,
			FString TextFileName,
			UCurveLinearColor*& OutColorCurve);

	/** Loads a RAW 3D texture into this classes FRHITexture3D member. Will output error log messages and return if unsuccessful */
	UFUNCTION(BlueprintCallable, Category = "Raymarcher", meta = (WorldContext = "WorldContextObject"))
		static void CustomLog(
			const UObject* WorldContextObject,
			FString LoggedString,
			float Duration);




};