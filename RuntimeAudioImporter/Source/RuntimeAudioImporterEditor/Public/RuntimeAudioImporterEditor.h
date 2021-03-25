// Georgy Treshchev 2021.

#pragma once

#include "Modules/ModuleManager.h"

class FRuntimeAudioImporterEditorModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
