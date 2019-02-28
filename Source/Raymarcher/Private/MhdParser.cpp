// (C) Technical University of Munich - Computer Aided Medical Procedures
// Developed by Tomas Bartipan (tomas.bartipan@tum.de)

#include "MhdParser.h"

FMhdInfo FMhdParser::ParseString(const FString FileString)
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
