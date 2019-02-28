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
    FBasicRaymarchRenderingResources Resources,
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

  // Call the actual rendering code on RenderThread.
  ENQUEUE_RENDER_COMMAND(CaptureCommand)
  ([=](FRHICommandListImmediate& RHICmdList) {
    AddDirLightToLightVolume_RenderThread(RHICmdList, Resources, ColorResources, LightParameters,
                                          Added, WorldParameters);
  });
}

/** Changes a light in the light volumes.	 */
void URaymarchBlueprintLibrary::ChangeDirLightInLightVolumes(
    FBasicRaymarchRenderingResources Resources,
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

  // Call the actual rendering code on RenderThread.
  ENQUEUE_RENDER_COMMAND(CaptureCommand)
  ([=](FRHICommandListImmediate& RHICmdList) {
    ChangeDirLightInLightVolume_RenderThread(RHICmdList, Resources, ColorResources,
                                             OldLightParameters, NewLightParameters,
                                             WorldParameters);
  });
}

void URaymarchBlueprintLibrary::ClearLightVolumes(
                                                  UVolumeTexture* RLightVolume,
                                                  UVolumeTexture* GLightVolume,
                                                  UVolumeTexture* BLightVolume,
                                                  UVolumeTexture* ALightVolume,
                                                  FVector4 ClearValues /*= FVector4(0,0,0,0)*/) {
  FRHITexture3D* RLightVolumeResource = RLightVolume->Resource->TextureRHI->GetTexture3D();
  FRHITexture3D* GLightVolumeResource = GLightVolume->Resource->TextureRHI->GetTexture3D();
  FRHITexture3D* BLightVolumeResource = BLightVolume->Resource->TextureRHI->GetTexture3D();
  FRHITexture3D* ALightVolumeResource = ALightVolume->Resource->TextureRHI->GetTexture3D();

  // Call the actual rendering code on RenderThread.
  ENQUEUE_RENDER_COMMAND(CaptureCommand)
  ([RLightVolumeResource, GLightVolumeResource, BLightVolumeResource, ALightVolumeResource,
    ClearValues](FRHICommandListImmediate& RHICmdList) {
    ClearLightVolumes_RenderThread(RHICmdList, RLightVolumeResource, GLightVolumeResource,
                                   BLightVolumeResource, ALightVolumeResource, ClearValues);
  });
}

/** Creates light volumes with the given dimensions */
void URaymarchBlueprintLibrary::CreateLightVolumes(
    FIntVector Dimensions, UVolumeTexture* inRTexture,
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
  if (!LightVolume) {
	  GEngine->AddOnScreenDebugMessage(10, 10, FColor::Red, "Trying to init light volume without providing the volume texture.");
	  return;
  }

  EPixelFormat PixelFormat = PF_G8;
  const long TotalSize = Dimensions.X * Dimensions.Y * Dimensions.Z * GPixelFormats[PixelFormat].BlockBytes;

  uint8* InitMemory = (uint8*)FMemory::Malloc(TotalSize);
  FMemory::Memset(InitMemory, 0, TotalSize);

  // FMemory::Memset(InitMemory, 1, TotalSize);
  UpdateVolumeTextureAsset(LightVolume, PixelFormat, Dimensions, InitMemory, false, false, true);

  FMemory::Free(InitMemory);

}

void URaymarchBlueprintLibrary::AddDirLightToSingleVolume(
    FBasicRaymarchRenderingResources Resources,
    const FDirLightParameters LightParameters, const bool Added,
    const FRaymarchWorldParameters WorldParameters, bool& LightAdded, FVector& LocalLightDir) {
  if (!Resources.VolumeTextureRef || !Resources.VolumeTextureRef->Resource ||
      !Resources.TFTextureRef->Resource || !Resources.ALightVolumeRef->Resource ||
      !Resources.VolumeTextureRef->Resource->TextureRHI ||
      !Resources.TFTextureRef->Resource->TextureRHI ||
      !Resources.ALightVolumeRef->Resource->TextureRHI) { //|| !Resources.ALightVolumeUAVRef) {
    LightAdded = false;
    return;
  } else {
    LightAdded = true;
  }

  LocalLightDir =
      WorldParameters.VolumeTransform.InverseTransformVector(LightParameters.LightDirection);

  LocalLightDir.Normalize();

  // Call the actual rendering code on RenderThread.
  ENQUEUE_RENDER_COMMAND(CaptureCommand)
  ([=](FRHICommandListImmediate& RHICmdList) {
    AddDirLightToSingleLightVolume_RenderThread(RHICmdList, Resources, LightParameters, Added,
                                                WorldParameters);
  });
}

void URaymarchBlueprintLibrary::CreateLightVolumeUAV(FBasicRaymarchRenderingResources Resources, FBasicRaymarchRenderingResources& OutResources, bool& Success)
{
	OutResources = Resources;
	if (!Resources.ALightVolumeRef || !Resources.ALightVolumeRef->Resource || !Resources.ALightVolumeRef->Resource->TextureRHI) {
		Success = false;
	}
	else if (Resources.ALightVolumeUAVRef) {
		Success = true;
	} else {
		Resources.ALightVolumeUAVRef = RHICreateUnorderedAccessView(Resources.ALightVolumeRef->Resource->TextureRHI);
		Success = true;
	}

}

void URaymarchBlueprintLibrary::ChangeDirLightInSingleVolume(
    FBasicRaymarchRenderingResources Resources,
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

  // Call the actual rendering code on RenderThread.
  ENQUEUE_RENDER_COMMAND(CaptureCommand)
  ([=](FRHICommandListImmediate& RHICmdList) {
    ChangeDirLightInSingleLightVolume_RenderThread(RHICmdList, Resources, OldLightParameters,
                                                   NewLightParameters, WorldParameters);
  });
}

void URaymarchBlueprintLibrary::ClearSingleLightVolume(UVolumeTexture* ALightVolume,
                                                       float ClearValue) {

  FRHITexture3D* ALightVolumeResource = ALightVolume->Resource->TextureRHI->GetTexture3D();

  // Call the actual rendering code on RenderThread.
  ENQUEUE_RENDER_COMMAND(CaptureCommand)
  ([ALightVolumeResource, ClearValue](FRHICommandListImmediate& RHICmdList) {
    ClearSingleLightVolume_RenderThread(RHICmdList, ALightVolumeResource, ClearValue);
  });
}

void URaymarchBlueprintLibrary::ClearResourceLightVolumes(const FBasicRaymarchRenderingResources Resources,
    float ClearValue) {
  if (!Resources.ALightVolumeRef) {
	  return;
  }

  FRHITexture3D* ALightVolumeResource =
      Resources.ALightVolumeRef->Resource->TextureRHI->GetTexture3D();

  // Call the actual rendering code on RenderThread.
  ENQUEUE_RENDER_COMMAND(CaptureCommand)
  ([ALightVolumeResource, ClearValue](FRHICommandListImmediate& RHICmdList) {
    ClearSingleLightVolume_RenderThread(RHICmdList, ALightVolumeResource, ClearValue);
  });
}

void URaymarchBlueprintLibrary::LoadRawIntoVolumeTextureAsset(FString RawFileName, UVolumeTexture* inTexture, FIntVector Dimensions, bool Persistent)
{
	const int64 TotalSize = Dimensions.X * Dimensions.Y * Dimensions.Z;

	uint8* TempArray = LoadFileIntoArray(RawFileName, TotalSize);
	if (!TempArray) {
		return;
	}
	
	// Actually update the asset.
	bool Success = UpdateVolumeTextureAsset(inTexture, EPixelFormat::PF_G8, Dimensions, TempArray,
		 Persistent, false);

	// Delete temp data.
	delete[] TempArray;
}

void URaymarchBlueprintLibrary::LoadRawIntoNewVolumeTextureAsset(
	FString RawFileName, FString TextureName, FIntVector Dimensions,
	bool Persistent, UVolumeTexture*& LoadedTexture) {
	
	const int64 TotalSize = Dimensions.X * Dimensions.Y * Dimensions.Z;

	uint8* TempArray = LoadFileIntoArray(RawFileName, TotalSize);
	if (!TempArray) {
		return;
	}

	// Actually create the asset.
	bool Success = CreateVolumeTextureAsset(TextureName, EPixelFormat::PF_G8, Dimensions, TempArray,
		LoadedTexture, Persistent, false, false);

	// Ddelete temp data.
	delete[] TempArray;
}

void URaymarchBlueprintLibrary::LoadMhdIntoNewVolumeTextureAsset(
    FString FileName, FString TextureName, bool Persistent,
    FIntVector& TextureDimensions, FVector& WorldDimensions, UVolumeTexture*& LoadedTexture) {

  FMhdInfo info = LoadAndParseMhdFile(FileName);
  if (!info.ParseSuccessful) {
    MY_LOG("MHD Parsing failed!");
    return;
  }

  PrintMHDFileInfo(info);
  WorldDimensions = GetMhdWorldDimensions(info);
  TextureDimensions = info.Dimensions;
  
  LoadRawIntoNewVolumeTextureAsset(FileName.Replace(TEXT(".mhd"), TEXT(".raw")), TextureName, 
							       info.Dimensions, Persistent, LoadedTexture);
  return;
}

void URaymarchBlueprintLibrary::LoadMhdIntoVolumeTextureAsset(FString FileName, UVolumeTexture* VolumeAsset, bool Persistent, FIntVector& TextureDimensions, FVector& WorldDimensions, UVolumeTexture*& LoadedTexture)
{
	FMhdInfo info = LoadAndParseMhdFile(FileName);
	if (!info.ParseSuccessful) {
		MY_LOG("MHD Parsing failed!");
		return;
	}

	PrintMHDFileInfo(info);
	WorldDimensions = GetMhdWorldDimensions(info);
	TextureDimensions = info.Dimensions;

	LoadRawIntoVolumeTextureAsset(FileName.Replace(TEXT(".mhd"), TEXT(".raw")), VolumeAsset, info.Dimensions,
		Persistent);

}

void URaymarchBlueprintLibrary::TryVolumeTextureSliceWrite(FIntVector Dimensions,
                                                           UVolumeTexture* inTexture) {
  // Enqueue
  ENQUEUE_RENDER_COMMAND(CaptureCommand)
  ([Dimensions, inTexture](FRHICommandListImmediate& RHICmdList) {
    WriteTo3DTexture_RenderThread(RHICmdList, Dimensions, inTexture);
  });
}

void URaymarchBlueprintLibrary::ExportColorCurveToTexture(UCurveLinearColor* Curve,
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
    UCurveLinearColor* Curve, UTexture2D* Texture,
    FTransferFunctionRangeParameters parameters) {
  if (parameters.IntensityDomain.Y <= parameters.IntensityDomain.X ||
      parameters.Cutoffs.Y <= parameters.Cutoffs.X) {
    CustomLog("Failed creating TF texture because of nonsense cutoff parameters.", 10);
    return;
  }
  if (!Curve || !Texture) {
    CustomLog("Cannot create TF with missing curve/texture asset", 10);
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
    UVolumeTexture* Volume, UVolumeTexture* ALightVolume,
    UTexture2D* TransferFunction, FTransferFunctionRangeParameters TFRangeParams,
    bool HalfResolution, const bool ColoredLightSupport,
    FBasicRaymarchRenderingResources& OutParameters) {
  if (!Volume || !ALightVolume || !TransferFunction) {
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

void URaymarchBlueprintLibrary::CheckBasicRaymarchingResources(FBasicRaymarchRenderingResources OutParameters)
{

	FString dgbmsg = "Resources X buff 0 address = " + FString::FromInt(OutParameters.isInitialized);
	GEngine->AddOnScreenDebugMessage(123, 12, FColor::Yellow,  dgbmsg);

}

void URaymarchBlueprintLibrary::CreateLightVolumeAsset(FString TextureName, FIntVector Dimensions,
                                                       UVolumeTexture*& CreatedVolume) {
  EPixelFormat PixelFormat = PF_G8;
  const long TotalSize = Dimensions.X * Dimensions.Y * Dimensions.Z * GPixelFormats[PixelFormat].BlockBytes;

  uint8* InitMemory = (uint8*)FMemory::Malloc(TotalSize);
  FMemory::Memset(InitMemory, 0, TotalSize);

  // FMemory::Memset(InitMemory, 1, TotalSize);
  bool Success = CreateVolumeTextureAsset(TextureName, PixelFormat, Dimensions, (uint8*)InitMemory,
                                          CreatedVolume, false, false, true);

  FMemory::Free(InitMemory);
}

void URaymarchBlueprintLibrary::ReadTransferFunctionFromFile(
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

void URaymarchBlueprintLibrary::GenerateVolumeTextureMipLevels(
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
    FIntVector Dimensions, UVolumeTexture* inTexture,
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

void URaymarchBlueprintLibrary::CustomLog(FString LoggedString,
                                          float Duration) {
  GEngine->AddOnScreenDebugMessage(-1, Duration, FColor::Yellow, LoggedString);
}

void URaymarchBlueprintLibrary::GetVolumeTextureDimensions(
                                                           UVolumeTexture* Texture,
                                                           FIntVector& Dimensions) {
  if (Texture && Texture->Resource && Texture->Resource->TextureRHI) {
    // This is slightly retarded...
    Dimensions = Texture->Resource->TextureRHI->GetTexture3D()->GetSizeXYZ();
  } else {
    Dimensions = FIntVector(0, 0, 0);
  }
}

void URaymarchBlueprintLibrary::TransformToMatrix(
                                                  const FTransform Transform, FMatrix& OutMatrix) {
  OutMatrix = Transform.ToMatrixNoScale();
}

void URaymarchBlueprintLibrary::ChangeTFInResources(
    FBasicRaymarchRenderingResources Resources,
    UTexture2D* TransferFunction, FTransferFunctionRangeParameters TFParameters,
    FBasicRaymarchRenderingResources& OutResources) {
  Resources.TFRangeParameters = TFParameters;
  Resources.TFTextureRef = TransferFunction;
  OutResources = Resources;
}

void URaymarchBlueprintLibrary::ChangeViewportProperties(
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




void URaymarchBlueprintLibrary::PrintMHDFileInfo(const FMhdInfo& MhdInfo)
{
	FVector WorldDimensions;
	WorldDimensions.X = MhdInfo.Dimensions.X * MhdInfo.Spacing.X;
	WorldDimensions.Y = MhdInfo.Dimensions.Y * MhdInfo.Spacing.Y;
	WorldDimensions.Z = MhdInfo.Dimensions.Z * MhdInfo.Spacing.Z;

	FString text =
		"Shit read from the MHD file :\nDimensions = " + FString::FromInt(MhdInfo.Dimensions.X) + " " +
		FString::FromInt(MhdInfo.Dimensions.Y) + " " + FString::FromInt(MhdInfo.Dimensions.Z) +
		"\nSpacing : " + FString::SanitizeFloat(MhdInfo.Spacing.X, 3) + " " +
		FString::SanitizeFloat(MhdInfo.Spacing.Y, 3) + " " + FString::SanitizeFloat(MhdInfo.Spacing.Z, 3) +
		"\nWorld Size MM : " + FString::SanitizeFloat(WorldDimensions.X, 3) + " " +
		FString::SanitizeFloat(WorldDimensions.Y, 3) + " " +
		FString::SanitizeFloat(WorldDimensions.Z, 3);

	GEngine->AddOnScreenDebugMessage(-1, 20.f, FColor::Yellow, text);
}

FMhdInfo URaymarchBlueprintLibrary::LoadAndParseMhdFile(FString FileName)
{
	FMhdInfo MhdInfo;
	MhdInfo.ParseSuccessful = false;

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
			return MhdInfo;
		}
	}

	MhdInfo = FMhdParser::ParseString(FileContent);
	return MhdInfo;

}

FVector URaymarchBlueprintLibrary::GetMhdWorldDimensions(const FMhdInfo& Info)
{
	return FVector(Info.Spacing.X * Info.Dimensions.X, Info.Spacing.Y * Info.Dimensions.Y, Info.Spacing.Z * Info.Dimensions.Z);
}

#undef LOCTEXT_NAMESPACE
