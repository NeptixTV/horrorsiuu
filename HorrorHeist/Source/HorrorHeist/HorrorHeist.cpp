#include "HorrorHeist.h"
#include "Modules/ModuleManager.h"
#include "UI/HHStyle.h"

DEFINE_LOG_CATEGORY(LogHorrorHeist);

class FHorrorHeistModule : public FDefaultGameModuleImpl
{
public:
	virtual void ShutdownModule() override
	{
		FHHStyle::Shutdown();
	}
};

IMPLEMENT_PRIMARY_GAME_MODULE(FHorrorHeistModule, HorrorHeist, "HorrorHeist");
