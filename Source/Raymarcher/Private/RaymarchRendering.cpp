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

FIntVector GetTransposedDimensions(FCubeFace face, FRHITexture3D* VolumeRef) {
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

// For making statistics about GPU use.
DECLARE_FLOAT_COUNTER_STAT(TEXT("AddingLights"), STAT_GPU_AddingLights, STATGROUP_GPU);
DECLARE_GPU_STAT_NAMED(GPUAddingLights, TEXT("AddingLightsToVolume"));

void AddDirLightToLightVolume_RenderThread(
    FRHICommandListImmediate& RHICmdList, FRHITexture3D* RLightVolumeResource,
    FRHITexture3D* GLightVolumeResource, FRHITexture3D* BLightVolumeResource,
    FRHITexture3D* ALightVolumeResource, FRHITexture3D* VolumeResource, FRHITexture2D* TFResource,
    const FDirLightParameters LightParams, const bool LightAdded,
    const FTransform VolumeInvTransform, const FClippingPlaneParameters ClippingParameters,
    const FVector MeshMaxBounds, ERHIFeatureLevel::Type FeatureLevel) { // TODO use a unit inv cube to avoid having to pass MeshMaxBounds!
  check(IsInRenderingThread());

  // Can't have directional light without direction...
  if (LightParams.LightDirection == FVector(0.0, 0.0, 0.0)) {
    GEngine->AddOnScreenDebugMessage(
        -1, 100.0f, FColor::Yellow,
        TEXT("Returning because the directional light doesn't have a direction."));
    return;
  }

  FDirLightParameters LocalLightParams = LightParams;
  // Transform light directions into local space.
  LocalLightParams.LightDirection =
      VolumeInvTransform.TransformVector(LocalLightParams.LightDirection);
  // Normalize Light Direction to get unit length.
  LocalLightParams.LightDirection.Normalize();

  // Get Major Axes (notice inverting of light Direction - for a directional light, the position of
  // the light is the opposite of the direction) e.g. Directional light with direction (1, 0, 0) is
  // assumed to be shining from (-1, 0, 0)
  FMajorAxes axes = GetMajorAxes(-LocalLightParams.LightDirection);

  SCOPED_DRAW_EVENTF(RHICmdList, AddDirLightToLightVolume_RenderThread, TEXT("Adding Lights"));
  SCOPED_GPU_STAT(RHICmdList, GPUAddingLights);
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

  // Set the second axis weight as  1 - (major axis)
  unsigned passes = 2;

  
  // Get clipping center to (0-1) texture local space. (Invert transform, divide by mesh size,
  // divide by 2 and add 0.5 to get to (0-1) space.
  FVector LocalClippingCenter =
      (VolumeInvTransform.TransformPosition(ClippingParameters.ClippingCenter) /
       (MeshMaxBounds * 2)) +
      FVector(0.5, 0.5, 0.5);
  // Get clipping direction in local space - here we don't care about the mesh size (as long as it's
  // a cube, which it really bloody better be).
  FVector LocalClippingDirection =
      VolumeInvTransform.TransformVectorNoScale(ClippingParameters.ClippingDirection);


  axes.FaceWeight[1].second = 1 - axes.FaceWeight[0].second;
  
  FString text = "Adding/Removing light with new Major axis weight = " +
                 FString::SanitizeFloat(axes.FaceWeight[0].second) +
                 ", new second axis weight = " + FString::SanitizeFloat(axes.FaceWeight[1].second);
  GEngine->AddOnScreenDebugMessage(-1, 0, FColor::Yellow, text);

  for (unsigned i = 0; i < passes; i++) {
    // Break if the main axis weight == 1
    if (axes.FaceWeight[i].second == 0) {
      break;
    }

    FVector2D textureOffset =
        GetPixOffset(axes.FaceWeight[i].first, -LocalLightParams.LightDirection);

    // Get the X, Y and Z transposed into the current axis orientation.
    FIntVector groups = GetTransposedDimensions(axes.FaceWeight[i].first, ALightVolumeResource);

    uint8 alpha = (uint8)(LocalLightParams.LightIntensity * axes.FaceWeight[i].second * 255);

    /*FString addRemove = (LightParams.Added ? "Adding" : "Removing" );
    FString text = addRemove + " Light @ Axis " + FString::SanitizeFloat(axes.FaceWeight[i].first,
    0) + ", weight = " + FString::SanitizeFloat(axes.FaceWeight[i].second, 3) +
      ", pixel offset = " + FString::SanitizeFloat(textureOffset.X, 2) + ", " +
    FString::SanitizeFloat(textureOffset.Y, 2) + ", total alpha = " + FString::SanitizeFloat(alpha,
    1); GEngine->AddOnScreenDebugMessage(-1, 100, (LightParams.Added ? FColor::Yellow :
    FColor::Red), text);
*/

    uint8 R, G, B, A;
    R = G = B = 255;
    A = alpha;
#if PLATFORM_LITTLE_ENDIAN
    // Win32 x86
    uint32 colorInt = A << 24 | R << 16 | G << 8 | B;
#else  // PLATFORM_LITTLE_ENDIAN
    uint32 colorInt = B << 24 | G << 16 | R << 8 | A;
#endif

    // Set the sampler for read buffer to use border with the proper light color.
    FSamplerStateInitializerRHI init = FSamplerStateInitializerRHI(
        SF_Bilinear, AM_Border, AM_Border, AM_Border, 0, 0, 0, 0, colorInt);
    FSamplerStateRHIRef readBuffSampler = RHICreateSamplerState(init);

    // Create read-write buffer textures (with UAV createflag so we can use them in compute
    // shaders).
    FRHIResourceCreateInfo CreateInfo(FClearValueBinding::Transparent);
    FTexture2DRHIRef Texture1 =
        RHICreateTexture2D(groups.X, groups.Y, PF_A32B32G32R32F, 1, 1,
                           TexCreate_ShaderResource | TexCreate_UAV, CreateInfo);
    FTexture2DRHIRef Texture2 =
        RHICreateTexture2D(groups.X, groups.Y, PF_A32B32G32R32F, 1, 1,
                           TexCreate_ShaderResource | TexCreate_UAV, CreateInfo);
    FUnorderedAccessViewRHIRef Texture1UAV = RHICreateUnorderedAccessView(Texture1);
    FUnorderedAccessViewRHIRef Texture2UAV = RHICreateUnorderedAccessView(Texture2);

    ComputeShader->SetParameters(RHICmdList, LocalLightParams, LightAdded, axes.FaceWeight[i].first,
                                 axes.FaceWeight[i].second, LocalClippingCenter,
                                 LocalClippingDirection);
    for (int j = 0; j < groups.Z; j++) {
      // Switch read and write buffers each cycle.
      if (j % 2 == 0) {
        RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, Texture1);
        RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier,
                                      EResourceTransitionPipeline::EComputeToCompute, Texture2UAV);
        ComputeShader->SetLoop(RHICmdList, j, Texture1, readBuffSampler, Texture2UAV);
        //	ComputeShader->SetBorderSampler(RHICmdList, LightParams.LightColor,
        // axes.FaceWeight[i].second * LightParams.LightIntensity);
      } else {
        RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, Texture2);
        RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier,
                                      EResourceTransitionPipeline::EComputeToCompute, Texture1UAV);
        ComputeShader->SetLoop(RHICmdList, j, Texture2, readBuffSampler, Texture1UAV);
        //	ComputeShader->SetBorderSampler(RHICmdList, LightParams.LightColor,
        // axes.FaceWeight[i].second * LightParams.LightIntensity);
      }

      uint32 GroupSizeX = FMath::DivideAndRoundUp(groups.X, NUM_THREADS_PER_GROUP_DIMENSION);
      uint32 GroupSizeY = FMath::DivideAndRoundUp(groups.Y, NUM_THREADS_PER_GROUP_DIMENSION);

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
    const FTransform VolumeInvTransform, const FClippingPlaneParameters ClippingParameters,
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
  FDirLightParameters LocalLightParams = LightParams;
  FDirLightParameters NewLocalLightParams = NewLightParams;

  // Transform light directions into local space.
  LocalLightParams.LightDirection =
      VolumeInvTransform.TransformVector(LocalLightParams.LightDirection);
  NewLocalLightParams.LightDirection =
      VolumeInvTransform.TransformVector(NewLocalLightParams.LightDirection);

  // Normalize Light Direction to get unit length, use old light's axes.
  LocalLightParams.LightDirection.Normalize();
  NewLocalLightParams.LightDirection.Normalize();

  // Get Major Axes (notice inverting of light Direction - for a directional light, the position of
  // the light is the opposite of the direction) e.g. Directional light with direction (1, 0, 0) is
  // assumed to be shining from (-1, 0, 0)
  FMajorAxes oldAxes = GetMajorAxes(-LocalLightParams.LightDirection);
  FMajorAxes newAxes = GetMajorAxes(-NewLocalLightParams.LightDirection);

  // Set the second axis weight as  1 - (major axis)
  unsigned passes = 2;

  // If lights have different major axes, do a proper removal and addition.
  // If first major axes are the same and above the dominance threshold, ignore whether the second
  // major axes are the same.
  if (oldAxes.FaceWeight[0].first != newAxes.FaceWeight[0].first || 
	  oldAxes.FaceWeight[1].first != newAxes.FaceWeight[1].first) {
    AddDirLightToLightVolume_RenderThread(
        RHICmdList, RLightVolumeResource, GLightVolumeResource, BLightVolumeResource,
        ALightVolumeResource, VolumeResource, TFResource, LightParams, false, VolumeInvTransform,
        ClippingParameters, MeshMaxBounds, FeatureLevel);
    AddDirLightToLightVolume_RenderThread(
        RHICmdList, RLightVolumeResource, GLightVolumeResource, BLightVolumeResource,
        ALightVolumeResource, VolumeResource, TFResource, NewLightParams, true, VolumeInvTransform,
        ClippingParameters, MeshMaxBounds, FeatureLevel);
    return;
  }

  oldAxes.FaceWeight[1].second = 1 - oldAxes.FaceWeight[0].second;
  newAxes.FaceWeight[1].second = 1 - newAxes.FaceWeight[0].second;

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

  //	oldAxes.FaceWeight[0].second = newAxes.FaceWeight[0].second = 1;

  FString text =
      "Moving light with new Major axis weight = " +
      FString::SanitizeFloat(newAxes.FaceWeight[0].second) +
      ", new second axis weight = " + FString::SanitizeFloat(newAxes.FaceWeight[1].second);
  GEngine->AddOnScreenDebugMessage(-1, 0, FColor::Yellow, text);

  // Get clipping center to (0-1) texture local space. (Invert transform, divide by mesh size,
  // divide by 2 and add 0.5 to get to (0-1) space.
  FVector LocalClippingCenter =
      (VolumeInvTransform.TransformPosition(ClippingParameters.ClippingCenter) /
       (MeshMaxBounds * 2)) +
      FVector(0.5, 0.5, 0.5);
  // Get clipping direction in local space - here we don't care about the mesh size (as long as
  // it's a cube, which it really bloody better be).
  FVector LocalClippingDirection =
      VolumeInvTransform.TransformVectorNoScale(ClippingParameters.ClippingDirection);

  for (unsigned i = 0; i < passes; i++) {
    FVector2D textureOffset =
        GetPixOffset(oldAxes.FaceWeight[i].first, -LocalLightParams.LightDirection);

    // Get the X, Y and Z transposed into the current axis orientation.
    FIntVector groups = GetTransposedDimensions(oldAxes.FaceWeight[i].first, ALightVolumeResource);

    uint8 alpha = (uint8)(LocalLightParams.LightIntensity * oldAxes.FaceWeight[i].second * 255);
    uint8 newAlpha =
        (uint8)(NewLocalLightParams.LightIntensity * newAxes.FaceWeight[i].second * 255);

    /*FString addRemove = (LocalLightParams.Added ? "Adding" : "Removing" );
    FString text = addRemove + " Light @ Axis " + FString::SanitizeFloat(axes.FaceWeight[i].first,
    0) + ", weight = " + FString::SanitizeFloat(axes.FaceWeight[i].second, 3) +
      ", pixel offset = " + FString::SanitizeFloat(textureOffset.X, 2) + ", " +
    FString::SanitizeFloat(textureOffset.Y, 2) + ", total alpha = " + FString::SanitizeFloat(alpha,
    1); GEngine->AddOnScreenDebugMessage(-1, 100, (LocalLightParams.Added ? FColor::Yellow :
    FColor::Red), text);
*/

    uint8 R, G, B;
    R = G = B = 255;
#if PLATFORM_LITTLE_ENDIAN
    // Win32 x86
    uint32 colorInt = alpha << 24 | R << 16 | G << 8 | B;
    uint32 newColorInt = newAlpha << 24 | R << 16 | G << 8 | B;
#else  // PLATFORM_LITTLE_ENDIAN
    uint32 colorInt = B << 24 | G << 16 | R << 8 | A;
#endif

    //		FColor actualColor = FColor(255, 255, 255, alpha);
    //		uint32 colorInt = actualColor.ToPackedBGRA();

    // Set the sampler for read buffer to use border with the proper light color.
    FSamplerStateInitializerRHI init = FSamplerStateInitializerRHI(
        SF_Trilinear, AM_Border, AM_Border, AM_Border, 0, 0, 0, 0, colorInt);
    FSamplerStateRHIRef readBuffSampler = RHICreateSamplerState(init);
    FSamplerStateInitializerRHI newinit = FSamplerStateInitializerRHI(
        SF_Trilinear, AM_Border, AM_Border, AM_Border, 0, 0, 0, 0, newColorInt);
    FSamplerStateRHIRef newReadBufferSampler = RHICreateSamplerState(newinit);

    // Create read-write buffer textures for the removed light (with UAV createflag so we can use
    // them in compute shaders).
    FRHIResourceCreateInfo CreateInfo(FClearValueBinding::Transparent);
    FTexture2DRHIRef Texture1 =
        RHICreateTexture2D(groups.X, groups.Y, PF_A32B32G32R32F, 1, 1,
                           TexCreate_ShaderResource | TexCreate_UAV, CreateInfo);
    FTexture2DRHIRef Texture2 =
        RHICreateTexture2D(groups.X, groups.Y, PF_A32B32G32R32F, 1, 1,
                           TexCreate_ShaderResource | TexCreate_UAV, CreateInfo);
    // Create read-write buffers for the added light.
    FTexture2DRHIRef NewTexture1 =
        RHICreateTexture2D(groups.X, groups.Y, PF_A32B32G32R32F, 1, 1,
                           TexCreate_ShaderResource | TexCreate_UAV, CreateInfo);
    FTexture2DRHIRef NewTexture2 =
        RHICreateTexture2D(groups.X, groups.Y, PF_A32B32G32R32F, 1, 1,
                           TexCreate_ShaderResource | TexCreate_UAV, CreateInfo);
    // Create the UAV accessors.
    FUnorderedAccessViewRHIRef Texture1UAV = RHICreateUnorderedAccessView(Texture1);
    FUnorderedAccessViewRHIRef Texture2UAV = RHICreateUnorderedAccessView(Texture2);
    FUnorderedAccessViewRHIRef NewTexture1UAV = RHICreateUnorderedAccessView(NewTexture1);
    FUnorderedAccessViewRHIRef NewTexture2UAV = RHICreateUnorderedAccessView(NewTexture2);

    ComputeShader->SetParameters(
        RHICmdList, LocalLightParams, NewLocalLightParams, oldAxes.FaceWeight[i].first,
        oldAxes.FaceWeight[i].second, newAxes.FaceWeight[i].second, LocalClippingCenter,
        LocalClippingDirection);
    for (int j = 0; j < groups.Z; j++) {
      // Switch read and write buffers each cycle.
      if (j % 2 == 0) {
        RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, Texture1);
        RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, NewTexture1);
        RHICmdList.TransitionResource(EResourceTransitionAccess::EWritable,
                                      EResourceTransitionPipeline::EComputeToCompute, Texture2UAV);
        RHICmdList.TransitionResource(EResourceTransitionAccess::EWritable,
                                      EResourceTransitionPipeline::EComputeToCompute,
                                      NewTexture2UAV);
        ComputeShader->SetLoop(RHICmdList, j, Texture1, readBuffSampler, Texture2UAV, NewTexture1,
                               newReadBufferSampler, NewTexture2UAV);
      } else {
        RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, Texture2);
        RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, NewTexture2);
        RHICmdList.TransitionResource(EResourceTransitionAccess::EWritable,
                                      EResourceTransitionPipeline::EComputeToCompute, Texture1UAV);
        RHICmdList.TransitionResource(EResourceTransitionAccess::EWritable,
                                      EResourceTransitionPipeline::EComputeToCompute,
                                      NewTexture1UAV);
        ComputeShader->SetLoop(RHICmdList, j, Texture2, readBuffSampler, Texture1UAV, NewTexture2,
                               newReadBufferSampler, NewTexture1UAV);
      }

      uint32 GroupSizeX = FMath::DivideAndRoundUp(groups.X, NUM_THREADS_PER_GROUP_DIMENSION);
      uint32 GroupSizeY = FMath::DivideAndRoundUp(groups.Y, NUM_THREADS_PER_GROUP_DIMENSION);

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
    FRHICommandListImmediate& RHICmdList, FRHITexture3D* ALightVolumeResource, FRHITexture3D* VolumeResource, FRHITexture2D* TFResource,
    const FDirLightParameters LightParams, const bool LightAdded,
    const FTransform VolumeInvTransform, const FClippingPlaneParameters ClippingParameters,
    const FVector MeshMaxBounds, ERHIFeatureLevel::Type FeatureLevel) { // TODO use a unit inv cube to avoid having to pass MeshMaxBounds!
  check(IsInRenderingThread());

  // Can't have directional light without direction...
  if (LightParams.LightDirection == FVector(0.0, 0.0, 0.0)) {
    GEngine->AddOnScreenDebugMessage(
        -1, 100.0f, FColor::Yellow,
        TEXT("Returning because the directional light doesn't have a direction."));
    return;
  }

  FDirLightParameters LocalLightParams = LightParams;
  // Transform light directions into local space.
  LocalLightParams.LightDirection =
  VolumeInvTransform.TransformVector(LocalLightParams.LightDirection);
  // Normalize Light Direction to get unit length.
  LocalLightParams.LightDirection.Normalize();

  // Get Major Axes (notice inverting of light Direction - for a directional light, the position of
  // the light is the opposite of the direction) e.g. Directional light with direction (1, 0, 0) is
  // assumed to be shining from (-1, 0, 0)
  FMajorAxes axes = GetMajorAxes(-LocalLightParams.LightDirection);

  SCOPED_DRAW_EVENTF(RHICmdList, AddDirLightToLightVolume_RenderThread, TEXT("Adding Lights"));
  SCOPED_GPU_STAT(RHICmdList, GPUAddingLights);
  TShaderMap<FGlobalShaderType>* GlobalShaderMap = GetGlobalShaderMap(FeatureLevel);
  TShaderMapRef<FAddOrRemoveDirLightSingleVolumeShader> ComputeShader(GlobalShaderMap);
  
  RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
  // RHICmdList.TransitionResource(EResourceTransitionAccess::ERWNoBarrier, LightVolumeResource);
  FUnorderedAccessViewRHIRef AVolumeUAV = RHICreateUnorderedAccessView(ALightVolumeResource);
  
  // Don't need barriers on these - we only ever read/write to the same pixel from one thread -> no
  // race conditions But we definitely need to transition the resource to Compute-shader accessible,
  // otherwise the renderer might touch our textures while we're writing them.
  RHICmdList.TransitionResource(EResourceTransitionAccess::EWritable, EResourceTransitionPipeline::EGfxToCompute, AVolumeUAV);
  ComputeShader->SetResources(RHICmdList, VolumeResource, TFResource, AVolumeUAV);

  // Set the second axis weight as  1 - (major axis)
  unsigned passes = 2;

  
  // Get clipping center to (0-1) texture local space. (Invert transform, divide by mesh size,
  // divide by 2 and add 0.5 to get to (0-1) space.
  FVector LocalClippingCenter =
      (VolumeInvTransform.TransformPosition(ClippingParameters.ClippingCenter) /
       (MeshMaxBounds * 2)) +
      FVector(0.5, 0.5, 0.5);
  // Get clipping direction in local space - here we don't care about the mesh size (as long as it's
  // a cube, which it really bloody better be).
  FVector LocalClippingDirection =
      VolumeInvTransform.TransformVectorNoScale(ClippingParameters.ClippingDirection);


  axes.FaceWeight[1].second = 1 - axes.FaceWeight[0].second;
  
  FString text = "SINGLE Adding/Removing light with new Major axis weight = " +
                 FString::SanitizeFloat(axes.FaceWeight[0].second) +
                 ", new second axis weight = " + FString::SanitizeFloat(axes.FaceWeight[1].second);
  GEngine->AddOnScreenDebugMessage(-1, 0, FColor::Yellow, text);

  for (unsigned i = 0; i < passes; i++) {
    // Break if the main axis weight == 1
    if (axes.FaceWeight[i].second == 0) {
      break;
    }

    FVector2D textureOffset =
        GetPixOffset(axes.FaceWeight[i].first, -LocalLightParams.LightDirection);

    // Get the X, Y and Z transposed into the current axis orientation.
    FIntVector groups = GetTransposedDimensions(axes.FaceWeight[i].first, ALightVolumeResource);

    uint8 alpha = (uint8)(LocalLightParams.LightIntensity * axes.FaceWeight[i].second * 255);

    uint8 R, G, B, A;
    R = G = B = 255;
    A = alpha;
#if PLATFORM_LITTLE_ENDIAN
    // Win32 x86
    uint32 colorInt = A << 24 | R << 16 | G << 8 | B;
#else  // PLATFORM_LITTLE_ENDIAN
    uint32 colorInt = B << 24 | G << 16 | R << 8 | A;
#endif

    // Set the sampler for read buffer to use border with the proper light color.
    FSamplerStateInitializerRHI init = FSamplerStateInitializerRHI(
        SF_Bilinear, AM_Border, AM_Border, AM_Border, 0, 0, 0, 0, colorInt);
    FSamplerStateRHIRef readBuffSampler = RHICreateSamplerState(init);

    // Create read-write buffer textures (with UAV createflag so we can use them in compute
    // shaders).
    FRHIResourceCreateInfo CreateInfo(FClearValueBinding::Transparent);
    FTexture2DRHIRef Texture1 =
        RHICreateTexture2D(groups.X, groups.Y, PF_A32B32G32R32F, 1, 1,
                           TexCreate_ShaderResource | TexCreate_UAV, CreateInfo);
    FTexture2DRHIRef Texture2 =
        RHICreateTexture2D(groups.X, groups.Y, PF_A32B32G32R32F, 1, 1,
                           TexCreate_ShaderResource | TexCreate_UAV, CreateInfo);
    FUnorderedAccessViewRHIRef Texture1UAV = RHICreateUnorderedAccessView(Texture1);
    FUnorderedAccessViewRHIRef Texture2UAV = RHICreateUnorderedAccessView(Texture2);

    ComputeShader->SetParameters(RHICmdList, LocalLightParams, LightAdded, axes.FaceWeight[i].first,
                                 axes.FaceWeight[i].second, LocalClippingCenter,
                                 LocalClippingDirection);
    for (int j = 0; j < groups.Z; j++) {
      // Switch read and write buffers each cycle.
      if (j % 2 == 0) {
        RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, Texture1);
        RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier,
                                      EResourceTransitionPipeline::EComputeToCompute, Texture2UAV);
        ComputeShader->SetLoop(RHICmdList, j, Texture1, readBuffSampler, Texture2UAV);
        //	ComputeShader->SetBorderSampler(RHICmdList, LightParams.LightColor,
        // axes.FaceWeight[i].second * LightParams.LightIntensity);
      } else {
        RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, Texture2);
        RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier,
                                      EResourceTransitionPipeline::EComputeToCompute, Texture1UAV);
        ComputeShader->SetLoop(RHICmdList, j, Texture2, readBuffSampler, Texture1UAV);
        //	ComputeShader->SetBorderSampler(RHICmdList, LightParams.LightColor,
        // axes.FaceWeight[i].second * LightParams.LightIntensity);
      }

      uint32 GroupSizeX = FMath::DivideAndRoundUp(groups.X, NUM_THREADS_PER_GROUP_DIMENSION);
      uint32 GroupSizeY = FMath::DivideAndRoundUp(groups.Y, NUM_THREADS_PER_GROUP_DIMENSION);

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
    FRHICommandListImmediate& RHICmdList, FRHITexture3D* ALightVolumeResource, FRHITexture3D* VolumeResource, FRHITexture2D* TFResource,
    const FDirLightParameters LightParams, const FDirLightParameters NewLightParams,
    const FTransform VolumeInvTransform, const FClippingPlaneParameters ClippingParameters,
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
  FDirLightParameters LocalLightParams = LightParams;
  FDirLightParameters NewLocalLightParams = NewLightParams;

  // Transform light directions into local space.
  LocalLightParams.LightDirection =
      VolumeInvTransform.TransformVector(LocalLightParams.LightDirection);
  NewLocalLightParams.LightDirection =
      VolumeInvTransform.TransformVector(NewLocalLightParams.LightDirection);

  // Normalize Light Direction to get unit length, use old light's axes.
  LocalLightParams.LightDirection.Normalize();
  NewLocalLightParams.LightDirection.Normalize();

  // Get Major Axes (notice inverting of light Direction - for a directional light, the position of
  // the light is the opposite of the direction) e.g. Directional light with direction (1, 0, 0) is
  // assumed to be shining from (-1, 0, 0)
  FMajorAxes oldAxes = GetMajorAxes(-LocalLightParams.LightDirection);
  FMajorAxes newAxes = GetMajorAxes(-NewLocalLightParams.LightDirection);

  // Set the second axis weight as  1 - (major axis)
  unsigned passes = 2;

  // If lights have different major axes, do a proper removal and addition.
  // If first major axes are the same and above the dominance threshold, ignore whether the second
  // major axes are the same.
  if (oldAxes.FaceWeight[0].first != newAxes.FaceWeight[0].first || 
	  oldAxes.FaceWeight[1].first != newAxes.FaceWeight[1].first) {
    AddDirLightToSingleLightVolume_RenderThread(
        RHICmdList, ALightVolumeResource, VolumeResource, TFResource, LightParams, false, VolumeInvTransform,
        ClippingParameters, MeshMaxBounds, FeatureLevel);
    AddDirLightToSingleLightVolume_RenderThread(
        RHICmdList, ALightVolumeResource, VolumeResource, TFResource, NewLightParams, true, VolumeInvTransform,
        ClippingParameters, MeshMaxBounds, FeatureLevel);
    return;
  }

  oldAxes.FaceWeight[1].second = 1 - oldAxes.FaceWeight[0].second;
  newAxes.FaceWeight[1].second = 1 - newAxes.FaceWeight[0].second;

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
  RHICmdList.TransitionResource(EResourceTransitionAccess::EWritable, EResourceTransitionPipeline::EGfxToCompute, AVolumeUAV);

  ComputeShader->SetResources(RHICmdList, VolumeResource, TFResource, AVolumeUAV);

  //	oldAxes.FaceWeight[0].second = newAxes.FaceWeight[0].second = 1;

  FString text =
      "Moving light with new Major axis weight = " +
      FString::SanitizeFloat(newAxes.FaceWeight[0].second) +
      ", new second axis weight = " + FString::SanitizeFloat(newAxes.FaceWeight[1].second);
  GEngine->AddOnScreenDebugMessage(-1, 0, FColor::Yellow, text);

  // Get clipping center to (0-1) texture local space. (Invert transform, divide by mesh size,
  // divide by 2 and add 0.5 to get to (0-1) space.
  FVector LocalClippingCenter =
      (VolumeInvTransform.TransformPosition(ClippingParameters.ClippingCenter) /
       (MeshMaxBounds * 2)) +
      FVector(0.5, 0.5, 0.5);
  // Get clipping direction in local space - here we don't care about the mesh size (as long as
  // it's a cube, which it really bloody better be).
  FVector LocalClippingDirection =
      VolumeInvTransform.TransformVectorNoScale(ClippingParameters.ClippingDirection);

  for (unsigned i = 0; i < passes; i++) {
    FVector2D textureOffset =
        GetPixOffset(oldAxes.FaceWeight[i].first, -LocalLightParams.LightDirection);

    // Get the X, Y and Z transposed into the current axis orientation.
    FIntVector groups = GetTransposedDimensions(oldAxes.FaceWeight[i].first, ALightVolumeResource);

    uint8 alpha = (uint8)(LocalLightParams.LightIntensity * oldAxes.FaceWeight[i].second * 255);
    uint8 newAlpha =
        (uint8)(NewLocalLightParams.LightIntensity * newAxes.FaceWeight[i].second * 255);
	
    uint8 R, G, B;
    R = G = B = 255;
#if PLATFORM_LITTLE_ENDIAN
    // Win32 x86
    uint32 colorInt = alpha << 24 | R << 16 | G << 8 | B;
    uint32 newColorInt = newAlpha << 24 | R << 16 | G << 8 | B;
#else  // PLATFORM_LITTLE_ENDIAN
    uint32 colorInt = B << 24 | G << 16 | R << 8 | A;
#endif
	
    // Set the sampler for read buffer to use border with the proper light color.
    FSamplerStateInitializerRHI init = FSamplerStateInitializerRHI(
        SF_Trilinear, AM_Border, AM_Border, AM_Border, 0, 0, 0, 0, colorInt);
    FSamplerStateRHIRef readBuffSampler = RHICreateSamplerState(init);

    FSamplerStateInitializerRHI newinit = FSamplerStateInitializerRHI(
        SF_Trilinear, AM_Border, AM_Border, AM_Border, 0, 0, 0, 0, newColorInt);
    FSamplerStateRHIRef newReadBufferSampler = RHICreateSamplerState(newinit);

    // Create read-write buffer textures for the removed light (with UAV createflag so we can use
    // them in compute shaders).
    FRHIResourceCreateInfo CreateInfo(FClearValueBinding::Transparent);
    FTexture2DRHIRef Texture1 =
        RHICreateTexture2D(groups.X, groups.Y, PF_A32B32G32R32F, 1, 1,
                           TexCreate_ShaderResource | TexCreate_UAV, CreateInfo);
    FTexture2DRHIRef Texture2 =
        RHICreateTexture2D(groups.X, groups.Y, PF_A32B32G32R32F, 1, 1,
                           TexCreate_ShaderResource | TexCreate_UAV, CreateInfo);
    // Create read-write buffers for the added light.
    FTexture2DRHIRef NewTexture1 =
        RHICreateTexture2D(groups.X, groups.Y, PF_A32B32G32R32F, 1, 1,
                           TexCreate_ShaderResource | TexCreate_UAV, CreateInfo);
    FTexture2DRHIRef NewTexture2 =
        RHICreateTexture2D(groups.X, groups.Y, PF_A32B32G32R32F, 1, 1,
                           TexCreate_ShaderResource | TexCreate_UAV, CreateInfo);
    // Create the UAV accessors.
    FUnorderedAccessViewRHIRef Texture1UAV = RHICreateUnorderedAccessView(Texture1);
    FUnorderedAccessViewRHIRef Texture2UAV = RHICreateUnorderedAccessView(Texture2);
    FUnorderedAccessViewRHIRef NewTexture1UAV = RHICreateUnorderedAccessView(NewTexture1);
    FUnorderedAccessViewRHIRef NewTexture2UAV = RHICreateUnorderedAccessView(NewTexture2);

    ComputeShader->SetParameters(
        RHICmdList, LocalLightParams, NewLocalLightParams, oldAxes.FaceWeight[i].first,
        oldAxes.FaceWeight[i].second, newAxes.FaceWeight[i].second, LocalClippingCenter,
        LocalClippingDirection);
    for (int j = 0; j < groups.Z; j++) {
      // Switch read and write buffers each cycle.
      if (j % 2 == 0) {
        RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, Texture1);
        RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, NewTexture1);
        RHICmdList.TransitionResource(EResourceTransitionAccess::EWritable,
                                      EResourceTransitionPipeline::EComputeToCompute, Texture2UAV);
        RHICmdList.TransitionResource(EResourceTransitionAccess::EWritable,
                                      EResourceTransitionPipeline::EComputeToCompute,
                                      NewTexture2UAV);
        ComputeShader->SetLoop(RHICmdList, j, Texture1, readBuffSampler, Texture2UAV, NewTexture1,
                               newReadBufferSampler, NewTexture2UAV);
      } else {
        RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, Texture2);
        RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, NewTexture2);
        RHICmdList.TransitionResource(EResourceTransitionAccess::EWritable,
                                      EResourceTransitionPipeline::EComputeToCompute, Texture1UAV);
        RHICmdList.TransitionResource(EResourceTransitionAccess::EWritable,
                                      EResourceTransitionPipeline::EComputeToCompute,
                                      NewTexture1UAV);
        ComputeShader->SetLoop(RHICmdList, j, Texture2, readBuffSampler, Texture1UAV, NewTexture2,
                               newReadBufferSampler, NewTexture1UAV);
      }

      uint32 GroupSizeX = FMath::DivideAndRoundUp(groups.X, NUM_THREADS_PER_GROUP_DIMENSION);
      uint32 GroupSizeY = FMath::DivideAndRoundUp(groups.Y, NUM_THREADS_PER_GROUP_DIMENSION);

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
