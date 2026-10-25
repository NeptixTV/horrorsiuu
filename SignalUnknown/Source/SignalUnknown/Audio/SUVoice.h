// SIGNAL UNKNOWN – proximity voice chat settings.
//
// Uses the engine's built-in VoIP (OnlineSubsystem voice, see Config/DefaultEngine.ini [Voice]).
// Every remote player's voice is played through a UVOIPTalker attached to their head with the
// attenuation below: volume falls off with distance, walls muffle it (occlusion) and the active
// reverb of the listener's environment applies.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "SUVoice.generated.h"

class USoundAttenuation;

UCLASS()
class SIGNALUNKNOWN_API USUVoice : public UObject
{
	GENERATED_BODY()

public:
	/** Shared attenuation asset created at runtime (kept alive for the whole session). */
	static USoundAttenuation* GetProximityAttenuation();

	/** Audible range of normal speech (cm). */
	static constexpr float SpeechRange = 2800.f;
};
