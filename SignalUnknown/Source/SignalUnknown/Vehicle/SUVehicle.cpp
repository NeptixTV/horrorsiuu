#include "Vehicle/SUVehicle.h"
#include "SignalUnknown.h"
#include "Core/SUGameState.h"
#include "Core/SUPlayerController.h"
#include "Core/SUEventSubsystem.h"
#include "Core/SUItemDatabase.h"
#include "Core/SUDeveloperSettings.h"
#include "Core/SUGameUserSettings.h"
#include "Player/SUInventoryComponent.h"
#include "Mission/SUMissionManager.h"
#include "Mission/SUMissionDatabase.h"
#include "Base/SUBaseDevices.h"
#include "GasStation/SUFuelPump.h"
#include "Audio/SUProceduralAudioComponent.h"
#include "World/SUAssetLibrary.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/CapsuleComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/CollisionProfile.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"

namespace
{
	constexpr float WheelRadius = 38.f;     // cm
	constexpr float RestLength = 45.f;      // cm
	constexpr float SpringRate = 25200.f;   // N/m
	constexpr float DamperRate = 3100.f;    // N s/m
	constexpr float MassKg = 1850.f;
	constexpr float MaxSteerDeg = 34.f;
	constexpr float BrakeForceN = 4300.f;
	constexpr float HandbrakeForceN = 6500.f;
	constexpr float AntiRollN = 9000.f;     // per meter of compression difference

	const FVector SeatPositions[4] = {FVector(20.f, -45.f, 10.f), FVector(20.f, 45.f, 10.f), FVector(-85.f, -45.f, 10.f), FVector(-85.f, 45.f, 10.f)};

	FName SeatTag(int32 Seat)
	{
		return FName(*FString::Printf(TEXT("Seat%d"), Seat));
	}

	struct FNewsLine
	{
		int32 MinStory;
		const TCHAR* Text;
	};

	const FNewsLine NewsLines[] = {
		{0, TEXT("Regionalnachrichten: Der Herbstmarkt in Harrow Falls öffnet am Wochenende.")},
		{0, TEXT("Die Straßenarbeiten an der Ostbrücke dauern bis Ende des Monats.")},
		{0, TEXT("Förster bitten Wanderer, auf den markierten Wegen zu bleiben.")},
		{1, TEXT("Die Polizei sucht weiterhin nach einem Wanderer, der seit Dienstag vermisst wird.")},
		{2, TEXT("Weitere Vermisstenmeldungen aus dem nördlichen Waldgebiet. Die Behörden raten zur Vorsicht.")},
		{2, TEXT("Tierschützer melden ungewöhnlich viele verlassene Wildfütterungen.")},
		{3, TEXT("Die Nordstraße ist nachts bis auf Weiteres nur eingeschränkt befahrbar.")},
		{4, TEXT("Sondermeldung: Der Bereich nördlich der Ostbrücke wird vorübergehend gesperrt. Folgen Sie den Anweisungen der Einsatzkräfte.")},
	};
}

ASUVehicle::ASUVehicle()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicatingMovement(true);

	Body = CreateDefaultSubobject<UBoxComponent>(TEXT("Body"));
	Body->SetBoxExtent(FVector(230.f, 92.f, 50.f));
	Body->SetCollisionProfileName(UCollisionProfile::PhysicsActor_ProfileName);
	Body->SetSimulatePhysics(true);
	Body->SetNotifyRigidBodyCollision(true);
	Body->SetGenerateOverlapEvents(true);
	Body->SetLinearDamping(0.05f);
	Body->SetAngularDamping(0.8f);
	Body->BodyInstance.bOverrideMass = true;
	Body->BodyInstance.SetMassOverride(MassKg);
	Body->BodyInstance.COMNudge = FVector(0.f, 0.f, -38.f);
	RootComponent = Body;

	Visual = CreateDefaultSubobject<USceneComponent>(TEXT("Visual"));
	Visual->SetupAttachment(Body);

	USceneComponent** SeatPtrs[4] = {nullptr, nullptr, nullptr, nullptr};
	SeatDriver = CreateDefaultSubobject<USceneComponent>(TEXT("SeatDriver"));
	SeatPassenger = CreateDefaultSubobject<USceneComponent>(TEXT("SeatPassenger"));
	SeatRearLeft = CreateDefaultSubobject<USceneComponent>(TEXT("SeatRearLeft"));
	SeatRearRight = CreateDefaultSubobject<USceneComponent>(TEXT("SeatRearRight"));
	USceneComponent* Seats[4] = {SeatDriver, SeatPassenger, SeatRearLeft, SeatRearRight};
	(void)SeatPtrs;
	for (int32 I = 0; I < 4; ++I)
	{
		Seats[I]->SetupAttachment(Body);
		Seats[I]->SetRelativeLocation(SeatPositions[I]);
	}

	auto MakeZone = [this](const TCHAR* Name, const FVector& Loc, const FVector& Extent, FName Tag) -> UBoxComponent*
	{
		UBoxComponent* Z = CreateDefaultSubobject<UBoxComponent>(FName(Name));
		Z->SetupAttachment(Body);
		Z->SetRelativeLocation(Loc);
		Z->SetBoxExtent(Extent);
		Z->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Z->SetCollisionResponseToAllChannels(ECR_Ignore);
		Z->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
		Z->BodyInstance.bAutoWeld = false;
		Z->SetGenerateOverlapEvents(false);
		Z->ComponentTags.Add(Tag);
		return Z;
	};
	DoorZones.Add(MakeZone(TEXT("DoorDriver"), FVector(20.f, -100.f, 40.f), FVector(45.f, 12.f, 45.f), SeatTag(0)));
	DoorZones.Add(MakeZone(TEXT("DoorPassenger"), FVector(20.f, 100.f, 40.f), FVector(45.f, 12.f, 45.f), SeatTag(1)));
	DoorZones.Add(MakeZone(TEXT("DoorRearLeft"), FVector(-85.f, -100.f, 40.f), FVector(45.f, 12.f, 45.f), SeatTag(2)));
	DoorZones.Add(MakeZone(TEXT("DoorRearRight"), FVector(-85.f, 100.f, 40.f), FVector(45.f, 12.f, 45.f), SeatTag(3)));
	TrunkZone = MakeZone(TEXT("TrunkZone"), FVector(-245.f, 0.f, 40.f), FVector(12.f, 80.f, 45.f), TEXT("Trunk"));
	FuelZone = MakeZone(TEXT("FuelZone"), FVector(-150.f, -102.f, 35.f), FVector(18.f, 10.f, 16.f), TEXT("Fuel"));
	HoodZone = MakeZone(TEXT("HoodZone"), FVector(185.f, 0.f, 42.f), FVector(45.f, 80.f, 12.f), TEXT("Hood"));

	Trunk = CreateDefaultSubobject<USUInventoryComponent>(TEXT("Trunk"));
	Trunk->MaxSlots = 16;

	auto MakeSpot = [this](const TCHAR* Name, const FVector& Loc) -> USpotLightComponent*
	{
		USpotLightComponent* L = CreateDefaultSubobject<USpotLightComponent>(FName(Name));
		L->SetupAttachment(Body);
		L->SetRelativeLocationAndRotation(Loc, FRotator(-3.f, 0.f, 0.f));
		L->IntensityUnits = ELightUnits::Candelas;
		L->Intensity = 3500.f;
		L->AttenuationRadius = 4500.f;
		L->OuterConeAngle = 38.f;
		L->InnerConeAngle = 22.f;
		L->LightColor = FColor(255, 238, 205);
		L->CastShadows = true;
		L->SetVisibility(false);
		return L;
	};
	HeadLeft = MakeSpot(TEXT("HeadLeft"), FVector(236.f, -65.f, 10.f));
	HeadRight = MakeSpot(TEXT("HeadRight"), FVector(236.f, 65.f, 10.f));

	auto MakePoint = [this](const TCHAR* Name, const FVector& Loc, const FColor& Color, float Intensity, float Radius) -> UPointLightComponent*
	{
		UPointLightComponent* L = CreateDefaultSubobject<UPointLightComponent>(FName(Name));
		L->SetupAttachment(Body);
		L->SetRelativeLocation(Loc);
		L->IntensityUnits = ELightUnits::Candelas;
		L->Intensity = Intensity;
		L->AttenuationRadius = Radius;
		L->LightColor = Color;
		L->CastShadows = false;
		L->SetVisibility(false);
		return L;
	};
	TailLeft = MakePoint(TEXT("TailLeft"), FVector(-238.f, -78.f, 20.f), FColor(255, 20, 10), 3.f, 300.f);
	TailRight = MakePoint(TEXT("TailRight"), FVector(-238.f, 78.f, 20.f), FColor(255, 20, 10), 3.f, 300.f);
	ReverseLight = MakePoint(TEXT("Reverse"), FVector(-238.f, 0.f, 15.f), FColor(255, 255, 255), 30.f, 600.f);
	BlinkFL = MakePoint(TEXT("BlinkFL"), FVector(232.f, -86.f, 5.f), FColor(255, 140, 10), 8.f, 250.f);
	BlinkFR = MakePoint(TEXT("BlinkFR"), FVector(232.f, 86.f, 5.f), FColor(255, 140, 10), 8.f, 250.f);
	BlinkRL = MakePoint(TEXT("BlinkRL"), FVector(-236.f, -86.f, 25.f), FColor(255, 140, 10), 8.f, 250.f);
	BlinkRR = MakePoint(TEXT("BlinkRR"), FVector(-236.f, 86.f, 25.f), FColor(255, 140, 10), 8.f, 250.f);
	DomeLight = MakePoint(TEXT("Dome"), FVector(-20.f, 0.f, 104.f), FColor(255, 225, 180), 4.f, 220.f);
	DashLight = MakePoint(TEXT("Dash"), FVector(70.f, -35.f, 32.f), FColor(140, 255, 190), 0.6f, 90.f);

	const FVector Mounts[4] = {FVector(145.f, -82.f, -30.f), FVector(145.f, 82.f, -30.f), FVector(-145.f, -82.f, -30.f), FVector(-145.f, 82.f, -30.f)};
	for (int32 I = 0; I < 4; ++I)
	{
		Wheels[I].LocalMount = Mounts[I];
		Wheels[I].bFront = I < 2;
		Wheels[I].bLeft = (I % 2) == 0;
	}
	Occupants.SetNum(NumSeats);
}

void ASUVehicle::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ASUVehicle, Occupants);
	DOREPLIFETIME(ASUVehicle, bEngineRunning);
	DOREPLIFETIME(ASUVehicle, FuelLiters);
	DOREPLIFETIME(ASUVehicle, EngineHealth);
	DOREPLIFETIME(ASUVehicle, LightsMode);
	DOREPLIFETIME(ASUVehicle, BlinkerMode);
	DOREPLIFETIME(ASUVehicle, bWipers);
	DOREPLIFETIME(ASUVehicle, bHorn);
	DOREPLIFETIME(ASUVehicle, bRadioOn);
	DOREPLIFETIME(ASUVehicle, RadioStation);
	DOREPLIFETIME(ASUVehicle, RadioVolume);
	DOREPLIFETIME(ASUVehicle, bFuelCapOpen);
	DOREPLIFETIME(ASUVehicle, InsertedNozzlePump);
	DOREPLIFETIME(ASUVehicle, RadioIntrusionStart);
	DOREPLIFETIME(ASUVehicle, RadioIntrusionLine);
	DOREPLIFETIME(ASUVehicle, RepThrottle);
	DOREPLIFETIME(ASUVehicle, RepBrake);
	DOREPLIFETIME(ASUVehicle, RepSteer);
	DOREPLIFETIME(ASUVehicle, bRepHandbrake);
}

void ASUVehicle::BeginPlay()
{
	Super::BeginPlay();
	const USUGameplaySettings* Settings = GetDefault<USUGameplaySettings>();
	FuelSpec.CapacityLiters = Settings->FuelTankLiters;
	if (HasAuthority())
	{
		FuelLiters = FMath::Clamp(Settings->StartFuelLiters, 0.f, FuelSpec.CapacityLiters);
		Body->OnComponentHit.AddDynamic(this, &ASUVehicle::HandleBodyHit);
	}
	BuildVisuals();
}

// ------------------------------------------------------------------------------------ visuals

UStaticMeshComponent* ASUVehicle::AddVisual(USceneComponent* Parent, int32 Shape, const FVector& Location, const FVector& Size, int32 Mat, const FRotator& Rot)
{
	USUAssetLibrary* Lib = USUAssetLibrary::Get(this);
	if (!Lib)
	{
		return nullptr;
	}
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
	C->SetupAttachment(Parent ? Parent : Visual.Get());
	C->SetStaticMesh(Lib->GetMesh(static_cast<ESUShape>(Shape)));
	C->SetMaterial(0, Lib->GetMaterial(static_cast<ESUMat>(Mat)));
	C->SetRelativeTransform(FTransform(Rot, Location, Size / 100.f));
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->RegisterComponent();
	return C;
}

void ASUVehicle::BuildVisuals()
{
	const int32 Cube = static_cast<int32>(ESUShape::Cube);
	const int32 Cyl = static_cast<int32>(ESUShape::Cylinder);
	const int32 Sph = static_cast<int32>(ESUShape::Sphere);
	const int32 BodyMat = static_cast<int32>(ESUMat::CarBody);
	const int32 Dark = static_cast<int32>(ESUMat::CarDark);
	const int32 Chrome = static_cast<int32>(ESUMat::Chrome);
	const int32 Glass = static_cast<int32>(ESUMat::Glass);
	const int32 Seat = static_cast<int32>(ESUMat::Fabric);

	// PLACEHOLDER body of an old off-road wagon (exterior)
	AddVisual(nullptr, Cube, FVector(0.f, 0.f, -12.f), FVector(460.f, 184.f, 72.f), BodyMat);            // lower body
	AddVisual(nullptr, Cube, FVector(168.f, 0.f, 30.f), FVector(126.f, 178.f, 16.f), BodyMat);           // hood
	AddVisual(nullptr, Cube, FVector(-40.f, 0.f, 116.f), FVector(270.f, 178.f, 8.f), BodyMat);           // roof
	AddVisual(nullptr, Cube, FVector(-40.f, 0.f, 124.f), FVector(200.f, 120.f, 4.f), Dark);              // roof rack
	for (int32 S = -1; S <= 1; S += 2)
	{
		AddVisual(nullptr, Cube, FVector(92.f, S * 86.f, 76.f), FVector(8.f, 6.f, 82.f), BodyMat, FRotator(-28.f, 0.f, 0.f)); // A pillar
		AddVisual(nullptr, Cube, FVector(-30.f, S * 88.f, 72.f), FVector(10.f, 5.f, 90.f), BodyMat);                        // B pillar
		AddVisual(nullptr, Cube, FVector(-172.f, S * 88.f, 72.f), FVector(10.f, 5.f, 90.f), BodyMat);                       // C pillar
		AddVisual(nullptr, Cube, FVector(80.f, S * 101.f, 52.f), FVector(10.f, 12.f, 9.f), Dark);                           // side mirror
		AddVisual(nullptr, Sph, FVector(234.f, S * 65.f, 10.f), FVector(10.f, 24.f, 18.f), static_cast<int32>(ESUMat::Lamp)); // headlight
		AddVisual(nullptr, Cube, FVector(-233.f, S * 78.f, 20.f), FVector(4.f, 22.f, 16.f), static_cast<int32>(ESUMat::Red));
	}
	AddVisual(nullptr, Cube, FVector(-232.f, 0.f, 70.f), FVector(6.f, 178.f, 22.f), BodyMat);           // tailgate top
	AddVisual(nullptr, Cube, FVector(238.f, 0.f, -35.f), FVector(14.f, 192.f, 24.f), Chrome);           // bumper
	AddVisual(nullptr, Cube, FVector(-238.f, 0.f, -35.f), FVector(14.f, 192.f, 24.f), Dark);
	AddVisual(nullptr, Cube, FVector(233.f, 0.f, 8.f), FVector(4.f, 110.f, 30.f), Dark);                // grille
	AddVisual(nullptr, Cyl, FVector(-246.f, 0.f, 48.f), FVector(64.f, 64.f, 22.f), static_cast<int32>(ESUMat::Tire), FRotator(90.f, 0.f, 0.f)); // spare wheel
	AddVisual(nullptr, Cube, FVector(97.f, 0.f, 112.f), FVector(8.f, 170.f, 8.f), BodyMat);              // windshield top frame

	// interior
	AddVisual(nullptr, Cube, FVector(-30.f, 0.f, -40.f), FVector(330.f, 170.f, 4.f), Dark);             // floor
	AddVisual(nullptr, Cube, FVector(96.f, 0.f, 22.f), FVector(36.f, 176.f, 34.f), Dark);               // dashboard
	for (int32 I = 0; I < 4; ++I)
	{
		AddVisual(nullptr, Cube, SeatPositions[I] + FVector(0.f, 0.f, -22.f), FVector(52.f, 48.f, 14.f), Seat);
		AddVisual(nullptr, Cube, SeatPositions[I] + FVector(-29.f, 0.f, 12.f), FVector(10.f, 48.f, 60.f), Seat, FRotator(-10.f, 0.f, 0.f));
	}
	AddVisual(nullptr, Cube, FVector(95.f, 0.f, 99.f), FVector(4.f, 26.f, 8.f), Dark); // rear view mirror (image on the HUD)
	for (int32 I = 0; I < 4; ++I)
	{
		WheelMeshes.Add(AddVisual(nullptr, Cyl, Wheels[I].LocalMount, FVector(WheelRadius * 2.f, WheelRadius * 2.f, 26.f), static_cast<int32>(ESUMat::Tire), FRotator(0.f, 0.f, 90.f)));
		AddVisual(WheelMeshes.Last(), Cyl, FVector(0.f, 0.f, 0.f), FVector(55.f, 55.f, 104.f), Chrome);
	}

	// steering wheel with spokes
	SteeringWheel = AddVisual(nullptr, Cyl, FVector(64.f, -45.f, 46.f), FVector(38.f, 38.f, 3.f), Dark, FRotator(70.f, 0.f, 0.f));
	if (SteeringWheel)
	{
		AddVisual(SteeringWheel, Cube, FVector::ZeroVector, FVector(110.f, 8.f, 120.f), Chrome);
	}
	// gauges
	UStaticMeshComponent* SpeedFace = AddVisual(nullptr, Cyl, FVector(78.f, -38.f, 34.f), FVector(16.f, 16.f, 1.f), static_cast<int32>(ESUMat::Black), FRotator(80.f, 0.f, 0.f));
	UStaticMeshComponent* FuelFace = AddVisual(nullptr, Cyl, FVector(78.f, -58.f, 32.f), FVector(10.f, 10.f, 1.f), static_cast<int32>(ESUMat::Black), FRotator(80.f, 0.f, 0.f));
	SpeedNeedle = AddVisual(SpeedFace, Cube, FVector(0.f, 0.f, 80.f), FVector(4.f, 80.f, 40.f), static_cast<int32>(ESUMat::RoadLineYellow));
	FuelNeedle = AddVisual(FuelFace, Cube, FVector(0.f, 0.f, 80.f), FVector(4.f, 80.f, 40.f), static_cast<int32>(ESUMat::RoadLineYellow));
	// wipers
	WiperLeft = AddVisual(nullptr, Cube, FVector(104.f, -30.f, 62.f), FVector(3.f, 2.f, 55.f), Dark);
	WiperRight = AddVisual(nullptr, Cube, FVector(104.f, 35.f, 62.f), FVector(3.f, 2.f, 55.f), Dark);
	// fuel cap lid
	FuelCapLid = AddVisual(nullptr, Cube, FVector(-150.f, -93.f, 35.f), FVector(3.f, 15.f, 15.f), BodyMat);

	// dashboard text (always readable at night)
	DashText = NewObject<UTextRenderComponent>(this);
	DashText->SetupAttachment(Visual);
	DashText->SetRelativeLocationAndRotation(FVector(79.f, -48.f, 22.f), FRotator(0.f, 180.f, 0.f));
	DashText->SetWorldSize(3.2f);
	DashText->SetHorizontalAlignment(EHTA_Center);
	DashText->SetTextRenderColor(FColor(150, 255, 190));
	DashText->RegisterComponent();
	RadioText = NewObject<UTextRenderComponent>(this);
	RadioText->SetupAttachment(Visual);
	RadioText->SetRelativeLocationAndRotation(FVector(79.f, 2.f, 14.f), FRotator(0.f, 180.f, 0.f));
	RadioText->SetWorldSize(2.4f);
	RadioText->SetHorizontalAlignment(EHTA_Center);
	RadioText->SetTextRenderColor(FColor(255, 160, 60));
	RadioText->RegisterComponent();

	if (GetNetMode() != NM_DedicatedServer)
	{
		auto MakeAudio = [this](ESUSoundKind Kind, int32 Category, float Gain, float Radius, const FVector& Loc) -> USUProceduralAudioComponent*
		{
			USUProceduralAudioComponent* A = NewObject<USUProceduralAudioComponent>(this);
			A->SetupAttachment(Body);
			A->SetRelativeLocation(Loc);
			A->ConfigureSound(Kind, Category, Gain, Radius);
			A->RegisterComponent();
			A->StartSound();
			return A;
		};
		EngineAudio = MakeAudio(ESUSoundKind::Engine, SUAudioCategory::Effects, 1.f, 6000.f, FVector(170.f, 0.f, 10.f));
		HornAudio = MakeAudio(ESUSoundKind::Horn, SUAudioCategory::Effects, 1.f, 9000.f, FVector(230.f, 0.f, 0.f));
		ClickAudio = MakeAudio(ESUSoundKind::Click, SUAudioCategory::Effects, 0.5f, 600.f, FVector(80.f, -40.f, 25.f));
		RadioAudio = MakeAudio(ESUSoundKind::Music, SUAudioCategory::Radio, 0.9f, 1400.f, FVector(85.f, 0.f, 15.f));
		DispatchAudio = MakeAudio(ESUSoundKind::RadioVoice, SUAudioCategory::Voice, 0.9f, 1200.f, FVector(85.f, 20.f, 30.f));
		ImpactAudio = MakeAudio(ESUSoundKind::Impact, SUAudioCategory::Effects, 1.f, 4000.f, FVector::ZeroVector);
	}
}

// ------------------------------------------------------------------------------------ seats

USceneComponent* ASUVehicle::GetSeatComponent(int32 SeatIndex) const
{
	switch (SeatIndex)
	{
	case 0: return SeatDriver;
	case 1: return SeatPassenger;
	case 2: return SeatRearLeft;
	case 3: return SeatRearRight;
	default: return nullptr;
	}
}

ASUCharacter* ASUVehicle::GetOccupant(int32 SeatIndex) const
{
	return Occupants.IsValidIndex(SeatIndex) ? Occupants[SeatIndex].Get() : nullptr;
}

bool ASUVehicle::HasOccupants() const
{
	for (const TObjectPtr<ASUCharacter>& C : Occupants)
	{
		if (C)
		{
			return true;
		}
	}
	return false;
}

bool ASUVehicle::IsLocalPlayerInside() const
{
	for (const TObjectPtr<ASUCharacter>& C : Occupants)
	{
		if (C && C->IsLocallyControlled())
		{
			return true;
		}
	}
	return false;
}

bool ASUVehicle::TryEnter(ASUCharacter* Char, int32 PreferredSeat)
{
	if (!HasAuthority() || !Char || Char->IsSeated() || Char->IsDead())
	{
		return false;
	}
	if (Char->GetHeldNozzlePump())
	{
		Char->NotifyOwner(SUText(TEXT("Erst die Zapfpistole zurückhängen.")));
		return false;
	}
	int32 Seat = INDEX_NONE;
	if (Occupants.IsValidIndex(PreferredSeat) && !Occupants[PreferredSeat])
	{
		Seat = PreferredSeat;
	}
	else
	{
		for (int32 I = 0; I < NumSeats; ++I)
		{
			if (!Occupants[I])
			{
				Seat = I;
				break;
			}
		}
	}
	if (Seat == INDEX_NONE)
	{
		Char->NotifyOwner(SUText(TEXT("Kein Platz mehr frei.")));
		return false;
	}
	Occupants[Seat] = Char;
	Char->SetSeated(this, Seat);
	if (ClickAudio)
	{
		ClickAudio->TriggerSound(2, 1.f);
	}
	USUEventSubsystem::Post(this, SUEvent::VehicleEntered, Seat == 0 ? FName(TEXT("Driver")) : FName(TEXT("Passenger")), Char);
	if (Seat == 0 && !bEngineRunning)
	{
		Char->NotifyOwner(SUText(TEXT("[F] Motor starten   [L] Licht   [N] Radio   [E] Aussteigen")));
	}
	return true;
}

bool ASUVehicle::FindExitLocation(int32 Seat, FVector& OutLocation) const
{
	const float Side = (Seat % 2 == 0) ? -1.f : 1.f;
	const FVector SeatLocal = SeatPositions[FMath::Clamp(Seat, 0, 3)];
	const FVector Candidates[4] = {
		FVector(SeatLocal.X, Side * 175.f, 20.f),
		FVector(SeatLocal.X, -Side * 175.f, 20.f),
		FVector(-320.f, 0.f, 20.f),
		FVector(330.f, 0.f, 20.f)};
	const FCollisionShape Capsule = FCollisionShape::MakeCapsule(34.f, 92.f);
	FCollisionQueryParams Params(FName(TEXT("SUVehicleExit")), false, this);
	for (const TObjectPtr<ASUCharacter>& C : Occupants)
	{
		if (C)
		{
			Params.AddIgnoredActor(C.Get());
		}
	}
	for (const FVector& Local : Candidates)
	{
		FVector P = GetActorTransform().TransformPosition(Local);
		FHitResult Ground;
		if (GetWorld()->LineTraceSingleByChannel(Ground, P + FVector(0.f, 0.f, 150.f), P - FVector(0.f, 0.f, 400.f), ECC_Visibility, Params))
		{
			P = Ground.Location + FVector(0.f, 0.f, 95.f);
		}
		if (!GetWorld()->OverlapBlockingTestByChannel(P, FQuat::Identity, ECC_Pawn, Capsule, Params))
		{
			OutLocation = P;
			return true;
		}
	}
	return false;
}

void ASUVehicle::RequestExit(ASUCharacter* Char)
{
	if (!HasAuthority() || !Char)
	{
		return;
	}
	if (GetSpeedKmh() > 12.f)
	{
		Char->NotifyOwner(SUText(TEXT("Erst anhalten!")));
		return;
	}
	RemoveOccupant(Char, false);
}

void ASUVehicle::RemoveOccupant(ASUCharacter* Char, bool bForce)
{
	if (!HasAuthority() || !Char)
	{
		return;
	}
	const int32 Seat = Occupants.IndexOfByKey(Char);
	if (Seat == INDEX_NONE)
	{
		return;
	}
	FVector ExitLocation;
	const bool bFound = FindExitLocation(Seat, ExitLocation);
	if (!bFound && !bForce)
	{
		Char->NotifyOwner(SUText(TEXT("Die Tür lässt sich hier nicht öffnen.")));
		return;
	}
	Occupants[Seat] = nullptr;
	Char->SetSeated(nullptr, INDEX_NONE);
	if (bFound)
	{
		Char->SetActorLocation(ExitLocation, false, nullptr, ETeleportType::TeleportPhysics);
	}
	if (Seat == 0)
	{
		SetDriverInput(0.f, 0.f, 0.f, true);
	}
	if (ClickAudio)
	{
		ClickAudio->TriggerSound(2, 1.f);
	}
	USUEventSubsystem::Post(this, SUEvent::VehicleExited, NAME_None, Char);
}

// ------------------------------------------------------------------------------------ interaction

bool ASUVehicle::GetInteractionPrompt(const ASUCharacter* User, const UPrimitiveComponent* Component, FSUInteractionPrompt& OutPrompt) const
{
	if (!Component || !User)
	{
		return false;
	}
	if (Component == TrunkZone)
	{
		OutPrompt.Primary = SUText(TEXT("Kofferraum öffnen"));
		OutPrompt.bPrimary = true;
		return true;
	}
	if (Component == HoodZone)
	{
		OutPrompt.Info = FText::FromString(FString::Printf(TEXT("Motorzustand %d %%"), FMath::RoundToInt(EngineHealth)));
		if (EngineHealth < 99.f && User->GetInventory() && User->GetInventory()->HasItem(SUItem::Toolkit))
		{
			OutPrompt.Secondary = SUText(TEXT("Mit Werkzeug reparieren"));
			OutPrompt.bSecondary = true;
		}
		return true;
	}
	if (Component == FuelZone)
	{
		OutPrompt.Info = FText::FromString(FString::Printf(TEXT("Tank: %.1f / %.0f l"), FuelLiters, GetFuelCapacity()));
		if (InsertedNozzlePump)
		{
			const bool bPumping = InsertedNozzlePump->IsPumping();
			OutPrompt.Primary = bPumping ? SUText(TEXT("Tanken stoppen")) : SUText(TEXT("Tanken starten"));
			OutPrompt.bPrimary = true;
			if (!bPumping)
			{
				OutPrompt.Secondary = SUText(TEXT("Zapfpistole herausnehmen"));
				OutPrompt.bSecondary = true;
			}
		}
		else if (!bFuelCapOpen)
		{
			OutPrompt.Primary = SUText(TEXT("Tankdeckel öffnen"));
			OutPrompt.bPrimary = true;
		}
		else if (User->GetHeldNozzlePump())
		{
			OutPrompt.Primary = SUText(TEXT("Zapfpistole einsetzen"));
			OutPrompt.bPrimary = true;
		}
		else
		{
			OutPrompt.Primary = SUText(TEXT("Tankdeckel schließen"));
			OutPrompt.bPrimary = true;
		}
		return true;
	}
	for (int32 I = 0; I < DoorZones.Num(); ++I)
	{
		if (Component == DoorZones[I])
		{
			static const TCHAR* Names[4] = {TEXT("Einsteigen (Fahrer)"), TEXT("Einsteigen (Beifahrer)"), TEXT("Einsteigen (hinten links)"), TEXT("Einsteigen (hinten rechts)")};
			OutPrompt.Primary = SUText(Names[I]);
			OutPrompt.bPrimary = !Occupants[I] && !User->GetHeldNozzlePump();
			if (Occupants[I])
			{
				OutPrompt.Info = SUText(TEXT("Platz besetzt"));
			}
			return true;
		}
	}
	return false;
}

void ASUVehicle::Interact(ASUCharacter* User, UPrimitiveComponent* Component, ESUInteractOption Option)
{
	if (!User)
	{
		return;
	}
	if (Component == TrunkZone)
	{
		if (ASUPlayerController* PC = Cast<ASUPlayerController>(User->GetController()))
		{
			PC->ClientOpenScreen(ESUScreen::Trunk, this);
		}
		return;
	}
	if (Component == HoodZone)
	{
		if (Option == ESUInteractOption::Secondary && User->GetInventory()->HasItem(SUItem::Toolkit))
		{
			if (EngineHealth >= 99.f)
			{
				User->NotifyOwner(SUText(TEXT("Am Motor ist nichts zu reparieren.")));
			}
			else
			{
				const float Before = EngineHealth;
				// field repairs only get the engine running reliably again, a workshop does the rest
				EngineHealth = FMath::Max(EngineHealth, FMath::Min(75.f, EngineHealth + 35.f));
				User->NotifyOwner(FText::FromString(FString::Printf(TEXT("Notdürftig repariert: %d %% → %d %%"), FMath::RoundToInt(Before), FMath::RoundToInt(EngineHealth))));
			}
		}
		return;
	}
	if (Component == FuelZone)
	{
		if (InsertedNozzlePump)
		{
			if (Option == ESUInteractOption::Primary)
			{
				InsertedNozzlePump->SetPumping(!InsertedNozzlePump->IsPumping(), User);
			}
			else if (!InsertedNozzlePump->IsPumping())
			{
				InsertedNozzlePump->TakeNozzleFromVehicle(User);
			}
		}
		else if (!bFuelCapOpen)
		{
			SetFuelCapOpen(true);
		}
		else if (ASUFuelPump* Pump = User->GetHeldNozzlePump())
		{
			Pump->InsertNozzle(this, User);
		}
		else
		{
			SetFuelCapOpen(false);
		}
		return;
	}
	for (int32 I = 0; I < DoorZones.Num(); ++I)
	{
		if (Component == DoorZones[I])
		{
			TryEnter(User, I);
			return;
		}
	}
}

bool ASUVehicle::TransferTrunkItem(ASUCharacter* Char, FName ItemId, bool bToTrunk)
{
	if (!HasAuthority() || !Char || FVector::Dist(Char->GetActorLocation(), GetActorLocation()) > 500.f)
	{
		return false;
	}
	USUInventoryComponent* From = bToTrunk ? Char->GetInventory() : Trunk.Get();
	USUInventoryComponent* To = bToTrunk ? Trunk.Get() : Char->GetInventory();
	if (!From || !To || !From->HasItem(ItemId) || !To->CanAdd(ItemId, 1))
	{
		return false;
	}
	const float Charge = From->GetCharge(ItemId);
	if (From->RemoveItem(ItemId, 1) == 1)
	{
		To->AddItem(ItemId, 1, Charge);
		if (!bToTrunk)
		{
			USUEventSubsystem::Post(this, SUEvent::ItemPicked, ItemId, Char);
		}
		return true;
	}
	return false;
}

// ------------------------------------------------------------------------------------ commands

void ASUVehicle::SetDriverInput(float InThrottle, float InBrake, float InSteer, bool bInHandbrake)
{
	Throttle = InThrottle;
	BrakeInput = InBrake;
	SteerInput = InSteer;
	bHandbrake = bInHandbrake;
	if (HasAuthority())
	{
		RepThrottle = InThrottle;
		RepBrake = InBrake;
		RepSteer = InSteer;
		bRepHandbrake = bInHandbrake;
	}
}

void ASUVehicle::HandleCommand(ASUCharacter* By, ESUVehicleCommand Command, float Value)
{
	if (!HasAuthority() || !By)
	{
		return;
	}
	const bool bDriver = GetOccupant(0) == By;
	const bool bInside = Occupants.Contains(By);
	if (!bInside)
	{
		return;
	}
	auto Click = [this]()
	{
		if (ClickAudio)
		{
			ClickAudio->TriggerSound(2, 0.8f);
		}
	};
	switch (Command)
	{
	case ESUVehicleCommand::ToggleEngine:
		if (bDriver)
		{
			if (bEngineRunning)
			{
				StopEngine(false);
			}
			else
			{
				StartEngineAttempt(By);
			}
		}
		break;
	case ESUVehicleCommand::ToggleLights:
		if (bDriver) { LightsMode = LightsMode == 0 ? 1 : 0; Click(); }
		break;
	case ESUVehicleCommand::ToggleHighBeam:
		if (bDriver) { LightsMode = LightsMode == 2 ? 1 : 2; Click(); }
		break;
	case ESUVehicleCommand::HornOn:
		if (bDriver) { bHorn = true; USUEventSubsystem::Noise(this, GetActorLocation(), 1.f, this); }
		break;
	case ESUVehicleCommand::HornOff:
		bHorn = false;
		break;
	case ESUVehicleCommand::BlinkLeft:
		if (bDriver) { BlinkerMode = BlinkerMode == 1 ? 0 : 1; }
		break;
	case ESUVehicleCommand::BlinkRight:
		if (bDriver) { BlinkerMode = BlinkerMode == 2 ? 0 : 2; }
		break;
	case ESUVehicleCommand::Hazard:
		if (bDriver) { BlinkerMode = BlinkerMode == 3 ? 0 : 3; }
		break;
	case ESUVehicleCommand::Wipers:
		if (bDriver) { bWipers = !bWipers; Click(); }
		break;
	case ESUVehicleCommand::RadioPower:
		bRadioOn = !bRadioOn;
		Click();
		break;
	case ESUVehicleCommand::RadioNext:
		RadioStation = static_cast<uint8>((RadioStation + 1) % 3);
		bRadioOn = true;
		Click();
		break;
	case ESUVehicleCommand::RadioPrev:
		RadioStation = static_cast<uint8>((RadioStation + 2) % 3);
		bRadioOn = true;
		Click();
		break;
	case ESUVehicleCommand::RadioVolume:
		RadioVolume = FMath::Clamp(RadioVolume + Value, 0.f, 1.f);
		break;
	default:
		break;
	}
}

void ASUVehicle::StartEngineAttempt(ASUCharacter* By)
{
	if (CrankTimer >= 0.f)
	{
		return;
	}
	bool bSuccess = true;
	FText Reason;
	if (FuelLiters <= 0.05f)
	{
		bSuccess = false;
		Reason = SUText(TEXT("Der Motor orgelt … der Tank ist leer."));
	}
	else if (EngineHealth <= 5.f)
	{
		bSuccess = false;
		Reason = SUText(TEXT("Der Motor springt nicht an – zu stark beschädigt."));
	}
	else if (StartBlockCount > 0)
	{
		--StartBlockCount;
		bSuccess = false;
		Reason = SUText(TEXT("Der Motor will nicht anspringen …"));
	}
	else if (EngineHealth < 35.f && FMath::FRand() < 0.4f)
	{
		bSuccess = false;
		Reason = SUText(TEXT("Der Motor stottert. Nochmal versuchen."));
	}
	bCrankSuccess = bSuccess;
	CrankTimer = bSuccess ? 0.8f : 1.4f;
	MulticastCrank(bSuccess);
	if (!bSuccess && By)
	{
		By->NotifyOwner(Reason, FLinearColor(1.f, 0.7f, 0.4f));
	}
}

void ASUVehicle::StopEngine(bool bStall)
{
	if (!bEngineRunning)
	{
		return;
	}
	bEngineRunning = false;
	if (bStall && EngineAudio)
	{
		EngineAudio->TriggerSound(2, 1.f);
	}
}

void ASUVehicle::MulticastCrank_Implementation(bool bSuccess)
{
	if (EngineAudio)
	{
		EngineAudio->TriggerSound(bSuccess ? 0 : 1, 1.f);
	}
}

void ASUVehicle::MulticastImpact_Implementation(float Strength)
{
	if (ImpactAudio)
	{
		ImpactAudio->TriggerSound(1, Strength);
		ImpactAudio->TriggerSound(3, Strength);
	}
}

// ------------------------------------------------------------------------------------ queries

float ASUVehicle::GetSpeedKmh() const
{
	return GetVelocity().Size() * 0.036f;
}

float ASUVehicle::GetForwardSpeedKmh() const
{
	return FVector::DotProduct(GetVelocity(), GetActorForwardVector()) * 0.036f;
}

float ASUVehicle::GetFuelCapacity() const
{
	return FuelSpec.CapacityLiters;
}

FString ASUVehicle::GetGearText() const
{
	if (!bEngineRunning)
	{
		return TEXT("P");
	}
	const float Fwd = GetForwardSpeedKmh();
	if (Fwd < -1.f)
	{
		return TEXT("R");
	}
	if (Fwd > 1.f || Throttle > 0.05f)
	{
		return TEXT("D");
	}
	return TEXT("N");
}

FText ASUVehicle::GetRadioStationName() const
{
	switch (RadioStation)
	{
	case 0: return SUText(TEXT("Pine FM 98.4 – Folk & Country"));
	case 1: return SUText(TEXT("KNTR 102.5 – Nachtwelle"));
	default: return SUText(TEXT("Radio Nordwald 91.1 – Nachrichten"));
	}
}

bool ASUVehicle::IsRadioIntrusionActive() const
{
	const AGameStateBase* GS = GetWorld()->GetGameState();
	return GS && (GS->GetServerWorldTimeSeconds() - RadioIntrusionStart) < 6.0;
}

bool ASUVehicle::GetRadioSubtitle(FSUSpeechLine& OutLine) const
{
	if (IsRadioIntrusionActive())
	{
		const TArray<FSUSpeechLine>& Strange = FSUMissionDatabase::StrangeTransmissions();
		if (Strange.IsValidIndex(RadioIntrusionLine))
		{
			OutLine = Strange[RadioIntrusionLine];
			return true;
		}
	}
	if (!bRadioOn || RadioStation != 2)
	{
		return false;
	}
	const ASUGameState* GS = ASUGameState::Get(this);
	const int32 Story = GS ? GS->GetStoryStage() : 0;
	TArray<FString> Lines;
	for (const FNewsLine& N : NewsLines)
	{
		if (Story >= N.MinStory)
		{
			Lines.Add(N.Text);
		}
	}
	if (GS)
	{
		Lines.Add(FString(TEXT("Das Wetter: ")) + GS->DescribeWeather(false).ToString());
	}
	const double T = GS ? GS->GetServerWorldTimeSeconds() : 0.0;
	const int32 Index = static_cast<int32>(T / 9.0) % FMath::Max(1, Lines.Num());
	OutLine.Speaker = SUText(TEXT("Radio Nordwald"));
	OutLine.Text = FText::FromString(Lines[Index]);
	OutLine.Duration = 9.f;
	return true;
}

FVector ASUVehicle::GetFuelCapLocation() const
{
	return GetActorTransform().TransformPosition(FVector(-150.f, -96.f, 35.f));
}

// ------------------------------------------------------------------------------------ fuel / damage

void ASUVehicle::SetFuelCapOpen(bool bOpen)
{
	if (HasAuthority() && !InsertedNozzlePump)
	{
		bFuelCapOpen = bOpen;
	}
}

float ASUVehicle::AddFuel(float Liters)
{
	if (!HasAuthority() || Liters <= 0.f)
	{
		return 0.f;
	}
	const float Added = FMath::Min(Liters, FuelSpec.CapacityLiters - FuelLiters);
	FuelLiters += Added;
	return Added;
}

void ASUVehicle::TriggerRadioIntrusion(int32 LineIndex)
{
	if (!HasAuthority())
	{
		return;
	}
	RadioIntrusionLine = LineIndex;
	RadioIntrusionStart = GetWorld()->GetGameState()->GetServerWorldTimeSeconds();
	bRadioOn = true;
}

void ASUVehicle::ApplyAttack(float Damage, const FVector& FromLocation)
{
	if (!HasAuthority())
	{
		return;
	}
	EngineHealth = FMath::Max(0.f, EngineHealth - Damage);
	MulticastImpact(1.2f);
	const FVector Push = (GetActorLocation() - FromLocation).GetSafeNormal2D() * MassKg * 220.f;
	Body->AddImpulseAtLocation(Push, GetActorLocation() + FVector(0.f, 0.f, 40.f));
	for (const TObjectPtr<ASUCharacter>& C : Occupants)
	{
		if (C)
		{
			C->ApplyDamageSU(Damage * 0.15f, this);
		}
	}
	if (EngineHealth <= 5.f)
	{
		StopEngine(true);
		NotifyOccupants(SUText(TEXT("Der Motor ist ausgefallen!")), FLinearColor(1.f, 0.3f, 0.3f));
	}
}

void ASUVehicle::RepairBy(float Amount)
{
	if (HasAuthority())
	{
		EngineHealth = FMath::Clamp(EngineHealth + Amount, 0.f, 100.f);
	}
}

void ASUVehicle::HandleBodyHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (!HasAuthority() || ImpactCooldown > 0.f)
	{
		return;
	}
	const float DeltaV = NormalImpulse.Size() / MassKg; // cm/s
	if (DeltaV < 450.f)
	{
		return;
	}
	ImpactCooldown = 0.4f;
	const float Damage = (DeltaV - 450.f) / 35.f;
	EngineHealth = FMath::Max(0.f, EngineHealth - Damage);
	MulticastImpact(FMath::Clamp(DeltaV / 1500.f, 0.3f, 1.5f));
	if (DeltaV > 1300.f)
	{
		for (const TObjectPtr<ASUCharacter>& C : Occupants)
		{
			if (C)
			{
				C->ApplyDamageSU((DeltaV - 1300.f) / 25.f, this);
			}
		}
	}
	if (EngineHealth <= 5.f && bEngineRunning)
	{
		StopEngine(true);
		NotifyOccupants(SUText(TEXT("Der Motor ist abgestorben – Totalschaden?")), FLinearColor(1.f, 0.3f, 0.3f));
	}
	else if (EngineHealth < LastHealthWarning - 20.f)
	{
		LastHealthWarning = EngineHealth;
		NotifyOccupants(FText::FromString(FString::Printf(TEXT("Schaden am Wagen – Motorzustand %d %%"), FMath::RoundToInt(EngineHealth))), FLinearColor(1.f, 0.6f, 0.3f));
	}
}

void ASUVehicle::NotifyOccupants(const FText& Text, FLinearColor Color) const
{
	for (const TObjectPtr<ASUCharacter>& C : Occupants)
	{
		if (C)
		{
			C->NotifyOwner(Text, Color);
		}
	}
}

float ASUVehicle::SurfaceGrip(const FHitResult& Hit) const
{
	float Mu = 0.62f; // grass / forest floor
	if (const UPrimitiveComponent* C = Hit.GetComponent())
	{
		if (C->ComponentHasTag(SUSurfaceTag::Asphalt)) { Mu = 1.0f; }
		else if (C->ComponentHasTag(SUSurfaceTag::Concrete)) { Mu = 0.95f; }
		else if (C->ComponentHasTag(SUSurfaceTag::Gravel)) { Mu = 0.74f; }
		else if (C->ComponentHasTag(SUSurfaceTag::Dirt)) { Mu = 0.66f; }
		else if (C->ComponentHasTag(SUSurfaceTag::Wood)) { Mu = 0.8f; }
	}
	if (const ASUGameState* GS = ASUGameState::Get(this))
	{
		Mu *= 1.f - 0.28f * GS->GetRain();
	}
	return Mu;
}

// ------------------------------------------------------------------------------------ simulation

void ASUVehicle::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Dt = FMath::Clamp(DeltaSeconds, 1.f / 240.f, 1.f / 20.f);

	// remote observers use the replicated driver input
	const ASUCharacter* Driver = GetOccupant(0);
	const bool bLocalDriver = Driver && Driver->IsLocallyControlled();
	if (!HasAuthority() && !bLocalDriver)
	{
		Throttle = RepThrottle;
		BrakeInput = RepBrake;
		SteerInput = RepSteer;
		bHandbrake = bRepHandbrake;
	}
	if (!Driver)
	{
		Throttle = 0.f;
		BrakeInput = 0.f;
		SteerInput = 0.f;
		bHandbrake = true;
	}

	SimulatePhysics(Dt);
	if (HasAuthority())
	{
		ServerUpdate(Dt);
	}
	if (GetNetMode() != NM_DedicatedServer)
	{
		UpdateVisuals(Dt);
		UpdateLights(Dt);
		UpdateAudio(Dt);
		UpdateMirror();
	}
}

void ASUVehicle::SimulatePhysics(float Dt)
{
	if (!Body || !Body->IsSimulatingPhysics())
	{
		return;
	}
	const FTransform& T = Body->GetComponentTransform();
	const FVector Up = T.GetUnitAxis(EAxis::Z);
	const FVector Fwd = T.GetUnitAxis(EAxis::X);
	const float ForwardSpeedMps = FVector::DotProduct(Body->GetPhysicsLinearVelocity(), Fwd) / 100.f;

	// steering (smoothed, less lock at speed)
	const float SpeedFactor = FMath::Clamp(FMath::Abs(ForwardSpeedMps) / 30.f, 0.f, 1.f);
	const float MaxSteer = FMath::Lerp(MaxSteerDeg, 11.f, SpeedFactor);
	SteerAngle = FMath::FInterpTo(SteerAngle, SteerInput * MaxSteer, Dt, 6.f);

	// throttle / brake / reverse logic (automatic gearbox)
	float DriveThrottle = 0.f;
	float Brake = 0.f;
	if (bEngineRunning)
	{
		if (Throttle > 0.02f)
		{
			if (ForwardSpeedMps < -1.f)
			{
				Brake = Throttle; // moving backwards: W brakes first
			}
			else
			{
				DriveThrottle = Throttle;
			}
		}
		if (BrakeInput > 0.02f)
		{
			if (ForwardSpeedMps > 1.f)
			{
				Brake = FMath::Max(Brake, BrakeInput);
			}
			else
			{
				DriveThrottle = -BrakeInput; // reverse
			}
		}
	}
	else
	{
		Brake = FMath::Max(Throttle, BrakeInput) * 0.6f;
	}
	if (DriveThrottle == 0.f && Brake < 0.05f && FMath::Abs(ForwardSpeedMps) < 1.2f)
	{
		Brake = 0.7f; // hold still on slopes
	}
	const float Health = EngineHealth / 100.f;
	const float DriveForce = static_cast<float>(SULogic::EngineDriveForce(EngineSpec, DriveThrottle, ForwardSpeedMps, Health));

	FCollisionQueryParams Params(FName(TEXT("SUWheel")), false, this);
	for (const TObjectPtr<ASUCharacter>& C : Occupants)
	{
		if (C)
		{
			Params.AddIgnoredActor(C.Get());
		}
	}
	if (InsertedNozzlePump)
	{
		Params.AddIgnoredActor(InsertedNozzlePump.Get());
	}

	float Compressions[4] = {0.f, 0.f, 0.f, 0.f};
	FHitResult Hits[4];
	bool bHit[4] = {false, false, false, false};
	for (int32 I = 0; I < 4; ++I)
	{
		const FVector Mount = T.TransformPosition(Wheels[I].LocalMount);
		const FVector End = Mount - Up * (RestLength + WheelRadius);
		bHit[I] = GetWorld()->LineTraceSingleByChannel(Hits[I], Mount, End, ECC_Visibility, Params);
		if (bHit[I])
		{
			const float LengthCm = FMath::Max(0.f, Hits[I].Distance - WheelRadius);
			Compressions[I] = FMath::Clamp((RestLength - LengthCm) / 100.f, 0.f, RestLength / 100.f);
		}
		else
		{
			Compressions[I] = 0.f;
		}
	}

	for (int32 I = 0; I < 4; ++I)
	{
		FWheelState& W = Wheels[I];
		W.bGrounded = bHit[I];
		const float Comp = Compressions[I];
		const float Rate = (Comp - W.LastCompression) / Dt;
		W.LastCompression = Comp;
		W.Compression = Comp;
		W.VisualOffset = bHit[I] ? FMath::Max(0.f, Hits[I].Distance - WheelRadius) : RestLength;
		if (!bHit[I])
		{
			continue;
		}
		// anti-roll: share load with the wheel on the other side of the axle
		const int32 Other = (I % 2 == 0) ? I + 1 : I - 1;
		const float AntiRoll = (Comp - Compressions[Other]) * AntiRollN;
		const float Normal = static_cast<float>(SULogic::SuspensionForce(Comp, Rate, SpringRate, DamperRate)) + AntiRoll;
		const FVector Mount = T.TransformPosition(W.LocalMount);
		Body->AddForceAtLocation(Up * FMath::Max(0.f, Normal) * 100.f, Mount);

		// tyre forces
		FVector WheelFwd = Fwd;
		if (W.bFront)
		{
			WheelFwd = FRotator(0.f, SteerAngle, 0.f).RotateVector(FVector::ForwardVector);
			WheelFwd = T.TransformVectorNoScale(WheelFwd);
		}
		const FVector N = Hits[I].ImpactNormal;
		WheelFwd = (WheelFwd - N * FVector::DotProduct(WheelFwd, N)).GetSafeNormal();
		const FVector WheelRight = FVector::CrossProduct(N, WheelFwd).GetSafeNormal();
		const FVector Contact = Hits[I].ImpactPoint;
		const FVector V = Body->GetPhysicsLinearVelocityAtPoint(Contact) / 100.f;

		SULogic::WheelInput In;
		In.NormalLoadN = FMath::Max(0.f, Normal);
		In.LongVelMps = FVector::DotProduct(V, WheelFwd);
		In.LatVelMps = FVector::DotProduct(V, WheelRight);
		In.DriveForceN = DriveForce * (W.bFront ? 0.2f : 0.3f); // 4x4: 40 % front, 60 % rear
		In.BrakeForceN = Brake * BrakeForceN * (W.bFront ? 0.6f : 0.4f);
		In.Mu = SurfaceGrip(Hits[I]);
		In.MassShareKg = MassKg / 4.f;
		In.Dt = Dt;
		In.LateralGrip = 0.85f;
		if (bHandbrake && !W.bFront)
		{
			In.BrakeForceN = FMath::Max(In.BrakeForceN, HandbrakeForceN);
			In.LateralGrip = 0.35f;
		}
		const SULogic::WheelOutput Out = SULogic::ComputeWheelForces(In);
		const FVector TyreForce = (WheelFwd * static_cast<float>(Out.LongForceN) + WheelRight * static_cast<float>(Out.LatForceN)) * 100.f;
		// apply slightly above the contact point to keep the old wagon from rolling over too easily
		Body->AddForceAtLocation(TyreForce, Contact + Up * 28.f);

		W.SpinAngle += FMath::RadiansToDegrees(static_cast<float>(In.LongVelMps) / (WheelRadius / 100.f)) * Dt;
	}

	// aerodynamic drag and rolling resistance
	const float Resist = static_cast<float>(SULogic::ResistanceForce(ForwardSpeedMps, MassKg));
	Body->AddForce(Fwd * Resist * 100.f);

	// rpm for sound / gauges (simple automatic gearbox)
	const float Kmh = FMath::Abs(ForwardSpeedMps) * 3.6f;
	static const float GearTops[] = {18.f, 38.f, 62.f, 95.f, 160.f};
	float GearMin = 0.f;
	float GearMax = GearTops[0];
	for (int32 G = 0; G < 5; ++G)
	{
		GearMax = GearTops[G];
		if (Kmh < GearTops[G])
		{
			break;
		}
		GearMin = GearTops[G];
	}
	const float TargetRpm = bEngineRunning ? 800.f + FMath::Clamp((Kmh - GearMin) / FMath::Max(1.f, GearMax - GearMin), 0.f, 1.f) * 3000.f + FMath::Abs(DriveThrottle) * 900.f : 0.f;
	Rpm = FMath::FInterpTo(Rpm, TargetRpm, Dt, 4.f);
}

void ASUVehicle::ServerUpdate(float Dt)
{
	ImpactCooldown = FMath::Max(0.f, ImpactCooldown - Dt);

	if (CrankTimer >= 0.f)
	{
		CrankTimer -= Dt;
		if (CrankTimer < 0.f)
		{
			CrankTimer = -1.f;
			if (bCrankSuccess)
			{
				bEngineRunning = true;
				USUEventSubsystem::Post(this, SUEvent::EngineStarted, NAME_None, GetOccupant(0));
			}
		}
	}

	if (bEngineRunning)
	{
		const float SpeedMps = GetSpeedKmh() / 3.6f;
		FuelLiters = FMath::Max(0.f, FuelLiters - static_cast<float>(SULogic::FuelFlowLitersPerSecond(FuelSpec, true, FMath::Max(Throttle, BrakeInput), SpeedMps)) * Dt);
		if (FuelLiters <= 0.f)
		{
			StopEngine(true);
			NotifyOccupants(SUText(TEXT("Der Motor geht aus – der Tank ist leer!")), FLinearColor(1.f, 0.4f, 0.3f));
		}
		NoiseTimer -= Dt;
		if (NoiseTimer <= 0.f)
		{
			NoiseTimer = 0.5f;
			USUEventSubsystem::Noise(this, GetActorLocation(), 0.35f + 0.35f * FMath::Clamp(Rpm / 4500.f, 0.f, 1.f), this);
		}
		if (FuelLiters < 6.f && FuelLiters + 0.01f >= 6.f - Dt * 0.05f)
		{
			NotifyOccupants(SUText(TEXT("Tankanzeige im roten Bereich.")), FLinearColor(1.f, 0.7f, 0.3f));
		}
	}
	// nozzle still in the tank when driving off: the hose snaps back to the pump
	if (InsertedNozzlePump && GetSpeedKmh() > 4.f)
	{
		InsertedNozzlePump->ForceReturnNozzle(true);
	}
}

void ASUVehicle::UpdateVisuals(float Dt)
{
	for (int32 I = 0; I < WheelMeshes.Num() && I < 4; ++I)
	{
		if (UStaticMeshComponent* W = WheelMeshes[I])
		{
			const FWheelState& S = Wheels[I];
			const FVector Loc = S.LocalMount - FVector(0.f, 0.f, S.VisualOffset);
			const float Steer = S.bFront ? SteerAngle : 0.f;
			// cylinder axis along Y (roll 90), spin around that axis, then steer around Z
			const FQuat Q = FQuat(FRotator(0.f, Steer, 0.f)) * FQuat(FRotator(-S.SpinAngle, 0.f, 0.f)) * FQuat(FRotator(0.f, 0.f, 90.f));
			W->SetRelativeLocationAndRotation(Loc, Q);
		}
	}
	if (SteeringWheel)
	{
		const FQuat Base(FRotator(70.f, 0.f, 0.f));
		SteeringWheel->SetRelativeRotation(Base * FQuat(FVector::UpVector, FMath::DegreesToRadians(-SteerAngle * 3.f)));
	}
	if (SpeedNeedle)
	{
		SpeedNeedle->SetRelativeRotation(FRotator(0.f, -120.f + FMath::Clamp(GetSpeedKmh() / 160.f, 0.f, 1.f) * 240.f, 0.f));
	}
	if (FuelNeedle)
	{
		FuelNeedle->SetRelativeRotation(FRotator(0.f, -60.f + FMath::Clamp(FuelLiters / GetFuelCapacity(), 0.f, 1.f) * 120.f, 0.f));
	}
	if (bWipers)
	{
		WiperPhase += Dt * 2.4f;
	}
	else
	{
		WiperPhase = FMath::FInterpTo(WiperPhase, FMath::RoundToFloat(WiperPhase / UE_TWO_PI) * UE_TWO_PI, Dt, 4.f);
	}
	const float WiperAngle = (FMath::Sin(WiperPhase - UE_HALF_PI) * 0.5f + 0.5f) * 95.f;
	if (WiperLeft)
	{
		WiperLeft->SetRelativeRotation(FRotator(-30.f, 0.f, -WiperAngle));
	}
	if (WiperRight)
	{
		WiperRight->SetRelativeRotation(FRotator(-30.f, 0.f, -WiperAngle));
	}
	if (FuelCapLid)
	{
		FuelCapLid->SetRelativeRotation(FRotator(0.f, bFuelCapOpen ? -80.f : 0.f, 0.f));
	}
	if (DashText)
	{
		FString Warn;
		if (FuelLiters < 6.f) { Warn += TEXT(" ⛽"); }
		if (EngineHealth < 40.f) { Warn += TEXT(" ⚠"); }
		DashText->SetText(FText::FromString(FString::Printf(TEXT("%3d km/h  %s%s"), FMath::RoundToInt(GetSpeedKmh()), *GetGearText(), *Warn)));
		DashText->SetVisibility(bEngineRunning || LightsMode > 0);
	}
	if (RadioText)
	{
		RadioText->SetText(bRadioOn ? FText::FromString(FString::Printf(TEXT("%s  %d%%"), RadioStation == 0 ? TEXT("98.4") : (RadioStation == 1 ? TEXT("102.5") : TEXT("91.1")), FMath::RoundToInt(RadioVolume * 100.f))) : FText::GetEmpty());
	}
}

void ASUVehicle::UpdateLights(float Dt)
{
	const bool bLow = LightsMode >= 1;
	const bool bHigh = LightsMode == 2;
	for (USpotLightComponent* H : {HeadLeft.Get(), HeadRight.Get()})
	{
		if (!H)
		{
			continue;
		}
		H->SetVisibility(bLow);
		H->SetIntensity(bHigh ? 9000.f : 3500.f);
		H->SetAttenuationRadius(bHigh ? 9500.f : 4500.f);
		H->SetOuterConeAngle(bHigh ? 28.f : 38.f);
		H->SetRelativeRotation(FRotator(bHigh ? -0.8f : -3.5f, 0.f, 0.f));
	}
	const bool bBraking = (BrakeInput > 0.05f && GetForwardSpeedKmh() > 2.f) || (Throttle > 0.05f && GetForwardSpeedKmh() < -2.f);
	for (UPointLightComponent* L : {TailLeft.Get(), TailRight.Get()})
	{
		if (L)
		{
			L->SetVisibility(bLow || bBraking);
			L->SetIntensity(bBraking ? 14.f : 3.f);
		}
	}
	if (ReverseLight)
	{
		ReverseLight->SetVisibility(bEngineRunning && GetGearText() == TEXT("R"));
	}
	// indicators with relay click
	BlinkTimer += Dt;
	if (BlinkTimer > 0.36f)
	{
		BlinkTimer = 0.f;
		bBlinkOn = !bBlinkOn;
		if (BlinkerMode != 0 && ClickAudio && IsLocalPlayerInside())
		{
			ClickAudio->TriggerSound(bBlinkOn ? 0 : 1, 1.f);
		}
	}
	const bool bLeft = BlinkerMode == 1 || BlinkerMode == 3;
	const bool bRight = BlinkerMode == 2 || BlinkerMode == 3;
	if (BlinkFL) { BlinkFL->SetVisibility(bLeft && bBlinkOn); }
	if (BlinkRL) { BlinkRL->SetVisibility(bLeft && bBlinkOn); }
	if (BlinkFR) { BlinkFR->SetVisibility(bRight && bBlinkOn); }
	if (BlinkRR) { BlinkRR->SetVisibility(bRight && bBlinkOn); }
	if (DashLight)
	{
		DashLight->SetVisibility(bEngineRunning || bLow);
	}
	if (DomeLight)
	{
		DomeLight->SetVisibility(!bEngineRunning && HasOccupants());
	}
}

void ASUVehicle::UpdateAudio(float Dt)
{
	const bool bInside = IsLocalPlayerInside();
	if (EngineAudio)
	{
		EngineAudio->SetSoundParam(SULogic::Dsp::Param::A, Rpm);
		EngineAudio->SetSoundParam(SULogic::Dsp::Param::B, FMath::Max(Throttle, 0.15f));
		EngineAudio->SetSoundParam(SULogic::Dsp::Param::C, bEngineRunning ? 1.f : 0.f);
		EngineAudio->SetInterior(bInside ? 0.6f : 0.f);
	}
	if (HornAudio)
	{
		HornAudio->SetSoundParam(SULogic::Dsp::Param::A, bHorn ? 1.f : 0.f);
	}
	if (RadioAudio)
	{
		const bool bIntrusion = IsRadioIntrusionActive();
		RadioAudio->SetSoundParam(SULogic::Dsp::Param::A, static_cast<float>(RadioStation));
		RadioAudio->SetSoundParam(SULogic::Dsp::Param::B, bRadioOn ? 1.f : 0.f);
		RadioAudio->SetSoundParam(SULogic::Dsp::Param::C, 0.85f);
		RadioAudio->SetSoundParam(SULogic::Dsp::Param::D, bIntrusion ? 1.f : 0.f);
		RadioAudio->SetBaseGain(0.2f + RadioVolume);
	}
	if (DispatchAudio)
	{
		// the car has a CB radio on the Zentrale channel
		bool bSpeaking = false;
		float Pitch = 1.f;
		if (const ASUMissionManager* MM = ASUMissionManager::Get(this))
		{
			FSUSpeechLine Line;
			float LineTime = 0.f;
			if (MM->GetTransmissionLine(Line, LineTime) && MM->GetTransmission().Kind != ESUTransmissionKind::Chatter)
			{
				bSpeaking = LineTime > 0.25f && LineTime < Line.GetDuration() - 0.15f;
				Pitch = ASURadioStation::SpeakerPitch(Line.Speaker);
			}
		}
		DispatchAudio->SetSoundParam(SULogic::Dsp::Param::A, bSpeaking ? 1.f : 0.f);
		DispatchAudio->SetSoundParam(SULogic::Dsp::Param::B, Pitch);
		DispatchAudio->SetSoundParam(SULogic::Dsp::Param::C, 0.8f);
		DispatchAudio->SetSoundParam(SULogic::Dsp::Param::D, 0.15f);
	}
}

void ASUVehicle::UpdateMirror()
{
	const USUGameUserSettings* S = USUGameUserSettings::Get();
	const bool bWant = IsLocalPlayerInside() && (!S || S->bRearMirror);
	if (bWant && !MirrorCapture)
	{
		MirrorTexture = UKismetRenderingLibrary::CreateRenderTarget2D(this, 512, 160, RTF_RGBA8, FLinearColor::Black);
		MirrorCapture = NewObject<USceneCaptureComponent2D>(this);
		MirrorCapture->SetupAttachment(Body);
		MirrorCapture->SetRelativeLocationAndRotation(FVector(-180.f, 0.f, 95.f), FRotator(-4.f, 180.f, 0.f));
		MirrorCapture->FOVAngle = 55.f;
		MirrorCapture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
		MirrorCapture->bAlwaysPersistRenderingState = true;
		MirrorCapture->TextureTarget = MirrorTexture;
		MirrorCapture->bCaptureEveryFrame = true;
		MirrorCapture->RegisterComponent();
	}
	if (MirrorCapture)
	{
		MirrorCapture->bCaptureEveryFrame = bWant;
	}
}

// ------------------------------------------------------------------------------------ save

FSUVehicleSave ASUVehicle::GetSaveData() const
{
	FSUVehicleSave Data;
	Data.bValid = true;
	Data.Transform = GetActorTransform();
	Data.FuelLiters = FuelLiters;
	Data.EngineHealth = EngineHealth;
	Data.Trunk = Trunk->GetSlots();
	return Data;
}

void ASUVehicle::ApplySaveData(const FSUVehicleSave& Data)
{
	if (!HasAuthority())
	{
		return;
	}
	for (int32 I = 0; I < NumSeats; ++I)
	{
		if (Occupants[I])
		{
			RemoveOccupant(Occupants[I], true);
		}
	}
	FuelLiters = FMath::Clamp(Data.FuelLiters, 0.f, FuelSpec.CapacityLiters);
	EngineHealth = FMath::Clamp(Data.EngineHealth, 0.f, 100.f);
	Trunk->SetSlots(Data.Trunk);
	bEngineRunning = false;
	const FVector Loc = Data.Transform.GetLocation() + FVector(0.f, 0.f, 60.f);
	Body->SetPhysicsLinearVelocity(FVector::ZeroVector);
	Body->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	SetActorLocationAndRotation(Loc, FRotator(0.f, Data.Transform.Rotator().Yaw, 0.f), false, nullptr, ETeleportType::TeleportPhysics);
}
