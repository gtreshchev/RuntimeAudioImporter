// Georgy Treshchev 2023.

#pragma once

#include "Modules/ModuleManager.h"

class FRuntimeAudioImporterEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
