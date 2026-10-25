// SIGNAL UNKNOWN – mission definitions (built in, optionally replaced by a DataTable).
#pragma once

#include "CoreMinimal.h"
#include "Mission/SUMissionTypes.h"

class SIGNALUNKNOWN_API FSUMissionDatabase
{
public:
	static const TArray<FSUMissionDef>& All();
	static const FSUMissionDef* Find(FName Id);

	/** Background radio chatter of other posts (atmosphere, not mission relevant). */
	static const TArray<FSUSpeechLine>& Chatter();

	/** Lines used when the radio is disturbed by the horror director. */
	static const TArray<FSUSpeechLine>& StrangeTransmissions();

	/** Final message after the last mission currently in the game. */
	static const TArray<FSUSpeechLine>& EndOfContentLines();

	/** Checks the built-in data for consistency (used by the automation test). Returns errors. */
	static TArray<FString> Validate();
};
