// Copyright 2026 Silvan Teufel. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

/** ArcCast — shippable trajectory and landing preview drawn on UCanvas (runtime module). */
class FArcCastModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
