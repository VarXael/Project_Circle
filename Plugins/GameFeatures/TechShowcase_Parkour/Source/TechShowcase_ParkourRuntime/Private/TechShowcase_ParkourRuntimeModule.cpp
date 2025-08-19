// Copyright Epic Games, Inc. All Rights Reserved.

#include "TechShowcase_ParkourRuntimeModule.h"

#define LOCTEXT_NAMESPACE "FTechShowcase_ParkourRuntimeModule"

void FTechShowcase_ParkourRuntimeModule::StartupModule()
{
	// This code will execute after your module is loaded into memory;
	// the exact timing is specified in the .uplugin file per-module
}

void FTechShowcase_ParkourRuntimeModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.
	// For modules that support dynamic reloading, we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FTechShowcase_ParkourRuntimeModule, TechShowcase_ParkourRuntime)
