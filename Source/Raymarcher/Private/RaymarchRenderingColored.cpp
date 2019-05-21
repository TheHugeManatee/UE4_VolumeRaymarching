//// Uncomment when (if) starting to develop colored light support

//IMPLEMENT_SHADER_TYPE(, FAddOrRemoveDirLightShader,
//                      TEXT("/Plugin/VolumeRaymarching/Private/DirLightShader.usf"),
//                      TEXT("MainComputeShader"), SF_Compute)
//IMPLEMENT_SHADER_TYPE(, FChangeDirLightShader,
//                      TEXT("/Plugin/VolumeRaymarching/Private/ChangeDirLightShader.usf"),
//                      TEXT("MainComputeShader"), SF_Compute)
//IMPLEMENT_SHADER_TYPE(, FClearLightVolumesShader,
//                      TEXT("/Plugin/VolumeRaymarching/Private/ClearVolumesShader.usf"),
//                      TEXT("MainComputeShader"), SF_Compute)
//
//

//// Returns the color int required for the given light color and major axis.
//uint32 GetBorderColorInt(FDirLightParameters LightParams, FMajorAxes MajorAxes, unsigned index) {
//	FVector LC = LightParams.LightColor;
//	FLinearColor LightColor = FLinearColor(
//		LC.X, LC.Y, LC.Z, LightParams.LightIntensity * MajorAxes.FaceWeight[index].second);
//	return LightColor.ToFColor(true).ToPackedARGB();
//}

//void AddDirLightToLightVolume_RenderThread(FRHICommandListImmediate& RHICmdList,
//	FBasicRaymarchRenderingResources Resources,
//	const FColorVolumesResources ColorResources,
//	const FDirLightParameters LightParameters,
//	const bool Added,
//	const FRaymarchWorldParameters WorldParameters) {
//	// Todo - template shaders on resource type.
//}
//
//void ChangeDirLightInLightVolume_RenderThread(FRHICommandListImmediate& RHICmdList,
//	FBasicRaymarchRenderingResources Resources,
//	const FColorVolumesResources ColorResources,
//	const FDirLightParameters OldLightParameters,
//	const FDirLightParameters NewLightParameters,
//	const FRaymarchWorldParameters WorldParameters) {
//	// Todo - template shaders on resource type
//}
//void ClearLightVolumes_RenderThread(
//	FRHICommandListImmediate & RHICmdList, FRHITexture3D * RLightVolumeResource,
//	FRHITexture3D * GLightVolumeResource, FRHITexture3D * BLightVolumeResource,
//	FRHITexture3D * ALightVolumeResource, FVector4 ClearValues) {
//	// Todo - template shaders on resource type
//}
