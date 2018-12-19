// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ModuleManager.h"

#include "string"
#include "sstream"

//Sample header

//
//ObjectType = Image
//NDims = 3
//DimSize = 512 512 128
//ElementSpacing = 0.00390625 0.0117188 0.046875
//Position = 0 -4.19925 -4.19785
//ElementType = MET_UCHAR
//ElementNumberOfChannels = 1
//ElementByteOrderMSB = False
//ElementDataFile = preprocessed.raw


class FMhdInfo {
public:
	FIntVector Dimensions;
	FVector Spacing;

	FMhdInfo(FIntVector dims, FVector spaces) : Dimensions(dims), Spacing(spaces) {}
	FMhdInfo() : Dimensions(0,0,0), Spacing(0,0,0){}
};

class FMhdParser {
public:

	static inline FMhdInfo ParseString(const FString inString)
	{
		FIntVector dims;
		FVector spacings;

		std::string MyStdString(TCHAR_TO_UTF8(*inString));
		std::istringstream inStream = std::istringstream(MyStdString);

		std::string readWord;

		// Skip until we get to Dimensions.
		while (inStream.good() && readWord != "DimSize") {
			inStream >> readWord;
		}
		// Should be at the "=" after DimSize now.
		if (inStream.good()) {
			// Get rid of equal sign.
			inStream >> readWord;
			// Read the three values;
			inStream >> dims.X;
			inStream >> dims.Y;
			inStream >> dims.Z;
		}
		else {
			return FMhdInfo(FIntVector(0, 0, 0), FVector(0, 0, 0));
		}

		// Skip until we get to spacing.
		while (inStream.good() && readWord != "ElementSpacing") {
			inStream >> readWord;
		}
		// Should be at the "=" after ElementSpacing now.
		if (inStream.good()) {
			// Get rid of equal sign.
			inStream >> readWord;
			// Read the three values;
			inStream >> spacings.X;
			inStream >> spacings.Y;
			inStream >> spacings.Z;
		}
		else {
			return FMhdInfo(FIntVector(0, 0, 0), FVector(0, 0, 0));
		}

		return FMhdInfo(dims, spacings);
	}

};