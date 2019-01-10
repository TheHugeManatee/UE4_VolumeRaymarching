// (C) Technical University of Munich - Computer Aided Medical Procedures
// Developed by Tomas Bartipan (tomas.bartipan@tum.de)

#include "../Public/VolumeMarking.h"
#include "AssetRegistryModule.h"
#include "RenderCore/Public/RenderUtils.h"
#include "Renderer/Public/VolumeRendering.h"

#define LOCTEXT_NAMESPACE "RaymarchPlugin"

IMPLEMENT_SHADER_TYPE(, FWriteCuboidToVolumeShader, TEXT("/Plugin/VolumeRaymarching/Private/WriteCuboidShader.usf"),
                      TEXT("MainComputeShader"), SF_Compute)

void WriteCuboidToVolume_RenderThread(FRHICommandListImmediate & RHICmdList, FRHITexture3D * MarkedVolume, const FVector BrushWorldCenter, const FVector BrushWorldSize, const FRaymarchWorldParameters WorldParameters, float WrittenValue)
{
	// Get local center.
	FVector localCenter = ((WorldParameters.VolumeTransform.InverseTransformPosition(BrushWorldCenter) /
		(WorldParameters.MeshMaxBounds)) / 2.0) + 0.5;
	// Get local center in integer space

	int32 x = localCenter.X * MarkedVolume->GetSizeX();
	int32 y = localCenter.Y * MarkedVolume->GetSizeY();
	int32 z = localCenter.Z * MarkedVolume->GetSizeZ();


	FIntVector localCenterIntCoords;
	localCenterIntCoords.X = x; localCenterIntCoords.Y = y; localCenterIntCoords.Z = z;

	//FString kkt = "a " + localCenter.ToString() + ", local = " + FString::FromInt(x)+ " " + FString::FromInt(y) + " "+ FString::FromInt(z) + ", local intvec = " + localCenterIntCoords.ToString();

	//GEngine->AddOnScreenDebugMessage(0, 10, FColor::Yellow, kkt);
	
	// Get shader ref from GlobalShaderMap
	TShaderMap<FGlobalShaderType>* GlobalShaderMap = GetGlobalShaderMap(ERHIFeatureLevel::SM5);
	TShaderMapRef<FWriteCuboidToVolumeShader> ComputeShader(GlobalShaderMap);

	RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
	// RHICmdList.TransitionResource(EResourceTransitionAccess::ERWNoBarrier, LightVolumeResource);
	FUnorderedAccessViewRHIRef MarkedVolumeUAV = RHICreateUnorderedAccessView(MarkedVolume);
	
	// Transfer from gfx to compute, otherwise the renderer might touch our textures while we're writing them.
	RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier,
		EResourceTransitionPipeline::EGfxToCompute, MarkedVolumeUAV);

	FIntVector brush = FIntVector(BrushWorldSize);

	FComputeShaderRHIParamRef cs = ComputeShader->GetComputeShader();
	ComputeShader->SetMarkedVolumeUAV(RHICmdList, cs, MarkedVolumeUAV);
	ComputeShader->SetParameters(RHICmdList, cs, localCenterIntCoords, brush, WrittenValue);

	DispatchComputeShader(RHICmdList, *ComputeShader, BrushWorldSize.X, BrushWorldSize.Y, BrushWorldSize.Z);
	
	ComputeShader->UnbindMarkedVolumeUAV(RHICmdList, cs);

	RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable,
		EResourceTransitionPipeline::EComputeToGfx, MarkedVolumeUAV);
}


void UVolumeMarkingLibrary::CreateMarkingVolume(FIntVector Dimensions, FString AssetName, UVolumeTexture*& OutTexture)
{
	int TotalSize = Dimensions.X * Dimensions.Y * Dimensions.Z * 4;
	uint8* dummy = (uint8*)FMemory::Malloc(TotalSize);
	FMemory::Memset(dummy, 0, TotalSize);

	if (CreateVolumeTextureAsset(AssetName, PF_R32_FLOAT, Dimensions, dummy, false, true, &OutTexture))
	{
		GEngine->AddOnScreenDebugMessage(0, 10, FColor::Yellow, "Marking volume created succesfuly");
	}
	else {
		GEngine->AddOnScreenDebugMessage(0, 10, FColor::Yellow, "Failed creating the marking volume.");
	}

	FMemory::Free(dummy);
}

void UVolumeMarkingLibrary::MarkCuboidInVolumeWorld(UVolumeTexture* MarkedVolume, const FVector BrushWorldCenter, const FVector BrushWorldSize, const FRaymarchWorldParameters WorldParameters, const float WrittenValue)
{
	if (MarkedVolume->Resource == NULL) return;
	// Call the actual rendering code on RenderThread.
	ENQUEUE_RENDER_COMMAND(CaptureCommand)
		([=](FRHICommandListImmediate& RHICmdList) {
		WriteCuboidToVolume_RenderThread(RHICmdList, MarkedVolume->Resource->TextureRHI->GetTexture3D(), BrushWorldCenter, BrushWorldSize, WorldParameters, WrittenValue);
	});
}


#undef LOCTEXT_NAMESPACE
