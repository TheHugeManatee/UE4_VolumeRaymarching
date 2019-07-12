// (C) Technical University of Munich - Computer Aided Medical Procedures
// Developed by Tomas Bartipan, Jakob Weiss (jakob.weiss@tum.de)

#include "MhdInfo.h"
#include "FileHelper.h"
#include "Paths.h"

#include "Runtime/Core/Public/Async/ParallelFor.h"

THIRD_PARTY_INCLUDES_START
#include <sstream>
#include <string>
THIRD_PARTY_INCLUDES_START

FMhdElementTypeInfo FMhdInfo::ElementTypeInfo[7] = {
    {"MET_UCHAR", 1, PF_G8, EMhdElementType::MET_UCHAR},
    {"MET_USHORT", 2, PF_G16, EMhdElementType::MET_USHORT},
    {"MET_SHORT", 2, PF_G16, EMhdElementType::MET_SHORT},
    {"MET_INT", 4, PF_R32_SINT, EMhdElementType::MET_INT},
    {"MET_FLOAT", 4, PF_R32_FLOAT, EMhdElementType::MET_FLOAT},
    {"MET_FLOAT64", 8, PF_R32_FLOAT, EMhdElementType::MET_FLOAT64},
    {"MET_UNKNOWN", 0, PF_Unknown, EMhdElementType::MET_UNKNOWN}};

FMhdInfo FMhdInfo::ParseFromString(const FString FileString) {
  {
    FMhdInfo info;

    std::string MyStdString(TCHAR_TO_UTF8(*FileString));
    std::istringstream inStream = std::istringstream(MyStdString);

    std::string DummyString;  // Dummy string object used to skip over input
    std::string Line;
    while (std::getline(inStream, Line)) {
      std::istringstream LineStream{Line};
      std::string KeyWord;
      LineStream >> KeyWord;

      LineStream >> DummyString;  // skip the equal sign

      if (KeyWord == "DimSize") {
        // Read the three values;
        LineStream >> info.Dimensions.X;
        LineStream >> info.Dimensions.Y;
        LineStream >> info.Dimensions.Z;
      } else if (KeyWord == "ElementSpacing") {
        // Read the three values;
        LineStream >> info.Spacing.X;
        LineStream >> info.Spacing.Y;
        LineStream >> info.Spacing.Z;
      } else if (KeyWord == "CompressedData") {
        std::string CompressedData;
        LineStream >> CompressedData;
        if (CompressedData == "True" || CompressedData == "TRUE" || CompressedData == "true") {
          info.CompressedData = true;
        }
      } else if (KeyWord == "ElementType") {
        std::string Value;
        LineStream >> Value;
        if (Value == "MET_UCHAR")
          info.ElementType = EMhdElementType::MET_UCHAR;
        else if (Value == "MET_USHORT")
          info.ElementType = EMhdElementType::MET_USHORT;
        else if (Value == "MET_SHORT")
          info.ElementType = EMhdElementType::MET_SHORT;
        else if (Value == "MET_INT")
          info.ElementType = EMhdElementType::MET_INT;
        else if (Value == "MET_FLOAT")
          info.ElementType = EMhdElementType::MET_FLOAT;
        else if (Value == "MET_FLOAT64" || Value == "MET_DOUBLE")
          info.ElementType = EMhdElementType::MET_FLOAT64;
      } else if (KeyWord == "ElementDataFile") {
        std::string File;
        std::getline(LineStream, File);
        info.DataFile = UTF8_TO_TCHAR(File.c_str());
      }
    }

    // check if at least basic information was read
    if (!info.Dimensions.IsZero() && !info.Spacing.IsZero() && !info.DataFile.IsEmpty())
      info.ParseSuccessful = true;

    return info;
  }
}

EPixelFormat FMhdInfo::ConvertToBestPixelFormat(TUniquePtr<uint8> &DataArray, uint64 NumElements,
                                                EMhdElementType ElementType) {
  int16 *DataPtrInt16;
  uint16 *DataPtrUint16;
  int32 *DataPtrInt32;
  float *DataPtrFloat32;
  double *DataPtrFloat64;
  TUniquePtr<uint8> DataFloat32;

  switch (ElementType) {
    case EMhdElementType::MET_UCHAR: return PF_G8;
    case EMhdElementType::MET_USHORT: return PF_G16;
    case EMhdElementType::MET_SHORT:
      DataPtrInt16 = reinterpret_cast<int16 *>(DataArray.Get());
      DataPtrUint16 = reinterpret_cast<uint16 *>(DataArray.Get());
      // Convert by adding 32767
      ParallelFor(NumElements, [&](int32 Idx) {
        int32 value = DataPtrInt16[Idx];
        DataPtrUint16[Idx] = uint16(value + 0x7FFF);
      });
      return PF_G16;
    case EMhdElementType::MET_INT:
      MY_LOG("Warning: Integer format is not directly supported. Converting data to float.");
      DataPtrInt32 = reinterpret_cast<int32 *>(DataArray.Get());
      DataPtrFloat32 = reinterpret_cast<float *>(DataArray.Get());
      // Convert directly to float
      ParallelFor(NumElements, [&](int32 Idx) {
        int32 value = DataPtrInt32[Idx];
        DataPtrFloat32[Idx] = float(value);
      });
      return PF_R32_FLOAT;
    case EMhdElementType::MET_FLOAT: return PF_R32_FLOAT;
    // convert double to float by lossy conversion
    case EMhdElementType::MET_FLOAT64:
      DataFloat32.Reset(new uint8[NumElements * 8]);
      DataPtrFloat32 = reinterpret_cast<float *>(DataFloat32.Get());
      DataPtrFloat64 = reinterpret_cast<double *>(DataArray.Get());
      ParallelFor(NumElements, [&](int32 Idx) { DataPtrFloat32[Idx] = DataPtrFloat64[Idx]; });
      Swap(DataArray, DataFloat32);
      return PF_R32_FLOAT;
  }
  return PF_Unknown;
}

FString FMhdInfo::ToString() const {
  FVector WorldDimensions;
  WorldDimensions.X = this->Dimensions.X * this->Spacing.X;
  WorldDimensions.Y = this->Dimensions.Y * this->Spacing.Y;
  WorldDimensions.Z = this->Dimensions.Z * this->Spacing.Z;

  FString text =
      "Info read from the MHD file :\nDimensions = " + FString::FromInt(this->Dimensions.X) + " " +
      FString::FromInt(this->Dimensions.Y) + " " + FString::FromInt(this->Dimensions.Z) +
      "\nSpacing : " + FString::SanitizeFloat(this->Spacing.X, 3) + " " +
      FString::SanitizeFloat(this->Spacing.Y, 3) + " " +
      FString::SanitizeFloat(this->Spacing.Z, 3) +
      "\nWorld Size MM : " + FString::SanitizeFloat(WorldDimensions.X, 3) + " " +
      FString::SanitizeFloat(WorldDimensions.Y, 3) + " " +
      FString::SanitizeFloat(WorldDimensions.Z, 3);
  return text;
}

EPixelFormat FMhdInfo::GetMatchingPixelFormat() const {
  switch (ElementType) {
    case EMhdElementType::MET_UCHAR: return PF_G8;
    case EMhdElementType::MET_USHORT:  // fallthrough
    case EMhdElementType::MET_SHORT: return PF_G16;
    case EMhdElementType::MET_INT:
      return EPixelFormat::PF_Unknown;  // Not sure about this, probably this is pretty
                                        // unsupported so we would need to convert the raw data
    case EMhdElementType::MET_FLOAT: return PF_R32_FLOAT;
    case EMhdElementType::MET_FLOAT64: return PF_Unknown;
    default: return PF_Unknown;
  }
}

FMhdInfo FMhdInfo::LoadAndParseMhdFile(FString FileName) {
  FMhdInfo MhdInfo;
  MhdInfo.ParseSuccessful = false;

  FString FileContent;
  // First, try to read as absolute path
  if (!FFileHelper::LoadFileToString(/*out*/ FileContent, *FileName)) {
    // Try it as a relative path
    FString RelativePath = FPaths::ProjectContentDir();
    FString FullPath =
        IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*RelativePath) + FileName;
    FileName = FullPath;
    if (!FFileHelper::LoadFileToString(/*out*/ FileContent, *FullPath)) {
      return MhdInfo;
    }
  }

  MhdInfo = FMhdInfo::ParseFromString(FileContent);
  return MhdInfo;
}

FVector FMhdInfo::GetWorldDimensions() const {
  return FVector(this->Spacing.X * this->Dimensions.X, this->Spacing.Y * this->Dimensions.Y,
                 this->Spacing.Z * this->Dimensions.Z);
}
