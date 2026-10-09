#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Online/HHSessionSubsystem.h"

class AHHLobbyHUD;
class SBox;
class SVerticalBox;
class UHHMissionDefinition;
struct FSlateBrush;

/**
 * The job board (corkboard station): browse houses, read the dossier, pin the job (crew
 * leader), manage the crew session and ready up.
 */
class SHHPlayPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SHHPlayPanel) {}
		SLATE_ARGUMENT(TWeakObjectPtr<AHHLobbyHUD>, HUD)
	SLATE_END_ARGS()

	virtual ~SHHPlayPanel() override;
	void Construct(const FArguments& InArgs);

private:
	TSharedRef<SWidget> BuildJobsTab();
	TSharedRef<SWidget> BuildCrewTab();
	TSharedRef<SWidget> BuildFooter();
	void RebuildJobList();
	void RebuildDossier();
	void RebuildSessionList();
	void SetTab(int32 NewTab);
	void Inspect(FName MissionId);
	void HandleSessionsFound(bool bSuccess, const TArray<FHHSessionInfo>& Results);

	bool IsLeader() const;
	FName GetPinnedMission() const;
	int32 GetPlayerLevel() const;

	TWeakObjectPtr<AHHLobbyHUD> HUD;
	TSharedPtr<SBox> TabHost;
	TSharedPtr<SVerticalBox> JobList;
	TSharedPtr<SBox> DossierHost;
	TSharedPtr<SVerticalBox> SessionList;

	int32 Tab = 0;
	FName Inspected;
	FText SessionStatus;
	bool bSearching = false;

	/** Mission photos used by the list and dossier. */
	TArray<TSharedPtr<FSlateBrush>> Brushes;
	TArray<FHHSessionInfo> SessionResults;
	FDelegateHandle SessionsFoundHandle;
};
