// Flash to Bang - player controller: runtime Enhanced Input (no template Input folder), HUD creation, autoplay bot.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "InputActionValue.h"
#include "FTBPlayerController.generated.h"

class UInputAction;
class UInputMappingContext;
class UFTBHUDWidget;
class UFTBAutoplayComponent;
class IFTBObserver;

UCLASS()
class FLASHTOBANG_API AFTBPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AFTBPlayerController();

	virtual void BeginPlay() override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void SetupInputComponent() override;

	UFTBAutoplayComponent* GetAutoplay() const { return Autoplay; }

	/** Mouse look degrees per input unit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float MouseDegreesPerUnit = 0.35f;

protected:
	UPROPERTY(EditAnywhere, Category = "UI") TSubclassOf<UFTBHUDWidget> HUDWidgetClass;

private:
	void BuildInput();
	void SetupHUD();
	IFTBObserver* Observer() const;
	bool HumanBlocked() const;

	void OnLook(const FInputActionValue& Value);
	void OnWatch();
	void OnFire();
	void OnBinocularsDown();
	void OnBinocularsUp();
	void OnPost1();
	void OnPost2();
	void OnPost3();
	void OnRestart();
	void OnMenu();

	UPROPERTY(Transient) TObjectPtr<UInputMappingContext> Mapping;
	UPROPERTY(Transient) TObjectPtr<UInputAction> LookAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> WatchAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> FireAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> BinocularsAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> Post1Action;
	UPROPERTY(Transient) TObjectPtr<UInputAction> Post2Action;
	UPROPERTY(Transient) TObjectPtr<UInputAction> Post3Action;
	UPROPERTY(Transient) TObjectPtr<UInputAction> RestartAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> MenuAction;
	UPROPERTY(Transient) TObjectPtr<UFTBHUDWidget> HUDWidget;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UFTBAutoplayComponent> Autoplay;
};
