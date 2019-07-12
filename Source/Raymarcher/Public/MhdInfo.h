// (C) Technical University of Munich - Computer Aided Medical Procedures
// Developed by Tomas Bartipan, Jakob Weiss (jakob.weiss@tum.de)

#pragma once

#include "CoreMinimal.h"
#include "ModuleManager.h"

#include "PixelFormat.h"

UENUM(BlueprintType)
enum class EMhdElementType : uint8 {
  MET_UCHAR UMETA(DisplayName = "MET_UCHAR"),
  MET_USHORT UMETA(DisplayName = "MET_USHORT"),
  MET_SHORT UMETA(DisplayName = "MET_SHORT"),
  MET_INT UMETA(DisplayName = "MET_INT"),
  MET_FLOAT UMETA(DisplayName = "MET_FLOAT"),
  MET_FLOAT64 UMETA(DisplayName = "MET_FLOAT64"),
  MET_UNKNOWN UMETA(DisplayName = "Unknown"),
};

struct FMhdElementTypeInfo {
  FString Name;
  int32 SizeBytes;
  EPixelFormat MatchingPixelFormat;
  EMhdElementType ElementType;
};

class FMhdInfo {
public:
  static FMhdElementTypeInfo ElementTypeInfo[7];

  bool ParseSuccessful{false};
  int32 NDims{0};
  FIntVector Dimensions{0, 0, 0};
  FVector Position{0.0, 0.0, 0.0};
  FVector Spacing{0.0f, 0.0f, 0.0f};
  bool CompressedData{false};
  FString DataFile{""};
  EMhdElementType ElementType{EMhdElementType::MET_UNKNOWN};

  FMhdInfo(FIntVector Dims, FVector Spaces)
    : ParseSuccessful(true), Dimensions(Dims), Spacing(Spaces) {}
  FMhdInfo() {}

  static FMhdInfo LoadAndParseMhdFile(const FString FileName);

  static FMhdInfo ParseFromString(const FString FileName);

  static EPixelFormat ConvertToBestPixelFormat(TUniquePtr<uint8> &DataArray, uint64 NumElements,
                                               EMhdElementType Type);

  FVector GetWorldDimensions() const;

  FString ToString() const;

  EPixelFormat GetMatchingPixelFormat() const;
};
