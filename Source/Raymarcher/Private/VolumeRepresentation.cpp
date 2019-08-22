// Fill out your copyright notice in the Description page of Project Settings.

#include "VolumeRepresentation.h"

UVolumeRepresentation::UVolumeRepresentation()
  : Texture{nullptr}
  , VolumeSizeInMM{1.0, 1.0, 1.0}
  , CenterLocation{0.0, 0.0, 0.0}
  , Dimensions{0, 0, 0} {
}

UVolumeRepresentation::~UVolumeRepresentation() {
}
