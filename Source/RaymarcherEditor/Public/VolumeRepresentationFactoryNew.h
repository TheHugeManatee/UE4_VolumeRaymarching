// (c) 2019 Technical University of Munich
// Jakob Weiss <jakob.weiss@tum.de>

#pragma once

#include "Factories/Factory.h"
#include "UObject/ObjectMacros.h"

#include "VolumeRepresentationFactoryNew.generated.h"

/**
 * Implements a factory for UTextAsset objects.
 */
UCLASS(hidecategories = Object)
class UVolumeRepresentationFactoryNew : public UFactory {
  GENERATED_UCLASS_BODY()

public:
  //~ UFactory Interface

  virtual UObject* FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName,
                                    EObjectFlags Flags, UObject* Context,
                                    FFeedbackContext* Warn) override;
  virtual bool ShouldShowInNewMenu() const override;
};
