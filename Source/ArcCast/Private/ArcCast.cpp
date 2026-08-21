// Copyright 2026 Simulated Flow. All Rights Reserved.

#include "ArcCast.h"
#include "ArcCastLog.h"

DEFINE_LOG_CATEGORY(LogArcCast);

#define LOCTEXT_NAMESPACE "FArcCastModule"

void FArcCastModule::StartupModule()
{
	UE_LOG(LogArcCast, Log, TEXT("ArcCast started."));
}

void FArcCastModule::ShutdownModule()
{
	UE_LOG(LogArcCast, Log, TEXT("ArcCast shut down."));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FArcCastModule, ArcCast)
