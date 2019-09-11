// (C) Technical University of Munich - Computer Aided Medical Procedures
// Developed by Tomas Bartipan (tomas.bartipan@tum.de)

#include "TextureHelperFunctions.h"

bool CreateVolumeTextureAsset(FString AssetName, EPixelFormat PixelFormat, FIntVector Dimensions,
	UVolumeTexture*& LoadedTexture, uint8* BulkData, bool Persistent,
	bool SaveNow, bool UAVCompatible) {
	FString PackageName = TEXT("/Game/GeneratedTextures/");
	PackageName += AssetName;
	UPackage* Package = CreatePackage(NULL, *PackageName);
	Package->FullyLoad();

	int PixelByteSize = GPixelFormats[PixelFormat].BlockBytes;
	const long long TotalSize = (long long)Dimensions.X * Dimensions.Y * Dimensions.Z * PixelByteSize;
	// Create the texture with given name and location. This will overwrite any existing asset with
	// that name.

	UVolumeTexture* VolumeTexture = NewObject<UVolumeTexture>(
		(UObject*)Package, FName(*AssetName), RF_Public | RF_Standalone | RF_MarkAsRootSet);
	// Set basic properties.
	VolumeTexture->AddToRoot();  // This line prevents garbage collection of the texture

	VolumeTexture->PlatformData = new FTexturePlatformData();
	VolumeTexture->PlatformData->SizeX = Dimensions.X;
	VolumeTexture->PlatformData->SizeY = Dimensions.Y;
	VolumeTexture->PlatformData->NumSlices = Dimensions.Z;
	VolumeTexture->PlatformData->PixelFormat = PixelFormat;

	VolumeTexture->SRGB = false;
	VolumeTexture->NeverStream = true;

	// Create the one and only mip in this texture.
	FTexture2DMipMap* mip = new FTexture2DMipMap();
	mip->SizeX = Dimensions.X;
	mip->SizeY = Dimensions.Y;
	mip->SizeZ = Dimensions.Z;

	// This probably doesn't need to be locked, since nobody else has the pointer to the mip as of
	// now, but it's good manners...
	mip->BulkData.Lock(LOCK_READ_WRITE);
	// Allocate memory in the mip and copy the actual texture data inside
	uint8* ByteArray = (uint8*)mip->BulkData.Realloc(TotalSize);

	if (BulkData) {
		FMemory::Memcpy(ByteArray, BulkData, TotalSize);
	}
	else {
		FMemory::Memset(ByteArray, 0, TotalSize);
	}

	mip->BulkData.Unlock();
	// Add the new MIP to the list of mips.
	VolumeTexture->PlatformData->Mips.Add(mip);

	// If saving fails because of unsupported source format when we want persistent, texture will be
	// updated, but false returned. This is a bit weird.
	bool RetVal =
		HandleTextureEditorData(VolumeTexture, PixelFormat, Persistent, Dimensions, ByteArray);

	// Set the texture to be UAV Compatible if requested. Beware! Not all formats support this
	// (notably anything compressed).
	VolumeTexture->bUAVCompatible = UAVCompatible;
	// Update resource, mark that the folder needs to be rescan and notify editor about asset
	// creation.
	VolumeTexture->UpdateResource();
	Package->MarkPackageDirty();
	FAssetRegistryModule::AssetCreated(VolumeTexture);
	// Pass out the reference to our brand new texture.
	LoadedTexture = VolumeTexture;

	// Only save the asset if that is needed (as this is a disk operation and takes a long time)
	// The texture does not need to be persistent to be saved, but if it's not, only a dummy 0x0x0
	// texture is saved.
	if (SaveNow) {
		FString PackageFileName = FPackageName::LongPackageNameToFilename(
			PackageName, FPackageName::GetAssetPackageExtension());
		return UPackage::SavePackage(Package, VolumeTexture,
			EObjectFlags::RF_Public | EObjectFlags::RF_Standalone,
			*PackageFileName, GError, nullptr, true, true, SAVE_NoError);
	}
	else {
		return true;
	}

  // Let's put mip creation aside now, these wouldn't play nice with persistence as of now...
  //// Add mips for higher levels - these are empty and will be filled by compute shaders
  // while(ActualMips < NumMips) {
  //	  // Each mip level of a volume texture is 8x smaller (2^3)
  // TotalSize /= 8;
  // MipDimensions /= 2;
  // if (MipDimensions.X == 0 || MipDimensions.Y == 0 || MipDimensions.Z == 0) {
  //  break;
  // }

  // FTexture2DMipMap* Mip = new FTexture2DMipMap();
  // Mip->SizeX = MipDimensions.X;
  // Mip->SizeY = MipDimensions.Y;
  // Mip->SizeZ = MipDimensions.Z;

  // Mip->BulkData.Lock(LOCK_READ_WRITE);

  // uint8* ByteArray = (uint8*)Mip->BulkData.Realloc(TotalSize);
  // FMemory::Memset(ByteArray, 0, TotalSize);

  // Mip->BulkData.Unlock();
  // NewTexture->PlatformData->Mips.Add(Mip);
  // ActualMips++;
  //}
}

bool UpdateVolumeTextureAsset(UVolumeTexture* VolumeTexture, EPixelFormat PixelFormat,
                              FIntVector Dimensions, uint8* BulkData, bool Persistent /*= false*/,
                              bool SaveNow /*= false*/, bool UAVCompatible /*= false*/) {
  if (!VolumeTexture) {
    return false;
  }

  if (!VolumeTexture->PlatformData) {
    VolumeTexture->PlatformData = new FTexturePlatformData();
  }

  int PixelByteSize = GPixelFormats[PixelFormat].BlockBytes;
  const long long TotalSize = (long long)Dimensions.X * Dimensions.Y * Dimensions.Z * PixelByteSize;
  // Create the texture with given name and location. This will overwrite any existing asset with
  // that name.

  // Set basic properties.
  // NewTexture->PlatformData = new FTexturePlatformData();
  VolumeTexture->PlatformData->SizeX = Dimensions.X;
  VolumeTexture->PlatformData->SizeY = Dimensions.Y;
  VolumeTexture->PlatformData->NumSlices = Dimensions.Z;
  VolumeTexture->PlatformData->PixelFormat = PixelFormat;

  VolumeTexture->SRGB = false;
  VolumeTexture->NeverStream = true;

  // Create or get the one and only mip in this texture.
  FTexture2DMipMap* Mip;
  // If texture doesn't have a single mip, create it.
  if (!VolumeTexture->PlatformData->Mips.IsValidIndex(0)) {
    Mip = new (VolumeTexture->PlatformData->Mips) FTexture2DMipMap();
  } else {
    Mip = &VolumeTexture->PlatformData->Mips[0];
  }

  Mip->SizeX = Dimensions.X;
  Mip->SizeY = Dimensions.Y;
  Mip->SizeZ = Dimensions.Z;

  // This probably doesn't need to be locked, since nobody else has the pointer to the mip as of
  // now, but it's good manners...
  Mip->BulkData.Lock(LOCK_READ_WRITE);
  // Allocate memory in the mip and copy the actual texture data inside
  uint8* ByteArray = (uint8*)Mip->BulkData.Realloc(TotalSize);

  if (BulkData) {
    FMemory::Memcpy(ByteArray, BulkData, TotalSize);
  } else {
    FMemory::Memset(ByteArray, 0, TotalSize);
  }
  Mip->BulkData.Unlock();

  // If saving fails because of unsupported source format when we want persistent, texture will be
  // updated, but false returned. This is a bit weird.
  bool RetVal =
      HandleTextureEditorData(VolumeTexture, PixelFormat, Persistent, Dimensions, ByteArray);

  // Set the texture to be UAV Compatible if requested. Beware! Not all formats support this
  // (notably anything compressed).
  VolumeTexture->bUAVCompatible = UAVCompatible;
  // Update resource, mark that the folder needs to be rescan and notify editor about asset
  // creation.
  VolumeTexture->UpdateResource();
  return RetVal;
}


bool HandleTextureEditorData(UTexture* Texture, const EPixelFormat PixelFormat,
                             const bool Persistent, const FIntVector Dimensions,
                             const uint8* BulkData) {
  // Handle persistency and mipgens only if we're in editor!
#if WITH_EDITORONLY_DATA
  // Todo - figure out how to tell the Texture Builder to REALLY LEAVE THE BLOODY MIPS ALONE
  // when setting TMGS_LeaveExistingMips and being persistent. Until then, we simply don't support
  // mips on generated textures. (We could support it on non-persistent textures, the code at the
  // bottom of this function shows how we could do that if needed.).
  Texture->MipGenSettings = TMGS_NoMipmaps;

  // CompressionNone assures the texture is actually saved as we want when it is made persistent and
  // not in DXT1 format. Todo: Saving without compression does not work, figure out why.
  Texture->CompressionNone = true;

  // If asset is to be persistent, handle creating the Source structure for it.
  if (Persistent) {
    // If using a format that's not supported as Source format, fail.
    ETextureSourceFormat TextureSourceFormat = PixelFormatToSourceFormat(PixelFormat);
    if (TextureSourceFormat == TSF_Invalid) {
      GEngine->AddOnScreenDebugMessage(
          0, 10, FColor::Red, "Trying to create persistent asset with unsupported pixel format!");
      return false;
    }
    // Otherwise initialize the source struct with our size and bulk data.
    Texture->Source.Init(Dimensions.X, Dimensions.Y, Dimensions.Z, 1, TextureSourceFormat,
                         BulkData);
  }
#endif  // WITH_EDITORONLY_DATA
  return true;
}

uint8* LoadRawFileIntoArray(const FString FileName, const int64 BytesToLoad) {
  IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
  // Try opening as absolute path.
  IFileHandle* FileHandle = PlatformFile.OpenRead(*FileName);

  // If opening as absolute path failed, open as relative to content directory.
  if (!FileHandle) {
    FString FullPath = FPaths::ProjectContentDir() + FileName;
    FileHandle = PlatformFile.OpenRead(*FullPath);
  }

  if (!FileHandle) {
    MY_LOG("File could not be opened.");
    return nullptr;
  } else if (FileHandle->Size() < BytesToLoad) {
    MY_LOG("File is smaller than expected, cannot read volume.")
    delete FileHandle;
    return nullptr;
  } else if (FileHandle->Size() > BytesToLoad) {
    MY_LOG(
        "File is larger than expected, check your dimensions and pixel format (nonfatal, but the "
        "texture will probably be screwed up)")
  }

  uint8* LoadedArray = new uint8[BytesToLoad];
  FileHandle->Read(LoadedArray, BytesToLoad);
  delete FileHandle;

  // Let the whole world know we were successful.
  MY_LOG("File was successfully read!");
  return LoadedArray;
}

bool Create2DTextureAsset(FString AssetName, EPixelFormat PixelFormat, FIntPoint Dimensions,
                          uint8* BulkData, bool Persistent, bool UAVCompatible, bool SaveNow,
                          TextureAddress TilingX, TextureAddress TilingY) {
  const long TotalSize = (long)Dimensions.X * Dimensions.Y * GPixelFormats[PixelFormat].BlockBytes;

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
  NewTexture->CompressionSettings = TC_Default;
  NewTexture->SRGB = false;

  FTexture2DMipMap* Mip = new (NewTexture->PlatformData->Mips) FTexture2DMipMap();
  Mip->SizeX = Dimensions.X;
  Mip->SizeY = Dimensions.Y;
  Mip->SizeZ = 1;

  Mip->BulkData.Lock(LOCK_READ_WRITE);

  uint8* ByteArray = (uint8*)Mip->BulkData.Realloc(TotalSize);

  if (BulkData) {
    FMemory::Memcpy(ByteArray, BulkData, TotalSize);
  } else {
    FMemory::Memset(ByteArray, 0, TotalSize);
  }
  Mip->BulkData.Unlock();

  FIntVector Dimensions3D = FIntVector(Dimensions.X, Dimensions.Y, 1);
  bool RetVal =
      HandleTextureEditorData(NewTexture, PixelFormat, Persistent, Dimensions3D, ByteArray);

  NewTexture->bUAVCompatible = UAVCompatible;
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

bool Update2DTextureAsset(UTexture2D* Texture, EPixelFormat PixelFormat, FIntPoint Dimensions,
	uint8* BulkData, bool Persistent, bool UAVCompatible,
	TextureAddress TilingX, TextureAddress TilingY) {
	if (!Texture || !Texture->PlatformData) {
		return false;
	}
	// Unlike Volume texture, when releasing the resource, there is a check if the resource and
	// texture mip sizes match, so release the resource now, when the sizes still match. Calling
	// UpdateResource at the end of this function without releasing the resource here runs into an
	// engine assert if the size of the texture changed.
	Texture->ReleaseResource();

	int PixelByteSize = GPixelFormats[PixelFormat].BlockBytes;
	const long long TotalSize = (long long)Dimensions.X * Dimensions.Y * PixelByteSize;
	// Set basic properties.
	// NewTexture->PlatformData = new FTexturePlatformData();
	Texture->PlatformData->SizeX = Dimensions.X;
	Texture->PlatformData->SizeY = Dimensions.Y;
	Texture->PlatformData->NumSlices = 1;

	Texture->PlatformData->PixelFormat = PixelFormat;

	Texture->AddressX = TilingX;
	Texture->AddressY = TilingY;

	Texture->SRGB = false;
	Texture->NeverStream = true;

	// Create or get the one and only mip in this texture.
	FTexture2DMipMap* Mip;
	// If texture doesn't have a single mip, create it.
	if (!Texture->PlatformData->Mips.IsValidIndex(0)) {
		Mip = new (Texture->PlatformData->Mips) FTexture2DMipMap();
	}
	else {
		Mip = &Texture->PlatformData->Mips[0];
	}

	Mip->SizeX = Dimensions.X;
	Mip->SizeY = Dimensions.Y;
	Mip->SizeZ = 1;

	// This probably doesn't need to be locked, since nobody else has the pointer to the mip as of
	// now, but it's good manners...
	Mip->BulkData.Lock(LOCK_READ_WRITE);
	// Allocate memory in the mip and copy the actual texture data inside
	uint8* ByteArray = (uint8*)Mip->BulkData.Realloc(TotalSize);

	if (BulkData) {
		FMemory::Memcpy(ByteArray, BulkData, TotalSize);
	}
	else {
		FMemory::Memset(ByteArray, 0, TotalSize);
	}
	Mip->BulkData.Unlock();

	// If saving fails because of unsupported source format when we want persistent, texture will be
	// updated, but false returned. This is a bit weird.

	FIntVector Dimensions3D = FIntVector(Dimensions.X, Dimensions.Y, 1);
	bool RetVal = HandleTextureEditorData(Texture, PixelFormat, Persistent, Dimensions3D, ByteArray);

	// Set the texture to be UAV Compatible if requested. Beware! Not all formats support this
	// (notably anything compressed).
	Texture->bUAVCompatible = UAVCompatible;
	// Update resource, mark that the folder needs to be rescan and notify editor about asset
	// creation.
	Texture->UpdateResource();
	return RetVal;
}

bool Create2DTextureTransient(UTexture2D*& OutTexture, EPixelFormat PixelFormat, FIntPoint Dimensions,
	uint8* BulkData, TextureAddress TilingX, TextureAddress TilingY) {
	int BlockBytes = GPixelFormats[PixelFormat].BlockBytes;
	int TotalBytes = Dimensions.X * Dimensions.Y * BlockBytes;

	UTexture2D* TransientTexture = UTexture2D::CreateTransient(Dimensions.X, Dimensions.Y, PixelFormat);
	TransientTexture->AddressX = TilingX;
	TransientTexture->AddressY = TilingY;
	
	FTexture2DMipMap& Mip = TransientTexture->PlatformData->Mips[0];
	void* Data = Mip.BulkData.Lock(LOCK_READ_WRITE);

	if (BulkData) {
		FMemory::Memcpy(Data, BulkData, TotalBytes);
	}
	else {
		FMemory::Memset(Data, 0, TotalBytes);
	}

	Mip.BulkData.Unlock();
	TransientTexture->UpdateResource();
	OutTexture = TransientTexture;
	return true;
}

bool CreateVolumeTextureTransient(UVolumeTexture*& OutTexture, EPixelFormat PixelFormat, FIntVector Dimensions,
	uint8* BulkData, bool bUAVTargetable) {
	int BlockBytes = GPixelFormats[PixelFormat].BlockBytes;
	int TotalBytes = Dimensions.X * Dimensions.Y * Dimensions.Z * BlockBytes;

	UVolumeTexture* TransientTexture = 
		NewObject<UVolumeTexture>(
			GetTransientPackage(),
			NAME_None,
			RF_Transient
		);

	// DEBUGING ONLY!!! Change to smth else!
	//TransientTexture->SetLightingGuid();
	//TransientTexture->CompressionSettings = TC_Masks;
	//TransientTexture->Filter = TF_Nearest;
	// END OF DEBUG

	TransientTexture->SRGB = false;
	TransientTexture->NeverStream = true;
	TransientTexture->CompressionNone = true;

	TransientTexture->PlatformData = new FTexturePlatformData();
	TransientTexture->PlatformData->SizeX = Dimensions.X;
	TransientTexture->PlatformData->SizeY = Dimensions.Y;
	TransientTexture->PlatformData->NumSlices = Dimensions.Z;

	TransientTexture->PlatformData->PixelFormat = PixelFormat;

	// Allocate first mipmap.
	FTexture2DMipMap* Mip = new FTexture2DMipMap();

	TransientTexture->PlatformData->Mips.Add(Mip);
	Mip->BulkData.Lock(LOCK_READ_WRITE);
	void* Data = Mip->BulkData.Realloc(TotalBytes);

	if (BulkData) {
		FMemory::Memcpy(Data, BulkData, TotalBytes);
	}
	else {
		FMemory::Memset(Data, 0, TotalBytes);
	}
	Mip->BulkData.Unlock();

	TransientTexture->UpdateResource();
	OutTexture = TransientTexture;
	return true;
}

