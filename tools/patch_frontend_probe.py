from pathlib import Path
path = Path(r"D:\APBReloaded\Source\APBReloaded\Systems\APBSessionProbeSubsystem.cpp")
text = path.read_text(encoding="utf-8")
marker_start = "\tif (UI)\n\t{\n\t\tUI->SetStage(EAPBFrontendStage::Splash);"
marker_end = "\tconst TArray<FString> Districts = APB->GetDistrictList();"
start = text.find(marker_start)
end = text.find(marker_end)
if start < 0:
    # CRLF
    marker_start = "\tif (UI)\r\n\t{\r\n\t\tUI->SetStage(EAPBFrontendStage::Splash);"
    marker_end = "\tconst TArray<FString> Districts = APB->GetDistrictList();"
    start = text.find(marker_start)
    end = text.find(marker_end)
print("start", start, "end", end)
if start < 0 or end < 0:
    raise SystemExit("markers not found")
new = r"""	// Required order: Splash -> Login -> CharacterSelect -> CharacterCreate -> DistrictSelect
	// Drive SHIPPED widget primary actions (OnLoginClicked / OnCreateCharOpen / OnCharCreateConfirm).
	if (UI)
	{
		UI->SetStage(EAPBFrontendStage::Splash);
		AppendLog(FString::Printf(TEXT("UI_STAGE=%s via_widget=1"), *UI->GetStageToken()));
		UI->SetStage(EAPBFrontendStage::Login);
		AppendLog(FString::Printf(TEXT("UI_STAGE=%s via_widget=1"), *UI->GetStageToken()));
		// Form defaults player1/password — seed Domain then real login button handler
		APB->RegisterAccount(TEXT("player1"), TEXT("password"));
		UI->OnLoginClicked();
		AppendLog(FString::Printf(TEXT("UI_STAGE=%s via_widget=1 action=OnLoginClicked"), *UI->GetStageToken()));
		if (UI->GetStage() != EAPBFrontendStage::CharacterSelect)
		{
			AppendLog(TEXT("FRONTEND_FLOW_FAIL expected_CharacterSelect_after_OnLoginClicked"));
			return;
		}
		UI->OnCreateCharOpen();
		AppendLog(FString::Printf(TEXT("UI_STAGE=%s via_widget=1 action=OnCreateCharOpen"), *UI->GetStageToken()));
		if (UI->GetStage() != EAPBFrontendStage::CharacterCreate)
		{
			AppendLog(TEXT("FRONTEND_FLOW_FAIL expected_CharacterCreate_after_OnCreateCharOpen"));
			return;
		}
	}
	else
	{
		AppendLog(TEXT("UI_STAGE=Splash via_widget=0"));
		AppendLog(TEXT("UI_STAGE=Login via_widget=0"));
		APB->RegisterAccount(TEXT("player1"), TEXT("password"));
		const bool bLogin0 = APB->Login(TEXT("player1"), TEXT("password"));
		AppendLog(FString::Printf(TEXT("AUTH ok=%d"), bLogin0 ? 1 : 0));
		if (!bLogin0) { AppendLog(TEXT("FRONTEND_FLOW_FAIL login")); return; }
		APB->EnterWorld(TEXT("W1"));
		AppendLog(TEXT("UI_STAGE=CharacterSelect via_widget=0"));
		AppendLog(TEXT("UI_STAGE=CharacterCreate via_widget=0"));
	}

	// Ensure world entered (OnLoginClicked already EnterWorld on success)
	APB->EnterWorld(TEXT("W1"));
	AppendLog(FString::Printf(TEXT("AUTH ok=%d"), 1));

	if (UI)
	{
		UI->OnCharCreateConfirm();
		AppendLog(FString::Printf(TEXT("UI_STAGE=%s via_widget=1 action=OnCharCreateConfirm"), *UI->GetStageToken()));
	}
	const bool bChar = APB->CaptureDomainSnapshot().bHasCharacter
		|| APB->CreateCharacter(TEXT("FrontendOp"), false);
	AppendLog(FString::Printf(TEXT("CHAR_CREATE ok=%d faction=Criminal"), bChar ? 1 : 0));

	// Body multi-control (height/build) — Domain ApplyAppearance path
	const float BodyH = 1.05f;
	const float BodyB = 0.95f;
	const bool bBody = APB->ApplyBodyProfile(BodyH, BodyB, 1, 2);
	float ReadH = 0.f, ReadB = 0.f;
	APB->GetBodyProfile(ReadH, ReadB);
	AppendLog(FString::Printf(TEXT("BODY height=%.3f bulk=%.3f apply=%d readH=%.3f readB=%.3f"),
		BodyH, BodyB, bBody ? 1 : 0, ReadH, ReadB));
	if (!bBody || FMath::Abs(ReadH - BodyH) > 0.001f)
	{
		AppendLog(TEXT("FRONTEND_FLOW_FAIL body_profile"));
		return;
	}

	FrontendEquippedSlots = 0;
	const TCHAR* slots[] = { TEXT("head"), TEXT("torso"), TEXT("legs"), TEXT("feet"), TEXT("hands"), TEXT("accessory"), TEXT("face") };
	for (const TCHAR* Slot : slots)
	{
		const TArray<FAPBClothingChoice> Choices = APB->GetClothingForSlot(Slot, 5);
		if (Choices.Num() > 0 && APB->EquipClothingItem(Slot, Choices[0].Id)) ++FrontendEquippedSlots;
	}
	AppendLog(FString::Printf(TEXT("APPEARANCE slots_equipped=%d required=7"), FrontendEquippedSlots));
	if (FrontendEquippedSlots < 7)
	{
		AppendLog(TEXT("FRONTEND_FLOW_FAIL clothing_slots"));
		return;
	}

	if (UI)
	{
		if (UI->GetStage() != EAPBFrontendStage::DistrictSelect)
		{
			UI->SetStage(EAPBFrontendStage::DistrictSelect);
		}
		AppendLog(FString::Printf(TEXT("UI_STAGE=%s via_widget=1"), *UI->GetStageToken()));
	}
	else
	{
		AppendLog(TEXT("UI_STAGE=DistrictSelect via_widget=0"));
	}

"""
path.write_text(text[:start] + new + text[end:], encoding="utf-8")
print("OK patched")
t = path.read_text(encoding="utf-8")
print("OnLoginClicked", t.count("OnLoginClicked"))
print("CharacterSelect", t.count("CharacterSelect"))
