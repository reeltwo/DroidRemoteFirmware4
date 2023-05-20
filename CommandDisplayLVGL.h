#define WHITE 0xFFFF
#define UI_BG_COLOR    lv_color_black()
#define UI_FRAME_COLOR lv_color_hex(0x282828)
#define UI_FONT_COLOR  lv_color_white()

LV_IMG_DECLARE(r2logo);

class CommandDisplayLVGL
{
public:
    CommandDisplayLVGL() { }

    inline void invertDisplay(bool invert)
    {
        fInvert = invert;
    }

    inline void setRotation(uint8_t r)
    {
        fRotation = r;
    }

    inline void clearDisplay()
    {
        fClearDisplay = true;
    }

    inline void setTextSize(int siz)
    {
        fTextSize = siz;
    }

    inline void drawTextCentered(String text)
    {
        fCentered = true;
        fString = text;
    }

    inline void setTextColor(uint16_t textColor)
    {
        fTextColor = textColor;
    }

    inline void setCursor(uint8_t x, uint8_t y)
    {
        fX = x;
        fY = y;
    }

    inline void print(String text)
    {
        fString += text;
    }

    inline void println(unsigned val)
    {
        if (fString.length() > 0)
        {
            fString += String("\n") + String(val);
        }
        else
        {
            fString = String(val);
        }
    }

    inline void println(String text)
    {
        if (fString.length() > 0)
        {
            fString += String("\n") + text;
        }
        else
        {
            fString = text;
        }
    }

    void display()
    {
        CommandScreenHandler* handler = getScreenHandler();
        if (fView == nullptr)
        {
            fScreenCount = handler->getNumScreens();
            fMenuText = new lv_obj_t*[fScreenCount];

            fView = lv_tileview_create(lv_scr_act());
            lv_obj_align(fView, LV_ALIGN_TOP_RIGHT, 0, 0);
            lv_obj_set_size(fView, LV_PCT(100), LV_PCT(100));
            for (unsigned i = 0; i < fScreenCount; i++)
            {
                lv_obj_t *page_view = lv_tileview_add_tile(fView, i, 0, LV_DIR_HOR);

                lv_obj_t *main_cout = lv_obj_create(page_view);
                lv_obj_set_size(main_cout, LV_PCT(100), LV_PCT(100));
                lv_obj_clear_flag(main_cout, LV_OBJ_FLAG_SCROLLABLE);
                lv_obj_set_style_border_width(main_cout, 0, 0);
                lv_obj_set_style_bg_color(main_cout, UI_BG_COLOR, 0);

                lv_obj_t *menu_cout = lv_obj_create(main_cout);
                lv_obj_set_size(menu_cout, LV_PCT(100), LV_PCT(100));
                lv_obj_align(menu_cout, LV_ALIGN_CENTER, 0, 0);
                lv_obj_set_style_bg_color(menu_cout, UI_FRAME_COLOR, 0);
                lv_obj_clear_flag(menu_cout, LV_OBJ_FLAG_SCROLLABLE);

                lv_obj_t *menu_text = lv_label_create(menu_cout);
                lv_obj_center(menu_text);
                lv_label_set_text(menu_text, "");
                lv_obj_set_style_text_color(menu_text, UI_FONT_COLOR, 0);

                fMenuText[i] = menu_text;
            }
            lv_obj_t* fBottomLeftStatus = lv_img_create(lv_scr_act());
            lv_img_set_src(fBottomLeftStatus, &r2logo);
            lv_obj_align(fBottomLeftStatus, LV_ALIGN_BOTTOM_LEFT, 0, 0);

            fBottomRightStatus = lv_label_create(lv_scr_act());
            lv_obj_align(fBottomRightStatus, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
            lv_label_set_text(fBottomRightStatus, "");

            fTopRightStatus = lv_label_create(lv_scr_act());
            lv_obj_align(fTopRightStatus, LV_ALIGN_TOP_RIGHT, 0, 0);
            lv_label_set_text(fTopRightStatus, "");

            fTopLeftStatus = lv_label_create(lv_scr_act());
            lv_obj_align(fTopLeftStatus, LV_ALIGN_TOP_LEFT, 0, 0);
            lv_label_set_text(fTopLeftStatus, "");
        }
        unsigned index = handler->currentID();
        lv_obj_t *menu_text = fMenuText[index];
        // printf("%d:menu_text{%p}: %s\n", index, menu_text, fString.c_str());
        lv_label_set_text(menu_text, fString.c_str());
        if (fTextSize == 4)
        {
            lv_obj_set_style_text_font(menu_text, font_large, 0);
        }
        else
        {
            lv_obj_set_style_text_font(menu_text, font_normal, 0);
        }
        if (fInvert)
        {
            lv_obj_set_style_text_color(menu_text, UI_BG_COLOR, 0);
            lv_obj_set_style_bg_opa(menu_text, LV_OPA_COVER, 0);
            lv_obj_set_style_bg_color(menu_text, UI_FONT_COLOR, 0);
        }
        else
        {
            lv_obj_set_style_text_color(menu_text, UI_FONT_COLOR, 0);
            lv_obj_set_style_bg_opa(menu_text, LV_OPA_TRANSP, 0);
            lv_obj_set_style_bg_color(menu_text, UI_BG_COLOR, 0);
        }
        // Disable tile view animation if either the previous screen or the new screen is a status screen
        CommandScreen* newScreen = handler->getScreenAt(index);
        CommandScreen* prevScreen = (fDisplayIndex >= 0) ? handler->getScreenAt(fDisplayIndex) : nullptr;
        lv_anim_enable_t anim_en = (prevScreen != nullptr) ? LV_ANIM_ON : LV_ANIM_OFF;
        bool prevScreenStatus = (prevScreen != nullptr && prevScreen->isStatus());
        bool newScreenStatus = (newScreen != nullptr && newScreen->isStatus());
        if (prevScreenStatus || newScreenStatus)
        {
            anim_en = LV_ANIM_OFF;
        }
        lv_obj_set_tile_id(fView, index, 0, anim_en);
        resetDisplay();
        fDisplayIndex = index;
    }

    void getTextBounds(const char *string, int16_t x, int16_t y,
                    int16_t *x1, int16_t *y1, uint16_t *w, uint16_t *h)
    {
        // Ignore all text is centered in container
        *x1 = 0;
        *y1 = 0;
        *w = SCREEN_WIDTH;
        *h = SCREEN_HEIGHT;
    }

    void refreshBatteryVoltage(unsigned level, unsigned milliVolt)
    {
        const char* batteryStatus[5] = {
            LV_SYMBOL_BATTERY_EMPTY,
            LV_SYMBOL_BATTERY_1,
            LV_SYMBOL_BATTERY_2,
            LV_SYMBOL_BATTERY_3,
            LV_SYMBOL_BATTERY_FULL
        };
        if (fBottomRightStatus == nullptr)
            return;
        if (milliVolt >= 4400)
        {
            // External power (with battery)
            // printf("External power milliVolt: %d\n", milliVolt);
            lv_label_set_text(fBottomRightStatus, LV_SYMBOL_CHARGE);
        }
        else
        {
            char buf[20];
            int icon = level/(100/SizeOfArray(batteryStatus)-1);
            // printf("Battery milliVolt: %d level=%d [%d]\n", milliVolt, level, icon);
            snprintf(buf, sizeof(buf), "%d%% %s", level, batteryStatus[icon]);
            lv_label_set_text(fBottomRightStatus, buf);
        }
    }

    void setWiFiStatus(bool wifiEnabled)
    {
        if (fTopRightStatus == nullptr)
            return;
        lv_label_set_text(fTopRightStatus, wifiEnabled ? LV_SYMBOL_WIFI : "");
    }

    void setRemoteStatus(String device)
    {
        setRemoteStatus(device.c_str());
    }

    void setRemoteStatus(const char* device)
    {
        if (fTopLeftStatus == nullptr)
            return;
        if (*device != '\0')
        {
            char buf[32];
            snprintf(buf, sizeof(buf), "%s %s", LV_SYMBOL_LEFT, device);
            lv_label_set_text(fTopLeftStatus, buf);
        }
        else
        {
            lv_label_set_text(fTopLeftStatus, LV_SYMBOL_HOME);
        }
    }

    void wakeDevice()
    {
        // Turn on LCD and backlight
        digitalWrite(PIN_POWER_ON, HIGH);
        digitalWrite(PIN_LCD_BL, HIGH);
    }

    void sleepDevice()
    {
        // Turn off LCD and backlight
        digitalWrite(PIN_POWER_ON, LOW);
        digitalWrite(PIN_LCD_BL, LOW);
    }

protected:
    int         fDisplayIndex = -1;
    bool        fInvert = false;
    bool        fClearDisplay = false;
    bool        fCentered = false;
    String      fString;
    uint16_t    fTextColor = WHITE;
    uint8_t     fRotation = 0;
    uint8_t     fX = 0;
    uint8_t     fY = 0;
    uint8_t     fTextSize = 0;
    lv_obj_t*   fView = nullptr;
    lv_obj_t*   fTopLeftStatus = nullptr;
    lv_obj_t*   fTopRightStatus = nullptr;
    lv_obj_t*   fBottomLeftStatus = nullptr;
    lv_obj_t*   fBottomRightStatus = nullptr;
    unsigned    fScreenCount = 0;
    lv_obj_t**  fMenuText = nullptr;

    void resetDisplay()
    {
        fInvert = false;
        fClearDisplay = false;
        fTextSize = 0;
        fCentered = false;
        fString = "";
        fX = 0;
        fY = 0;
    }
};
