#include "Experimental.h"
#include "RaymarchRendering.h"
#include "RenderCore/Public/RenderUtils.h"
#include "Renderer/Public/VolumeRendering.h"

	IMPLEMENT_SHADER_TYPE(, FMakeMaxMipsShader,
							TEXT("/Plugin/VolumeRaymarching/Private/CreateMaxMipsShader.usf"),
							TEXT("MainComputeShader"), SF_Compute)

	IMPLEMENT_SHADER_TYPE(, FMakeDistanceFieldShader,
							TEXT("/Plugin/VolumeRaymarching/Private/CreateDistanceFieldShader.usf"),
							TEXT("MainComputeShader"), SF_Compute)

	IMPLEMENT_SHADER_TYPE(, FWriteSliceToTextureShader,
		TEXT("/Plugin/VolumeRaymarching/Private/WriteSliceToTextureShader.usf"),
		TEXT("MainComputeShader"), SF_Compute)


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
	// This is just hardcoded - will write to 5 slices along X-axis.
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

	// 16 must match the group size in the shader itself!
	uint32 GroupSizeX = FMath::DivideAndRoundUp(Dimensions.X, 16);
	uint32 GroupSizeY = FMath::DivideAndRoundUp(Dimensions.Y, 16);

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