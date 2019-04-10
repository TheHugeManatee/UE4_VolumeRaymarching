// (C) Technical University of Munich - Computer Aided Medical Procedures
// Developed by Tomas Bartipan (tomas.bartipan@tum.de)

#pragma once

#include "CoreMinimal.h"
#include "ModuleManager.h"

#include "sstream"
#include "string"

// Sample header

//
// ObjectType = Image
// NDims = 3
// DimSize = 512 512 128
// ElementSpacing = 0.00390625 0.0117188 0.046875
// Position = 0 -4.19925 -4.19785
// ElementType = MET_UCHAR
// ElementNumberOfChannels = 1
// ElementByteOrderMSB = False
// ElementDataFile = preprocessed.raw

class FMhdInfo {
public:
  bool ParseSuccessful;
  FIntVector Dimensions;
  FVector Spacing;

  FMhdInfo(FIntVector Dims, FVector Spaces)
    : ParseSuccessful(true), Dimensions(Dims), Spacing(Spaces) {}
  FMhdInfo() : ParseSuccessful(false), Dimensions(0, 0, 0), Spacing(0, 0, 0) {}

  static FMhdInfo LoadAndParseMhdFile(const FString FileName);

  static FMhdInfo ParseFromString(const FString FileName);

  FVector GetWorldDimensions() const;

  FString ToString() const;
};
