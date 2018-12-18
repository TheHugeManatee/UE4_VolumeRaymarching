// (C) Technical University of Munich - Computer Aided Medical Procedures
// Developed by Tomas Bartipan (tomas.bartipan@tum.de)

#include "../Public/RaymarchRendering.h"
#include "AssetRegistryModule.h"
#include "RenderCore/Public/RenderUtils.h"
#include "Renderer/Public/VolumeRendering.h"

#define LOCTEXT_NAMESPACE "RaymarchPlugin"

IMPLEMENT_SHADER_TYPE(, FVolumePS, TEXT("/Plugin/VolumeRaymarching/Private/RaymarchShader.usf"),
                      TEXT("PassthroughPS"), SF_Pixel)
IMPLEMENT_SHADER_TYPE(, FAddOrRemoveDirLightShader,
                      TEXT("/Plugin/VolumeRaymarching/Private/DirLightShader.usf"),
                      TEXT("MainComputeShader"), SF_Compute)
IMPLEMENT_SHADER_TYPE(, FChangeDirLightShader,
                      TEXT("/Plugin/VolumeRaymarching/Private/ChangeDirLightShader.usf"),
                      TEXT("MainComputeShader"), SF_Compute)
IMPLEMENT_SHADER_TYPE(, FClearLightVolumesShader,
                      TEXT("/Plugin/VolumeRaymarching/Private/ClearVolumesShader.usf"),
                      TEXT("MainComputeShader"), SF_Compute)

IMPLEMENT_SHADER_TYPE(, FAddOrRemoveDirLightSingleVolumeShader,
                      TEXT("/Plugin/VolumeRaymarching/Private/DirLightShaderSingle.usf"),
                      TEXT("MainComputeShader"), SF_Compute)
IMPLEMENT_SHADER_TYPE(, FChangeDirLightSingleVolumeShader,
                      TEXT("/Plugin/VolumeRaymarching/Private/ChangeDirLightShaderSingle.usf"),
                      TEXT("MainComputeShader"), SF_Compute)
IMPLEMENT_SHADER_TYPE(, FClearSingleLightVolumeShader,
                      TEXT("/Plugin/VolumeRaymarching/Private/ClearVolumesShaderSingle.usf"),
                      TEXT("MainComputeShader"), SF_Compute)

#define NUM_THREADS_PER_GROUP_DIMENSION \
  32  // This has to be the same as in the compute shader's spec [X, X, 1]

// Writes to 3D texture slice(s).
void WriteTo3DTexture_RenderThread(FRHICommandListImmediate& RHICmdList, FIntVector Size,
                                   UVolumeTexture* inTexture, ERHIFeatureLevel::Type FeatureLevel) {
  // Set render target to our volume texture.
  SetRenderTarget(RHICmdList, inTexture->Resource->TextureRHI, FTextureRHIRef());

  // Initialize Pipeline
  FGraphicsPipelineStateInitializer GraphicsPSOInit;
  RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

  // No blend, no depth checking.
  GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
  GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
  GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

  // Get shaders from GlobalShaderMap
  TShaderMap<FGlobalShaderType>* GlobalShaderMap = GetGlobalShaderMap(FeatureLevel);
  // Get Geometry and Vertex shaders for Volume texture writes (provided by EPIC).
  TShaderMapRef<FWriteToSliceVS> VertexShader(GlobalShaderMap);
  TOptionalShaderMapRef<FWriteToSliceGS> GeometryShader(GlobalShaderMap);
  // Get a primitive shader that just writes a constant value everywhere (provided by me).
  TShaderMapRef<FVolumePS> PixelShader(GlobalShaderMap);

  // Set the bounds of where to write (you can write to multiple slices in any orientation - along
  // X/Y/Z)
  FVolumeBounds VolumeBounds(Size.X);
  // This will write to 5 slices along X-axis.
  VolumeBounds.MinX = Size.X - 10;
  VolumeBounds.MaxX = Size.X - 5;

  // Set the shaders into the pipeline.
  GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI =
      GScreenVertexDeclaration.VertexDeclarationRHI;
  GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
  GraphicsPSOInit.BoundShaderState.GeometryShaderRHI = GETSAFERHISHADER_GEOMETRY(*GeometryShader);
  GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
  GraphicsPSOInit.PrimitiveType = PT_TriangleStrip;
  SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

  // Set shader parameters - must set the volume bounds to vertex shader.
  VertexShader->SetParameters(RHICmdList, VolumeBounds, Size);
  if (GeometryShader.IsValid()) {
    GeometryShader->SetParameters(RHICmdList, VolumeBounds.MinZ);
  }
  // Pixel shader doesn't have parameters now, it was just for testing.

  // Do the rendering.
  RasterizeToVolumeTexture(RHICmdList, VolumeBounds);

  // Ummmmmmm.......
  // This is probably unnecessary or actually a bad idea...
  FResolveParams ResolveParams;
  RHICmdList.CopyToResolveTarget(inTexture->Resource->TextureRHI, inTexture->Resource->TextureRHI,
                                 ResolveParams);
}

bool CreateVolumeTextureAsset(FString AssetName, EPixelFormat PixelFormat, FIntVector Dimensions,
                              uint8* BulkData, bool SaveNow, bool UAVCompatible,
                              UVolumeTexture** pOutCreatedTexture) {
  ETextureSourceFormat TextureSourceFormat = PixelFormatToSourceFormat(PixelFormat);

  if (TextureSourceFormat == TSF_Invalid) {
    return false;
  }

  int TotalSize =
      Dimensions.X * Dimensions.Y * Dimensions.Z * GPixelFormats[PixelFormat].BlockBytes;

  FString PackageName = TEXT("/Game/GeneratedTextures/");
  PackageName += AssetName;
  UPackage* Package = CreatePackage(NULL, *PackageName);
  Package->FullyLoad();

  UVolumeTexture* NewTexture = NewObject<UVolumeTexture>(
      (UObject*)Package, FName(*AssetName), RF_Public | RF_Standalone | RF_MarkAsRootSet);

  NewTexture->AddToRoot();  // This line prevents garbage collection of the texture
  NewTexture->PlatformData = new FTexturePlatformData();  // Then we initialize the PlatformData
  NewTexture->PlatformData->SizeX = Dimensions.X;
  NewTexture->PlatformData->SizeY = Dimensions.Y;
  NewTexture->PlatformData->NumSlices = Dimensions.Z;
  NewTexture->PlatformData->PixelFormat = PixelFormat;

  NewTexture->MipGenSettings = TMGS_NoMipmaps;
  NewTexture->SRGB = false;

  FTexture2DMipMap* Mip = new (NewTexture->PlatformData->Mips) FTexture2DMipMap();
  Mip->SizeX = Dimensions.X;
  Mip->SizeY = Dimensions.Y;
  Mip->SizeZ = Dimensions.Z;

  Mip->BulkData.Lock(LOCK_READ_WRITE);

  uint8* ByteArray = (uint8*)Mip->BulkData.Realloc(TotalSize);
  FMemory::Memcpy(ByteArray, BulkData, TotalSize);

  Mip->BulkData.Unlock();

  if (UAVCompatible) {
    NewTexture->bUAVCompatible = true;
  }

  NewTexture->Source.Init(Dimensions.X, Dimensions.Y, Dimensions.Z, 1, TextureSourceFormat,
                          ByteArray);
  NewTexture->UpdateResource();
  Package->MarkPackageDirty();
  FAssetRegistryModule::AssetCreated(NewTexture);

  // Pass the pointer out if requested.
  if (pOutCreatedTexture != nullptr) {
    *pOutCreatedTexture = NewTexture;
  }

  // Only save the asset if that is needed (as this is a disk operation and takes a long time)
  if (SaveNow) {
    FString PackageFileName = FPackageName::LongPackageNameToFilename(
        PackageName, FPackageName::GetAssetPackageExtension());
    return UPackage::SavePackage(Package, NewTexture,
                                 EObjectFlags::RF_Public | EObjectFlags::RF_Standalone,
                                 *PackageFileName, GError, nullptr, true, true, SAVE_NoError);
  } else {
    return true;
  }
}

bool Create2DTextureAsset(FString AssetName, EPixelFormat PixelFormat, FIntPoint Dimensions,
                          uint8* BulkData, bool SaveNow, TextureAddress TilingX,
                          TextureAddress TilingY) {
  ETextureSourceFormat TextureSourceFormat = PixelFormatToSourceFormat(PixelFormat);

  if (TextureSourceFormat == TSF_Invalid) {
    return false;
  }

  int TotalSize = Dimensions.X * Dimensions.Y * GPixelFormats[PixelFormat].BlockBytes;

  FString PackageName = TEXT("/Game/GeneratedTextures/");
  PackageName += AssetName;
  UPackage* Package = CreatePackage(NULL, *PackageName);
  Package->FullyLoad();

  UTexture2D* NewTexture = NewObject<UTexture2D>((UObject*)Package, FName(*AssetName),
                                                 RF_Public | RF_Standalone | RF_MarkAsRootSet);

  NewTexture->AddToRoot();  // This line prevents garbage collection of the texture
  NewTexture->PlatformData = new FTexturePlatformData();  // Then we initialize the PlatformData
  NewTexture->PlatformData->SizeX = Dimensions.X;
  NewTexture->PlatformData->SizeY = Dimensions.Y;
  NewTexture->PlatformData->NumSlices = 1;
  NewTexture->PlatformData->PixelFormat = PixelFormat;

  NewTexture->AddressX = TA_Clamp;
  NewTexture->AddressY = TA_Clamp;
  NewTexture->MipGenSettings = TMGS_NoMipmaps;
  NewTexture->CompressionSettings = TC_Default;
  NewTexture->SRGB = false;

  FTexture2DMipMap* Mip = new (NewTexture->PlatformData->Mips) FTexture2DMipMap();
  Mip->SizeX = Dimensions.X;
  Mip->SizeY = Dimensions.Y;
  Mip->SizeZ = 1;

  Mip->BulkData.Lock(LOCK_READ_WRITE);

  uint8* ByteArray = (uint8*)Mip->BulkData.Realloc(TotalSize);
  FMemory::Memcpy(ByteArray, BulkData, TotalSize);

  Mip->BulkData.Unlock();

  NewTexture->Source.Init(Dimensions.X, Dimensions.Y, 1, 1, TextureSourceFormat, ByteArray);
  NewTexture->UpdateResource();
  Package->MarkPackageDirty();
  FAssetRegistryModule::AssetCreated(NewTexture);

  // Only save the asset if that is needed (as this is a disk operation and takes a long time)
  if (SaveNow) {
    FString PackageFileName = FPackageName::LongPackageNameToFilename(
        PackageName, FPackageName::GetAssetPackageExtension());
    return UPackage::SavePackage(Package, NewTexture,
                                 EObjectFlags::RF_Public | EObjectFlags::RF_Standalone,
                                 *PackageFileName, GError, nullptr, true, true, SAVE_NoError);
  } else {
    return true;
  }
}

ETextureSourceFormat PixelFormatToSourceFormat(EPixelFormat PixelFormat) {
  // THIS IS UNTESTED FOR FORMATS OTHER THAN G8 AND R16G16B16A16_SNORM!
  // USE AT YOUR OWN PERIL!
  switch (PixelFormat) {
    case PF_G8: return TSF_G8;

    case PF_B8G8R8A8: return TSF_BGRA8;
    case PF_R8G8B8A8: return TSF_RGBA8;

    case PF_R16G16B16A16_SINT:
    case PF_R16G16B16A16_UINT: return TSF_RGBA16;

    case PF_R16G16B16A16_SNORM:
    case PF_R16G16B16A16_UNORM:
    case PF_FloatRGBA: return TSF_RGBA16F;

    default: return TSF_Invalid;
  }
}

FIntVector GetTransposedDimensions(FMajorAxes Axes, FRHITexture3D* VolumeRef, unsigned index) {
  FCubeFace face = Axes.FaceWeight[index].first;
  unsigned axis = face / 2;
  switch (axis) {
    case 0:  // going along X -> Volume Y = x, volume Z = y
      return FIntVector(VolumeRef->GetSizeY(), VolumeRef->GetSizeZ(), VolumeRef->GetSizeX());
    case 1:  // going along Y -> Volume X = x, volume Z = y
      return FIntVector(VolumeRef->GetSizeX(), VolumeRef->GetSizeZ(), VolumeRef->GetSizeY());
    case 2:  // going along Z -> Volume X = x, volume Y = y
      return FIntVector(VolumeRef->GetSizeX(), VolumeRef->GetSizeY(), VolumeRef->GetSizeZ());
    default: check(false); return FIntVector(0, 0, 0);
  }
}

FVector2D GetPixOffset(int Axis, FVector LightPosition) {
  FVector normLightPosition = LightPosition;
  // Normalize the light position to get the major axis to be one. The other 2 components are then
  // an offset to apply to current pos to read from our read buffer texture.
  switch (Axis) {
    case 0:
      normLightPosition /= normLightPosition.X;
      return FVector2D(normLightPosition.Y, normLightPosition.Z);
    case 1:
      normLightPosition /= -normLightPosition.X;
      return FVector2D(-normLightPosition.Y, normLightPosition.Z);
    case 2:
      normLightPosition /= normLightPosition.Y;
      return FVector2D(-normLightPosition.X, normLightPosition.Z);
    case 3:
      normLightPosition /= -normLightPosition.Y;
      return FVector2D(normLightPosition.X, normLightPosition.Z);
    case 4:
      normLightPosition /= normLightPosition.Z;
      return FVector2D(normLightPosition.X, normLightPosition.Y);
    case 5:
      normLightPosition /= -normLightPosition.Z;
      return FVector2D(normLightPosition.X, -normLightPosition.Y);
    default: return FVector2D(0, 0);
  }
}

void GetLocalLightParamsAndAxes(const FDirLightParameters& LightParameters,
                                const FTransform& VolumeTransform,
                                FDirLightParameters& OutLocalLightParameters,
                                FMajorAxes& OutLocalMajorAxes) {

	// TODO Why the fuck does light direction need NoScale and no multiplication by scale and clipping plane
	// needs to be multiplied?
	
	// Transform light directions into local space.
  OutLocalLightParameters.LightDirection =
      VolumeTransform.InverseTransformVectorNoScale(LightParameters.LightDirection);
  // Normalize Light Direction to get unit length.
  OutLocalLightParameters.LightDirection.Normalize();
  
  // Color and Intensity are the same in local space -> copy.
  OutLocalLightParameters.LightColor = LightParameters.LightColor;
  OutLocalLightParameters.LightIntensity = LightParameters.LightIntensity;

  // Get Major Axes (notice inverting of light Direction - for a directional light, the position of
  // the light is the opposite of the direction) e.g. Directional light with direction (1, 0, 0) is
  // assumed to be shining from (-1, 0, 0)
  OutLocalMajorAxes = GetMajorAxes(-OutLocalLightParameters.LightDirection);

  // Set second axis weight to (1 - (first axis weight))
  OutLocalMajorAxes.FaceWeight[1].second = 1 - OutLocalMajorAxes.FaceWeight[0].second;


  FString debug = "Global light dir : " + LightParameters.LightDirection.ToString()  + ", Local light dir : " + OutLocalLightParameters.LightDirection.ToString();
  GEngine->AddOnScreenDebugMessage(-1, 0, FColor::Yellow, debug);

  // RetVal.Direction *= VolumeTransform.GetScale3D();
  

}

FClippingPlaneParameters GetLocalClippingParameters(
    const FClippingPlaneParameters WorldClippingParams, const FVector MeshMaxBounds,
    const FTransform VolumeTransform) {

	FClippingPlaneParameters RetVal;
  // Get clipping center to (0-1) texture local space. (Invert transform, divide by mesh size,
  // divide by 2 and add 0.5 to get to (0-1) space.
  RetVal.Center =
      ((VolumeTransform.InverseTransformPosition(WorldClippingParams.Center) / (MeshMaxBounds) ) / 2.0) + 0.5;
  // Get clipping direction in local space - here we don't care about the mesh size (as long as
  // it's a cube, which it really bloody better be).


	// TODO Why the fuck does light direction need NoScale and no multiplication by scale and clipping
  // plane needs to be multiplied?
  RetVal.Direction = VolumeTransform.InverseTransformVectorNoScale(WorldClippingParams.Direction);
  RetVal.Direction *= VolumeTransform.GetScale3D();
  RetVal.Direction.Normalize();

  FString debug = "Global clip dir : " + WorldClippingParams.Direction.ToString() + ", Local clip dir : " +  RetVal.Direction.ToString();
  GEngine->AddOnScreenDebugMessage(-1, 0, FColor::Yellow, debug);


  return RetVal;
}

FSamplerStateRHIRef GetBufferSamplerRef(uint32 BorderColorInt) {
  // Return a sampler for RW buffers - bordered by specified color.
  return RHICreateSamplerState(FSamplerStateInitializerRHI(SF_Trilinear, AM_Border, AM_Border,
                                                           AM_Border, 0, 0, 0, 0, BorderColorInt));
}

void CreateBufferTexturesAndUAVs(FIntVector Size, EPixelFormat PixelFormat,
                                 FTexture2DRHIRef& OutTexture1,
                                 FUnorderedAccessViewRHIRef& OutTexture1UAV,
                                 FTexture2DRHIRef& OutTexture2,
                                 FUnorderedAccessViewRHIRef& OutTexture2UAV) {
  FRHIResourceCreateInfo CreateInfo(FClearValueBinding::Transparent);
  OutTexture1 = RHICreateTexture2D(Size.X, Size.Y, PixelFormat, 1, 1,
                                   TexCreate_ShaderResource | TexCreate_UAV, CreateInfo);
  OutTexture2 = RHICreateTexture2D(Size.X, Size.Y, PixelFormat, 1, 1,
                                   TexCreate_ShaderResource | TexCreate_UAV, CreateInfo);
  OutTexture1UAV = RHICreateUnorderedAccessView(OutTexture1);
  OutTexture2UAV = RHICreateUnorderedAccessView(OutTexture2);
}

uint32 GetBorderColorIntSingle(FDirLightParameters LightParams, FMajorAxes MajorAxes,
                               unsigned index) {
  // Set alpha channel to the texture's red channel (when reading single-channel, only red component
  // is read)
  FLinearColor LightColor = FLinearColor(LightParams.LightIntensity * MajorAxes.FaceWeight[index].second, 0.0, 0.0, 0.0);
  return LightColor.ToFColor(true).ToPackedARGB();
 }

uint32 GetBorderColorInt(FDirLightParameters LightParams, FMajorAxes MajorAxes, unsigned index) {
	FVector LC = LightParams.LightColor;
	FLinearColor LightColor =
      FLinearColor(LC.X, LC.Y, LC.Z, LightParams.LightIntensity * MajorAxes.FaceWeight[index].second);
  return LightColor.ToFColor(true).ToPackedARGB();
}

void TransitionBufferResources(FRHICommandListImmediate& RHICmdList,
                               FTextureRHIParamRef NewlyReadableTexture,
                               FUnorderedAccessViewRHIParamRef NewlyWriteableUAV) {
  RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, NewlyReadableTexture);
  RHICmdList.TransitionResource(EResourceTransitionAccess::EWritable,
                                EResourceTransitionPipeline::EComputeToCompute, NewlyWriteableUAV);
}

// For making statistics about GPU use.
DECLARE_FLOAT_COUNTER_STAT(TEXT("AddingLights"), STAT_GPU_AddingLights, STATGROUP_GPU);
DECLARE_GPU_STAT_NAMED(GPUAddingLights, TEXT("AddingLightsToVolume"));

void AddDirLightToLightVolume_RenderThread(
    FRHICommandListImmediate& RHICmdList, FRHITexture3D* RLightVolumeResource,
    FRHITexture3D* GLightVolumeResource, FRHITexture3D* BLightVolumeResource,
    FRHITexture3D* ALightVolumeResource, FRHITexture3D* VolumeResource, FRHITexture2D* TFResource,
    const FDirLightParameters LightParams, const bool LightAdded,
    const FTransform VolumeTransform, const FClippingPlaneParameters ClippingParameters,
    const FVector MeshMaxBounds,
    ERHIFeatureLevel::Type
        FeatureLevel) {  // TODO use a unit inv cube to avoid having to pass MeshMaxBounds!
  check(IsInRenderingThread());

  // Can't have directional light without direction...
  if (LightParams.LightDirection == FVector(0.0, 0.0, 0.0)) {
    GEngine->AddOnScreenDebugMessage(
        -1, 100.0f, FColor::Yellow,
        TEXT("Returning because the directional light doesn't have a direction."));
    return;
  }

  FDirLightParameters LocalLightParams;
  FMajorAxes LocalMajorAxes;
  // Calculate local Light parameters and corresponding axes.
  GetLocalLightParamsAndAxes(LightParams, VolumeTransform, LocalLightParams, LocalMajorAxes);
  // Transform clipping parameters into local space.
  FClippingPlaneParameters LocalClippingParameters =
      GetLocalClippingParameters(ClippingParameters, MeshMaxBounds, VolumeTransform);

  // For GPU profiling.
  SCOPED_DRAW_EVENTF(RHICmdList, AddDirLightToLightVolume_RenderThread, TEXT("Adding Lights"));
  SCOPED_GPU_STAT(RHICmdList, GPUAddingLights);
  // Get shader ref from GlobalShaderMap
  TShaderMap<FGlobalShaderType>* GlobalShaderMap = GetGlobalShaderMap(FeatureLevel);
  TShaderMapRef<FAddOrRemoveDirLightShader> ComputeShader(GlobalShaderMap);

  RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
  // RHICmdList.TransitionResource(EResourceTransitionAccess::ERWNoBarrier, LightVolumeResource);
  FUnorderedAccessViewRHIRef RVolumeUAV = RHICreateUnorderedAccessView(RLightVolumeResource);
  FUnorderedAccessViewRHIRef GVolumeUAV = RHICreateUnorderedAccessView(GLightVolumeResource);
  FUnorderedAccessViewRHIRef BVolumeUAV = RHICreateUnorderedAccessView(BLightVolumeResource);
  FUnorderedAccessViewRHIRef AVolumeUAV = RHICreateUnorderedAccessView(ALightVolumeResource);

  FUnorderedAccessViewRHIParamRef UAVs[4];
  UAVs[0] = RVolumeUAV;
  UAVs[1] = GVolumeUAV;
  UAVs[2] = BVolumeUAV;
  UAVs[3] = AVolumeUAV;

  // Don't need barriers on these - we only ever read/write to the same pixel from one thread -> no
  // race conditions But we definitely need to transition the resource to Compute-shader accessible,
  // otherwise the renderer might touch our textures while we're writing them.
  RHICmdList.TransitionResources(EResourceTransitionAccess::EWritable,
                                 EResourceTransitionPipeline::EGfxToCompute, UAVs, 4);

  ComputeShader->SetResources(RHICmdList, VolumeResource, TFResource, UAVs);

  for (unsigned i = 0; i < 2; i++) {
    // Break if the main axis weight == 1
    if (LocalMajorAxes.FaceWeight[i].second == 0) {
      break;
    }

    FVector2D textureOffset =
        GetPixOffset(LocalMajorAxes.FaceWeight[i].first, -LocalLightParams.LightDirection);

    // Get the X, Y and Z transposed into the current axis orientation.
    FIntVector TransposedDimensions =
        GetTransposedDimensions(LocalMajorAxes, ALightVolumeResource, i);

    uint32 ColorInt = GetBorderColorInt(LocalLightParams, LocalMajorAxes, i);
    FSamplerStateRHIRef readBuffSampler = GetBufferSamplerRef(ColorInt);

    FTexture2DRHIRef Texture1, Texture2;
    FUnorderedAccessViewRHIRef Texture1UAV, Texture2UAV;

    CreateBufferTexturesAndUAVs(TransposedDimensions, PF_A32B32G32R32F, Texture1, Texture1UAV,
                                Texture2, Texture2UAV);

    ComputeShader->SetParameters(RHICmdList, LocalLightParams, LightAdded, LocalClippingParameters,
                                 LocalMajorAxes, i);

    uint32 GroupSizeX =
        FMath::DivideAndRoundUp(TransposedDimensions.X, NUM_THREADS_PER_GROUP_DIMENSION);
    uint32 GroupSizeY =
        FMath::DivideAndRoundUp(TransposedDimensions.Y, NUM_THREADS_PER_GROUP_DIMENSION);

    for (int j = 0; j < TransposedDimensions.Z; j++) {
      // Switch read and write buffers each cycle.
      if (j % 2 == 0) {
        TransitionBufferResources(RHICmdList, Texture1, Texture2UAV);
        ComputeShader->SetLoop(RHICmdList, j, Texture1, readBuffSampler, Texture2UAV);
      } else {
        TransitionBufferResources(RHICmdList, Texture2, Texture1UAV);
        ComputeShader->SetLoop(RHICmdList, j, Texture2, readBuffSampler, Texture1UAV);
      }

      DispatchComputeShader(RHICmdList, *ComputeShader, GroupSizeX, GroupSizeY, 1);
    }
  }

  // Unbind UAVs.
  ComputeShader->UnbindUAVs(RHICmdList);
  // Transition resources back to the renderer.
  RHICmdList.TransitionResources(EResourceTransitionAccess::EReadable,
                                 EResourceTransitionPipeline::EComputeToGfx, UAVs, 4);
}

void ChangeDirLightInLightVolume_RenderThread(
    FRHICommandListImmediate& RHICmdList, FRHITexture3D* RLightVolumeResource,
    FRHITexture3D* GLightVolumeResource, FRHITexture3D* BLightVolumeResource,
    FRHITexture3D* ALightVolumeResource, FRHITexture3D* VolumeResource, FRHITexture2D* TFResource,
    const FDirLightParameters LightParams, const FDirLightParameters NewLightParams,
    const FTransform VolumeTransform, const FClippingPlaneParameters ClippingParameters,
    const FVector MeshMaxBounds, ERHIFeatureLevel::Type FeatureLevel) {
  // Can't have directional light without direction...
  if (LightParams.LightDirection == FVector(0.0, 0.0, 0.0)) {
    GEngine->AddOnScreenDebugMessage(
        -1, 100.0f, FColor::Yellow,
        TEXT("Returning because the directional light doesn't have a direction."));
    return;
  }

  // Create local copies of Light Params, so that if we have to fall back to 2x AddOrRemoveLight, we
  // can just pass the original parameters.
  FDirLightParameters LocalLightParams, NewLocalLightParams;
  FMajorAxes LocalMajorAxes, NewLocalMajorAxes;
  // Calculate local Light parameters and corresponding axes.
  GetLocalLightParamsAndAxes(LightParams, VolumeTransform, LocalLightParams, LocalMajorAxes);
  GetLocalLightParamsAndAxes(NewLightParams, VolumeTransform, NewLocalLightParams,
                             NewLocalMajorAxes);

  // If lights have different major axes, do a proper removal and addition.
  // If first major axes are the same and above the dominance threshold, ignore whether the second
  // major axes are the same.
  if (LocalMajorAxes.FaceWeight[0].first != NewLocalMajorAxes.FaceWeight[0].first ||
      LocalMajorAxes.FaceWeight[1].first != NewLocalMajorAxes.FaceWeight[1].first) {
    AddDirLightToLightVolume_RenderThread(
        RHICmdList, RLightVolumeResource, GLightVolumeResource, BLightVolumeResource,
        ALightVolumeResource, VolumeResource, TFResource, LightParams, false, VolumeTransform,
        ClippingParameters, MeshMaxBounds, FeatureLevel);
    AddDirLightToLightVolume_RenderThread(
        RHICmdList, RLightVolumeResource, GLightVolumeResource, BLightVolumeResource,
        ALightVolumeResource, VolumeResource, TFResource, NewLightParams, true, VolumeTransform,
        ClippingParameters, MeshMaxBounds, FeatureLevel);
    return;
  }

  // For GPU profiling.
  SCOPED_DRAW_EVENTF(RHICmdList, AddDirLightToLightVolume_RenderThread, TEXT("Adding Lights"));
  SCOPED_GPU_STAT(RHICmdList, GPUAddingLights);

  TShaderMap<FGlobalShaderType>* GlobalShaderMap = GetGlobalShaderMap(FeatureLevel);
  TShaderMapRef<FChangeDirLightShader> ComputeShader(GlobalShaderMap);

  RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
  // RHICmdList.TransitionResource(EResourceTransitionAccess::ERWNoBarrier, LightVolumeResource);
  FUnorderedAccessViewRHIRef RVolumeUAV = RHICreateUnorderedAccessView(RLightVolumeResource);
  FUnorderedAccessViewRHIRef GVolumeUAV = RHICreateUnorderedAccessView(GLightVolumeResource);
  FUnorderedAccessViewRHIRef BVolumeUAV = RHICreateUnorderedAccessView(BLightVolumeResource);
  FUnorderedAccessViewRHIRef AVolumeUAV = RHICreateUnorderedAccessView(ALightVolumeResource);

  FUnorderedAccessViewRHIParamRef UAVs[4];
  UAVs[0] = RVolumeUAV;
  UAVs[1] = GVolumeUAV;
  UAVs[2] = BVolumeUAV;
  UAVs[3] = AVolumeUAV;

  // Don't need barriers on these - we only ever read/write to the same pixel from one thread -> no
  // race conditions But we definitely need to transition the resource to Compute-shader accessible,
  // otherwise the renderer might touch our textures while we're writing them.
  RHICmdList.TransitionResources(EResourceTransitionAccess::EWritable,
                                 EResourceTransitionPipeline::EGfxToCompute, UAVs, 4);

  ComputeShader->SetResources(RHICmdList, VolumeResource, TFResource, UAVs);

  FClippingPlaneParameters LocalClippingParams =
      GetLocalClippingParameters(ClippingParameters, MeshMaxBounds, VolumeTransform);

  for (unsigned i = 0; i < 2; i++) {
    FVector2D textureOffset =
        GetPixOffset(LocalMajorAxes.FaceWeight[i].first, -LocalLightParams.LightDirection);

    // Get the X, Y and Z transposed into the current axis orientation.
    FIntVector TransposedDimensions =
        GetTransposedDimensions(LocalMajorAxes, ALightVolumeResource, i);

    // Get Color ints for texture borders.
    uint32 ColorInt = GetBorderColorInt(LocalLightParams, LocalMajorAxes, i);
    uint32 NewColorInt = GetBorderColorInt(NewLocalLightParams, NewLocalMajorAxes, i);
    // Get the sampler for read buffer to use border with the proper light color.
    FSamplerStateRHIRef readBuffSampler = GetBufferSamplerRef(ColorInt);
    FSamplerStateRHIRef newReadBufferSampler = GetBufferSamplerRef(NewColorInt);

    // Create read-write buffer textures for both lights.
    FTexture2DRHIRef Texture1, Texture2, NewTexture1, NewTexture2;
    FUnorderedAccessViewRHIRef Texture1UAV, Texture2UAV, NewTexture1UAV, NewTexture2UAV;

    CreateBufferTexturesAndUAVs(TransposedDimensions, PF_A32B32G32R32F, Texture1, Texture1UAV,
                                Texture2, Texture2UAV);
    CreateBufferTexturesAndUAVs(TransposedDimensions, PF_A32B32G32R32F, NewTexture1, NewTexture1UAV,
                                NewTexture2, NewTexture2UAV);

    ComputeShader->SetParameters(RHICmdList, LocalLightParams, NewLocalLightParams, LocalMajorAxes,
                                 NewLocalMajorAxes, LocalClippingParams, i);

    // Get group sizes for compute shader
    uint32 GroupSizeX =
        FMath::DivideAndRoundUp(TransposedDimensions.X, NUM_THREADS_PER_GROUP_DIMENSION);
    uint32 GroupSizeY =
        FMath::DivideAndRoundUp(TransposedDimensions.Y, NUM_THREADS_PER_GROUP_DIMENSION);

    for (int j = 0; j < TransposedDimensions.Z; j++) {
      // Switch read and write buffers each cycle.
      if (j % 2 == 0) {
        TransitionBufferResources(RHICmdList, Texture1, Texture2UAV);
        TransitionBufferResources(RHICmdList, NewTexture1, NewTexture2UAV);
        ComputeShader->SetLoop(RHICmdList, j, Texture1, readBuffSampler, Texture2UAV, NewTexture1,
                               newReadBufferSampler, NewTexture2UAV);
      } else {
        TransitionBufferResources(RHICmdList, Texture2, Texture1UAV);
        TransitionBufferResources(RHICmdList, NewTexture2, NewTexture1UAV);
        ComputeShader->SetLoop(RHICmdList, j, Texture2, readBuffSampler, Texture1UAV, NewTexture2,
                               newReadBufferSampler, NewTexture1UAV);
      }
      // Everything ready for this loop -> dispatch shader!
      DispatchComputeShader(RHICmdList, *ComputeShader, GroupSizeX, GroupSizeY, 1);
    }
  }

  // Unbind UAVs.
  ComputeShader->UnbindUAVs(RHICmdList);

  // Transition resources back to the renderer.
  RHICmdList.TransitionResources(EResourceTransitionAccess::EReadable,
                                 EResourceTransitionPipeline::EComputeToGfx, UAVs, 4);
}

void ClearLightVolumes_RenderThread(FRHICommandListImmediate& RHICmdList,
                                    FRHITexture3D* RLightVolumeResource,
                                    FRHITexture3D* GLightVolumeResource,
                                    FRHITexture3D* BLightVolumeResource,
                                    FRHITexture3D* ALightVolumeResource, FVector4 ClearValues,
                                    ERHIFeatureLevel::Type FeatureLevel) {
  TShaderMap<FGlobalShaderType>* GlobalShaderMap = GetGlobalShaderMap(FeatureLevel);
  TShaderMapRef<FClearLightVolumesShader> ComputeShader(GlobalShaderMap);

  RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
  // RHICmdList.TransitionResource(EResourceTransitionAccess::ERWNoBarrier, LightVolumeResource);
  FUnorderedAccessViewRHIRef RVolumeUAV = RHICreateUnorderedAccessView(RLightVolumeResource);
  FUnorderedAccessViewRHIRef GVolumeUAV = RHICreateUnorderedAccessView(GLightVolumeResource);
  FUnorderedAccessViewRHIRef BVolumeUAV = RHICreateUnorderedAccessView(BLightVolumeResource);
  FUnorderedAccessViewRHIRef AVolumeUAV = RHICreateUnorderedAccessView(ALightVolumeResource);

  FUnorderedAccessViewRHIParamRef UAVs[4];
  UAVs[0] = RVolumeUAV;
  UAVs[1] = GVolumeUAV;
  UAVs[2] = BVolumeUAV;
  UAVs[3] = AVolumeUAV;

  // Don't need barriers on these - we only ever read/write to the same pixel from one thread -> no
  // race conditions But we definitely need to transition the resource to Compute-shader accessible,
  // otherwise the renderer might touch our textures while we're writing them.
  RHICmdList.TransitionResources(EResourceTransitionAccess::ERWNoBarrier,
                                 EResourceTransitionPipeline::EGfxToCompute, UAVs, 4);

  ComputeShader->SetLightVolumeUAVs(RHICmdList, UAVs);
  ComputeShader->SetParameters(RHICmdList, ClearValues, RLightVolumeResource->GetSizeZ());

  uint32 GroupSizeX = FMath::DivideAndRoundUp((int32)RLightVolumeResource->GetSizeX(),
                                              NUM_THREADS_PER_GROUP_DIMENSION);
  uint32 GroupSizeY = FMath::DivideAndRoundUp((int32)RLightVolumeResource->GetSizeY(),
                                              NUM_THREADS_PER_GROUP_DIMENSION);

  DispatchComputeShader(RHICmdList, *ComputeShader, GroupSizeX, GroupSizeY, 1);
  ComputeShader->UnbindLightVolumeUAVs(RHICmdList);

  RHICmdList.TransitionResources(EResourceTransitionAccess::EReadable,
                                 EResourceTransitionPipeline::EComputeToGfx, UAVs, 4);
}

void AddDirLightToSingleLightVolume_RenderThread(
    FRHICommandListImmediate& RHICmdList, FRHITexture3D* ALightVolumeResource,
    FRHITexture3D* VolumeResource, FRHITexture2D* TFResource, const FDirLightParameters LightParams,
    const bool LightAdded, const FTransform VolumeTransform,
    const FClippingPlaneParameters ClippingParameters, const FVector MeshMaxBounds,
    ERHIFeatureLevel::Type
        FeatureLevel) {  // TODO use a unit inv cube to avoid having to pass MeshMaxBounds!
  check(IsInRenderingThread());

  // Can't have directional light without direction...
  if (LightParams.LightDirection == FVector(0.0, 0.0, 0.0)) {
    GEngine->AddOnScreenDebugMessage(
        -1, 100.0f, FColor::Yellow,
        TEXT("Returning because the directional light doesn't have a direction."));
    return;
  }

  FDirLightParameters LocalLightParams;
  FMajorAxes LocalMajorAxes;
  // Calculate local Light parameters and corresponding axes.
  GetLocalLightParamsAndAxes(LightParams, VolumeTransform, LocalLightParams, LocalMajorAxes);
  // Transform clipping parameters into local space.
  FClippingPlaneParameters LocalClippingParameters =
      GetLocalClippingParameters(ClippingParameters, MeshMaxBounds, VolumeTransform);

  // For GPU profiling.
  SCOPED_DRAW_EVENTF(RHICmdList, AddDirLightToLightVolume_RenderThread, TEXT("Adding Lights"));
  SCOPED_GPU_STAT(RHICmdList, GPUAddingLights);
  // Get shader ref from GlobalShaderMap
  TShaderMap<FGlobalShaderType>* GlobalShaderMap = GetGlobalShaderMap(FeatureLevel);
  TShaderMapRef<FAddOrRemoveDirLightSingleVolumeShader> ComputeShader(GlobalShaderMap);

  RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
  // RHICmdList.TransitionResource(EResourceTransitionAccess::ERWNoBarrier, LightVolumeResource);
  FUnorderedAccessViewRHIRef AVolumeUAV = RHICreateUnorderedAccessView(ALightVolumeResource);

  // Don't need barriers on these - we only ever read/write to the same pixel from one thread -> no
  // race conditions But we definitely need to transition the resource to Compute-shader accessible,
  // otherwise the renderer might touch our textures while we're writing them.
  RHICmdList.TransitionResource(EResourceTransitionAccess::EWritable,
                                EResourceTransitionPipeline::EGfxToCompute, AVolumeUAV);
  ComputeShader->SetResources(RHICmdList, VolumeResource, TFResource, AVolumeUAV);

  for (unsigned i = 0; i < 2; i++) {
    // Break if the main axis weight == 1
    if (LocalMajorAxes.FaceWeight[i].second == 0) {
      break;
    }

    FVector2D textureOffset =
        GetPixOffset(LocalMajorAxes.FaceWeight[i].first, -LocalLightParams.LightDirection);

    // Get the X, Y and Z transposed into the current axis orientation.
    FIntVector TransposedDimensions =
        GetTransposedDimensions(LocalMajorAxes, ALightVolumeResource, i);

    uint32 ColorInt = GetBorderColorIntSingle(LocalLightParams, LocalMajorAxes, i);
    FLinearColor LinearBorderColor = FColor(ColorInt);
	
    FSamplerStateRHIRef readBuffSampler = GetBufferSamplerRef(ColorInt);
    // Create read-write buffer textures for both lights.
    FTexture2DRHIRef Texture1, Texture2;
    FUnorderedAccessViewRHIRef Texture1UAV, Texture2UAV;

    CreateBufferTexturesAndUAVs(TransposedDimensions, PF_R32_FLOAT, Texture1, Texture1UAV, Texture2,
                                Texture2UAV);

    ComputeShader->SetParameters(RHICmdList, LocalLightParams, LightAdded, LocalClippingParameters,
                                 LocalMajorAxes, i);

    uint32 GroupSizeX =
        FMath::DivideAndRoundUp(TransposedDimensions.X, NUM_THREADS_PER_GROUP_DIMENSION);
    uint32 GroupSizeY =
        FMath::DivideAndRoundUp(TransposedDimensions.Y, NUM_THREADS_PER_GROUP_DIMENSION);

    for (int j = 0; j < TransposedDimensions.Z; j++) {
      // Switch read and write buffers each row.
      if (j % 2 == 0) {
        TransitionBufferResources(RHICmdList, Texture1, Texture2UAV);
        ComputeShader->SetLoop(RHICmdList, j, Texture1, readBuffSampler, Texture2UAV);
      } else {
        TransitionBufferResources(RHICmdList, Texture2, Texture1UAV);
        ComputeShader->SetLoop(RHICmdList, j, Texture2, readBuffSampler, Texture1UAV);
      }

      DispatchComputeShader(RHICmdList, *ComputeShader, GroupSizeX, GroupSizeY, 1);
    }
  }

  // Unbind UAVs.
  ComputeShader->UnbindUAVs(RHICmdList);
  // Transition resources back to the renderer.
  RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable,
                                EResourceTransitionPipeline::EComputeToGfx, AVolumeUAV);
}

void ChangeDirLightInSingleLightVolume_RenderThread(
    FRHICommandListImmediate& RHICmdList, FRHITexture3D* ALightVolumeResource,
    FRHITexture3D* VolumeResource, FRHITexture2D* TFResource, const FDirLightParameters LightParams,
    const FDirLightParameters NewLightParams, const FTransform VolumeTransform,
    const FClippingPlaneParameters ClippingParameters, const FVector MeshMaxBounds,
    ERHIFeatureLevel::Type FeatureLevel) {
  // Can't have directional light without direction...
  if (LightParams.LightDirection == FVector(0.0, 0.0, 0.0)) {
    GEngine->AddOnScreenDebugMessage(
        -1, 100.0f, FColor::Yellow,
        TEXT("Returning because the directional light doesn't have a direction."));
    return;
  }

  FClippingPlaneParameters LocalClippingParameters =
      GetLocalClippingParameters(ClippingParameters, MeshMaxBounds, VolumeTransform);
  // Create local copies of Light Params, so that if we have to fall back to 2x AddOrRemoveLight, we
  // can just pass the original parameters.
  FDirLightParameters LocalLightParams, NewLocalLightParams;
  FMajorAxes LocalMajorAxes, NewLocalMajorAxes;
  // Calculate local Light parameters and corresponding axes.
  GetLocalLightParamsAndAxes(LightParams, VolumeTransform, LocalLightParams, LocalMajorAxes);
  GetLocalLightParamsAndAxes(NewLightParams, VolumeTransform, NewLocalLightParams,
                             NewLocalMajorAxes);

  // If lights have different major axes, do a proper removal and addition.
  // If first major axes are the same and above the dominance threshold, ignore whether the second
  // major axes are the same.
  if (LocalMajorAxes.FaceWeight[0].first != NewLocalMajorAxes.FaceWeight[0].first ||
      LocalMajorAxes.FaceWeight[1].first != NewLocalMajorAxes.FaceWeight[1].first) {
    //
    AddDirLightToSingleLightVolume_RenderThread(RHICmdList, ALightVolumeResource, VolumeResource,
                                                TFResource, LightParams, false, VolumeTransform,
                                                ClippingParameters, MeshMaxBounds, FeatureLevel);
    AddDirLightToSingleLightVolume_RenderThread(
        RHICmdList, ALightVolumeResource, VolumeResource, TFResource, NewLightParams, true,
        VolumeTransform, ClippingParameters, MeshMaxBounds, FeatureLevel);
    return;
  }

  // For GPU profiling.
  SCOPED_DRAW_EVENTF(RHICmdList, ChangeDirLightInLightVolume_RenderThread, TEXT("Adding Lights"));
  SCOPED_GPU_STAT(RHICmdList, GPUAddingLights);

  TShaderMap<FGlobalShaderType>* GlobalShaderMap = GetGlobalShaderMap(FeatureLevel);
  TShaderMapRef<FChangeDirLightSingleVolumeShader> ComputeShader(GlobalShaderMap);

  RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
  // RHICmdList.TransitionResource(EResourceTransitionAccess::ERWNoBarrier, LightVolumeResource);
  FUnorderedAccessViewRHIRef AVolumeUAV = RHICreateUnorderedAccessView(ALightVolumeResource);

  // Don't need barriers on these - we only ever read/write to the same pixel from one thread -> no
  // race conditions But we definitely need to transition the resource to Compute-shader accessible,
  // otherwise the renderer might touch our textures while we're writing them.
  RHICmdList.TransitionResource(EResourceTransitionAccess::EWritable,
                                EResourceTransitionPipeline::EGfxToCompute, AVolumeUAV);

  ComputeShader->SetResources(RHICmdList, VolumeResource, TFResource, AVolumeUAV);

  for (unsigned i = 0; i < 2; i++) {
    FVector2D textureOffset =
        GetPixOffset(LocalMajorAxes.FaceWeight[i].first, -LocalLightParams.LightDirection);

    // Get the X, Y and Z transposed into the current axis orientation.
    FIntVector TransposedDimensions =
        GetTransposedDimensions(LocalMajorAxes, ALightVolumeResource, i);

    // Get Color ints for texture borders.
    uint32 ColorInt = GetBorderColorIntSingle(LocalLightParams, LocalMajorAxes, i);
    uint32 NewColorInt = GetBorderColorIntSingle(NewLocalLightParams, NewLocalMajorAxes, i);
    // Get the sampler for read buffer to use border with the proper light color.
    FSamplerStateRHIRef readBuffSampler = GetBufferSamplerRef(ColorInt);
    FSamplerStateRHIRef newReadBufferSampler = GetBufferSamplerRef(NewColorInt);

	FLinearColor OldLinearBorderColor = FColor(ColorInt);
    FLinearColor NewLinearBorderColor = FColor(NewColorInt);
/*
    FString text = "Border color removed  = ";
    text += FString::SanitizeFloat(OldLinearBorderColor.R, 3) + ", " +
            FString::SanitizeFloat(OldLinearBorderColor.G, 3) + ", " +
            FString::SanitizeFloat(OldLinearBorderColor.B, 3) + ", " +
            FString::SanitizeFloat(OldLinearBorderColor.A, 3) +", added = " + 
			FString::SanitizeFloat(NewLinearBorderColor.R, 3) + ", " +
            FString::SanitizeFloat(NewLinearBorderColor.G, 3) + ", " +
            FString::SanitizeFloat(NewLinearBorderColor.B, 3) + ", " +
            FString::SanitizeFloat(NewLinearBorderColor.A, 3);
    GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Yellow, text);*/

    // Create read-write buffer textures for both lights.
    FTexture2DRHIRef Texture1, Texture2, NewTexture1, NewTexture2;
    FUnorderedAccessViewRHIRef Texture1UAV, Texture2UAV, NewTexture1UAV, NewTexture2UAV;

    CreateBufferTexturesAndUAVs(TransposedDimensions, PF_R32_FLOAT, Texture1, Texture1UAV, Texture2,
                                Texture2UAV);
    CreateBufferTexturesAndUAVs(TransposedDimensions, PF_R32_FLOAT, NewTexture1, NewTexture1UAV,
                                NewTexture2, NewTexture2UAV);

    ComputeShader->SetParameters(RHICmdList, LocalLightParams, NewLocalLightParams, LocalMajorAxes,
                                 NewLocalMajorAxes, LocalClippingParameters, i);

    // Get group sizes for compute shader
    uint32 GroupSizeX =
        FMath::DivideAndRoundUp(TransposedDimensions.X, NUM_THREADS_PER_GROUP_DIMENSION);
    uint32 GroupSizeY =
        FMath::DivideAndRoundUp(TransposedDimensions.Y, NUM_THREADS_PER_GROUP_DIMENSION);

    for (int j = 0; j < TransposedDimensions.Z; j++) {
      // Switch read and write buffers each cycle.
      if (j % 2 == 0) {
        TransitionBufferResources(RHICmdList, Texture1, Texture2UAV);
        TransitionBufferResources(RHICmdList, NewTexture1, NewTexture2UAV);
        ComputeShader->SetLoop(RHICmdList, j, Texture1, readBuffSampler, Texture2UAV, NewTexture1,
                               newReadBufferSampler, NewTexture2UAV);
      } else {
        TransitionBufferResources(RHICmdList, Texture2, Texture1UAV);
        TransitionBufferResources(RHICmdList, NewTexture2, NewTexture1UAV);
        ComputeShader->SetLoop(RHICmdList, j, Texture2, readBuffSampler, Texture1UAV, NewTexture2,
                               newReadBufferSampler, NewTexture1UAV);
      }

      DispatchComputeShader(RHICmdList, *ComputeShader, GroupSizeX, GroupSizeY, 1);
    }
  }

  // Unbind UAVs.
  ComputeShader->UnbindUAVs(RHICmdList);
  // Transition resources back to the renderer.
  RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable,
                                EResourceTransitionPipeline::EComputeToGfx, AVolumeUAV);
}

void ClearSingleLightVolume_RenderThread(FRHICommandListImmediate& RHICmdList,
                                         FRHITexture3D* ALightVolumeResource, float ClearValues,
                                         ERHIFeatureLevel::Type FeatureLevel) {
  TShaderMap<FGlobalShaderType>* GlobalShaderMap = GetGlobalShaderMap(FeatureLevel);
  TShaderMapRef<FClearSingleLightVolumeShader> ComputeShader(GlobalShaderMap);

  RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
  // RHICmdList.TransitionResource(EResourceTransitionAccess::ERWNoBarrier, LightVolumeResource);
  FUnorderedAccessViewRHIRef AVolumeUAV = RHICreateUnorderedAccessView(ALightVolumeResource);

  // Don't need barriers on these - we only ever read/write to the same pixel from one thread -> no
  // race conditions But we definitely need to transition the resource to Compute-shader accessible,
  // otherwise the renderer might touch our textures while we're writing them.
  RHICmdList.TransitionResource(EResourceTransitionAccess::ERWNoBarrier,
                                EResourceTransitionPipeline::EGfxToCompute, AVolumeUAV);

  ComputeShader->SetLightVolumeUAV(RHICmdList, AVolumeUAV);
  ComputeShader->SetParameters(RHICmdList, ClearValues, ALightVolumeResource->GetSizeZ());

  uint32 GroupSizeX = FMath::DivideAndRoundUp((int32)ALightVolumeResource->GetSizeX(),
                                              NUM_THREADS_PER_GROUP_DIMENSION);
  uint32 GroupSizeY = FMath::DivideAndRoundUp((int32)ALightVolumeResource->GetSizeY(),
                                              NUM_THREADS_PER_GROUP_DIMENSION);

  DispatchComputeShader(RHICmdList, *ComputeShader, GroupSizeX, GroupSizeY, 1);
  ComputeShader->UnbindLightVolumeUAV(RHICmdList);

  RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable,
                                EResourceTransitionPipeline::EComputeToGfx, AVolumeUAV);
}

#undef LOCTEXT_NAMESPACE

/*
  double end = FPlatformTime::Seconds();
  FString text = "Time elapsed before shader & copy creation = ";
  text += FString::SanitizeFloat(end - start, 6);
  GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Yellow, text);*/

// start = FPlatformTime::Seconds();

// text = "Time elapsed in shader & copy = ";
// text += FString::SanitizeFloat(start - end, 6);
// GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Yellow, text);
