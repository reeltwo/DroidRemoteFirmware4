///////////////////////////////////////////////////////////////////////////////
//
// Main screen shows current dome position if dome position changes.
// Push button will activate SelectScreen
// 
///////////////////////////////////////////////////////////////////////////////

class StatusScreen : public CommandScreen
{
public:
    StatusScreen() :
        CommandScreen(sDisplay, kStatusScreen)
    {}

    virtual void init() override
    {
        fCurrentDisplayItem = -1;
    }

    virtual void render() override
    {
        if (fCurrentItem != fCurrentDisplayItem)
        {
            sDisplay.invertDisplay(false);
            sDisplay.clearDisplay();
            sDisplay.setTextSize(2);
            sDisplay.setCursor(0, 0);
            sDisplay.println(fCurrentItem);
            sDisplay.display();
            fCurrentDisplayItem = fCurrentItem;
        }
        if (sDisplay.elapsed() >= STATUS_SCREEN_DURATION)
        {
            if (fCallback != nullptr)
            {
                fCallback();
            }
            else
            {
                switchToScreen(kMainScreen);
            }
        }
    }

    virtual bool isStatus() override { return true; }

    void showStatus(String msg, void (*callback)() = nullptr)
    {
        fCurrentItem = msg;
        fCallback = callback;
        sDisplay.switchToScreen(kStatusScreen);
    }

protected:
    String fCurrentItem = "";
    String fCurrentDisplayItem = "dummy";
    void (*fCallback)() = nullptr;
};

///////////////////////////////////////////////////////////////////////////////
//
// Instantiate the screen
//
///////////////////////////////////////////////////////////////////////////////

StatusScreen sStatusScreen;
