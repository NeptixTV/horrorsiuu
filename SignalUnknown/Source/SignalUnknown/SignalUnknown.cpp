#include "SignalUnknown.h"
#include "Modules/ModuleManager.h"
#include "Misc/CoreDelegates.h"
#include "World/SUModelLibrary.h"

DEFINE_LOG_CATEGORY(LogSignalUnknown);

class FSignalUnknownModule : public FDefaultGameModuleImpl
{
public:
	virtual void StartupModule() override
	{
#if WITH_EDITOR
		// Editor and cook: make sure the generated model materials exist as assets, so that a
		// packaged game can load them.
		FCoreDelegates::OnPostEngineInit.AddStatic(&USUModelLibrary::EnsureMaterialAssets);
#endif
	}
};

IMPLEMENT_PRIMARY_GAME_MODULE(FSignalUnknownModule, SignalUnknown, "SignalUnknown");
