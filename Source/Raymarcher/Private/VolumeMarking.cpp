// (C) Technical University of Munich - Computer Aided Medical Procedures
// Developed by Tomas Bartipan (tomas.bartipan@tum.de)

#include "../Public/VolumeMarking.h"
#include "AssetRegistryModule.h"
#include "RenderCore/Public/RenderUtils.h"
#include "Renderer/Public/VolumeRendering.h"

#define LOCTEXT_NAMESPACE "RaymarchPlugin"

IMPLEMENT_SHADER_TYPE(, FWriteCuboidToVolumeShader, TEXT("/Plugin/VolumeRaymarching/Private/WriteCuboidShader.usf"),
                      TEXT("MainComputeShader"), SF_Compute)

void WriteCuboidToVolume_RenderThread(FRHICommandListImmediate & RHICmdList, FRHITexture3D * MarkedVolume, FVector BrushWorldCenter, FVector BrushWorldSize, const FRaymarchWorldParameters WorldParameters, unsigned WrittenValue)
{
	// Get local center.
	FVector localCenter = ((WorldParameters.VolumeTransform.InverseTransformPosition(BrushWorldCenter) /
		(WorldParameters.MeshMaxBounds)) / 2.0) + 0.5;
	// Get local center in integer space

	int x = localCenter.X * MarkedVolume->GetSizeX();
	int y = localCenter.Y * MarkedVolume->GetSizeY();
	int z = localCenter.Z * MarkedVolume->GetSizeZ();

	FString kkt = "a " + localCenter.ToString() + ", local = " + FString::FromInt(x)+ " " + FString::FromInt(y) + " "+ FString::FromInt(z);

	GEngine->AddOnScreenDebugMessage(0, 10, FColor::Yellow, kkt);

	FIntVector localCenterIntCoords(x, y, z);
	
	// Get shader ref from GlobalShaderMap
	TShaderMap<FGlobalShaderType>* GlobalShaderMap = GetGlobalShaderMap(ERHIFeatureLevel::SM5);
	TShaderMapRef<FWriteCuboidToVolumeShader> ComputeShader(GlobalShaderMap);

	RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
	// RHICmdList.TransitionResource(EResourceTransitionAccess::ERWNoBarrier, LightVolumeResource);
	FUnorderedAccessViewRHIRef MarkedVolumeUAV = RHICreateUnorderedAccessView(MarkedVolume);
	
	// Transfer from gfx to compute, otherwise the renderer might touch our textures while we're writing them.
	RHICmdList.TransitionResource(EResourceTransitionAccess::EWritable,
		EResourceTransitionPipeline::EGfxToCompute, MarkedVolumeUAV);

	FComputeShaderRHIParamRef cs = ComputeShader->GetComputeShader();
	ComputeShader->SetMarkedVolumeUAV(RHICmdList, cs, MarkedVolumeUAV);
	ComputeShader->SetParameters(RHICmdList, cs, localCenterIntCoords, FIntVector(7, 7, 7), WrittenValue);

	DispatchComputeShader(RHICmdList, *ComputeShader, 7, 7, 7);
	
	ComputeShader->UnbindMarkedVolumeUAV(RHICmdList, cs);

	RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable,
		EResourceTransitionPipeline::EComputeToGfx, MarkedVolumeUAV);
}


void UVolumeMarkingLibrary::MarkCuboidInVolumeWorld(UVolumeTexture* MarkedVolume, FVector BrushWorldCenter, FVector BrushWorldSize, const FRaymarchWorldParameters WorldParameters, int WrittenValue)
{
	if (MarkedVolume->Resource == NULL) return;
	// Call the actual rendering code on RenderThread.
	ENQUEUE_RENDER_COMMAND(CaptureCommand)
		([=](FRHICommandListImmediate& RHICmdList) {
		WriteCuboidToVolume_RenderThread(RHICmdList, MarkedVolume->Resource->TextureRHI->GetTexture3D(), BrushWorldCenter, BrushWorldSize, WorldParameters, WrittenValue);
	});
}


#undef LOCTEXT_NAMESPACE
