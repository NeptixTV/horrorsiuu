#include "World/Sites/SUSiteM02.h"
#include "Core/SUTypes.h"
#include "Core/SUItemDatabase.h"
#include "Interaction/SUDocument.h"

void ASUSiteM02::BuildSite(ASUWorldBuilder* InBuilder, bool bInServer)
{
	Super::BuildSite(InBuilder, bInServer);

	// pad faces the road (+X); the car is parked parallel to the road (along local Y)
	const FVector Car(150.f, 0.f, GroundZ(150.f, 0.f));
	const FRotator R(0.f, 90.f, 2.f);
	auto P = [&](const FVector& L) { return Car + R.RotateVector(L); };

	// sedan body
	AddBox(P(FVector(0.f, 0.f, 55.f)), FVector(450.f, 180.f, 55.f), ESUMat::MetalWhite, R);
	AddBox(P(FVector(-20.f, 0.f, 112.f)), FVector(240.f, 170.f, 55.f), ESUMat::MetalWhite, R);
	AddBox(P(FVector(98.f, 0.f, 112.f)), FVector(6.f, 160.f, 48.f), ESUMat::Glass, R + FRotator(-25.f, 0.f, 0.f), false);
	AddBox(P(FVector(-20.f, -86.f, 112.f)), FVector(200.f, 2.f, 40.f), ESUMat::Glass, R, false);
	for (int32 W = 0; W < 4; ++W)
	{
		AddShape(ESUShape::Cylinder, P(FVector((W & 1) ? 145.f : -145.f, (W & 2) ? 85.f : -85.f, 33.f)), FVector(66.f, 66.f, 22.f), ESUMat::Tire, R + FRotator(0.f, 0.f, 90.f));
	}
	// open doors (driver and rear on the left side)
	AddBox(P(FVector(40.f, 160.f, 80.f)), FVector(110.f, 5.f, 90.f), ESUMat::MetalWhite, R + FRotator(0.f, 60.f, 0.f), true);
	AddBox(P(FVector(-90.f, 150.f, 80.f)), FVector(100.f, 5.f, 90.f), ESUMat::MetalWhite, R + FRotator(0.f, 40.f, 0.f), true);
	// seats with blood on the driver seat
	AddBox(P(FVector(10.f, 40.f, 70.f)), FVector(50.f, 50.f, 15.f), ESUMat::Cloth, R, false);
	AddBox(P(FVector(-15.f, 40.f, 100.f)), FVector(10.f, 50.f, 60.f), ESUMat::Cloth, R, false);
	AddBox(P(FVector(10.f, 40.f, 78.f)), FVector(36.f, 30.f, 1.f), ESUMat::Blood, R, false);
	AddBox(P(FVector(-14.f, 42.f, 105.f)), FVector(2.f, 30.f, 35.f), ESUMat::Blood, R, false);
	AddEvidence(TEXT("EV_M02_BloodSeat"), TEXT("Blut am Fahrersitz"), TEXT("Traces"), P(FVector(0.f, 40.f, 85.f)), FVector(35.f, 30.f, 30.f), false, 900.f, R);
	// hand print on the side window (inside)
	AddBox(P(FVector(-20.f, -84.f, 115.f)), FVector(14.f, 1.f, 18.f), ESUMat::BloodDry, R, false);
	AddEvidence(TEXT("EV_M02_Handprint"), TEXT("Handabdruck an der Scheibe"), TEXT("Traces"), P(FVector(-20.f, -84.f, 115.f)), FVector(15.f, 5.f, 15.f), false, 700.f, R);
	AddEvidence(TEXT("EV_M02_Car"), TEXT("Verlassener Wagen"), TEXT("Vehicle"), P(FVector(0.f, 0.f, 80.f)), FVector(230.f, 95.f, 80.f), false, 3000.f, R);

	// dim lights: the battery is almost dead
	AddPointLight(SceneRoot, P(FVector(-10.f, 0.f, 120.f)), 6.f, 250.f, FLinearColor(1.f, 0.85f, 0.6f), false);
	AddSpotLight(SceneRoot, P(FVector(230.f, 50.f, 60.f)), R + FRotator(-4.f, 0.f, 0.f), 600.f, 2000.f, 30.f, FLinearColor(1.f, 0.9f, 0.7f));
	AddSpotLight(SceneRoot, P(FVector(230.f, -50.f, 60.f)), R + FRotator(-4.f, 0.f, 0.f), 600.f, 2000.f, 30.f, FLinearColor(1.f, 0.9f, 0.7f));

	// drag marks from the driver door into the forest (-X, away from the road)
	for (int32 I = 0; I < 18; ++I)
	{
		for (int32 Side = -1; Side <= 1; Side += 2)
		{
			const float X = 60.f - I * 60.f;
			const float Y = 250.f + Side * 25.f + I * 8.f;
			AddBox(FVector(X, Y, GroundZ(X, Y) + 1.f), FVector(62.f, 9.f, 1.f), ESUMat::Mud, FRotator(0.f, -8.f, 0.f), false);
		}
	}
	AddEvidence(TEXT("EV_M02_DragMarks"), TEXT("Schleifspuren in den Wald"), TEXT("Traces"), FVector(-250.f, 290.f, GroundZ(-250.f, 290.f) + 5.f), FVector(300.f, 60.f, 10.f), false, 1500.f);

	// scattered belongings
	AddBox(FVector(-30.f, 380.f, GroundZ(-30.f, 380.f) + 10.f), FVector(40.f, 30.f, 20.f), ESUMat::Fabric, FRotator(0.f, 30.f, 15.f), false);
	AddShape(ESUShape::Cylinder, FVector(-200.f, 330.f, GroundZ(-200.f, 330.f) + 4.f), FVector(8.f, 8.f, 20.f), ESUMat::Green, FRotator(0.f, 0.f, 90.f), false);

	AddArea(SUArea::M02Site, FVector(0.f, 0.f, 300.f), FVector(2500.f, 2500.f, 800.f));

	AddEvidence(TEXT("EV_M02_Phone"), TEXT("Zurückgelassenes Telefon"), TEXT("Clue"), FVector(-60.f, 300.f, GroundZ(-60.f, 300.f) + 2.f), FVector(10.f, 6.f, 3.f), false, 500.f);
	if (bServer)
	{
		SpawnDocument(FVector(-60.f, 300.f, GroundZ(-60.f, 300.f) + 0.6f), 40.f, TEXT("Fremdes Telefon"),
			TEXT("Akku 3 %.\n\nNachrichten an „Mama“:\n\n21:47 – Bin gleich da, nur noch die Nordstraße.\n")
			TEXT("21:52 – Da steht jemand auf der Straße. Ganz dünn. Er bewegt sich nicht.\n")
			TEXT("21:53 – Ich bleibe im Auto.\n21:58 – Er steht jetzt neben dem Fenster\n22:03 – (nicht gesendet) er kennt meinen Namen"),
			static_cast<int32>(ESUDocumentLook::Phone), SUItem::M02Phone, SUItem::M02Phone);
	}
}
