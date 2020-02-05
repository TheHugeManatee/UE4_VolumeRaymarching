// (C) Technical University of Munich - Computer Aided Medical Procedures
// Developed by Tomas Bartipan (tomas.bartipan@tum.de)

// Contains functions for creating and updating volume texture assets. Also contains helper
// functions for loading and reading MHD and RAW files.

#pragma once

#include "CoreMinimal.h"

#include "Engine.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/VolumeTexture.h"
#include "Engine/World.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Logging/MessageLog.h"
#include "PipelineStateCache.h"
#include "RHIStaticStates.h"
#include "SceneInterface.h"
#include "SceneUtils.h"
#include "UObject/ObjectMacros.h"

/** Creates a Volume Texture asset with the given name, pixel format and dimensions and fills it
  with the bulk data provided. It can be set to be persistent and UAV compatible and can also
  be immediately saved to disk.
  Returns a reference to the created texture in the CreatedTexture param.
*/
bool CreateVolumeTextureAsset(FString AssetName, EPixelFormat PixelFormat, FIntVector Dimensions,
                              UVolumeTexture*& CreatedTexture, uint8* BulkData = nullptr,
                              bool Persistent = false, bool SaveNow = false,
                              bool UAVCompatible = false);

/** Updates the provided Volume Texture asset to have the provided format, dimensions and pixel
 * data*/
bool UpdateVolumeTextureAsset(UVolumeTexture* VolumeTexture, EPixelFormat PixelFormat,
                              FIntVector Dimensions, uint8* BulkData = nullptr,
                              bool Persistent = false, bool SaveNow = false,
                              bool UAVCompatible = false);

/** Creates a 2D Texture asset with the given name from the provided bulk data with the given
 * format.*/
bool Create2DTextureAsset(FString AssetName, EPixelFormat PixelFormat, FIntPoint Dimensions,
                          uint8* BulkData = nullptr, bool Persistent = false,
                          bool UAVCompatible = false, bool SaveNow = false,
                          TextureAddress TilingX = TA_Clamp, TextureAddress TilingY = TA_Clamp);

/** Updates the provided 2D Texture asset to have the provided format, dimensions and pixel data*/
bool Update2DTextureAsset(UTexture2D* Texture, EPixelFormat PixelFormat, FIntPoint Dimensions,
                          uint8* BulkData = nullptr, bool Persistent = false,
                          bool UAVCompatible = false, TextureAddress TilingX = TA_Clamp,
                          TextureAddress TilingY = TA_Clamp);

/** Handles the saving of source data to persistent textures. Only works in-editor, as packaged
 builds no longer have source data for textures.*/
bool HandleTextureEditorData(UTexture* Texture, const EPixelFormat PixelFormat,
                             const bool Persistent, const FIntVector Dimensions,
                             const uint8* BulkData);

/**
 */
TUniquePtr<uint8> LoadRawFileIntoArray(const FString FileName, const int64 BytesToLoad);

ETextureSourceFormat PixelFormatToSourceFormat(EPixelFormat PixelFormat);
