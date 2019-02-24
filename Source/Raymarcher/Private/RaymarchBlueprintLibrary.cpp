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
    const UObject* WorldContextObject, FBasicRaymarchRenderingResources Resources,
    const FColorVolumesResources ColorResources, const FDirLightParameters LightParameters,
    const bool Added, const FRaymarchWorldParameters WorldParameters, bool& LightAdded) {
  if (!Resources.VolumeTextureRef->Resource || !Resources.TFTextureRef->Resource ||
      !Resources.ALightVolumeRef->Resource || !Resources.VolumeTextureRef->Resource->TextureRHI ||
      !ColorResources.RLightVolumeRef->Resource ||
      !ColorResources.RLightVolumeRef->Resource->TextureRHI ||
      !ColorResources.GLightVolumeRef->Resource ||
      !ColorResources.GLightVolumeRef->Resource->TextureRHI ||
      !ColorResources.BLightVolumeRef->Resource ||
      !ColorResources.BLightVolumeRef->Resource->TextureRHI ||
      !Resources.TFTextureRef->Resource->TextureRHI ||
      !Resources.ALightVolumeRef->Resource->TextureRHI) {
    LightAdded = false;
    return;
  } else {
    LightAdded = true;
  }

  ERHIFeatureLevel::Type FeatureLevel = WorldContextObject->GetWorld()->Scene->GetFeatureLevel();

  // Call the actual rendering code on RenderThread.
  ENQUEUE_RENDER_COMMAND(CaptureCommand)
  ([=](FRHICommandListImmediate& RHICmdList) {
    AddDirLightToLightVolume_RenderThread(RHICmdList, Resources, ColorResources, LightParameters,
                                          Added, WorldParameters, FeatureLevel);
  });
}

/** Changes a light in the light volumes.	 */
void URaymarchBlueprintLibrary::ChangeDirLightInLightVolumes(
    const UObject* WorldContextObject, FBasicRaymarchRenderingResources Resources,
    const FColorVolumesResources ColorResources, const FDirLightParameters OldLightParameters,
    const FDirLightParameters NewLightParameters, const FRaymarchWorldParameters WorldParameters,
    bool& LightAdded) {
  if (!Resources.VolumeTextureRef->Resource || !Resources.TFTextureRef->Resource ||
      !Resources.ALightVolumeRef->Resource || !Resources.VolumeTextureRef->Resource->TextureRHI ||
      !ColorResources.RLightVolumeRef->Resource ||
      !ColorResources.RLightVolumeRef->Resource->TextureRHI ||
      !ColorResources.GLightVolumeRef->Resource ||
      !ColorResources.GLightVolumeRef->Resource->TextureRHI ||
      !ColorResources.BLightVolumeRef->Resource ||
      !ColorResources.BLightVolumeRef->Resource->TextureRHI ||
      !Resources.TFTextureRef->Resource->TextureRHI ||
      !Resources.ALightVolumeRef->Resource->TextureRHI) {
    LightAdded = false;
    return;
  } else {
    LightAdded = true;
  }

  ERHIFeatureLevel::Type FeatureLevel = WorldContextObject->GetWorld()->Scene->GetFeatureLevel();

  // Call the actual rendering code on RenderThread.
  ENQUEUE_RENDER_COMMAND(CaptureCommand)
  ([=](FRHICommandListImmediate& RHICmdList) {
    ChangeDirLightInLightVolume_RenderThread(RHICmdList, Resources, ColorResources,
                                             OldLightParameters, NewLightParameters,
                                             WorldParameters, FeatureLevel);
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
  const long TotalSize = Dimensions.X * Dimensions.Y * Dimensions.Z;

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

#if WITH_EDITORONLY_DATA
  inTexture->MipGenSettings = TMGS_LeaveExistingMips;
  inTexture->CompressionNone = true;
#endif

  inTexture->NeverStream = false;
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

// void URaymarchBlueprintLibrary::Fill

/** Creates light volumes with the given dimensions */
void URaymarchBlueprintLibrary::CreateLightVolumes(
    const UObject* WorldContextObject, FIntVector Dimensions, UVolumeTexture* inRTexture,
    UVolumeTexture* inGTexture, UVolumeTexture* inBTexture, UVolumeTexture* inATexture) {
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

    InitLightVolume(inTexture, Dimensions);
  }
  return;
}

void URaymarchBlueprintLibrary::InitLightVolume(UVolumeTexture* LightVolume,
                                                FIntVector Dimensions) {
  if (!LightVolume) return;

  const long long TotalSize = Dimensions.X * Dimensions.Y * Dimensions.Z * 2;

  // todo remove?
#if WITH_EDITORONLY_DATA
  LightVolume->MipGenSettings = TMGS_LeaveExistingMips;
  LightVolume->CompressionNone = true;
#endif

  // Set volume texture parameters.
  LightVolume->NeverStream = false;
  LightVolume->SRGB = false;

  // Set PlatformData parameters (create PlatformData if it doesn't exist)
  if (!LightVolume->PlatformData) {
    LightVolume->PlatformData = new FTexturePlatformData();
  }
  LightVolume->PlatformData->PixelFormat = PF_G16;
  LightVolume->PlatformData->SizeX = Dimensions.X;
  LightVolume->PlatformData->SizeY = Dimensions.Y;
  LightVolume->PlatformData->NumSlices = Dimensions.Z;

  FTexture2DMipMap* mip = new FTexture2DMipMap();
  mip->SizeX = Dimensions.X;
  mip->SizeY = Dimensions.Y;
  mip->SizeZ = Dimensions.Z;
  mip->BulkData.Lock(LOCK_READ_WRITE);

  uint8* ByteArray = (uint8*)mip->BulkData.Realloc(TotalSize);
  FMemory::Memset(ByteArray, 0, TotalSize);

  mip->BulkData.Unlock();

  // If the texture already has MIPs in it, destroy and free them (Empty() calls destructors and
  // frees space).
  if (LightVolume->PlatformData->Mips.Num() != 0) {
    LightVolume->PlatformData->Mips.Empty();
  }

  // Add the new MIP.
  LightVolume->PlatformData->Mips.Add(mip);
  LightVolume->bUAVCompatible = true;

  LightVolume->UpdateResource();
}

void URaymarchBlueprintLibrary::AddDirLightToSingleVolume(
    const UObject* WorldContextObject, FBasicRaymarchRenderingResources Resources,
    const FDirLightParameters LightParameters, const bool Added,
    const FRaymarchWorldParameters WorldParameters, bool& LightAdded, FVector& LocalLightDir) {
  if (!Resources.VolumeTextureRef || !Resources.VolumeTextureRef->Resource ||
      !Resources.TFTextureRef->Resource || !Resources.ALightVolumeRef->Resource ||
      !Resources.VolumeTextureRef->Resource->TextureRHI ||
      !Resources.TFTextureRef->Resource->TextureRHI ||
      !Resources.ALightVolumeRef->Resource->TextureRHI || !Resources.ALightVolumeUAVRef) {
    LightAdded = false;
    return;
  } else {
    LightAdded = true;
  }

  LocalLightDir =
      WorldParameters.VolumeTransform.InverseTransformVector(LightParameters.LightDirection);

  LocalLightDir.Normalize();

  ERHIFeatureLevel::Type FeatureLevel = WorldContextObject->GetWorld()->Scene->GetFeatureLevel();

  // Call the actual rendering code on RenderThread.
  ENQUEUE_RENDER_COMMAND(CaptureCommand)
  ([=](FRHICommandListImmediate& RHICmdList) {
    AddDirLightToSingleLightVolume_RenderThread(RHICmdList, Resources, LightParameters, Added,
                                                WorldParameters, FeatureLevel);
  });
}

void URaymarchBlueprintLibrary::ChangeDirLightInSingleVolume(
    const UObject* WorldContextObject, FBasicRaymarchRenderingResources Resources,
    const FDirLightParameters OldLightParameters, const FDirLightParameters NewLightParameters,
    const FRaymarchWorldParameters WorldParameters, bool& LightAdded, FVector& LocalLightDir) {
  if (!Resources.VolumeTextureRef || !Resources.VolumeTextureRef->Resource ||
      !Resources.TFTextureRef->Resource || !Resources.ALightVolumeRef->Resource ||
      !Resources.VolumeTextureRef->Resource->TextureRHI ||
      !Resources.TFTextureRef->Resource->TextureRHI ||
      !Resources.ALightVolumeRef->Resource->TextureRHI) {
    LightAdded = false;
    return;
  } else {
    LightAdded = true;
  }

  LocalLightDir = WorldParameters.VolumeTransform.InverseTransformVectorNoScale(
      NewLightParameters.LightDirection);

  LocalLightDir.Normalize();

  ERHIFeatureLevel::Type FeatureLevel = WorldContextObject->GetWorld()->Scene->GetFeatureLevel();

  // Call the actual rendering code on RenderThread.
  ENQUEUE_RENDER_COMMAND(CaptureCommand)
  ([=](FRHICommandListImmediate& RHICmdList) {
    ChangeDirLightInSingleLightVolume_RenderThread(RHICmdList, Resources, OldLightParameters,
                                                   NewLightParameters, WorldParameters,
                                                   FeatureLevel);
  });
}

void URaymarchBlueprintLibrary::ClearSingleLightVolume(const UObject* WorldContextObject,
                                                       UVolumeTexture* ALightVolume,
                                                       float ClearValue) {
  ERHIFeatureLevel::Type FeatureLevel = WorldContextObject->GetWorld()->Scene->GetFeatureLevel();

  FRHITexture3D* ALightVolumeResource = ALightVolume->Resource->TextureRHI->GetTexture3D();

  // Call the actual rendering code on RenderThread.
  ENQUEUE_RENDER_COMMAND(CaptureCommand)
  ([ALightVolumeResource, ClearValue, FeatureLevel](FRHICommandListImmediate& RHICmdList) {
    ClearSingleLightVolume_RenderThread(RHICmdList, ALightVolumeResource, ClearValue, FeatureLevel);
  });
}

void URaymarchBlueprintLibrary::ClearResourceLightVolumes(
    const UObject* WorldContextObject, const FBasicRaymarchRenderingResources Resources,
    float ClearValue) {
  ERHIFeatureLevel::Type FeatureLevel = WorldContextObject->GetWorld()->Scene->GetFeatureLevel();

  FRHITexture3D* ALightVolumeResource =
      Resources.ALightVolumeRef->Resource->TextureRHI->GetTexture3D();

  // Call the actual rendering code on RenderThread.
  ENQUEUE_RENDER_COMMAND(CaptureCommand)
  ([ALightVolumeResource, ClearValue, FeatureLevel](FRHICommandListImmediate& RHICmdList) {
    ClearSingleLightVolume_RenderThread(RHICmdList, ALightVolumeResource, ClearValue, FeatureLevel);
  });
}

void URaymarchBlueprintLibrary::LoadRawVolumeIntoVolumeTextureAsset(
    const UObject* WorldContextObject, FString FileName, FIntVector Dimensions, FString TextureName,
    bool Persistent, UVolumeTexture*& LoadedTexture) {
  const long TotalSize = Dimensions.X * Dimensions.Y * Dimensions.Z;

  IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

  IFileHandle* FileHandle = PlatformFile.OpenRead(*FileName);

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
  bool Success = CreateVolumeTextureAsset(TextureName, EPixelFormat::PF_G8, Dimensions, TempArray,
                                          LoadedTexture, Persistent, false, false);
  if (Success) {
    MY_LOG("Asset created and saved successfuly.")
  }
  // Close the RAW file and delete temp data.

  delete[] TempArray;
  delete FileHandle;

  return;
}

void URaymarchBlueprintLibrary::LoadMhdFileIntoVolumeTextureAsset(
    const UObject* WorldContextObject, FString FileName, FString TextureName, bool Persistent,
    FIntVector& TextureDimensions, FVector& WorldDimensions, UVolumeTexture*& LoadedTexture) {
  FString FileContent;
  // First, try to read as absolute path
  if (!FFileHelper::LoadFileToString(/*out*/ FileContent, *FileName)) {
    // Try it as a relative path
    FString RelativePath = FPaths::ProjectContentDir();
    FString FullPath =
        IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*RelativePath) + FileName;
    FileName = FullPath;
    if (!FFileHelper::LoadFileToString(/*out*/ FileContent, *FullPath)) {
      MY_LOG("Reading mhd file failed!");
      return;
    }
  }

  FMhdInfo info = FMhdParser::ParseString(FileContent);

  if (!info.ParseSuccessful) {
    MY_LOG("MHD Parsing failed!");
    return;
  }

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
                                      TextureName, Persistent, LoadedTexture);

  // LoadRawVolumeIntoVolumeTextureAsset2(WorldContextObject,
  //                                    FileName.Replace(TEXT(".mhd"), TEXT(".raw")),
  //                                    info.Dimensions.X, info.Dimensions.Y, info.Dimensions.Z ,
  //                                    TextureName, LoadedTexture);

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
                                                          UCurveLinearColor* Curve,
                                                          FString TextureName) {
  // Using float16 format because RGBA8 wouldn't be persistent for some reason.
  const unsigned sampleCount = 1000;

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

  Create2DTextureAsset(TextureName, PF_FloatRGBA, FIntPoint(sampleCount, TextureHeight),
                       (uint8*)samples, true);

  delete[] samples;  // Don't forget to free the memory here
  return;
}
void URaymarchBlueprintLibrary::ColorCurveToTextureRanged(
    const UObject* WorldContextObject, UCurveLinearColor* Curve, UTexture2D* Texture,
    FTransferFunctionRangeParameters parameters) {
  if (parameters.IntensityDomain.Y <= parameters.IntensityDomain.X ||
      parameters.Cutoffs.Y <= parameters.Cutoffs.X) {
    CustomLog(WorldContextObject,
              "Failed creating TF texture because of nonsense cutoff parameters.", 10);
    return;
  }
  if (!Curve || !Texture) {
    CustomLog(WorldContextObject, "Cannot create TF with missing curve/texture asset", 10);
    return;
  }

  // Sanity check
  parameters.IntensityDomain.X = FMath::Clamp(parameters.IntensityDomain.X, 0.0f, 1.0f);
  parameters.IntensityDomain.Y = FMath::Clamp(parameters.IntensityDomain.Y, 0.0f, 1.0f);
  parameters.Cutoffs.X = FMath::Clamp(parameters.Cutoffs.X, parameters.IntensityDomain.X,
                                      parameters.IntensityDomain.Y);
  parameters.Cutoffs.Y = FMath::Clamp(parameters.Cutoffs.Y, parameters.IntensityDomain.X,
                                      parameters.IntensityDomain.Y);

  // Using float16 format because RGBA8 wouldn't be persistent for some reason.
  const unsigned sampleCount = 1000;

  // Give the texture some height, so it can be inspected in the asset editor.
  const unsigned TextureHeight = 30;

  FFloat16* samples = new FFloat16[sampleCount * 4 * TextureHeight];

  float range = parameters.IntensityDomain.Y - parameters.IntensityDomain.X;

  // Position in original TF space.
  float position = parameters.IntensityDomain.X;
  float step = range / ((float)sampleCount - 1);  // -1 ?
  unsigned i = 0;
  FLinearColor picked;

  // Fill the low end of the transfer function
  if (parameters.Cutoffs.X > parameters.IntensityDomain.X) {
    picked = FLinearColor(0, 0, 0, 0);
    // for cutting off with clamp instead of clear
    if (parameters.LowCutMode == FTransferFunctionCutoffMode::TF_Clamp) {
      // Get color at bottom cutoff
      picked = Curve->GetLinearColorValue(parameters.Cutoffs.X);
    }

    for (i = 0; i < parameters.Cutoffs.X * sampleCount; i++) {
      samples[i * 4] = picked.R;
      samples[i * 4 + 1] = picked.G;
      samples[i * 4 + 2] = picked.B;
      samples[i * 4 + 3] = picked.A;
    }
    // After cutoff, set current position in original TF space
    position = parameters.Cutoffs.X;
  }

  // Fill the center of the Transfer function
  for (position; position < parameters.Cutoffs.Y; position += step) {
    picked = Curve->GetLinearColorValue(position);
    samples[i * 4] = picked.R;
    samples[i * 4 + 1] = picked.G;
    samples[i * 4 + 2] = picked.B;
    samples[i * 4 + 3] = picked.A;
    i++;
  }

  // Fill the cutoff high end of the transfer function
  if (parameters.Cutoffs.Y < parameters.IntensityDomain.Y) {
    // Only clear picked if we're clamping by clearing.
    if (parameters.LowCutMode == FTransferFunctionCutoffMode::TF_Clear) {
      // Get color at bottom cutoff
      picked = FLinearColor(0, 0, 0, 0);
    }

    for (i; i < sampleCount; i++) {
      samples[i * 4] = picked.R;
      samples[i * 4 + 1] = picked.G;
      samples[i * 4 + 2] = picked.B;
      samples[i * 4 + 3] = picked.A;
    }
    // After cutoff, set current position in original TF space
    position = parameters.Cutoffs.X;
  }

  //  assert(i == sampleCount);

  for (unsigned i = 1; i < TextureHeight; i++) {
    FMemory::Memcpy(samples + (i * sampleCount * 4), samples, sampleCount * 4 * 2);
  }

  Update2DTextureAsset(Texture, PF_FloatRGBA, FIntPoint(sampleCount, TextureHeight),
                       (uint8*)samples);

  delete[] samples;  // Don't forget to free the memory here
  return;
}

void CreateBufferTextures(FIntPoint Size, EPixelFormat PixelFormat,
                          OneAxisReadWriteBufferResources& RWBuffers) {
  if (Size.X == 0 || Size.Y == 0) {
    UE_LOG(LogTemp, Error, TEXT("Error: Creating Buffer Textures: Size is Zero!"), 3);
    return;
  }
  FRHIResourceCreateInfo CreateInfo(FClearValueBinding::Transparent);
  for (int i = 0; i < 4; i++) {
    RWBuffers.Buffers[i] = RHICreateTexture2D(Size.X, Size.Y, PixelFormat, 1, 1,
                                              TexCreate_ShaderResource | TexCreate_UAV, CreateInfo);
  }
}

void URaymarchBlueprintLibrary::CreateBasicRaymarchingResources(
    const UObject* WorldContextObject, UVolumeTexture* Volume, UVolumeTexture* ALightVolume,
    UTexture2D* TransferFunction, FTransferFunctionRangeParameters TFRangeParams,
    bool HalfResolution, const bool ColoredLightSupport,
    struct FBasicRaymarchRenderingResources& OutParameters) {
  if (!ensure(Volume != nullptr) || !ensure(ALightVolume != nullptr) ||
      !ensure(TransferFunction != nullptr)) {
    UE_LOG(LogTemp, Error, TEXT("Invalid input parameters!"));
    return;
  }

  OutParameters.VolumeTextureRef = Volume;
  OutParameters.ALightVolumeRef = ALightVolume;
  OutParameters.TFTextureRef = TransferFunction;

  int X = Volume->GetSizeX();
  int Y = Volume->GetSizeY();
  int Z = Volume->GetSizeZ();

  // If using half res, divide by two.
  if (HalfResolution) {
    X = FMath::DivideAndRoundUp(X, 2);
    Y = FMath::DivideAndRoundUp(Y, 2);
    Z = FMath::DivideAndRoundUp(Z, 2);
  }

  // Initialize the Alpha Light volume
  InitLightVolume(ALightVolume, FIntVector(X, Y, Z));

  // GEngine->AddOnScreenDebugMessage(-1, 20.0f, FColor::Yellow, "Made some fucking buffers, yo!");

  FIntPoint XBufferSize = FIntPoint(Y, Z);
  FIntPoint YBufferSize = FIntPoint(X, Z);
  FIntPoint ZBufferSize = FIntPoint(X, Y);

  // Make buffers fully colored if we need to support colored lights.
  EPixelFormat PixelFormat = (ColoredLightSupport ? PF_A32B32G32R32F : PF_G16);

  CreateBufferTextures(XBufferSize, PixelFormat, OutParameters.XYZReadWriteBuffers[0]);
  CreateBufferTextures(YBufferSize, PixelFormat, OutParameters.XYZReadWriteBuffers[1]);
  CreateBufferTextures(ZBufferSize, PixelFormat, OutParameters.XYZReadWriteBuffers[2]);

  OutParameters.isInitialized = true;
  OutParameters.supportsColor = ColoredLightSupport;
}

void URaymarchBlueprintLibrary::CreateLightVolumeAsset(const UObject* WorldContextObject,
                                                       FString TextureName, FIntVector Dimensions,
                                                       UVolumeTexture*& CreatedVolume) {
  EPixelFormat PixelFormat = PF_G8;
  const long TotalElements = Dimensions.X * Dimensions.Y * Dimensions.Z;
  const long TotalSize = TotalElements * GPixelFormats[PixelFormat].BlockBytes;

  FFloat16* InitMemory = new FFloat16[TotalElements];

  FFloat16 num = 0.0f;
  for (long i = 0; i < TotalElements; i++) {
    InitMemory[i] = num;
  }

  // FMemory::Memset(InitMemory, 1, TotalSize);
  bool Success = CreateVolumeTextureAsset(TextureName, PixelFormat, Dimensions, (uint8*)InitMemory,
                                          CreatedVolume, false, false, true);
  if (Success) {
    MY_LOG("Asset created and saved successfuly.")
  }
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

void URaymarchBlueprintLibrary::GenerateVolumeTextureMipLevels(const UObject* WorldContextObject,
                                                               FIntVector Dimensions,
                                                               UVolumeTexture* inTexture,
                                                               UTexture2D* TransferFunction,
                                                               bool& success) {
  success = true;
  if (!(inTexture->Resource && inTexture->Resource->TextureRHI && TransferFunction->Resource &&
        TransferFunction->Resource->TextureRHI)) {
    success = false;
    return;
  }

  // Call the actual rendering code on RenderThread.
  ENQUEUE_RENDER_COMMAND(CaptureCommand)
  ([=](FRHICommandListImmediate& RHICmdList) {
    GenerateVolumeTextureMipLevels_RenderThread(
        RHICmdList, Dimensions, inTexture->Resource->TextureRHI->GetTexture3D(),
        TransferFunction->Resource->TextureRHI->GetTexture2D());
  });
}

void URaymarchBlueprintLibrary::GenerateDistanceField(
    const UObject* WorldContextObject, FIntVector Dimensions, UVolumeTexture* inTexture,
    UTexture2D* TransferFunction, UVolumeTexture* SDFTexture, float localSphereDiameter,
    float threshold, bool& success) {
  success = true;
  if (!(inTexture->Resource && inTexture->Resource->TextureRHI && TransferFunction->Resource &&
        TransferFunction->Resource->TextureRHI && SDFTexture->Resource &&
        SDFTexture->Resource->TextureRHI)) {
    success = false;
    return;
  }

  // Call the actual rendering code on RenderThread.
  ENQUEUE_RENDER_COMMAND(CaptureCommand)
  ([=](FRHICommandListImmediate& RHICmdList) {
    GenerateDistanceField_RenderThread(
        RHICmdList, Dimensions, inTexture->Resource->TextureRHI->GetTexture3D(),
        TransferFunction->Resource->TextureRHI->GetTexture2D(),
        SDFTexture->Resource->TextureRHI->GetTexture3D(), localSphereDiameter, threshold);
  });
}

void URaymarchBlueprintLibrary::CustomLog(const UObject* WorldContextObject, FString LoggedString,
                                          float Duration) {
  GEngine->AddOnScreenDebugMessage(-1, Duration, FColor::Yellow, LoggedString);
}

void URaymarchBlueprintLibrary::GetVolumeTextureDimensions(const UObject* WorldContextObject,
                                                           UVolumeTexture* Texture,
                                                           FIntVector& Dimensions) {
  if (Texture && Texture->Resource) {
    // This is slightly retarded...
    Dimensions = Texture->Resource->TextureRHI->GetTexture3D()->GetSizeXYZ();
  } else {
    Dimensions = FIntVector(0, 0, 0);
  }
}

void URaymarchBlueprintLibrary::TransformToMatrix(const UObject* WorldContextObject,
                                                  const FTransform Transform, FMatrix& OutMatrix) {
  OutMatrix = Transform.ToMatrixNoScale();
}

void URaymarchBlueprintLibrary::ChangeTFInResources(
    const UObject* WorldContextObject, FBasicRaymarchRenderingResources Resources,
    UTexture2D* TransferFunction, FTransferFunctionRangeParameters TFParameters,
    FBasicRaymarchRenderingResources& OutResources) {
  Resources.TFRangeParameters = TFParameters;
  Resources.TFTextureRef = TransferFunction;
  OutResources = Resources;
}

void URaymarchBlueprintLibrary::ChangeViewportProperties(const UObject* WorldContextObject,
                                                         FVector2D Origin, FVector2D Size) {
  GEngine->GameViewport->SplitscreenInfo[0].PlayerData[0].OriginX = Origin.X;  // default: 0.f
  GEngine->GameViewport->SplitscreenInfo[0].PlayerData[0].OriginY = Origin.Y;  // default: 0.f
  GEngine->GameViewport->SplitscreenInfo[0].PlayerData[0].SizeX = Size.X;      // default 1.f
  GEngine->GameViewport->SplitscreenInfo[0].PlayerData[0].SizeY = Size.Y;      // default 1.f
}

void URaymarchBlueprintLibrary::GetCutplaneMaterialParams(FCubeFace DominantFace, FVector& Origin,
                                                          FVector& UMultiplier,
                                                          FVector& SliceMultiplier) {
  switch (DominantFace) {
      // Bottom = going from the bottom -> dominant face is at -Z, so slices are going along Z, U
      // coordinate increases Y)
    case FCubeFace::Bottom:
      Origin = FVector(0, 0, 0);
      UMultiplier = FVector(0, 1, 0);
      SliceMultiplier = FVector(0, 0, 1);
      break;
      // Top = going from the top -> dominant face is at +Z, so slices going along going along -Z,
      // U coordinate decreases Y))
    case FCubeFace::Top:
      Origin = FVector(0, 1, 1);
      UMultiplier = FVector(0, -1, 0);
      SliceMultiplier = FVector(0, 0, -1);
      break;
      // Back = going from back -> dominant face is at -Y,  so slices going along +Y, U coordinate
      // decreases Z)
    case FCubeFace::Back:
      Origin = FVector(0, 0, 1);
      UMultiplier = FVector(0, 0, -1);
      SliceMultiplier = FVector(0, 1, 0);
      break;
      // Front = going from Front -> dominant face is at +Y, so slices going along -Y, U coordinate
      // increases Z))
    case FCubeFace::Front:
      Origin = FVector(0, 1, 0);
      UMultiplier = FVector(0, 0, 1);
      SliceMultiplier = FVector(0, -1, 0);
      break;
      // Should never go from +-X, so assert here.
    case FCubeFace::Left:
    case FCubeFace::Right: check(0);
  }
}

void URaymarchBlueprintLibrary::GetDominantFace(FVector LocalDirectionVector,
                                                FCubeFace& DominantFace) {
  FMajorAxes axes = GetMajorAxes(LocalDirectionVector);
  DominantFace = axes.FaceWeight[0].first;
}

void URaymarchBlueprintLibrary::GetFaceNormal(FCubeFace CubeFace, FVector& FaceNormalLocal) {
  FaceNormalLocal = FCubeFaceNormals[(uint8)CubeFace];
}

#undef LOCTEXT_NAMESPACE
