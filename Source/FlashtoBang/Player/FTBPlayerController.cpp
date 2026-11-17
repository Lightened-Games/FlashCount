// Flash to Bang - player controller.

#include "Player/FTBPlayerController.h"
#include "FlashtoBang.h"
#include "Core/FTBInterfaces.h"
#include "Core/FTBWorld.h"
#include "Player/FTBAutoplayComponent.h"
#include "UI/FTBHUDWidget.h"
#include "FactoryKit/FactoryFlowSubsystem.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputAction.h"
#include "InputMappingContext.h"

AFTBPlayerController::AFTBPlayerController()
{
	bShowMouseCursor = false;
	HUDWidgetClass = UFTBHUDWidget::StaticClass();
	Autoplay = CreateDefaultSubobject<UFTBAutoplayComponent>(TEXT("Autoplay"));
}

void AFTBPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (IsLocalController())
	{
		SetupHUD();
		SetInputMode(FInputModeGameOnly());
	}
}

void AFTBPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	if (IsLocalController())
	{
		SetupHUD();
	}
}

void AFTBPlayerController::SetupHUD()
{
	if (HUDWidget || !HUDWidgetClass)
	{
		return;
	}
	HUDWidget = CreateWidget<UFTBHUDWidget>(this, HUDWidgetClass);
	if (HUDWidget)
	{
		HUDWidget->AddToViewport(0);
	}
	else
	{
		UE_LOG(LogFTB, Error, TEXT("[FTB] Failed to create HUD widget"));
	}
}

void AFTBPlayerController::BuildInput()
{
	if (Mapping)
	{
		return;
	}
	auto Make = [this](const TCHAR* Name, EInputActionValueType Type)
	{
		UInputAction* A = NewObject<UInputAction>(this, Name);
		A->ValueType = Type;
		return A;
	};
	LookAction = Make(TEXT("IA_Look"), EInputActionValueType::Axis2D);
	WatchAction = Make(TEXT("IA_Watch"), EInputActionValueType::Boolean);
	FireAction = Make(TEXT("IA_CallFire"), EInputActionValueType::Boolean);
	BinocularsAction = Make(TEXT("IA_Binoculars"), EInputActionValueType::Boolean);
	Post1Action = Make(TEXT("IA_Post1"), EInputActionValueType::Boolean);
	Post2Action = Make(TEXT("IA_Post2"), EInputActionValueType::Boolean);
	Post3Action = Make(TEXT("IA_Post3"), EInputActionValueType::Boolean);
	RestartAction = Make(TEXT("IA_Restart"), EInputActionValueType::Boolean);
	MenuAction = Make(TEXT("IA_Menu"), EInputActionValueType::Boolean);

	Mapping = NewObject<UInputMappingContext>(this, TEXT("IMC_FlashToBang"));
	Mapping->MapKey(LookAction, EKeys::Mouse2D);
	Mapping->MapKey(WatchAction, EKeys::SpaceBar);
	Mapping->MapKey(FireAction, EKeys::LeftMouseButton);
	Mapping->MapKey(BinocularsAction, EKeys::RightMouseButton);
	Mapping->MapKey(Post1Action, EKeys::One);
	Mapping->MapKey(Post1Action, EKeys::NumPadOne);
	Mapping->MapKey(Post2Action, EKeys::Two);
	Mapping->MapKey(Post2Action, EKeys::NumPadTwo);
	Mapping->MapKey(Post3Action, EKeys::Three);
	Mapping->MapKey(Post3Action, EKeys::NumPadThree);
	Mapping->MapKey(RestartAction, EKeys::R);
	Mapping->MapKey(MenuAction, EKeys::Escape);
}

void AFTBPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	BuildInput();
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		Subsystem->AddMappingContext(Mapping, 0);
	}
	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(InputComponent))
	{
		EIC->BindAction(LookAction, ETriggerEvent::Triggered, this, &AFTBPlayerController::OnLook);
		EIC->BindAction(WatchAction, ETriggerEvent::Started, this, &AFTBPlayerController::OnWatch);
		EIC->BindAction(FireAction, ETriggerEvent::Started, this, &AFTBPlayerController::OnFire);
		EIC->BindAction(BinocularsAction, ETriggerEvent::Started, this, &AFTBPlayerController::OnBinocularsDown);
		EIC->BindAction(BinocularsAction, ETriggerEvent::Completed, this, &AFTBPlayerController::OnBinocularsUp);
		EIC->BindAction(Post1Action, ETriggerEvent::Started, this, &AFTBPlayerController::OnPost1);
		EIC->BindAction(Post2Action, ETriggerEvent::Started, this, &AFTBPlayerController::OnPost2);
		EIC->BindAction(Post3Action, ETriggerEvent::Started, this, &AFTBPlayerController::OnPost3);
		EIC->BindAction(RestartAction, ETriggerEvent::Started, this, &AFTBPlayerController::OnRestart);
		EIC->BindAction(MenuAction, ETriggerEvent::Started, this, &AFTBPlayerController::OnMenu);
	}
	else
	{
		UE_LOG(LogFTB, Error, TEXT("[FTB] InputComponent is not an EnhancedInputComponent - check DefaultInput.ini"));
	}
}

IFTBObserver* AFTBPlayerController::Observer() const
{
	return GetPawn() ? Cast<IFTBObserver>(GetPawn()) : nullptr;
}

bool AFTBPlayerController::HumanBlocked() const
{
	// While the bot drives, the round is the bot's. Any real input hands control to the player (FactoryKit take-over),
	// and the bot resumes after a few idle seconds.
	if (!Autoplay || !Autoplay->IsBotEnabled())
	{
		return false;
	}
	const UFactoryFlowSubsystem* Flow = UFactoryFlowSubsystem::Get(this);
	return !Flow || Flow->IsBotDriving();
}

void AFTBPlayerController::OnLook(const FInputActionValue& Value)
{
	if (HumanBlocked()) { return; }
	const FVector2D V = Value.Get<FVector2D>();
	const float Sens = MouseDegreesPerUnit * UFactoryFlowSubsystem::GetMouseSensitivity();
	const float Invert = UFactoryFlowSubsystem::GetInvertY() ? -1.0f : 1.0f;
	if (IFTBObserver* O = Observer()) { O->Obs_Look(V.X * Sens, V.Y * Sens * Invert); }
}

void AFTBPlayerController::OnWatch()            { if (!HumanBlocked()) { if (IFTBObserver* O = Observer()) { O->Obs_ToggleWatch(); } } }
void AFTBPlayerController::OnFire()             { if (!HumanBlocked()) { if (IFTBObserver* O = Observer()) { O->Obs_CallFire(); } } }
void AFTBPlayerController::OnBinocularsDown()   { if (!HumanBlocked()) { if (IFTBObserver* O = Observer()) { O->Obs_SetBinoculars(true); } } }
void AFTBPlayerController::OnBinocularsUp()     { if (!HumanBlocked()) { if (IFTBObserver* O = Observer()) { O->Obs_SetBinoculars(false); } } }
void AFTBPlayerController::OnPost1()            { if (!HumanBlocked()) { if (IFTBObserver* O = Observer()) { O->Obs_RunToPost(0); } } }
void AFTBPlayerController::OnPost2()            { if (!HumanBlocked()) { if (IFTBObserver* O = Observer()) { O->Obs_RunToPost(1); } } }
void AFTBPlayerController::OnPost3()            { if (!HumanBlocked()) { if (IFTBObserver* O = Observer()) { O->Obs_RunToPost(2); } } }

void AFTBPlayerController::OnRestart()
{
	if (HumanBlocked()) { return; }
	IFTBWorldQuery* Q = FTB::Query(this);
	if (UFTBEventHub* Hub = FTB::Hub(this)) { Hub->OnInput.Broadcast(FTB::KeyRestart, false); }
	if (Q && (Q->FTB_GetState() == EFTBState::Victory || Q->FTB_GetState() == EFTBState::Defeat))
	{
		UGameplayStatics::SetGlobalTimeDilation(this, 1.0f);
		if (UFactoryFlowSubsystem* Flow = UFactoryFlowSubsystem::Get(this)) { Flow->MarkRestart(); }
		UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this, true)));
	}
}

void AFTBPlayerController::OnMenu()
{
	// Esc in play: back to the start screen (the level reloads under the menu).
	if (HumanBlocked()) { return; }
	if (UFactoryFlowSubsystem* Flow = UFactoryFlowSubsystem::Get(this))
	{
		if (Flow->GetState() == EFactoryFlowState::Playing || Flow->GetState() == EFactoryFlowState::Ending)
		{
			UGameplayStatics::SetGlobalTimeDilation(this, 1.0f);
			Flow->ReturnToMenu();
		}
	}
}
