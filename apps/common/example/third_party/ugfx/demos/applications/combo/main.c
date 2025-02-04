/*
 * Copyright (c) 2012, 2013, Joel Bodenmann aka Tectu <joel@unormal.org>
 * Copyright (c) 2012, 2013, Andrew Hannam aka inmarket
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *    * Neither the name of the <organization> nor the
 *      names of its contributors may be used to endorse or promote products
 *      derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "gfx.h"
#include "tasks.h"

/**
 * This demo demonstrates many of the ugfx features.
 */

/**
 * The image files must be stored on a GFILE file-system.
 * Use either GFILE_NEED_NATIVEFS or GFILE_NEED_ROMFS (or both).
 *
 * The ROMFS uses the file "romfs_files.h" to describe the set of files in the ROMFS.
 */

/* Our custom yellow style */
static const GWidgetStyle YellowWidgetStyle = {
    GFX_YELLOW,							// window background
    HTML2COLOR(0x800000),			// focus - for text edit.

    // enabled color set
    {
        HTML2COLOR(0x0000FF),		// text
        HTML2COLOR(0x404040),		// edge
        HTML2COLOR(0xE0E0E0),		// fill
        HTML2COLOR(0xE0E0E0)		// progress - inactive area
    },

    // disabled color set
    {
        HTML2COLOR(0xC0C0C0),		// text
        HTML2COLOR(0x808080),		// edge
        HTML2COLOR(0xE0E0E0),		// fill
        HTML2COLOR(0xC0E0C0)		// progress - active area
    },

    // pressed color set
    {
        HTML2COLOR(0xFF00FF),		// text
        HTML2COLOR(0x404040),		// edge
        HTML2COLOR(0x808080),		// fill
        HTML2COLOR(0x00E000),		// progress - active area
    }
};

/* The variables we need */
static gFont		font;
static GListener	gl;
static GHandle		ghConsole;
static GTimer		FlashTimer;

static GHandle		ghTabset;
static GHandle		ghPgControls, ghPgSliders, ghPgLabels, ghPgRadios, ghPgLists, ghPgImages, ghPgBounce, ghPgMandelbrot;
static GHandle		ghButton1, ghButton2, ghButton3, ghButton4;
static GHandle		ghSlider1, ghSlider2, ghSlider3, ghSlider4;
static GHandle		ghCheckbox1, ghCheckbox2, ghCheckbox3, ghCheckDisableAll;
static GHandle		ghLabelSlider1, ghLabelSlider2, ghLabelSlider3, ghLabelSlider4, ghLabelRadio1;
static GHandle		ghRadio1, ghRadio2;
static GHandle		ghRadioBlack, ghRadioWhite, ghRadioYellow;
static GHandle		ghList1, ghList2, ghList3, ghList4;
static GHandle		ghImage1;
static GHandle		ghProgressbar1;
static gdispImage	imgYesNo;

/* Some useful macros */
#define	ScrWidth			gdispGetWidth()
#define	ScrHeight			gdispGetHeight()

#define BUTTON_PADDING		20
#define TAB_HEIGHT			30
#define LABEL_HEIGHT		15
#define BUTTON_WIDTH		50
#define BUTTON_HEIGHT		30
#define LIST_WIDTH			75
#define LIST_HEIGHT			80
#define SLIDER_WIDTH		20
#define CHECKBOX_WIDTH		80
#define CHECKBOX_HEIGHT		20
#define RADIO_WIDTH			50
#define RADIO_HEIGHT		20
#define COLOR_WIDTH			80
#define DISABLEALL_WIDTH	100
#define GROUP_TABS			0
#define GROUP_YESNO			1
#define GROUP_COLORS		2

static void nextline(GWidgetInit *pwi)
{
    pwi->g.x = 5;
    pwi->g.y += pwi->g.height + 1;
}

static void setbtntext(GWidgetInit *pwi, gCoord maxwidth, char *txt)
{
    pwi->text = txt;
    pwi->g.width = gdispGetStringWidth(pwi->text, font) + BUTTON_PADDING;
    if (pwi->g.x + pwi->g.width > maxwidth) {
        nextline(pwi);
    }
}

static void nextpos(GWidgetInit *pwi, gCoord maxwidth, gCoord nextwidth)
{
    pwi->g.x += pwi->g.width + 1;
    pwi->g.width = nextwidth;
    if (pwi->g.x + nextwidth > maxwidth) {
        nextline(pwi);
    }
}
/**
 * Create all the widgets.
 * With the exception of the Pages they are all initially visible.
 *
 * This routine is complicated by the fact that we want a dynamic
 * layout so it looks good on small and large displays.
 * It is tested to work on 320x272 as a minimum LCD size.
 */
static void createWidgets(void)
{
    GWidgetInit		wi;
    gCoord			border, pagewidth;

    gwinWidgetClearInit(&wi);

    // Calculate page borders based on screen size
    border = ScrWidth < 450 ? 1 : 5;

    // Create the Tabs
    wi.g.show = gTrue;
    wi.g.x = border;
    wi.g.y = 0;
    wi.g.width = ScrWidth - 2 * border;
    wi.g.height = ScrHeight - wi.g.y - border;
    ghTabset			= gwinTabsetCreate(0, &wi, GWIN_TABSET_BORDER);
    ghPgControls		= gwinTabsetAddTab(ghTabset, "Controls", gFalse);
    ghPgSliders			= gwinTabsetAddTab(ghTabset, "Sliders", gFalse);
    ghPgRadios			= gwinTabsetAddTab(ghTabset, "Radios", gFalse);
    ghPgLists			= gwinTabsetAddTab(ghTabset, "Lists", gFalse);
    ghPgLabels			= gwinTabsetAddTab(ghTabset, "Labels", gFalse);
    ghPgImages			= gwinTabsetAddTab(ghTabset, "Images", gFalse);
    ghPgBounce			= gwinTabsetAddTab(ghTabset, "Bounce", gFalse);
    ghPgMandelbrot		= gwinTabsetAddTab(ghTabset, "Mandelbrot", gFalse);

    pagewidth = gwinGetInnerWidth(ghTabset) / 2;

    // Console - we apply some special colors before making it visible
    //	We put the console on the tabset itself rather than a tab-page.
    //	This makes it appear on every page :)
    wi.g.parent = ghTabset;
    wi.g.x = pagewidth;
    wi.g.width = pagewidth;
    ghConsole = gwinConsoleCreate(0, &wi.g);
    gwinSetColor(ghConsole, GFX_BLACK);
    gwinSetBgColor(ghConsole, HTML2COLOR(0xF0F0F0));

    // Buttons
    wi.g.parent = ghPgControls;
    wi.g.width = BUTTON_WIDTH;
    wi.g.height = BUTTON_HEIGHT;
    wi.g.y = 5;
    wi.g.x = 5;
    setbtntext(&wi, pagewidth, "Button 1");
    ghButton1 = gwinButtonCreate(0, &wi);
    wi.g.x += wi.g.width + 3;
    setbtntext(&wi, pagewidth, "Button 2");
    ghButton2 = gwinButtonCreate(0, &wi);
    wi.g.x += wi.g.width + 3;
    setbtntext(&wi, pagewidth, "Button 3");
    ghButton3 = gwinButtonCreate(0, &wi);
    wi.g.x += wi.g.width + 3;
    setbtntext(&wi, pagewidth, "Button 4");
    ghButton4 = gwinButtonCreate(0, &wi);

    nextline(&wi);
    wi.g.width = CHECKBOX_WIDTH;
    wi.g.height = CHECKBOX_HEIGHT;
    wi.text = "C1";
    ghCheckbox1 = gwinCheckboxCreate(0, &wi);
    wi.customDraw = gwinCheckboxDraw_CheckOnRight;
    nextpos(&wi, pagewidth, CHECKBOX_WIDTH);
    wi.text = "C2";
    ghCheckbox2 = gwinCheckboxCreate(0, &wi);
    wi.customDraw = gwinCheckboxDraw_Button;
    nextline(&wi);
    wi.text = "C3";
    wi.g.width = BUTTON_WIDTH;
    wi.g.height = BUTTON_HEIGHT;
    ghCheckbox3 = gwinCheckboxCreate(0, &wi);
    nextpos(&wi, pagewidth, DISABLEALL_WIDTH);
    wi.text = "Disable All";
    wi.customDraw = 0;
    wi.g.height = CHECKBOX_HEIGHT;
    ghCheckDisableAll = gwinCheckboxCreate(0, &wi);


    // Horizontal Sliders
    wi.g.parent = ghPgSliders;
    wi.g.width = pagewidth - 10;
    wi.g.height = SLIDER_WIDTH;
    wi.g.x = 5;
    wi.g.y = 5;
    wi.text = "S1";
    ghSlider1 = gwinSliderCreate(0, &wi);
    gwinSliderSetPosition(ghSlider1, 33);
    wi.g.y += wi.g.height + 1;
    wi.text = "S2";
    ghSlider2 = gwinSliderCreate(0, &wi);
    gwinSliderSetPosition(ghSlider2, 86);

    // Progressbar
    wi.g.y += wi.g.height + 1;
    wi.text = "Progressbar 1";
    ghProgressbar1 = gwinProgressbarCreate(0, &wi);
    gwinProgressbarSetResolution(ghProgressbar1, 10);

    // Vertical Sliders
    wi.g.y += wi.g.height + 5;
    wi.g.width = SLIDER_WIDTH;
    wi.g.height = gwinGetInnerHeight(ghPgSliders) - 5 - wi.g.y;
    wi.g.x = 5;
    wi.text = "S3";
    ghSlider3 = gwinSliderCreate(0, &wi);
    gwinSliderSetPosition(ghSlider3, 13);
    wi.g.x += wi.g.width + 1;
    wi.text = "S4";
    ghSlider4 = gwinSliderCreate(0, &wi);
    gwinSliderSetPosition(ghSlider4, 76);

    // Labels
    wi.g.parent = ghPgLabels;
    wi.g.width = pagewidth - 10;
    wi.g.height = LABEL_HEIGHT;
    wi.g.x = wi.g.y = 5;
    wi.text = "N/A";
    ghLabelSlider1 = gwinLabelCreate(0, &wi);
    gwinLabelSetAttribute(ghLabelSlider1, 100, "Slider 1:");
    wi.g.y += LABEL_HEIGHT + 2;
    ghLabelSlider2 = gwinLabelCreate(0, &wi);
    gwinLabelSetAttribute(ghLabelSlider2, 100, "Slider 2:");
    wi.g.y += LABEL_HEIGHT + 2;
    ghLabelSlider3 = gwinLabelCreate(0, &wi);
    gwinLabelSetAttribute(ghLabelSlider3, 100, "Slider 3:");
    wi.g.y += LABEL_HEIGHT + 2;
    ghLabelSlider4 = gwinLabelCreate(0, &wi);
    gwinLabelSetAttribute(ghLabelSlider4, 100, "Slider 4:");
    wi.g.y += LABEL_HEIGHT + 2;
    ghLabelRadio1 = gwinLabelCreate(0, &wi);
    gwinLabelSetAttribute(ghLabelRadio1, 100, "RadioButton 1:");


    // Radio Buttons
    wi.g.parent = ghPgRadios;
    wi.g.width = RADIO_WIDTH;
    wi.g.height = RADIO_HEIGHT;
    wi.g.y = 5;
    wi.g.x = 5;
    wi.text = "Yes";
    ghRadio1 = gwinRadioCreate(0, &wi, GROUP_YESNO);
    wi.g.x += wi.g.width;
    wi.text = "No";
    if (wi.g.x + wi.g.width > pagewidth) {
        wi.g.x = 5;
        wi.g.y += RADIO_HEIGHT;
    }
    ghRadio2 = gwinRadioCreate(0, &wi, GROUP_YESNO);
    gwinRadioPress(ghRadio1);
    wi.g.width = COLOR_WIDTH;
    wi.g.y += RADIO_HEIGHT + 5;
    wi.g.x = 5;
    wi.text = "Black";
    ghRadioBlack = gwinRadioCreate(0, &wi, GROUP_COLORS);
    wi.g.x += wi.g.width;
    wi.text = "White";
    if (wi.g.x + wi.g.width > pagewidth) {
        wi.g.x = 5;
        wi.g.y += RADIO_HEIGHT;
    }
    ghRadioWhite = gwinRadioCreate(0, &wi, GROUP_COLORS);
    wi.g.x += wi.g.width;
    wi.text = "Yellow";
    if (wi.g.x + wi.g.width > pagewidth) {
        wi.g.x = 5;
        wi.g.y += RADIO_HEIGHT;
    }
    ghRadioYellow = gwinRadioCreate(0, &wi, GROUP_COLORS);
    gwinRadioPress(ghRadioWhite);

    // Lists
    border = pagewidth < 10 + 2 * LIST_WIDTH ? 2 : 5;
    wi.g.parent = ghPgLists;
    wi.g.width = LIST_WIDTH;
    wi.g.height = LIST_HEIGHT;
    wi.g.y = border;
    wi.g.x = border;
    wi.text = "L1";
    ghList1 = gwinListCreate(0, &wi, gFalse);
    gwinListAddItem(ghList1, "Item 0", gFalse);
    gwinListAddItem(ghList1, "Item 1", gFalse);
    gwinListAddItem(ghList1, "Item 2", gFalse);
    gwinListAddItem(ghList1, "Item 3", gFalse);
    gwinListAddItem(ghList1, "Item 4", gFalse);
    gwinListAddItem(ghList1, "Item 5", gFalse);
    gwinListAddItem(ghList1, "Item 6", gFalse);
    gwinListAddItem(ghList1, "Item 7", gFalse);
    gwinListAddItem(ghList1, "Item 8", gFalse);
    gwinListAddItem(ghList1, "Item 9", gFalse);
    gwinListAddItem(ghList1, "Item 10", gFalse);
    gwinListAddItem(ghList1, "Item 11", gFalse);
    gwinListAddItem(ghList1, "Item 12", gFalse);
    gwinListAddItem(ghList1, "Item 13", gFalse);
    wi.text = "L2";
    wi.g.x += LIST_WIDTH + border;
    if (wi.g.x + LIST_WIDTH > pagewidth) {
        wi.g.x = border;
        wi.g.y += LIST_HEIGHT + border;
    }
    ghList2 = gwinListCreate(0, &wi, gTrue);
    gwinListAddItem(ghList2, "Item 0", gFalse);
    gwinListAddItem(ghList2, "Item 1", gFalse);
    gwinListAddItem(ghList2, "Item 2", gFalse);
    gwinListAddItem(ghList2, "Item 3", gFalse);
    gwinListAddItem(ghList2, "Item 4", gFalse);
    gwinListAddItem(ghList2, "Item 5", gFalse);
    gwinListAddItem(ghList2, "Item 6", gFalse);
    gwinListAddItem(ghList2, "Item 7", gFalse);
    gwinListAddItem(ghList2, "Item 8", gFalse);
    gwinListAddItem(ghList2, "Item 9", gFalse);
    gwinListAddItem(ghList2, "Item 10", gFalse);
    gwinListAddItem(ghList2, "Item 11", gFalse);
    gwinListAddItem(ghList2, "Item 12", gFalse);
    gwinListAddItem(ghList2, "Item 13", gFalse);
    wi.text = "L3";
    wi.g.x += LIST_WIDTH + border;
    if (wi.g.x + LIST_WIDTH > pagewidth) {
        wi.g.x = border;
        wi.g.y += LIST_HEIGHT + border;
    }
    ghList3 = gwinListCreate(0, &wi, gTrue);
    gwinListAddItem(ghList3, "Item 0", gFalse);
    gwinListAddItem(ghList3, "Item 1", gFalse);
    gwinListAddItem(ghList3, "Item 2", gFalse);
    gwinListAddItem(ghList3, "Item 3", gFalse);
    gdispImageOpenFile(&imgYesNo, "image_yesno.gif");
    gwinListItemSetImage(ghList3, 1, &imgYesNo);
    gwinListItemSetImage(ghList3, 3, &imgYesNo);
    wi.text = "L4";
    wi.g.x += LIST_WIDTH + border;
    if (wi.g.x + LIST_WIDTH > pagewidth) {
        wi.g.x = border;
        wi.g.y += LIST_HEIGHT + border;
    }
    ghList4 = gwinListCreate(0, &wi, gTrue);
    gwinListAddItem(ghList4, "Item 0", gFalse);
    gwinListAddItem(ghList4, "Item 1", gFalse);
    gwinListAddItem(ghList4, "Item 2", gFalse);
    gwinListAddItem(ghList4, "Item 3", gFalse);
    gwinListAddItem(ghList4, "Item 4", gFalse);
    gwinListAddItem(ghList4, "Item 5", gFalse);
    gwinListAddItem(ghList4, "Item 6", gFalse);
    gwinListAddItem(ghList4, "Item 7", gFalse);
    gwinListAddItem(ghList4, "Item 8", gFalse);
    gwinListAddItem(ghList4, "Item 9", gFalse);
    gwinListAddItem(ghList4, "Item 10", gFalse);
    gwinListAddItem(ghList4, "Item 11", gFalse);
    gwinListAddItem(ghList4, "Item 12", gFalse);
    gwinListAddItem(ghList4, "Item 13", gFalse);
    gwinListSetScroll(ghList4, scrollSmooth);

    // Image
    wi.g.parent = ghPgImages;
    wi.g.x = wi.g.y = 0;
    wi.g.width = pagewidth;
    wi.g.height = gwinGetInnerHeight(ghPgImages) / 2;
    ghImage1 = gwinImageCreate(0, &wi.g);
    gwinImageOpenFile(ghImage1, "ugfx.bmp");
}

/**
 * Set the value of the labels
 */
static void setLabels(void)
{
    char tmp[20];

    // The sliders
    snprintg(tmp, sizeof(tmp), "%d%%", gwinSliderGetPosition(ghSlider1));
    gwinSetText(ghLabelSlider1, tmp, gTrue);
    snprintg(tmp, sizeof(tmp), "%d%%", gwinSliderGetPosition(ghSlider2));
    gwinSetText(ghLabelSlider2, tmp, gTrue);
    snprintg(tmp, sizeof(tmp), "%d%%", gwinSliderGetPosition(ghSlider3));
    gwinSetText(ghLabelSlider3, tmp, gTrue);
    snprintg(tmp, sizeof(tmp), "%d%%", gwinSliderGetPosition(ghSlider4));
    gwinSetText(ghLabelSlider4, tmp, gTrue);

    // The radio buttons
    if (gwinRadioIsPressed(ghRadio1)) {
        gwinSetText(ghLabelRadio1, "Yes", gTrue);
    } else if (gwinRadioIsPressed(ghRadio2)) {
        gwinSetText(ghLabelRadio1, "No", gTrue);
    }
}

/**
 * Control the progress bar auto-increment
 */
static void setProgressbar(gBool onoff)
{
    if (onoff) {
        gwinProgressbarStart(ghProgressbar1, 500);
    } else {
        gwinProgressbarStop(ghProgressbar1);		// Stop the progress bar
        gwinProgressbarReset(ghProgressbar1);
    }
}

/**
 * Set the enabled state of every widget (except the tabs etc)
 */
static void setEnabled(gBool ena)
{
    //gwinSetEnabled(ghPgControls, ena);
    gwinSetEnabled(ghPgSliders, ena);
    gwinSetEnabled(ghPgLabels, ena);
    gwinSetEnabled(ghPgRadios, ena);
    gwinSetEnabled(ghPgLists, ena);
    gwinSetEnabled(ghPgImages, ena);
    gwinSetEnabled(ghPgBounce, ena);
    gwinSetEnabled(ghPgMandelbrot, ena);
    // Checkboxes and Buttons we need to do individually so we don't disable the checkbox to re-enable everything
    gwinSetEnabled(ghButton1, ena);
    gwinSetEnabled(ghButton2, ena);
    gwinSetEnabled(ghButton3, ena);
    gwinSetEnabled(ghButton4, ena);
    gwinSetEnabled(ghCheckbox1, ena);
    gwinSetEnabled(ghCheckbox2, ena);
    gwinSetEnabled(ghCheckbox3, ena);
    //gwinSetEnabled(ghCheckDisableAll, gTrue);
}

static void FlashOffFn(void *param)
{
    (void)	param;

    gwinNoFlash(ghCheckbox3);
}

int main(void)
{
    GEvent 			*pe;

    // Initialize the display
    gfxInit();

    // Set the widget defaults
    font = gdispOpenFont("*");			// Get the first defined font.
    gwinSetDefaultFont(font);
    gwinSetDefaultStyle(&WhiteWidgetStyle, gFalse);
    gdispClear(GFX_WHITE);

    // Create the gwin windows/widgets
    createWidgets();

    // Assign toggles and dials to specific buttons & sliders etc.
#if GINPUT_NEED_TOGGLE
    gwinAttachToggle(ghButton1, 0, 0);
    gwinAttachToggle(ghButton2, 0, 1);
#endif
#if GINPUT_NEED_DIAL
    gwinAttachDial(ghSlider1, 0, 0);
    gwinAttachDial(ghSlider3, 0, 1);
#endif

    // Make the console visible
    gwinShow(ghConsole);
    gwinClear(ghConsole);

    // We want to listen for widget events
    geventListenerInit(&gl);
    gwinAttachListener(&gl);
    gtimerInit(&FlashTimer);

#if !GWIN_NEED_TABSET
    // Press the Tab we want visible
    gwinRadioPress(ghTabButtons);
#endif

    while (1) {
        // Get an Event
        pe = geventEventWait(&gl, gDelayForever);

        switch (pe->type) {
        case GEVENT_GWIN_BUTTON:
            gwinPrintf(ghConsole, "Button %s\n", gwinGetText(((GEventGWinButton *)pe)->gwin));
            break;

        case GEVENT_GWIN_SLIDER:
            gwinPrintf(ghConsole, "Slider %s=%d\n", gwinGetText(((GEventGWinSlider *)pe)->gwin), ((GEventGWinSlider *)pe)->position);
            break;

        case GEVENT_GWIN_CHECKBOX:
            gwinPrintf(ghConsole, "Checkbox %s=%s\n", gwinGetText(((GEventGWinCheckbox *)pe)->gwin), ((GEventGWinCheckbox *)pe)->isChecked ? "Checked" : "UnChecked");

            // If it is the Disable All checkbox then do that.
            if (((GEventGWinCheckbox *)pe)->gwin == ghCheckDisableAll) {
                gwinPrintf(ghConsole, "%s All\n", ((GEventGWinCheckbox *)pe)->isChecked ? "Disable" : "Enable");
                setEnabled(!((GEventGWinCheckbox *)pe)->isChecked);

                // If it is the toggle button checkbox start the flash.
            } else if (((GEventGWinCheckbox *)pe)->gwin == ghCheckbox3) {
                gwinFlash(ghCheckbox3);
                gtimerStart(&FlashTimer, FlashOffFn, 0, gFalse, 3000);
            }
            break;

        case GEVENT_GWIN_LIST:
            gwinPrintf(ghConsole, "List %s Item %d %s\n", gwinGetText(((GEventGWinList *)pe)->gwin), ((GEventGWinList *)pe)->item,
                       gwinListItemIsSelected(((GEventGWinList *)pe)->gwin, ((GEventGWinList *)pe)->item) ? "Selected" : "Unselected");
            break;

        case GEVENT_GWIN_RADIO:
            gwinPrintf(ghConsole, "Radio Group %u=%s\n", ((GEventGWinRadio *)pe)->group, gwinGetText(((GEventGWinRadio *)pe)->gwin));

            switch (((GEventGWinRadio *)pe)->group) {
#if !GWIN_NEED_TABSET
            case GROUP_TABS:

                // Set control visibility depending on the tab selected
                setTab(((GEventGWinRadio *)pe)->gwin);

                // We show the state of some of the GUI elements here
                setProgressbar(((GEventGWinRadio *)pe)->gwin == ghTabProgressbar);
                if (((GEventGWinRadio *)pe)->gwin == ghTabLabels) {
                    setLabels();
                }
                break;
#endif

            case GROUP_COLORS: {
                const GWidgetStyle	*pstyle;

                gwinPrintf(ghConsole, "Change Color Scheme\n");

                if (((GEventGWinRadio *)pe)->gwin == ghRadioYellow) {
                    pstyle = &YellowWidgetStyle;
                } else if (((GEventGWinRadio *)pe)->gwin == ghRadioBlack) {
                    pstyle = &BlackWidgetStyle;
                } else {
                    pstyle = &WhiteWidgetStyle;
                }

                // Clear the screen to the new color
                gdispClear(pstyle->background);

                // Update the style on all controls
                gwinSetDefaultStyle(pstyle, gTrue);
            }
            break;
            }
            break;

#if GWIN_NEED_TABSET
        case GEVENT_GWIN_TABSET:
            gwinPrintf(ghConsole, "TabPage %u (%s)\n", ((GEventGWinTabset *)pe)->nPage, gwinTabsetGetTitle(((GEventGWinTabset *)pe)->ghPage));

            // We show the state of some of the GUI elements here
            setProgressbar(((GEventGWinTabset *)pe)->ghPage == ghPgSliders);
            doMandlebrot(ghPgMandelbrot, ((GEventGWinTabset *)pe)->ghPage == ghPgMandelbrot);
            doBounce(ghPgBounce, ((GEventGWinTabset *)pe)->ghPage == ghPgBounce);
            if (((GEventGWinTabset *)pe)->ghPage == ghPgLabels) {
                setLabels();
            }
            break;
#endif

        default:
            gwinPrintf(ghConsole, "Unknown %d\n", pe->type);
            break;
        }
    }
    return 0;
}

