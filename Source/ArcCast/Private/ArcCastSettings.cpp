// Copyright 2026 Simulated Flow. All Rights Reserved.

#include "ArcCastSettings.h"

#include "ArcCastProfile.h"
#include "ArcCastSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

UArcCastSettings::UArcCastSettings()
{
}

FName UArcCastSettings::GetCategoryName() const
{
	return FName(TEXT("Plugins"));
}

FName UArcCastSettings::GetSectionName() const
{
	return FName(TEXT("ArcCast"));
}

const UArcCastSettings& UArcCastSettings::Get()
{
	const UArcCastSettings* Settings = GetDefault<UArcCastSettings>();
	check(Settings);
	return *Settings;
}

void UArcCastSettings::PushToLiveWorlds()
{
	if (!GEngine)
	{
		return;
	}

	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		if (UWorld* World = Context.World())
		{
			if (UArcCastSubsystem* Subsystem = World->GetSubsystem<UArcCastSubsystem>())
			{
				Subsystem->ApplySettings();
			}
		}
	}
}

void UArcCastSettings::SetEnabled(bool bInEnabled)
{
	GetMutableDefault<UArcCastSettings>()->bEnabled = bInEnabled;
	PushToLiveWorlds();
}

void UArcCastSettings::SetOcclusionAware(bool bInOcclusionAware)
{
	GetMutableDefault<UArcCastSettings>()->bOcclusionAware = bInOcclusionAware;
	PushToLiveWorlds();
}

void UArcCastSettings::SetOccludedOpacity(float InOpacity)
{
	GetMutableDefault<UArcCastSettings>()->OccludedOpacity = FMath::Clamp(InOpacity, 0.0f, 1.0f);
	PushToLiveWorlds();
}

void UArcCastSettings::SetOcclusionEveryNthPoint(int32 InEveryNth)
{
	GetMutableDefault<UArcCastSettings>()->OcclusionEveryNthPoint = FMath::Max(1, InEveryNth);
	PushToLiveWorlds();
}

void UArcCastSettings::SetMaxSimStepsPerFrame(int32 InMaxSteps)
{
	GetMutableDefault<UArcCastSettings>()->MaxSimStepsPerFrame = FMath::Max(16, InMaxSteps);
	PushToLiveWorlds();
}

void UArcCastSettings::SetGlobalOpacity(float InOpacity)
{
	GetMutableDefault<UArcCastSettings>()->GlobalOpacity = FMath::Clamp(InOpacity, 0.0f, 1.0f);
	PushToLiveWorlds();
}

void UArcCastSettings::SetDrawInEditorViewport(bool bInDraw)
{
	GetMutableDefault<UArcCastSettings>()->bDrawInEditorViewport = bInDraw;
	PushToLiveWorlds();
}

void UArcCastSettings::SetAutoSpawnHUDComponent(bool bInAuto)
{
	GetMutableDefault<UArcCastSettings>()->bAutoSpawnHUDComponent = bInAuto;
	PushToLiveWorlds();
}

void UArcCastSettings::SetDefaultProfile(UArcCastProfile* InProfile)
{
	GetMutableDefault<UArcCastSettings>()->DefaultProfile = InProfile;
	PushToLiveWorlds();
}
