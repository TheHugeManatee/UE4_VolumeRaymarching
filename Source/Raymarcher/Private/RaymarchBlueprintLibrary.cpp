// (C) Technical University of Munich - Computer Aided Medical Procedures
// Developed by Tomas Bartipan (tomas.bartipan@tum.de)

#include "RaymarchBlueprintLibrary.h"
#include "MhdInfo.h"
#include "RaymarchRendering.h"

#include <cstdio>
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
#include "TextureHelperFunctions.h"

#define LOCTEXT_NAMESPACE "RaymarchPlugin"

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

	FMhdInfo info = FMhdInfo::LoadAndParseMhdFile(FileName);
	if (!info.ParseSuccessful) {
		MY_LOG("MHD Parsing failed!");
		return;
	}

	FString MHDInfoString = info.ToString();
	WorldDimensions = info.GetWorldDimensions();
	TextureDimensions = info.Dimensions;

	LoadRawIntoNewVolumeTextureAsset(FileName.Replace(TEXT(".mhd"), TEXT(".raw")), TextureName,
		info.Dimensions, Persistent, LoadedTexture);
	return;
}

void URaymarchBlueprintLibrary::LoadMhdIntoVolumeTextureAsset(FString FileName, UVolumeTexture* VolumeAsset, bool Persistent, FIntVector& TextureDimensions, FVector& WorldDimensions, UVolumeTexture*& LoadedTexture)
{
	FMhdInfo info = FMhdInfo::LoadAndParseMhdFile(FileName);
	if (!info.ParseSuccessful) {
		MY_LOG("MHD Parsing failed!");
		return;
	}

	FString MHDInfoString = info.ToString();
	WorldDimensions = info.GetWorldDimensions();
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
    FTransferFunctionRangeParameters Parameters) {
  if (Parameters.IntensityDomain.Y <= Parameters.IntensityDomain.X ||
      Parameters.Cutoffs.Y <= Parameters.Cutoffs.X) {
    CustomLog("Failed creating TF texture because of nonsense cutoff parameters.", 10);
    return;
  }
  if (!Curve || !Texture) {
    CustomLog("Cannot create TF with missing curve/texture asset", 10);
    return;
  }

  // Sanity check
  Parameters.IntensityDomain.X = FMath::Clamp(Parameters.IntensityDomain.X, 0.0f, 1.0f);
  Parameters.IntensityDomain.Y = FMath::Clamp(Parameters.IntensityDomain.Y, 0.0f, 1.0f);
  Parameters.Cutoffs.X = FMath::Clamp(Parameters.Cutoffs.X, Parameters.IntensityDomain.X,
                                      Parameters.IntensityDomain.Y);
  Parameters.Cutoffs.Y = FMath::Clamp(Parameters.Cutoffs.Y, Parameters.IntensityDomain.X,
                                      Parameters.IntensityDomain.Y);

  // Using float16 format because RGBA8 wouldn't be persistent for some reason.
  const unsigned sampleCount = 1000;

  // Give the texture some height, so it can be inspected in the asset editor.
  const unsigned TextureHeight = 30;

  FFloat16* samples = new FFloat16[sampleCount * 4 * TextureHeight];

  float range = Parameters.IntensityDomain.Y - Parameters.IntensityDomain.X;

  // Position in original TF space.
  float position = Parameters.IntensityDomain.X;
  float step = range / ((float)sampleCount - 1);  // -1 ?
  unsigned i = 0;
  FLinearColor picked;

  // Fill the low end of the transfer function
  if (Parameters.Cutoffs.X > Parameters.IntensityDomain.X) {
    picked = FLinearColor(0, 0, 0, 0);
    // for cutting off with clamp instead of clear
    if (Parameters.LowCutMode == FTransferFunctionCutoffMode::TF_Clamp) {
      // Get color at bottom cutoff
      picked = Curve->GetLinearColorValue(Parameters.Cutoffs.X);
    }

    for (i = 0; i < Parameters.Cutoffs.X * sampleCount; i++) {
      samples[i * 4] = picked.R;
      samples[i * 4 + 1] = picked.G;
      samples[i * 4 + 2] = picked.B;
      samples[i * 4 + 3] = picked.A;
    }
    // After cutoff, set current position in original TF space
    position = Parameters.Cutoffs.X;
  }

  // Fill the center of the Transfer function
  for (position; position < Parameters.Cutoffs.Y; position += step) {
    picked = Curve->GetLinearColorValue(position);
    samples[i * 4] = picked.R;
    samples[i * 4 + 1] = picked.G;
    samples[i * 4 + 2] = picked.B;
    samples[i * 4 + 3] = picked.A;
    i++;
  }

  // Fill the cutoff high end of the transfer function
  if (Parameters.Cutoffs.Y < Parameters.IntensityDomain.Y) {
    // Only clear picked if we're clamping by clearing.
    if (Parameters.LowCutMode == FTransferFunctionCutoffMode::TF_Clear) {
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
    position = Parameters.Cutoffs.X;
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
  EPixelFormat PixelFormat = (ColoredLightSupport ? PF_A32B32G32R32F : PF_G8);

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

void URaymarchBlueprintLibrary::GetVolumeTextureDimensions(UVolumeTexture* Texture,
                                                           FIntVector& Dimensions) {
  if (Texture) {
    // This is slightly retarded...
    Dimensions = FIntVector(Texture->GetSizeX(), Texture->GetSizeY(), Texture->GetSizeZ());
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

#undef LOCTEXT_NAMESPACE
