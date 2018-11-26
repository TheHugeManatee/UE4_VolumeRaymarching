// (C) Technical University of Munich - Computer Aided Medical Procedures
// Developed by Tomas Bartipan (tomas.bartipan@tum.de)

#include "../Public/RaymarchBlueprintLibrary.h"
#include <cstdio>
#include "../Public/MhdParser.h"
#include "../Public/RaymarchRendering.h"
#include "Classes/Engine/World.h"
#include "Public/AssetRegistryModule.h"
#include "Public/GlobalShader.h"
#include "Public/Logging/MessageLog.h"
#include "Public/PipelineStateCache.h"
#include "Public/RHIDefinitions.h"
#include "Public/RHIStaticStates.h"
#include "Public/SceneInterface.h"
#include "Public/SceneUtils.h"
#include "Public/ShaderParameterUtils.h"
#include "RHICommandList.h"
#include "UnrealString.h"

#define LOCTEXT_NAMESPACE "RaymarchPlugin"

bool dooone = false;

void URaymarchBlueprintLibrary::AddDirLightToVolumes(
    const UObject* WorldContextObject, UVolumeTexture* RLightVolume, UVolumeTexture* GLightVolume,
    UVolumeTexture* BLightVolume, UVolumeTexture* ALightVolume, const UVolumeTexture* DataVolume,
    const UTexture2D* TransferFunction, const FDirLightParameters LightParameters, const bool Added, 			
	const FTransform VolumeInvTransform,
	const FVector LocalClippingCenter,
	const FVector LocalClippingDirection, bool& LightAdded) {
  if (!DataVolume->Resource || !TransferFunction->Resource || !RLightVolume->Resource ||
      !GLightVolume->Resource || !BLightVolume->Resource || !ALightVolume->Resource ||
      !DataVolume->Resource->TextureRHI || !TransferFunction->Resource->TextureRHI ||
      !RLightVolume->Resource->TextureRHI || !GLightVolume->Resource->TextureRHI ||
      !BLightVolume->Resource->TextureRHI || !ALightVolume->Resource->TextureRHI) {
    LightAdded = false;
    return;
  } else {
    LightAdded = true;
  }

  FRHITexture3D* VolumeResource = DataVolume->Resource->TextureRHI->GetTexture3D();
  FRHITexture2D* TFResource = TransferFunction->Resource->TextureRHI->GetTexture2D();
  FRHITexture3D* RLightVolumeResource = RLightVolume->Resource->TextureRHI->GetTexture3D();
  FRHITexture3D* GLightVolumeResource = GLightVolume->Resource->TextureRHI->GetTexture3D();
  FRHITexture3D* BLightVolumeResource = BLightVolume->Resource->TextureRHI->GetTexture3D();
  FRHITexture3D* ALightVolumeResource = ALightVolume->Resource->TextureRHI->GetTexture3D();

  ERHIFeatureLevel::Type FeatureLevel = WorldContextObject->GetWorld()->Scene->GetFeatureLevel();

  // Call the actual rendering code on RenderThread.
  ENQUEUE_RENDER_COMMAND(CaptureCommand)
  ([RLightVolumeResource, GLightVolumeResource, BLightVolumeResource, ALightVolumeResource,
	  VolumeResource, TFResource, LightParameters, Added, VolumeInvTransform,
LocalClippingCenter,
LocalClippingDirection,
    FeatureLevel](FRHICommandListImmediate& RHICmdList) {
    AddDirLightToLightVolume_RenderThread(
        RHICmdList, RLightVolumeResource, GLightVolumeResource, BLightVolumeResource,
        ALightVolumeResource, VolumeResource, TFResource, LightParameters, Added, VolumeInvTransform, LocalClippingCenter, LocalClippingDirection, FeatureLevel);
  });
}

/** Changes a light in the light volumes.	 */
void URaymarchBlueprintLibrary::ChangeDirLightInLightVolumes(
    const UObject* WorldContextObject, UVolumeTexture* RLightVolume, UVolumeTexture* GLightVolume,
    UVolumeTexture* BLightVolume, UVolumeTexture* ALightVolume, const UVolumeTexture* DataVolume,
    const UTexture2D* TransferFunction, FDirLightParameters OldLightParameters,
    FDirLightParameters NewLightParameters,	const FTransform VolumeInvTransform,
	const FVector OldLocalClippingCenter,
	const FVector OldLocalClippingDirection,
	const FVector NewLocalClippingCenter,
	const FVector NewLocalClippingDirection,
bool& LightAdded) {
  if (!DataVolume->Resource || !TransferFunction->Resource || !RLightVolume->Resource ||
      !GLightVolume->Resource || !BLightVolume->Resource || !ALightVolume->Resource ||
      !DataVolume->Resource->TextureRHI || !TransferFunction->Resource->TextureRHI ||
      !RLightVolume->Resource->TextureRHI || !GLightVolume->Resource->TextureRHI ||
      !BLightVolume->Resource->TextureRHI || !ALightVolume->Resource->TextureRHI) {
    LightAdded = false;
    return;
  } else {
    LightAdded = true;
  }

  FRHITexture3D* VolumeResource = DataVolume->Resource->TextureRHI->GetTexture3D();
  FRHITexture2D* TFResource = TransferFunction->Resource->TextureRHI->GetTexture2D();
  FRHITexture3D* RLightVolumeResource = RLightVolume->Resource->TextureRHI->GetTexture3D();
  FRHITexture3D* GLightVolumeResource = GLightVolume->Resource->TextureRHI->GetTexture3D();
  FRHITexture3D* BLightVolumeResource = BLightVolume->Resource->TextureRHI->GetTexture3D();
  FRHITexture3D* ALightVolumeResource = ALightVolume->Resource->TextureRHI->GetTexture3D();

  ERHIFeatureLevel::Type FeatureLevel = WorldContextObject->GetWorld()->Scene->GetFeatureLevel();

  // Call the actual rendering code on RenderThread.
  ENQUEUE_RENDER_COMMAND(CaptureCommand)
  ([RLightVolumeResource, GLightVolumeResource, BLightVolumeResource, ALightVolumeResource,
    VolumeResource, TFResource, OldLightParameters, NewLightParameters, VolumeInvTransform, OldLocalClippingCenter, OldLocalClippingDirection, NewLocalClippingCenter, NewLocalClippingDirection,
    FeatureLevel](FRHICommandListImmediate& RHICmdList) {
    ChangeDirLightInLightVolume_RenderThread(RHICmdList, RLightVolumeResource, GLightVolumeResource,
                                             BLightVolumeResource, ALightVolumeResource,
                                             VolumeResource, TFResource, OldLightParameters,
                                             NewLightParameters, VolumeInvTransform, OldLocalClippingCenter, OldLocalClippingDirection, NewLocalClippingCenter, NewLocalClippingDirection, FeatureLevel);
  });
}

void URaymarchBlueprintLibrary::ClearLightVolumes(const UObject* WorldContextObject,
                                                  UVolumeTexture* RLightVolume,
                                                  UVolumeTexture* GLightVolume,
                                                  UVolumeTexture* BLightVolume,
                                                  UVolumeTexture* ALightVolume,
                                                  FVector4 ClearValues /*= FVector4(0,0,0,0)*/) {
  ERHIFeatureLevel::Type FeatureLevel = WorldContextObject->GetWorld()->Scene->GetFeatureLevel();

  FRHITexture3D* RLightVolumeResource = RLightVolume->Resource->TextureRHI->GetTexture3D();
  FRHITexture3D* GLightVolumeResource = GLightVolume->Resource->TextureRHI->GetTexture3D();
  FRHITexture3D* BLightVolumeResource = BLightVolume->Resource->TextureRHI->GetTexture3D();
  FRHITexture3D* ALightVolumeResource = ALightVolume->Resource->TextureRHI->GetTexture3D();

  // Call the actual rendering code on RenderThread.
  ENQUEUE_RENDER_COMMAND(CaptureCommand)
  ([RLightVolumeResource, GLightVolumeResource, BLightVolumeResource, ALightVolumeResource,
    ClearValues, FeatureLevel](FRHICommandListImmediate& RHICmdList) {
    ClearLightVolumes_RenderThread(RHICmdList, RLightVolumeResource, GLightVolumeResource,
                                   BLightVolumeResource, ALightVolumeResource, ClearValues,
                                   FeatureLevel);
  });
}

/** Loads a RAW 3D volume (G8 format) into the provided Volume Texture asset. Will output error log
 * messages and return if unsuccessful */
void URaymarchBlueprintLibrary::LoadRawVolumeIntoVolumeTexture(const UObject* WorldContextObject,
                                                               FString textureName,
                                                               FIntVector Dimensions,
                                                               UVolumeTexture* inTexture) {
  const int TotalSize = Dimensions.X * Dimensions.Y * Dimensions.Z;

  IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

  FString RelativePath = FPaths::ProjectContentDir();
  FString FullPath =
      IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*RelativePath) + textureName;

  IFileHandle* FileHandle = PlatformFile.OpenRead(*FullPath);

  if (!FileHandle) {
    // Or not.
    MY_LOG("File could not be opened.");
    return;
  }
  MY_LOG("RAW file was opened!")

  if (FileHandle->Size() < TotalSize) {
    MY_LOG("File is smaller than expected, cannot read volume.")
    return;
  } else if (FileHandle->Size() > TotalSize) {
    MY_LOG(
        "File is larger than expected, check your dimensions and pixel format (nonfatal, but the "
        "texture will probably be screwed up)")
  }

  // Set volume texture parameters.
  inTexture->MipGenSettings = TMGS_LeaveExistingMips;
  inTexture->NeverStream = false;
  inTexture->CompressionNone = true;
  inTexture->SRGB = false;

  // Set PlatformData parameters (create PlatformData if it doesn't exist)
  if (!inTexture->PlatformData) {
    inTexture->PlatformData = new FTexturePlatformData();
  }
  inTexture->PlatformData->PixelFormat = PF_G8;
  inTexture->PlatformData->SizeX = Dimensions.X;
  inTexture->PlatformData->SizeY = Dimensions.Y;
  inTexture->PlatformData->NumSlices = Dimensions.Z;

  FTexture2DMipMap* mip = new FTexture2DMipMap();
  mip->SizeX = Dimensions.X;
  mip->SizeY = Dimensions.Y;
  mip->SizeZ = Dimensions.Z;
  mip->BulkData.Lock(LOCK_READ_WRITE);

  uint8* ByteArray = (uint8*)mip->BulkData.Realloc(TotalSize);
  FileHandle->Read(ByteArray, TotalSize);

  mip->BulkData.Unlock();

  // Close the RAW file.
  delete FileHandle;

  // Let the whole world know we were successful.
  MY_LOG("File was successfully read!");

  // If the texture already has MIPs in it, destroy and free them (Empty() calls destructors and
  // frees space).
  if (inTexture->PlatformData->Mips.Num() != 0) {
    inTexture->PlatformData->Mips.Empty();
  }

  // Add the new MIP.
  inTexture->PlatformData->Mips.Add(mip);
  inTexture->bUAVCompatible = true;

  inTexture->UpdateResource();
  return;
}

/** Loads a RAW 3D volume (G8 format) into the provided Volume Texture asset. Will output error log
 * messages and return if unsuccessful */
void URaymarchBlueprintLibrary::CreateLightVolumes(
    const UObject* WorldContextObject, FIntVector Dimensions, UVolumeTexture* inRTexture,
    UVolumeTexture* inGTexture, UVolumeTexture* inBTexture, UVolumeTexture* inATexture) {
  const int TotalSize = Dimensions.X * Dimensions.Y * Dimensions.Z * 4;
  dooone = false;
  UVolumeTexture* inTexture = nullptr;
  for (int i = 0; i < 4; i++) {
    switch (i) {
      case 0: inTexture = inRTexture; break;
      case 1: inTexture = inGTexture; break;
      case 2: inTexture = inBTexture; break;
      case 3: inTexture = inATexture; break;
      default: return;
    }

    if (!inTexture) break;

    // Set volume texture parameters.
    inTexture->MipGenSettings = TMGS_LeaveExistingMips;
    inTexture->NeverStream = false;
    inTexture->CompressionNone = true;
    inTexture->SRGB = false;

    // Set PlatformData parameters (create PlatformData if it doesn't exist)
    if (!inTexture->PlatformData) {
      inTexture->PlatformData = new FTexturePlatformData();
    }
    inTexture->PlatformData->PixelFormat = PF_R32_FLOAT;
    inTexture->PlatformData->SizeX = Dimensions.X;
    inTexture->PlatformData->SizeY = Dimensions.Y;
    inTexture->PlatformData->NumSlices = Dimensions.Z;

    FTexture2DMipMap* mip = new FTexture2DMipMap();
    mip->SizeX = Dimensions.X;
    mip->SizeY = Dimensions.Y;
    mip->SizeZ = Dimensions.Z;
    mip->BulkData.Lock(LOCK_READ_WRITE);

    uint8* ByteArray = (uint8*)mip->BulkData.Realloc(TotalSize);
    FMemory::Memset(ByteArray, 0, TotalSize);

    mip->BulkData.Unlock();

    // Let the whole world know we were successful.
    MY_LOG("File was successfully read!");

    // If the texture already has MIPs in it, destroy and free them (Empty() calls destructors and
    // frees space).
    if (inTexture->PlatformData->Mips.Num() != 0) {
      inTexture->PlatformData->Mips.Empty();
    }

    // Add the new MIP.
    inTexture->PlatformData->Mips.Add(mip);
    inTexture->bUAVCompatible = true;

    inTexture->UpdateResource();
  }
  return;
}

void URaymarchBlueprintLibrary::LoadRawVolumeIntoVolumeTextureAsset(
    const UObject* WorldContextObject, FString FileName, FIntVector Dimensions, FString TextureName,
    UVolumeTexture*& LoadedTexture) {
  const int TotalSize = Dimensions.X * Dimensions.Y * Dimensions.Z;

  IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

  FString RelativePath = FPaths::ProjectContentDir();
  FString FullPath =
      IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*RelativePath) + FileName;

  IFileHandle* FileHandle = PlatformFile.OpenRead(*FullPath);

  if (!FileHandle) {
    // Or not.
    MY_LOG("File could not be opened.");
    return;
  } else if (FileHandle->Size() < TotalSize) {
    MY_LOG("File is smaller than expected, cannot read volume.")
    delete FileHandle;
    return;
  } else if (FileHandle->Size() > TotalSize) {
    MY_LOG(
        "File is larger than expected, check your dimensions and pixel format (nonfatal, but the "
        "texture will probably be screwed up)")
  }

  uint8* TempArray = new uint8[TotalSize];
  FileHandle->Read(TempArray, TotalSize);
  // Let the whole world know we were successful.
  MY_LOG("File was successfully read!");

  // Actually create the asset.
  UVolumeTexture* OutTexture = nullptr;
  bool Success = CreateVolumeTextureAsset(TextureName, PF_G8, Dimensions, TempArray, false, false,
                                          &OutTexture);
  if (Success) {
    MY_LOG("Asset created and saved successfuly.")
  }
  LoadedTexture = OutTexture;
  // Close the RAW file and delete temp data.

  delete[] TempArray;
  delete FileHandle;

  return;
}

void URaymarchBlueprintLibrary::LoadMhdFileIntoVolumeTextureAsset(
    const UObject* WorldContextObject, FString FileName, FString TextureName,
    FIntVector& TextureDimensions, FVector& WorldDimensions, UVolumeTexture*& LoadedTexture) {
  FString RelativePath = FPaths::ProjectContentDir();
  FString FullPath =
      IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*RelativePath) + FileName;

  FString FileContent;
  if (!FFileHelper::LoadFileToString(/*out*/ FileContent, *FullPath)) {
    MY_LOG("Reading mhd file failed!");
    return;
  }

  FMhdInfo info = FMhdParser::ParseString(FileContent);

  WorldDimensions.X = info.Dimensions.X * info.Spacing.X;
  WorldDimensions.Y = info.Dimensions.Y * info.Spacing.Y;
  WorldDimensions.Z = info.Dimensions.Z * info.Spacing.Z;

  TextureDimensions = info.Dimensions;

  FString text =
      "Shit read from the MHD file :\nDimensions = " + FString::FromInt(info.Dimensions.X) + " " +
      FString::FromInt(info.Dimensions.Y) + " " + FString::FromInt(info.Dimensions.Z) +
      "\nSpacing : " + FString::SanitizeFloat(info.Spacing.X, 3) + " " +
      FString::SanitizeFloat(info.Spacing.Y, 3) + " " + FString::SanitizeFloat(info.Spacing.Z, 3) +
      "\nFinal Dimensions : " + FString::SanitizeFloat(WorldDimensions.X, 3) + " " +
      FString::SanitizeFloat(WorldDimensions.Y, 3) + " " +
      FString::SanitizeFloat(WorldDimensions.Z, 3);
  GEngine->AddOnScreenDebugMessage(-1, 20.f, FColor::Yellow, text);

  LoadRawVolumeIntoVolumeTextureAsset(WorldContextObject,
                                      FileName.Replace(TEXT(".mhd"), TEXT(".raw")), info.Dimensions,
                                      TextureName, LoadedTexture);

  return;
}

void URaymarchBlueprintLibrary::TryVolumeTextureSliceWrite(const UObject* WorldContextObject,
                                                           FIntVector Dimensions,
                                                           UVolumeTexture* inTexture) {
  ERHIFeatureLevel::Type FeatureLevel = WorldContextObject->GetWorld()->Scene->GetFeatureLevel();
  // Enqueue
  ENQUEUE_RENDER_COMMAND(CaptureCommand)
  ([Dimensions, inTexture, FeatureLevel](FRHICommandListImmediate& RHICmdList) {
    WriteTo3DTexture_RenderThread(RHICmdList, Dimensions, inTexture, FeatureLevel);
  });
}

void URaymarchBlueprintLibrary::ExportColorCurveToTexture(const UObject* WorldContextObject,
                                                          UCurveLinearColor* Curve, int xDim,
                                                          FString TextureName) {
  // Using float16 format because RGBA8 wouldn't be persistent for some reason.
  const unsigned sampleCount = xDim;

  // Give the texture some height, so it can be inspected in the asset editor.
  const unsigned TextureHeight = 30;

  FFloat16* samples = new FFloat16[sampleCount * 4 * TextureHeight];

  for (unsigned i = 0; i < sampleCount; i++) {
    float index = ((float)i) / ((float)sampleCount - 1);
    FLinearColor picked = Curve->GetLinearColorValue(index);

    samples[i * 4] = picked.R;
    samples[i * 4 + 1] = picked.G;
    samples[i * 4 + 2] = picked.B;
    samples[i * 4 + 3] = picked.A;
  }

  for (unsigned i = 1; i < TextureHeight; i++) {
    FMemory::Memcpy(samples + (i * sampleCount * 4), samples, sampleCount * 4 * 2);
  }

  Create2DTextureAsset(TextureName, PF_FloatRGBA, FIntPoint(xDim, TextureHeight), (uint8*)samples,
                       true);

  delete[] samples;  // Don't forget to free the memory here
  return;
}

void URaymarchBlueprintLibrary::CreateLightVolumeAsset(const UObject* WorldContextObject,
                                                       FString TextureName, FIntVector Dimensions,
                                                       UVolumeTexture*& CreatedVolume) {
  EPixelFormat PixelFormat = PF_R16F;
  const unsigned TotalElements =
      Dimensions.X * Dimensions.Y * Dimensions.Z * GPixelFormats[PixelFormat].NumComponents;
  const unsigned TotalSize = TotalElements * 2;

  FFloat16* InitMemory = new FFloat16[TotalElements];

  FFloat16 num = 0.0f;
  for (unsigned i = 0; i < TotalElements; i++) {
    InitMemory[i] = num;
  }

  // FMemory::Memset(InitMemory, 1, TotalSize);
  UVolumeTexture* OutTexture = nullptr;
  bool Success = CreateVolumeTextureAsset(TextureName, PixelFormat, Dimensions, (uint8*)InitMemory,
                                          false, true, &OutTexture);
  if (Success) {
    MY_LOG("Asset created and saved successfuly.")
  }
  CreatedVolume = OutTexture;
}

void URaymarchBlueprintLibrary::ReadTransferFunctionFromFile(const UObject* WorldContextObject,
                                                             FString TextFileName,
                                                             UCurveLinearColor*& OutColorCurve) {
  FString RelativePath = FPaths::ProjectContentDir();
  FString FullPath =
      IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*RelativePath) + TextFileName;

  FString FileContent;
  if (!FFileHelper::LoadFileToString(/*out*/ FileContent, *FullPath)) {
    MY_LOG("Reading TF text file failed!");
    return;
  }

  OutColorCurve = NewObject<UCurveLinearColor>();
}

void URaymarchBlueprintLibrary::CustomLog(const UObject* WorldContextObject, FString LoggedString,
                                          float Duration) {
  GEngine->AddOnScreenDebugMessage(-1, Duration, FColor::Yellow, LoggedString);
}

#undef LOCTEXT_NAMESPACE
