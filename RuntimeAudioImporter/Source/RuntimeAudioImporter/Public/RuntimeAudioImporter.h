// Respirant 2020.

#pragma once

#include "Modules/ModuleManager.h"

class FRuntimeAudioImporterModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;


};
