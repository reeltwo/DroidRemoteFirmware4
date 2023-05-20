///////////////////////////////////////////////////////////////////////////////
//
///////////////////////////////////////////////////////////////////////////////

class PairingScreen : public CommandScreen
{
public:
    PairingScreen() :
        CommandScreen(sDisplay, kPairingScreen)
    {}

    virtual void init() override
    {
        fCurrentDisplayItem = -1;
    }

    virtual void render() override
    {
        if (!SMQ::isPairing())
        {
            sDisplay.switchToScreen(kMainScreen);
        }
        else if (fCurrentItem != fCurrentDisplayItem)
        {
            sDisplay.invertDisplay(false);
            sDisplay.clearDisplay();
            sDisplay.setTextSize(2);
            sDisplay.setCursor(0, 0);
            sDisplay.println(fCurrentItem);
            sDisplay.display();
            fCurrentDisplayItem = fCurrentItem;
            sDisplayLVGL.setRemoteStatus("");
        }
    }

    virtual bool isStatus() override { return true; }

protected:
    String fCurrentItem = "Pairing ...";
    String fCurrentDisplayItem = "dummy";
};

///////////////////////////////////////////////////////////////////////////////
//
// Instantiate the screen
//
///////////////////////////////////////////////////////////////////////////////

PairingScreen sPairingScreen;
