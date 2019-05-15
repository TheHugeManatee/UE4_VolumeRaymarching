// (C) Technical University of Munich - Computer Aided Medical Procedures
// Developed by Tomas Bartipan (tomas.bartipan@tum.de)

#include "RaymarchRendering.h"
#include "AssetRegistryModule.h"
#include "RenderCore/Public/RenderUtils.h"
#include "Renderer/Public/VolumeRendering.h"
#include "TextureHelperFunctions.h"

#define LOCTEXT_NAMESPACE "RaymarchPlugin"

IMPLEMENT_SHADER_TYPE(, FVolumePS, TEXT("/Plugin/VolumeRaymarching/Private/RaymarchShader.usf"),
                      TEXT("PassthroughPS"), SF_Pixel)

IMPLEMENT_SHADER_TYPE(, FAddDirLightShaderSingle,
                      TEXT("/Plugin/VolumeRaymarching/Private/AddDirLightShader.usf"),
                      TEXT("MainComputeShader"), SF_Compute)

IMPLEMENT_SHADER_TYPE(, FChangeDirLightShaderSingle,
                      TEXT("/Plugin/VolumeRaymarching/Private/ChangeDirLightShader.usf"),
                      TEXT("MainComputeShader"), SF_Compute)

IMPLEMENT_SHADER_TYPE(, FClearVolumeTextureShader,
                      TEXT("/Plugin/VolumeRaymarching/Private/ClearVolumeTextureShader.usf"),
                      TEXT("MainComputeShader"), SF_Compute)

IMPLEMENT_SHADER_TYPE(, FClearFloatRWTextureCS,
                      TEXT("/Plugin/VolumeRaymarching/Private/ClearTextureShader.usf"),
                      TEXT("MainComputeShader"), SF_Compute)

IMPLEMENT_SHADER_TYPE(, FMakeMaxMipsShader,
                      TEXT("/Plugin/VolumeRaymarching/Private/CreateMaxMipsShader.usf"),
                      TEXT("MainComputeShader"), SF_Compute)

IMPLEMENT_SHADER_TYPE(, FMakeDistanceFieldShader,
                      TEXT("/Plugin/VolumeRaymarching/Private/CreateDistanceFieldShader.usf"),
                      TEXT("MainComputeShader"), SF_Compute)

IMPLEMENT_SHADER_TYPE(, FWriteSliceToTextureShader,
                      TEXT("/Plugin/VolumeRaymarching/Private/WriteSliceToTextureShader.usf"),
                      TEXT("MainComputeShader"), SF_Compute)

#define NUM_THREADS_PER_GROUP_DIMENSION \
  16  // This has to be the same as in the compute shader's spec [X, X, 1]

// Writes to 3D texture slice(s).
void WriteTo3DTexture_RenderThread(FRHICommandListImmediate& RHICmdList, FIntVector Size,
                                   UVolumeTexture* inTexture) {
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
  TShaderMap<FGlobalShaderType>* GlobalShaderMap = GetGlobalShaderMap(ERHIFeatureLevel::SM5);
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

// For making statistics about GPU use.
DECLARE_FLOAT_COUNTER_STAT(TEXT("GeneratingMIPs"), STAT_GPU_GeneratingMIPs, STATGROUP_GPU);
DECLARE_GPU_STAT_NAMED(GPUGeneratingMIPs, TEXT("Generating Volume MIPs"));

void GenerateVolumeTextureMipLevels_RenderThread(FRHICommandListImmediate& RHICmdList,
                                                 FIntVector Dimensions,
                                                 FRHITexture3D* VolumeResource,
                                                 FRHITexture2D* TransferFunc) {
  check(IsInRenderingThread());

  // For GPU profiling.
  SCOPED_DRAW_EVENTF(RHICmdList, GenerateVolumeTextureMipLevels_RenderThread,
                     TEXT("Generating MIPs"));
  SCOPED_GPU_STAT(RHICmdList, GPUGeneratingMIPs);
  // Get shader ref from GlobalShaderMap

  // Find and set compute shader
  TShaderMap<FGlobalShaderType>* GlobalShaderMap = GetGlobalShaderMap(ERHIFeatureLevel::SM5);
  TShaderMapRef<FMakeMaxMipsShader> ComputeShader(GlobalShaderMap);
  FComputeShaderRHIParamRef ShaderRHI = ComputeShader->GetComputeShader();
  RHICmdList.SetComputeShader(ShaderRHI);

  RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, VolumeResource);

  FUnorderedAccessViewRHIRef VolumeUAVs[8];

  for (uint8 i = 0; i < 8; i++) {
    VolumeUAVs[i] = RHICreateUnorderedAccessView(VolumeResource, i);
  }

  for (uint8 i = 0; i < 7; i++) {
    ComputeShader->SetResources(RHICmdList, ShaderRHI, VolumeUAVs[i], VolumeUAVs[i + 1]);
    Dimensions /= 2;
    if (Dimensions.X == 0 || Dimensions.Y == 0 || Dimensions.Z == 0) {
      GEngine->AddOnScreenDebugMessage(0, 10, FColor::Red,
                                       "Zero dimension mip detected! Aborting...");
      break;
    }
    DispatchComputeShader(RHICmdList, *ComputeShader, Dimensions.X, Dimensions.Y, Dimensions.Z);
  }

  // Unbind UAVs.
  ComputeShader->UnbindResources(RHICmdList, ShaderRHI);

  // Transition resources back to the renderer.
  RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, VolumeResource);
}

void GenerateDistanceField_RenderThread(FRHICommandListImmediate& RHICmdList, FIntVector Dimensions,
                                        FRHITexture3D* VolumeResource, FRHITexture2D* TransferFunc,
                                        FRHITexture3D* DistanceFieldResource,
                                        float localSphereDiameter, float threshold) {
  check(IsInRenderingThread());

  // For GPU profiling.
  SCOPED_DRAW_EVENTF(RHICmdList, GenerateDistanceField_RenderThread, TEXT("Generating MIPs"));
  SCOPED_GPU_STAT(RHICmdList, GPUGeneratingMIPs);
  // Get shader ref from GlobalShaderMap

  // Find and set compute shader
  TShaderMap<FGlobalShaderType>* GlobalShaderMap = GetGlobalShaderMap(ERHIFeatureLevel::SM5);
  TShaderMapRef<FMakeDistanceFieldShader> ComputeShader(GlobalShaderMap);
  FComputeShaderRHIParamRef ShaderRHI = ComputeShader->GetComputeShader();
  RHICmdList.SetComputeShader(ShaderRHI);

  RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, VolumeResource);
  RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, TransferFunc);
  RHICmdList.TransitionResource(EResourceTransitionAccess::ERWNoBarrier, DistanceFieldResource);

  FIntVector brush = FIntVector(localSphereDiameter * VolumeResource->GetSizeX(),
                                localSphereDiameter * VolumeResource->GetSizeY(),
                                localSphereDiameter * VolumeResource->GetSizeZ());

  // brush = FIntVector(9,9, 21);

  FUnorderedAccessViewRHIRef DistanceFieldUAV = RHICreateUnorderedAccessView(DistanceFieldResource);

  ComputeShader->SetResources(RHICmdList, ShaderRHI, DistanceFieldUAV, VolumeResource, TransferFunc,
                              brush, threshold, localSphereDiameter / 2);

  uint32 GroupSizeX = FMath::DivideAndRoundUp(Dimensions.X, NUM_THREADS_PER_GROUP_DIMENSION);
  uint32 GroupSizeY = FMath::DivideAndRoundUp(Dimensions.Y, NUM_THREADS_PER_GROUP_DIMENSION);

  // Separate into parts so that the GPU doesn't hang.
  for (int i = 0; i < 10; i++) {
    ComputeShader->SetZOffset(RHICmdList, ShaderRHI, i * (Dimensions.Z / 10));
    DispatchComputeShader(RHICmdList, *ComputeShader, GroupSizeX, GroupSizeY, Dimensions.Z / 10);
  }

  // Unbind UAVs.
  ComputeShader->UnbindResources(RHICmdList, ShaderRHI);

  // Transition resources back to the renderer.
  RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, DistanceFieldResource);
}

ETextureSourceFormat PixelFormatToSourceFormat(EPixelFormat PixelFormat) {
  // THIS IS UNTESTED FOR FORMATS OTHER THAN G8 AND R16G16B16A16_SNORM!
  // USE AT YOUR OWN PERIL!
  switch (PixelFormat) {
    case PF_G8:
    case PF_R8_UINT: return TSF_G8;

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

FIntVector GetTransposedDimensions(const FMajorAxes& Axes, const FRHITexture3D* VolumeRef,
                                   const unsigned index) {
  FCubeFace face = Axes.FaceWeight[index].first;
  unsigned axis = (uint8)face / 2;
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

int GetAxisDirection(const FMajorAxes& Axes, unsigned index) {
  // All even axis number are going down on their respective axes.
  return ((uint8)Axes.FaceWeight[index].first % 2 ? 1 : -1);
}

OneAxisReadWriteBufferResources& GetBuffers(const FMajorAxes Axes, const unsigned index,
                                            FBasicRaymarchRenderingResources& InParams) {
  FCubeFace face = Axes.FaceWeight[index].first;
  unsigned axis = (uint8)face / 2;
  return InParams.XYZReadWriteBuffers[axis];
}

void ClearFloatTextureRW(FRHICommandListImmediate& RHICmdList,
                         FUnorderedAccessViewRHIParamRef TextureRW, FIntPoint TextureSize,
                         float Value) {
  TShaderMapRef<FClearFloatRWTextureCS> ComputeShader(GetGlobalShaderMap(ERHIFeatureLevel::SM5));
  RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());

  RHICmdList.TransitionResource(EResourceTransitionAccess::ERWNoBarrier,
                                EResourceTransitionPipeline::EComputeToCompute, TextureRW);

  ComputeShader->SetParameters(RHICmdList, TextureRW, Value);
  uint32 GroupSizeX = FMath::DivideAndRoundUp(TextureSize.X, NUM_THREADS_PER_GROUP_DIMENSION);
  uint32 GroupSizeY = FMath::DivideAndRoundUp(TextureSize.Y, NUM_THREADS_PER_GROUP_DIMENSION);

  DispatchComputeShader(RHICmdList, *ComputeShader, GroupSizeX, GroupSizeY, 1);
  ComputeShader->UnbindUAV(RHICmdList);
}

FVector2D GetUVOffset(FCubeFace Axis, FVector LightPosition, FIntVector TransposedDimensions) {
  FVector normLightPosition = LightPosition;
  // Normalize the light position to get the major axis to be one. The other 2 components are then
  // an offset to apply to current pos to read from our read buffer texture.
  FVector2D RetVal;
  switch (Axis) {
    case FCubeFace::Right:  // +X
      normLightPosition /= normLightPosition.X;
      RetVal = FVector2D(normLightPosition.Y, normLightPosition.Z);
      break;
    case FCubeFace::Left:  // -X
      normLightPosition /= -normLightPosition.X;
      RetVal = FVector2D(normLightPosition.Y, normLightPosition.Z);
      break;
    case FCubeFace::Back:  // +Y
      normLightPosition /= normLightPosition.Y;
      RetVal = FVector2D(normLightPosition.X, normLightPosition.Z);
      break;
    case FCubeFace::Front:  // -Y
      normLightPosition /= -normLightPosition.Y;
      RetVal = FVector2D(normLightPosition.X, normLightPosition.Z);
      break;
    case FCubeFace::Top:  // +Z
      normLightPosition /= normLightPosition.Z;
      RetVal = FVector2D(normLightPosition.X, normLightPosition.Y);
      break;
    case FCubeFace::Bottom:  // -Z
      normLightPosition /= -normLightPosition.Z;
      RetVal = FVector2D(normLightPosition.X, normLightPosition.Y);
      break;
    default: {
      check(false);
      return FVector2D(0, 0);
    };
  }

  // Now normalize for different voxel sizes. Because we have Transposed Dimensions as input, we
  // know that we're propagating along TD.Z, The X-size of the buffer is in TD.X and Y-size of the
  // buffer is in TD.Y
  //// Divide by length of step
  RetVal /= TransposedDimensions.Z;

  return RetVal;
}

void GetStepSizeAndUVWOffset(FCubeFace Axis, FVector LightPosition, FIntVector TransposedDimensions,
                             const FRaymarchWorldParameters WorldParameters, float& OutStepSize,
                             FVector& OutUVWOffset) {
  OutUVWOffset = LightPosition;
  // Since we don't care about the direction, just the size, ignore signs.
  switch (Axis) {
    case FCubeFace::Right:  // +-X
    case FCubeFace::Left: OutUVWOffset /= abs(LightPosition.X) * TransposedDimensions.Z; break;
    case FCubeFace::Back:  // +-Y
    case FCubeFace::Front: OutUVWOffset /= abs(LightPosition.Y) * TransposedDimensions.Z; break;
    case FCubeFace::Top:  // +-Z
    case FCubeFace::Bottom: OutUVWOffset /= abs(LightPosition.Z) * TransposedDimensions.Z; break;
    default: check(false); ;
  }

  // Transform local vector to world space.
  FVector WorldVec = WorldParameters.VolumeTransform.TransformVector(OutUVWOffset);
  OutStepSize = WorldVec.Size();
}

void GetLocalLightParamsAndAxes(const FDirLightParameters& LightParameters,
                                const FTransform& VolumeTransform,
                                FDirLightParameters& OutLocalLightParameters,
                                FMajorAxes& OutLocalMajorAxes) {
  // TODO Why the fuck does light direction need NoScale and no multiplication by scale and clipping
  // plane needs to be multiplied?

  // Transform light directions into local space.
  OutLocalLightParameters.LightDirection =
      VolumeTransform.InverseTransformVector(LightParameters.LightDirection);
  // Normalize Light Direction to get unit length.
  OutLocalLightParameters.LightDirection.Normalize();

  // Color and Intensity are the same in local space -> copy.
  OutLocalLightParameters.LightColor = LightParameters.LightColor;
  OutLocalLightParameters.LightIntensity = LightParameters.LightIntensity;

  // Get Major Axes (notice inverting of light Direction - for a directional light, the position of
  // the light is the opposite of the direction) e.g. Directional light with direction (1, 0, 0) is
  // assumed to be shining from (-1, 0, 0)
  OutLocalMajorAxes = GetMajorAxes(-OutLocalLightParameters.LightDirection);
  if (OutLocalMajorAxes.FaceWeight[0].second > 0.99f) {
    OutLocalMajorAxes.FaceWeight[0].second = 1.0f;
  }

  // Set second axis weight to (1 - (first axis weight))
  OutLocalMajorAxes.FaceWeight[1].second = 1 - OutLocalMajorAxes.FaceWeight[0].second;

  // FString debug = "Global light dir : " + LightParameters.LightDirection.ToString() +
  //                ", Local light dir : " + OutLocalLightParameters.LightDirection.ToString();
  // GEngine->AddOnScreenDebugMessage(-1, 0, FColor::Yellow, debug);

  // RetVal.Direction *= VolumeTransform.GetScale3D();
}

FClippingPlaneParameters GetLocalClippingParameters(
    const FRaymarchWorldParameters WorldParameters) {
  FClippingPlaneParameters RetVal;
  // Get clipping center to (0-1) texture local space. (Invert transform, add 0.5 to get to (0-1) space of a unit cube centered on 0,0,0)
  RetVal.Center = WorldParameters.VolumeTransform.InverseTransformPosition(
                        WorldParameters.ClippingPlaneParameters.Center) + 0.5;
  // Get clipping direction in local space - here we don't care about the mesh size (as long as
  // it's a cube, which it really bloody better be).

  // TODO Why the fuck does light direction work with regular InverseTransformVector
  // but clipping direction only works with NoScale and multiplying by scale afterwards?
  RetVal.Direction = WorldParameters.VolumeTransform.InverseTransformVectorNoScale(
      WorldParameters.ClippingPlaneParameters.Direction);
  RetVal.Direction *= WorldParameters.VolumeTransform.GetScale3D();
  RetVal.Direction.Normalize();

  //  FString debug = "Global clip dir : " + WorldParameters.ClippingPlaneParameters.ToString() + ",
  //  Local clip dir : " +  RetVal.Direction.ToString(); GEngine->AddOnScreenDebugMessage(-1, 0,
  //  FColor::Yellow, debug);

  return RetVal;
}

/** Writes a single layer (along X axis) of a volume texture to a 2D texture.*/
void WriteVolumeTextureSlice_RenderThread(FRHICommandListImmediate& RHICmdList,
                                          UVolumeTexture* VolumeTexture,
                                          UTexture2D* WrittenSliceTexture, int Layer) {
  TShaderMap<FGlobalShaderType>* GlobalShaderMap = GetGlobalShaderMap(ERHIFeatureLevel::SM5);
  TShaderMapRef<FWriteSliceToTextureShader> ComputeShader(GlobalShaderMap);

  RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
  // RHICmdList.TransitionResource(EResourceTransitionAccess::ERWNoBarrier,
  // LightVolumeResource);
  FUnorderedAccessViewRHIRef TextureUAV = RHICreateUnorderedAccessView(WrittenSliceTexture->Resource->TextureRHI);

  // Don't need barriers on these - we only ever read/write to the same pixel from one thread ->
  // no race conditions But we definitely need to transition the resource to Compute-shader
  // accessible, otherwise the renderer might touch our textures while we're writing them.
  RHICmdList.TransitionResource(EResourceTransitionAccess::EWritable,
                                EResourceTransitionPipeline::EGfxToCompute, TextureUAV);

  ComputeShader->SetParameters(RHICmdList, VolumeTexture->Resource->TextureRHI->GetTexture3D(), TextureUAV, Layer);

  uint32 GroupSizeX = FMath::DivideAndRoundUp((int32)WrittenSliceTexture->GetSizeX(),
                                              NUM_THREADS_PER_GROUP_DIMENSION);
  uint32 GroupSizeY = FMath::DivideAndRoundUp((int32)WrittenSliceTexture->GetSizeY(),
                                              NUM_THREADS_PER_GROUP_DIMENSION);

  DispatchComputeShader(RHICmdList, *ComputeShader, GroupSizeX, GroupSizeY, 1);
  ComputeShader->UnbindUAV(RHICmdList);

  RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable,
                                EResourceTransitionPipeline::EComputeToGfx, TextureUAV);
}

FSamplerStateRHIRef GetBufferSamplerRef(uint32 BorderColorInt) {
  // Return a sampler for RW buffers - bordered by specified color.
  return RHICreateSamplerState(FSamplerStateInitializerRHI(SF_Bilinear, AM_Border, AM_Border,
                                                           AM_Border, 0, 0, 0, 1, BorderColorInt));
}

// Returns the color int required for the given light color and major axis (single channel)
uint32 GetBorderColorIntSingle(FDirLightParameters LightParams, FMajorAxes MajorAxes,
                               unsigned index) {
  // Set alpha channel to the texture's red channel (when reading single-channel, only red component
  // is read)
  FLinearColor LightColor =
      FLinearColor(LightParams.LightIntensity * MajorAxes.FaceWeight[index].second, 0.0, 0.0, 0.0);
  return LightColor.ToFColor(true).ToPackedARGB();
}

// Returns the light's alpha at this major axis and weight (single channel)
float GetLightAlpha(FDirLightParameters LightParams, FMajorAxes MajorAxes, unsigned index) {
  // Set alpha channel to the texture's red channel (when reading single-channel, only red component
  // is read)
  return LightParams.LightIntensity * MajorAxes.FaceWeight[index].second;
}

FMatrix GetPermutationMatrix(FMajorAxes MajorAxes, unsigned index) {
  uint8 Axis = (uint8)MajorAxes.FaceWeight[index].first / 2;
  FMatrix retVal;
  retVal.SetIdentity();

  FVector xVec(1, 0, 0);
  FVector yVec(0, 1, 0);
  FVector zVec(0, 0, 1);
  switch (Axis) {
    case 0:  // X Axis
      retVal.SetAxes(&yVec, &zVec, &xVec);
      break;
    case 1:  // Y Axis
      retVal.SetAxes(&xVec, &zVec, &yVec);
      break;
    case 2:  // We keep identity set...
    default: break;
  }
  return retVal;
}

float GetStepSize(FVector2D PixelOffset, FIntVector TransposedDimensions) {
  // Pixel offset is increased by 0.5 to point to the center of pixels from int coords -> decrease
  // it by that so that the calculations are correct
  PixelOffset = PixelOffset - 0.5;
  // Get step to previous voxel in the volume (distance between two propagation layers).
  float LayerThickness = 1.0f / (float)TransposedDimensions.Z;
  // Get diagonal length if the step to previous pixel was one
  float PixelOffsetSize = sqrt(1 + PixelOffset.Size() * PixelOffset.Size());
  // Return the product.
  FString debugMsg =
      "Layer thickness = " + FString::SanitizeFloat(LayerThickness) +
      ", Pix offset = " + PixelOffset.ToString() +
      ", diagonal = " + FString::SanitizeFloat(PixelOffsetSize) +
      ", final StepSize = " + FString::SanitizeFloat(PixelOffsetSize * LayerThickness);

  //	GEngine->AddOnScreenDebugMessage(-1, 0, FColor::Yellow,debugMsg);

  return PixelOffsetSize * LayerThickness;
}

void GetLoopStartStopIndexes(int& OutStart, int& OutStop, int& OutAxisDirection,
                             const FMajorAxes& MajorAxes, const unsigned& index,
                             const int zDimension) {
  OutAxisDirection = GetAxisDirection(MajorAxes, index);
  if (OutAxisDirection == -1) {
    OutStart = zDimension - 1;
    OutStop = -1;  // We want to go all the way to zero, so stop at -1
  } else {
    OutStart = 0;
    OutStop = zDimension;  // want to go to Z - 1, so stop at Z
  }
}

// Returns the color int required for the given light color and major axis.
uint32 GetBorderColorInt(FDirLightParameters LightParams, FMajorAxes MajorAxes, unsigned index) {
  FVector LC = LightParams.LightColor;
  FLinearColor LightColor = FLinearColor(
      LC.X, LC.Y, LC.Z, LightParams.LightIntensity * MajorAxes.FaceWeight[index].second);
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

// For making statistics about GPU use.
DECLARE_FLOAT_COUNTER_STAT(TEXT("ChangingLights"), STAT_GPU_ChangingLights, STATGROUP_GPU);
DECLARE_GPU_STAT_NAMED(GPUChangingLights, TEXT("ChangingLightsInVolume"));

// For making statistics about GPU use.
DECLARE_FLOAT_COUNTER_STAT(TEXT("ClearingLights"), STAT_GPU_ClearingLights, STATGROUP_GPU);
DECLARE_GPU_STAT_NAMED(GPUClearingLights, TEXT("ClearingLightsInVolume"));

void AddDirLightToSingleLightVolume_RenderThread(FRHICommandListImmediate& RHICmdList,
                                                 FBasicRaymarchRenderingResources Resources,
                                                 const FDirLightParameters LightParameters,
                                                 const bool Added,
                                                 const FRaymarchWorldParameters WorldParameters) {
  check(IsInRenderingThread());

  // Can't have directional light without direction...
  if (LightParameters.LightDirection == FVector(0.0, 0.0, 0.0)) {
    GEngine->AddOnScreenDebugMessage(
        -1, 100.0f, FColor::Yellow,
        TEXT("Returning because the directional light doesn't have a direction."));
    return;
  }

  FDirLightParameters LocalLightParams;
  FMajorAxes LocalMajorAxes;
  // Calculate local Light parameters and corresponding axes.
  GetLocalLightParamsAndAxes(LightParameters, WorldParameters.VolumeTransform, LocalLightParams,
                             LocalMajorAxes);

  // For testing...
  // LocalMajorAxes.FaceWeight[0].second = 1;
  // LocalMajorAxes.FaceWeight[1].second = 0;

  // Transform clipping parameters into local space.
  FClippingPlaneParameters LocalClippingParameters = GetLocalClippingParameters(WorldParameters);

  // For GPU profiling.
  SCOPED_DRAW_EVENTF(RHICmdList, AddDirLightToSingleLightVolume_RenderThread,
                     TEXT("Adding Lights"));
  SCOPED_GPU_STAT(RHICmdList, GPUAddingLights);

  // TODO create structure with 2 sets of buffers so we don't have to look for them again in the
  // actual shader loop! Clear buffers for the two axes we will be using.
  for (unsigned i = 0; i < 2; i++) {
    // Break if the axis weight == 0
    if (LocalMajorAxes.FaceWeight[i].second == 0) {
      break;
    }
    // Get the X, Y and Z transposed into the current axis orientation.
    FIntVector TransposedDimensions = GetTransposedDimensions(
        LocalMajorAxes, Resources.ALightVolumeRef->Resource->TextureRHI->GetTexture3D(), i);
    OneAxisReadWriteBufferResources& Buffers = GetBuffers(LocalMajorAxes, i, Resources);

    // for (int i = 0; i < 2; i++) {
    //	if (!Buffers.UAVs[i]) Buffers.UAVs[i] = RHICreateUnorderedAccessView(Buffers.Buffers[i]);
    //}

    float LightAlpha = GetLightAlpha(LocalLightParams, LocalMajorAxes, i);

    ClearFloatTextureRW(RHICmdList, Buffers.UAVs[0],
                        FIntPoint(TransposedDimensions.X, TransposedDimensions.Y), LightAlpha);
    ClearFloatTextureRW(RHICmdList, Buffers.UAVs[1],
                        FIntPoint(TransposedDimensions.X, TransposedDimensions.Y), LightAlpha);
  }

  // Find and set compute shader
  TShaderMap<FGlobalShaderType>* GlobalShaderMap = GetGlobalShaderMap(ERHIFeatureLevel::SM5);
  TShaderMapRef<FAddDirLightShaderSingle> ComputeShader(GlobalShaderMap);
  FComputeShaderRHIParamRef ShaderRHI = ComputeShader->GetComputeShader();
  RHICmdList.SetComputeShader(ShaderRHI);

  FUnorderedAccessViewRHIRef AVolumeUAV = Resources.ALightVolumeUAVRef;
  //		RHICreateUnorderedAccessView(Resources.ALightVolumeRef->Resource->TextureRHI);

  // Don't need barriers on these - we only ever read/write to the same pixel from one thread ->
  // no race conditions But we definitely need to transition the resource to Compute-shader
  // accessible, otherwise the renderer might touch our textures while we're writing them.
  RHICmdList.TransitionResource(EResourceTransitionAccess::ERWNoBarrier,
                                EResourceTransitionPipeline::EGfxToCompute, AVolumeUAV);

  // Set parameters, resources, LightAdded and ALightVolume
  ComputeShader->SetRaymarchParameters(RHICmdList, ShaderRHI, LocalClippingParameters,
                                       Resources.TFRangeParameters.IntensityDomain);
  ComputeShader->SetRaymarchResources(
      RHICmdList, ShaderRHI, Resources.VolumeTextureRef->Resource->TextureRHI->GetTexture3D(),
      Resources.TFTextureRef->Resource->TextureRHI->GetTexture2D());
  ComputeShader->SetLightAdded(RHICmdList, ShaderRHI, Added);
  ComputeShader->SetALightVolume(RHICmdList, ShaderRHI, AVolumeUAV);

  for (unsigned i = 0; i < 2; i++) {
    // Break if the main axis weight == 1
    if (LocalMajorAxes.FaceWeight[i].second == 0) {
      break;
    }
    OneAxisReadWriteBufferResources& Buffers = GetBuffers(LocalMajorAxes, i, Resources);

    uint32 ColorInt = GetBorderColorIntSingle(LocalLightParams, LocalMajorAxes, i);
    FSamplerStateRHIRef readBuffSampler = GetBufferSamplerRef(ColorInt);

    // Get the X, Y and Z transposed into the current axis orientation.
    FIntVector TransposedDimensions = GetTransposedDimensions(
        LocalMajorAxes, Resources.ALightVolumeRef->Resource->TextureRHI->GetTexture3D(), i);

    FVector2D UVOffset = GetUVOffset(LocalMajorAxes.FaceWeight[i].first,
                                     -LocalLightParams.LightDirection, TransposedDimensions);
    FMatrix PermutationMatrix = GetPermutationMatrix(LocalMajorAxes, i);

    FIntVector LightVolumeSize =
        FIntVector(Resources.ALightVolumeRef->GetSizeX(), Resources.ALightVolumeRef->GetSizeY(),
                   Resources.ALightVolumeRef->GetSizeZ());

    FVector UVWOffset;
    float StepSize;
    GetStepSizeAndUVWOffset(LocalMajorAxes.FaceWeight[i].first, -LocalLightParams.LightDirection,
                            TransposedDimensions, WorldParameters, StepSize, UVWOffset);

    // Normalize UVW offset to length of largest voxel size to get rid of artifacts. (Not correct,
    // but consistent!)
    int LowestVoxelCount =
        FMath::Min3(TransposedDimensions.X, TransposedDimensions.Y, TransposedDimensions.Z);
    float LongestVoxelSide = 1.0f / LowestVoxelCount;
    UVWOffset.Normalize();
    UVWOffset *= LongestVoxelSide;

    ComputeShader->SetStepSize(RHICmdList, ShaderRHI, StepSize);
    ComputeShader->SetPermutationMatrix(RHICmdList, ShaderRHI, PermutationMatrix);
    ComputeShader->SetUVOffset(RHICmdList, ShaderRHI, UVOffset);
    ComputeShader->SetUVWOffset(RHICmdList, ShaderRHI, UVWOffset);

    uint32 GroupSizeX =
        FMath::DivideAndRoundUp(TransposedDimensions.X, NUM_THREADS_PER_GROUP_DIMENSION);
    uint32 GroupSizeY =
        FMath::DivideAndRoundUp(TransposedDimensions.Y, NUM_THREADS_PER_GROUP_DIMENSION);

    int Start, Stop, AxisDirection;
    GetLoopStartStopIndexes(Start, Stop, AxisDirection, LocalMajorAxes, i, TransposedDimensions.Z);

    for (int j = Start; j != Stop; j += AxisDirection) {
      // Switch read and write buffers each row.
      if (j % 2 == 0) {
        ComputeShader->SetLoop(RHICmdList, ShaderRHI, j, Buffers.Buffers[0], readBuffSampler,
                               Buffers.UAVs[1]);
      } else {
        ComputeShader->SetLoop(RHICmdList, ShaderRHI, j, Buffers.Buffers[1], readBuffSampler,
                               Buffers.UAVs[0]);
      }
      DispatchComputeShader(RHICmdList, *ComputeShader, GroupSizeX, GroupSizeY, 1);
    }
  }

  // Unbind UAVs.
  ComputeShader->UnbindResources(RHICmdList, ShaderRHI);

  // Transition resources back to the renderer.
  RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable,
                                EResourceTransitionPipeline::EComputeToGfx, AVolumeUAV);
}

void ChangeDirLightInSingleLightVolume_RenderThread(
    FRHICommandListImmediate& RHICmdList, FBasicRaymarchRenderingResources Resources,
    const FDirLightParameters RemovedLightParameters,
    const FDirLightParameters AddedLightParameters,
    const FRaymarchWorldParameters WorldParameters) {
  // Can't have directional light without direction...
  if (AddedLightParameters.LightDirection == FVector(0.0, 0.0, 0.0)) {
    GEngine->AddOnScreenDebugMessage(
        -1, 100.0f, FColor::Yellow,
        TEXT("Returning because the directional light doesn't have a direction."));
    return;
  }

  FClippingPlaneParameters LocalClippingParameters = GetLocalClippingParameters(WorldParameters);
  // Create local copies of Light Params, so that if we have to fall back to 2x
  // AddOrRemoveLight, we can just pass the original parameters.
  FDirLightParameters RemovedLocalLightParams, AddedLocalLightParams;
  FMajorAxes RemovedLocalMajorAxes, AddedLocalMajorAxes;
  // Calculate local Light parameters and corresponding axes.
  GetLocalLightParamsAndAxes(RemovedLightParameters, WorldParameters.VolumeTransform,
                             RemovedLocalLightParams, RemovedLocalMajorAxes);
  GetLocalLightParamsAndAxes(AddedLightParameters, WorldParameters.VolumeTransform,
                             AddedLocalLightParams, AddedLocalMajorAxes);

  // If lights have different major axes, do a proper removal and addition.
  if (RemovedLocalMajorAxes.FaceWeight[0].first != AddedLocalMajorAxes.FaceWeight[0].first ||
      RemovedLocalMajorAxes.FaceWeight[1].first != AddedLocalMajorAxes.FaceWeight[1].first) {
    AddDirLightToSingleLightVolume_RenderThread(RHICmdList, Resources, RemovedLightParameters,
                                                false, WorldParameters);
    AddDirLightToSingleLightVolume_RenderThread(RHICmdList, Resources, AddedLightParameters, true,
                                                WorldParameters);
    return;
  }

  // Clear buffers for the two axes we will be using.
  for (unsigned i = 0; i < 2; i++) {
    // Break if the main axis weight == 1
    if (RemovedLocalMajorAxes.FaceWeight[i].second == 0) {
      break;
    }
    // Get the X, Y and Z transposed into the current axis orientation.
    FIntVector TransposedDimensions = GetTransposedDimensions(
        RemovedLocalMajorAxes, Resources.ALightVolumeRef->Resource->TextureRHI->GetTexture3D(), i);
    OneAxisReadWriteBufferResources& Buffers = GetBuffers(RemovedLocalMajorAxes, i, Resources);

    // for (int i = 0; i < 4; i++) {
    //	if (!Buffers.UAVs[i]) Buffers.UAVs[i] = RHICreateUnorderedAccessView(Buffers.Buffers[i]);
    //}

    float RemovedLightAlpha = GetLightAlpha(RemovedLocalLightParams, RemovedLocalMajorAxes, i);
    float AddedLightAlpha = GetLightAlpha(AddedLocalLightParams, AddedLocalMajorAxes, i);

    // Clear R/W buffers for Removed Light
    ClearFloatTextureRW(RHICmdList, Buffers.UAVs[0],
                        FIntPoint(TransposedDimensions.X, TransposedDimensions.Y),
                        RemovedLightAlpha);
    ClearFloatTextureRW(RHICmdList, Buffers.UAVs[1],
                        FIntPoint(TransposedDimensions.X, TransposedDimensions.Y),
                        RemovedLightAlpha);
    // Clear R/W buffers for Added Light
    ClearFloatTextureRW(RHICmdList, Buffers.UAVs[2],
                        FIntPoint(TransposedDimensions.X, TransposedDimensions.Y), AddedLightAlpha);
    ClearFloatTextureRW(RHICmdList, Buffers.UAVs[3],
                        FIntPoint(TransposedDimensions.X, TransposedDimensions.Y), AddedLightAlpha);
  }

  // For GPU profiling.
  SCOPED_DRAW_EVENTF(RHICmdList, ChangeDirLightInLightVolume_RenderThread, TEXT("Changing Lights"));
  SCOPED_GPU_STAT(RHICmdList, GPUChangingLights);

  TShaderMap<FGlobalShaderType>* GlobalShaderMap = GetGlobalShaderMap(ERHIFeatureLevel::SM5);
  TShaderMapRef<FChangeDirLightShaderSingle> ComputeShader(GlobalShaderMap);

  FComputeShaderRHIParamRef ShaderRHI = ComputeShader->GetComputeShader();
  RHICmdList.SetComputeShader(ShaderRHI);
  // RHICmdList.TransitionResource(EResourceTransitionAccess::ERWNoBarrier,
  // LightVolumeResource);
  FUnorderedAccessViewRHIRef AVolumeUAV = Resources.ALightVolumeUAVRef;
  //		RHICreateUnorderedAccessView(Resources.ALightVolumeRef->Resource->TextureRHI->GetTexture3D());

  // Don't need barriers on these - we only ever read/write to the same pixel from one thread ->
  // no race conditions But we definitely need to transition the resource to Compute-shader
  // accessible, otherwise the renderer might touch our textures while we're writing them.
  RHICmdList.TransitionResource(EResourceTransitionAccess::ERWNoBarrier,
                                EResourceTransitionPipeline::EGfxToCompute, AVolumeUAV);

  ComputeShader->SetRaymarchParameters(RHICmdList, ShaderRHI, LocalClippingParameters,
                                       Resources.TFRangeParameters.IntensityDomain);
  ComputeShader->SetRaymarchResources(
      RHICmdList, ShaderRHI, Resources.VolumeTextureRef->Resource->TextureRHI->GetTexture3D(),
      Resources.TFTextureRef->Resource->TextureRHI->GetTexture2D());
  ComputeShader->SetALightVolume(RHICmdList, ShaderRHI, AVolumeUAV);

  for (unsigned i = 0; i < 2; i++) {
    // Get Color ints for texture borders.
    uint32 RemovedColorInt =
        GetBorderColorIntSingle(RemovedLocalLightParams, RemovedLocalMajorAxes, i);
    uint32 AddedColorInt = GetBorderColorIntSingle(AddedLocalLightParams, AddedLocalMajorAxes, i);
    // Get the sampler for read buffer to use border with the proper light color.
    FSamplerStateRHIRef RemovedReadBuffSampler = GetBufferSamplerRef(RemovedColorInt);
    FSamplerStateRHIRef AddedReadBuffSampler = GetBufferSamplerRef(AddedColorInt);

    OneAxisReadWriteBufferResources& Buffers = GetBuffers(RemovedLocalMajorAxes, i, Resources);
    // TODO take these from buffers.
    FIntVector TransposedDimensions = GetTransposedDimensions(
        RemovedLocalMajorAxes, Resources.ALightVolumeRef->Resource->TextureRHI->GetTexture3D(), i);

    FVector2D AddedPixOffset =
        GetUVOffset(AddedLocalMajorAxes.FaceWeight[i].first, -AddedLocalLightParams.LightDirection,
                    TransposedDimensions);
    FVector2D RemovedPixOffset =
        GetUVOffset(RemovedLocalMajorAxes.FaceWeight[i].first,
                    -RemovedLocalLightParams.LightDirection, TransposedDimensions);

    ComputeShader->SetStepSizes(RHICmdList, ShaderRHI,
                                GetStepSize(RemovedPixOffset, TransposedDimensions),
                                GetStepSize(AddedPixOffset, TransposedDimensions));
    ComputeShader->SetPixelOffsets(RHICmdList, ShaderRHI, AddedPixOffset, RemovedPixOffset);

    FMatrix perm = GetPermutationMatrix(RemovedLocalMajorAxes, i);
    ComputeShader->SetPermutationMatrix(RHICmdList, ShaderRHI, perm);

    // Get group sizes for compute shader
    uint32 GroupSizeX =
        FMath::DivideAndRoundUp(TransposedDimensions.X, NUM_THREADS_PER_GROUP_DIMENSION);
    uint32 GroupSizeY =
        FMath::DivideAndRoundUp(TransposedDimensions.Y, NUM_THREADS_PER_GROUP_DIMENSION);

    int Start, Stop, AxisDirection;
    GetLoopStartStopIndexes(Start, Stop, AxisDirection, RemovedLocalMajorAxes, i,
                            TransposedDimensions.Z);

    for (int j = Start; j != Stop;
         j += AxisDirection) {  // Switch read and write buffers each cycle.
      if (j % 2 == 0) {
        TransitionBufferResources(RHICmdList, Buffers.Buffers[0], Buffers.UAVs[1]);
        TransitionBufferResources(RHICmdList, Buffers.Buffers[2], Buffers.UAVs[3]);
        ComputeShader->SetLoop(RHICmdList, ShaderRHI, j, Buffers.Buffers[0], RemovedReadBuffSampler,
                               Buffers.UAVs[1], Buffers.Buffers[2], AddedReadBuffSampler,
                               Buffers.UAVs[3]);
      } else {
        TransitionBufferResources(RHICmdList, Buffers.Buffers[1], Buffers.UAVs[0]);
        TransitionBufferResources(RHICmdList, Buffers.Buffers[3], Buffers.UAVs[2]);
        ComputeShader->SetLoop(RHICmdList, ShaderRHI, j, Buffers.Buffers[1], RemovedReadBuffSampler,
                               Buffers.UAVs[0], Buffers.Buffers[3], AddedReadBuffSampler,
                               Buffers.UAVs[2]);
      }
      DispatchComputeShader(RHICmdList, *ComputeShader, GroupSizeX, GroupSizeY, 1);
    }
  }

  // Unbind Resources.
  ComputeShader->UnbindResources(RHICmdList, ShaderRHI);

  // Transition resources back to the renderer.
  RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable,
                                EResourceTransitionPipeline::EComputeToGfx, AVolumeUAV);
}

void ClearVolumeTexture_RenderThread(FRHICommandListImmediate& RHICmdList,
                                     FRHITexture3D* VolumeResourceRef, float ClearValues) {
  TShaderMap<FGlobalShaderType>* GlobalShaderMap = GetGlobalShaderMap(ERHIFeatureLevel::SM5);
  TShaderMapRef<FClearVolumeTextureShader> ComputeShader(GlobalShaderMap);

  // For GPU profiling.
  SCOPED_DRAW_EVENTF(RHICmdList, ClearVolumeTexture_RenderThread, TEXT("Clearing lights"));
  SCOPED_GPU_STAT(RHICmdList, GPUClearingLights);

  RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
  // RHICmdList.TransitionResource(EResourceTransitionAccess::ERWNoBarrier,
  // LightVolumeResource);
  FUnorderedAccessViewRHIRef VolumeUAVRef = RHICreateUnorderedAccessView(VolumeResourceRef);

  // Don't need barriers on these - we only ever read/write to the same pixel from one thread ->
  // no race conditions But we definitely need to transition the resource to Compute-shader
  // accessible, otherwise the renderer might touch our textures while we're writing them.
  RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier,
                                EResourceTransitionPipeline::EGfxToCompute, VolumeUAVRef);

  ComputeShader->SetParameters(RHICmdList, VolumeUAVRef, ClearValues,
                               VolumeResourceRef->GetSizeZ());

  uint32 GroupSizeX = FMath::DivideAndRoundUp((int32)VolumeResourceRef->GetSizeX(),
                                              NUM_THREADS_PER_GROUP_DIMENSION);
  uint32 GroupSizeY = FMath::DivideAndRoundUp((int32)VolumeResourceRef->GetSizeY(),
                                              NUM_THREADS_PER_GROUP_DIMENSION);

  DispatchComputeShader(RHICmdList, *ComputeShader, GroupSizeX, GroupSizeY, 1);
  ComputeShader->UnbindUAV(RHICmdList);

  RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable,
                                EResourceTransitionPipeline::EComputeToGfx, VolumeUAVRef);
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
