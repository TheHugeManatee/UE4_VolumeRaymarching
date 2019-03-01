// (C) Technical University of Munich - Computer Aided Medical Procedures
// Developed by Tomas Bartipan (tomas.bartipan@tum.de)

#include "MhdInfo.h"
#include "FileHelper.h"
#include "Paths.h"

FMhdInfo FMhdInfo::ParseFromString(const FString FileString)
{
	{
		FIntVector Dimensions;
		FVector Spacings;

		std::string MyStdString(TCHAR_TO_UTF8(*FileString));
		std::istringstream inStream = std::istringstream(MyStdString);

		std::string ReadWord;

		// Skip until we get to Dimensions.
		while (inStream.good() && ReadWord != "DimSize") {
			inStream >> ReadWord;
		}
		// Should be at the "=" after DimSize now.
		if (inStream.good()) {
			// Get rid of equal sign.
			inStream >> ReadWord;
			// Read the three values;
			inStream >> Dimensions.X;
			inStream >> Dimensions.Y;
			inStream >> Dimensions.Z;
		}
		else {
			return FMhdInfo();
		}

		// Skip until we get to spacing.
		while (inStream.good() && ReadWord != "ElementSpacing") {
			inStream >> ReadWord;
		}
		// Should be at the "=" after ElementSpacing now.
		if (inStream.good()) {
			// Get rid of equal sign.
			inStream >> ReadWord;
			// Read the three values;
			inStream >> Spacings.X;
			inStream >> Spacings.Y;
			inStream >> Spacings.Z;
		}
		else {
			return FMhdInfo();
		}

		// Return with constructor that sets success to true.
		return FMhdInfo(Dimensions, Spacings);
	}
}

FString FMhdInfo::ToString() const
{
	FVector WorldDimensions;
	WorldDimensions.X = this->Dimensions.X * this->Spacing.X;
	WorldDimensions.Y = this->Dimensions.Y * this->Spacing.Y;
	WorldDimensions.Z = this->Dimensions.Z * this->Spacing.Z;

	FString text =
		"Shit read from the MHD file :\nDimensions = " + FString::FromInt(this->Dimensions.X) + " " +
		FString::FromInt(this->Dimensions.Y) + " " + FString::FromInt(this->Dimensions.Z) +
		"\nSpacing : " + FString::SanitizeFloat(this->Spacing.X, 3) + " " +
		FString::SanitizeFloat(this->Spacing.Y, 3) + " " + FString::SanitizeFloat(this->Spacing.Z, 3) +
		"\nWorld Size MM : " + FString::SanitizeFloat(WorldDimensions.X, 3) + " " +
		FString::SanitizeFloat(WorldDimensions.Y, 3) + " " +
		FString::SanitizeFloat(WorldDimensions.Z, 3);
	return text;
}

FMhdInfo FMhdInfo::LoadAndParseMhdFile(FString FileName)
{
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

FVector FMhdInfo::GetWorldDimensions() const
{
	return FVector(this->Spacing.X * this->Dimensions.X, this->Spacing.Y * this->Dimensions.Y, this->Spacing.Z * this->Dimensions.Z);
}
