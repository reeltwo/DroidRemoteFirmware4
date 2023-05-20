///////////////////////////////////////////////////////////////////////////////
//
// Main screen shows current dome position if dome position changes.
// Push button will activate SelectScreen
// 
///////////////////////////////////////////////////////////////////////////////

class RebootScreen : public CommandScreen
{
public:
    RebootScreen() :
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
        if (sDisplay.elapsed() >= 3000)
        {
            reboot();
        }
    }

    virtual bool isStatus() override { return true; }

    void showStatus(String msg)
    {
        fCurrentItem = msg;
        sDisplay.switchToScreen(kStatusScreen);
    }

protected:
    String fCurrentItem = "";
    String fCurrentDisplayItem = "dummy";
};

///////////////////////////////////////////////////////////////////////////////
//
// Instantiate the screen
//
///////////////////////////////////////////////////////////////////////////////

RebootScreen sRebootScreen;
