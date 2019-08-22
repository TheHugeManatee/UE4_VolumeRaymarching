// (c) 2019 Technical University of Munich
// Jakob Weiss <jakob.weiss@tum.de>

#include "VolumeRepresentationFactoryNew.h"

#include "VolumeRepresentation.h"

UVolumeRepresentationFactoryNew::UVolumeRepresentationFactoryNew(
    const FObjectInitializer& ObjectInitializer)
  : Super(ObjectInitializer) {
  SupportedClass = UVolumeRepresentation::StaticClass();
  bCreateNew = true;
  bEditAfterNew = true;
}

/* UFactory overrides
*****************************************************************************/

UObject* UVolumeRepresentationFactoryNew::FactoryCreateNew(UClass* InClass, UObject* InParent,
                                                           FName InName, EObjectFlags Flags,
                                                           UObject* Context,
                                                           FFeedbackContext* Warn) {
  return NewObject<UVolumeRepresentation>(InParent, InClass, InName, Flags);
}

bool UVolumeRepresentationFactoryNew::ShouldShowInNewMenu() const {
  return true;
}
