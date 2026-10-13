#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Core/HHTypes.h"
#include "Online/HHSessionSubsystem.h"

class AHHLobbyHUD;
class AHHLobbyGameState;
class SBox;
class SVerticalBox;
class UHHMissionDefinition;
struct FHHJobIntel;
struct FSlateBrush;

/**
 * The job board (corkboard station): browse houses, read the dossier (owners, valuables and where
 * they are, ways in, the crew's floor plan), pin the job and pick the getaway (crew leader), manage
 * the crew session (lobby code) and ready up. Says plainly why the van can't leave yet.
 */
class SHHPlayPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SHHPlayPanel) {}
		SLATE_ARGUMENT(TWeakObjectPtr<AHHLobbyHUD>, HUD)
	SLATE_END_ARGS()

	virtual ~SHHPlayPanel() override;
	void Construct(const FArguments& InArgs);
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

private:
	TSharedRef<SWidget> BuildJobsTab();
	TSharedRef<SWidget> BuildCrewTab();
	TSharedRef<SWidget> BuildFooter();
	TSharedRef<SWidget> BuildActionStrip(const UHHMissionDefinition* Mission, bool bLocked);
	TSharedRef<SWidget> BuildIntelSection(const UHHMissionDefinition* Mission, EHHIntelKind Kind);
	TSharedRef<SWidget> BuildGetawaySection();
	void RebuildJobList();
	void RebuildDossier();
	void RebuildSessionList();
	void SetTab(int32 NewTab);
	void Inspect(FName MissionId);
	void Pin(FName MissionId) const;
	void HandleSessionsFound(bool bSuccess, const TArray<FHHSessionInfo>& Results);
	void BindLobbyState();
	void UnbindLobbyState();
	void HandleLobbyDataChanged();

	bool IsLeader() const;
	FName GetPinnedMission() const;
	int32 GetPlayerLevel() const;
	bool IsIntelKnown(const FHHJobIntel& Intel) const;
	/** Empty when the generated content matches this build, otherwise what to do about it. */
	FText GetContentWarning() const;
	FText GetFooterStatus() const;

	TWeakObjectPtr<AHHLobbyHUD> HUD;
	TWeakObjectPtr<AHHLobbyGameState> BoundState;
	TSharedPtr<SBox> TabHost;
	TSharedPtr<SVerticalBox> JobList;
	TSharedPtr<SBox> DossierHost;
	TSharedPtr<SVerticalBox> SessionList;

	int32 Tab = 0;
	FName Inspected;
	FText SessionStatus;
	bool bSearching = false;
	bool bDossierDirty = false;

	/** Mission photos / floor plans used by the list and dossier. */
	TArray<TSharedPtr<FSlateBrush>> Brushes;
	TArray<FHHSessionInfo> SessionResults;
	FDelegateHandle SessionsFoundHandle;
	FDelegateHandle IntelHandle;
	FDelegateHandle MissionHandle;
};
