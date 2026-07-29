// Copyright Epic Games, Inc. All Rights Reserved.

#include "APBReloaded.h"
#include "APBSecretProvider.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogAPBReloaded)

class FAPBReloadedModule final : public FDefaultGameModuleImpl
{
public:
	virtual void StartupModule() override
	{
		FDefaultGameModuleImpl::StartupModule();
		FAPBSecretProvider::PreflightRole();
	}
};

IMPLEMENT_PRIMARY_GAME_MODULE(FAPBReloadedModule, APBReloaded, "APBReloaded");
