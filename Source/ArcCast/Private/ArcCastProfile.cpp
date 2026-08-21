// Copyright 2026 Silvan Teufel. All Rights Reserved.

#include "ArcCastProfile.h"

UArcCastProfile::UArcCastProfile()
{
}

FLinearColor UArcCastProfile::GetVerdictColor(EArcCastVerdict Verdict) const
{
	switch (Verdict)
	{
	case EArcCastVerdict::Valid:
		return ValidColor;

	// "There is something there, it is just not a floor you can stand on." Amber, not red, because the
	// throw itself is perfectly legal - it is the landing that is not.
	case EArcCastVerdict::TooSteep:
	case EArcCastVerdict::OutOfRange:
		return WarnColor;

	case EArcCastVerdict::Blocked:
	case EArcCastVerdict::NoGround:
	default:
		return InvalidColor;
	}
}

FPrimaryAssetId UArcCastProfile::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(FPrimaryAssetType(TEXT("ArcCastProfile")), GetFName());
}
