// SPDX-License-Identifier: MIT
//
// Copyright (c) 2021-2022, Fewnity - Grégory Machefer
//
// This file is part of Counter Strike Nintendo DS Multiplayer Edition (CS:DS)

#include "main.h"
#include "ui.h"
#include "movements.h"
#include "collisions.h"
#include "keyboard.h"
#include "network.h"
#include "party.h"
#include "player.h"
#include "input.h"
#include "ai.h"
#include "map.h"
#include "sounds.h"
#include "gun.h"
#include "equipment.h"
#include "grenade.h"
#include "tutorial.h"
#include "keyboard.h"
#include "draw3d.h"
#include "stats.h"
#include "debug.h"

enum shopCategory ShopCategory = PISTOLS; // 0 pistols, 1 Heavy, 2 sgm, 3 rifles, 4 equipment, 5 grenades

// Unused (Server list)
int selectedServer = 0;
int serverListOffset = 0;

// Current selected map in the map list menu
int currentSelectionMap = DUST2;

// Is showin the map in game
bool isShowingMap = false;
// Is showing the keyboard
bool isShowingKeyBoard = false;
// Is the gamepad in left mode
bool isLeftControls = false;

// Function to call to render the menu
void (*renderFunction)();
// Function to call after closing a menu
void (*onCloseMenu)();
// Function to call after closing a menu to open the last opened menu
void (*lastOpenedMenu)();
// Have to call the onCloseMenu function?
bool haveToCallOnCloseMenu = false;
// Is the menu using the quit button?
bool useQuitButton = false;
// Number of checkbox in the menu to render
int checkBoxToShow = 0;
// Number of Slider in the menu to render
int sliderToShow = 0;
// Current controls page (settings)
int controlsPage = 0; // [0,2]
// Error text to display
char errorText[ERROR_TEXT_LENGTH];

// Dialog text
char *dialogText = "";
// Game event notification text
char gameEventNotificationText[30] = "";
// Kill notification text
char killText[33] = "";

// for debug
int trace = 0;
float debugValue1 = -1;
float debugValue2 = -1;
float debugValue3 = -1;

// Is showing the final score of the party
bool showFinalScore = false;
// 0 Kill text, 1 connection/disconnection
int notificationType = 0;
// Show ping option
bool showPing = false;
// Number of frame remaining to show the shoot friend message
int showShootFriendMessage = 0;

float cursorTransparency = 1;

const int NULL_PARAMETER = -1; // A constant that skips setting the parameter variable when adding a button

void *AllocateLargestFreeBlock(size_t *Size)
{
    size_t s0, s1;
    void *p;

    s0 = ~(size_t)0 ^ (~(size_t)0 >> 1);

    while (s0 && (p = malloc(s0)) == NULL)
        s0 >>= 1;

    if (p)
        free(p);

    s1 = s0 >> 1;

    while (s1)
    {
        if ((p = malloc(s0 + s1)) != NULL)
        {
            s0 += s1;
            free(p);
        }
        s1 >>= 1;
    }

    while (s0 && (p = malloc(s0)) == NULL)
        s0 ^= s0 & -s0;

    *Size = s0;
    return p;
}

size_t GetFreeSize()
{
    size_t total = 0;
    void *pFirst = NULL;
    void *pLast = NULL;

    for (;;)
    {
        size_t largest;
        void *p = AllocateLargestFreeBlock(&largest);

        if (largest < sizeof(void *))
        {
            if (p != NULL)
                free(p);
            break;
        }

        *(void **)p = NULL;

        total += largest;

        if (pFirst == NULL)
            pFirst = p;

        if (pLast != NULL)
            *(void **)pLast = p;

        pLast = p;
    }

    while (pFirst != NULL)
    {
        void *p = *(void **)pFirst;
        free(pFirst);
        pFirst = p;
    }

    return total;
}

void SetCheckBoxToShow(int value)
{
    checkBoxToShow = value;
}

void SetSliderToShow(int value)
{
    sliderToShow = value;
}

enum shopCategory GetShopCategory()
{
    return ShopCategory;
}

/**
 * @brief  Print a long text in a box
 *
 * @param minX Left border position (in characters)
 * @param maxX Right border position (in characters)
 * @param y Top position (in lines)
 * @param text Text to print
 * @return int The number of lines printed
 */
int printLongText(int minX, int maxX, int y, char *text)
{
    // TempText is a line of text
    char tempText[maxX - minX];
    tempText[0] = '\0';

    // Store the text in ptrText to avoid modifying the original text (because of strtok)
    char ptrText[strlen(text) + 1];
    strcpy(ptrText, text);

    // Cut the text every spaces
    char *ptr = strtok(ptrText, " ");
    int currentLine = 0;
    // While there is still text to cut
    while (ptr != NULL)
    {
        // If the tempText with the next word to add is too long, print the tempText and start a new line
        int TempTextLen = strlen(tempText);
        if (TempTextLen > 0 && TempTextLen + 1 + strlen(ptr) > maxX - minX)
        {
            // Get the middle positon of the text in the box
            int fixedXPos = centerPositionOfAText(minX, maxX, TempTextLen);
            NE_TextPrint(0,                          // Font slot
                         fixedXPos, y + currentLine, // Coordinates x(column), y(row)
                         NE_White,                   // Color
                         tempText);

            for (int i = 0; i < TempTextLen; i++)
                tempText[i] = '\0';

            currentLine++;
        }

        // Copy the next word to print in the tempText variable
        sprintf(tempText + strlen(tempText), ptr);
        sprintf(tempText + strlen(tempText), " ");

        // Check for a new word
        ptr = strtok(NULL, " ");
    }

    // Print the last line
    int fixedXPos = centerPositionOfAText(minX, maxX, strlen(tempText));
    NE_TextPrint(0,                          // Font slot
                 fixedXPos, y + currentLine, // Coordinates x(column), y(row)
                 NE_White,                   // Color
                 tempText);

    return currentLine + 1;
}

/**
 * @brief Print a long text in a box
 *
 * @param minX Left border position (in characters)
 * @param maxX Right border position (in characters)
 * @param y Top position (in lines)
 * @param text Text to print
 * @return int The number of lines printed
 */
int printLongConstChar(int minX, int maxX, int y, const char *text)
{
    return printLongText(minX, maxX, y, (char *)text);
}

/**
 * @brief Get the middle positon of a text in a box
 *
 * @param xMin Left border position (in characters)
 * @param xMax Right border position (in characters)
 * @param textLength Text length (in characters)
 * @return int The middle position of the text (in characters)
 */
int centerPositionOfAText(int xMin, int xMax, int textLength)
{
    return xMin + ceil((xMax - xMin) / 2.0) - floor(textLength / 2.0);
}

/**
 * @brief Set dialog text
 *
 * @param text
 */
void setDialogText(char *text)
{
    // Copy the text to another text variable
    dialogText = text;
}

/**
 * @brief Draw the dialog box
 *
 */
void showDialog()
{
    // If the text is not empty and a menu is not on the top screen
    if (strlen(dialogText) > 0 && !isShowingKeyBoard && !isShowingMap)
    {
        NE_PolyFormat(31, 0, NE_LIGHT_0, NE_CULL_BACK, NE_MODULATION);
        int ligneCount = printLongText(1, 30, 1, dialogText);
        NE_PolyFormat(20, 0, NE_LIGHT_0, NE_CULL_BACK, NE_MODULATION);
        NE_2DDrawQuad(0, 0, 256, (ligneCount + 2) * 8, 1, RGB15(0, 0, 0));
    }
}

/**
 * @brief  To call before changing the menu
 *
 * @param menuToShow Menu to show
 */
void startChangeMenu(enum UiMenu menuToShow)
{
    // Reset UI
    SetButtonToShow(0);
    SetCheckBoxToShow(0);
    SetSliderToShow(0);
    isShowingKeyBoard = false;

    // Call the on close function
    if (haveToCallOnCloseMenu)
    {
        haveToCallOnCloseMenu = false;
        onCloseMenu();
    }

    // Re enable all buttons
    for (int i = 0; i < ButtonCount; i++)
    {
        AllButtons[i].isHidden = false;
    }

    // Set current menu
    currentMenu = menuToShow;

    // Refresh the screen
    UpdateBottomScreenFrameCount += 8;
}

/**
 * @brief Open a menu
 *
 * @param menuId Menu id to open
 */
void ChangeMenu(int menuId)
{
    switch(menuId)
    {
        case GAME:
            initGameMenu();
            break;
        case SCORE_BOARD: // Score board screen
            initScoreMenu();
            break;
        case SHOPCATEGORIES: // Shop categories
            initShopCategoriesMenu();
            break;
        case SETTINGS: // Settings menu
            initSettingsMenu();
            break;
        case QUIT: // Quit menu
            initQuitMenu();
            break;
        case SHOP: // SHOP
            initShopMenu();
            break;
        case CONTROLLER: // Game pad menu
            initControllerMenu();
            break;
        case MAIN:
            initMainMenu();
            break;
        case CHANGENAME: // Change name menu
            initChangeNameMenu();
            break;
        case SHOP: // SHOP
            initShopMenu();
            break;
        case CHAT: // Online chat
            initChatMenu();
            break;
        case CONTROLSSETTINGS: // Controls settings
            initControlsSettingsMenu();
            break;
        case CONTROLSCHANGE: // Controls customization
            initControlsChangeMenu();
            break;
        case MAP_SELECTION_IMAGE:
            initSelectionMapImageMenu();
            break;
        case MAP_SELECTION_LIST:
            initSelectionMapListMenu();
            break;
        case PARTY_MODE_SELECTION_LIST:
            // Not implemented
            break;
        case FINAL_SCORE:
            initFinalScoreMenu();
            break;
        case JOIN_CREATE_PARTY:
            initJoinCreatePartyMenu();
            break;
        case ENTER_CODE:
            initEnterCodeMenu();
            break;
    }
}

/**
 * @brief Open the shop category
 *
 * @param categoryId
 */
void OpenShopCategory(int categoryId)
{
    SetSelectedGunShop(-1);
    ShopCategory = categoryId;
    initShopMenu();
    ChangeShopElement(0);
}

/**
 * @brief Disable or enable the map
 *
 */
void changeMapState()
{
    isShowingMap = !isShowingMap;
    ForceUpdateLookRotation(localPlayer->cameraAngle);
}

/**
 * @brief Close the map
 *
 */
void closeMap()
{
    isShowingMap = false;
}

/**
 * @brief Buy a weapon (In online the game will send a packet to the server)
 *
 * @param unused
 */
void BuyWeapon(int unused)
{
    PlayBasicSound(SFX_KEYBOARD_SOUND);
    DisableAim();
    if (Connection != OFFLINE)
    {
        SetSendBuyWeapon(true);
    }
    else
    {
        buyGun();
    }
}

/**
 * @brief Change the selected element in the shop
 *
 * @param Left
 */
void ChangeShopElement(int Left)
{
    int Selected = GetSelectedGunShop();
    bool FoundNewGun = false;
    int FirstGunFound = -1;
    if (Left == 0)
    {
        // Go to the next element of the same category and team
        for (int i = 0; i < GunCount + equipementCount + shopGrenadeCount; i++)
        {
            if ((ShopCategory < EQUIPMENT && i < GunCount && (AllGuns[i].gunCategory == ShopCategory && (AllPlayers[0].Team == AllGuns[i].team || AllGuns[i].team == -1))) || (ShopCategory == GRENADES && i >= GunCount && i < GunCount + shopGrenadeCount && (AllPlayers[0].Team == GetAllGrenades()[i - GunCount].team || GetAllGrenades()[i - GunCount].team == -1)) || (ShopCategory == EQUIPMENT && i >= GunCount + shopGrenadeCount && !allEquipments[i - GunCount - shopGrenadeCount].isHided && (AllPlayers[0].Team == allEquipments[i - GunCount - shopGrenadeCount].team || allEquipments[i - GunCount - shopGrenadeCount].team == -1)))
            {
                if (FirstGunFound == -1)
                    FirstGunFound = i;

                if (i > Selected)
                {
                    Selected = i;
                    FoundNewGun = true;
                    break;
                }
            }
        }
    }
    else
    {
        // Go to the next element of the same category and team
        for (int i = GunCount + equipementCount + shopGrenadeCount - 1; i > -1; i--)
        {
            if ((ShopCategory < EQUIPMENT && i < GunCount && (AllGuns[i].gunCategory == ShopCategory && (AllPlayers[0].Team == AllGuns[i].team || AllGuns[i].team == -1))) || (ShopCategory == GRENADES && i >= GunCount && i < GunCount + shopGrenadeCount && (AllPlayers[0].Team == GetAllGrenades()[i - GunCount].team || GetAllGrenades()[i - GunCount].team == -1)) || (ShopCategory == EQUIPMENT && i >= GunCount + shopGrenadeCount && !allEquipments[i - GunCount - shopGrenadeCount].isHided && (AllPlayers[0].Team == allEquipments[i - GunCount - shopGrenadeCount].team || allEquipments[i - GunCount - shopGrenadeCount].team == -1)))
            {
                if (FirstGunFound == -1)
                    FirstGunFound = i;

                if (i < Selected)
                {
                    Selected = i;
                    FoundNewGun = true;
                    break;
                }
            }
        }
    }

    if (!FoundNewGun)
        Selected = FirstGunFound;

    // Update the texture
    CreateShopMenuNE();
    SetSelectedGunShop(Selected);

    // Refresh the screen
    SetUpdateBottomScreenOneFrame(4);
}

void AskGameMode()
{
    if (allMaps[currentSelectionMap].forcePartyMode == -1)
    {
        initPartyModeSelectionMenu();
    }
    else
    {
        StartSinglePlayer(allMaps[currentSelectionMap].forcePartyMode);
    }
}

/**
 * @brief Change map in the map selection screen
 *
 * @param Left
 */
void ChangeMap(int Left)
{
    int nextIndex = currentSelectionMap;
    nextIndex += (Left) ? 1 : -1;
    currentSelectionMap = wrapi(nextIndex, 0, MAP_COUNT);

    // Update texture
    NE_PaletteDelete(Palettes[10]);
    NE_MaterialDelete(BottomScreenSpritesMaterials[6]);
    BottomScreenSpritesMaterials[6] = NE_MaterialCreate();
    Palettes[10] = NE_PaletteCreate();
    NE_MaterialTexLoadBMPtoRGB256(BottomScreenSpritesMaterials[6], Palettes[10], allMaps[currentSelectionMap].image, 1);
}

void setQuitButton(bool value)
{
    useQuitButton = value;
    // Set button sprite
    NE_SpriteVisible(BottomScreenSprites[0], value);
}

/**
 * @brief Set local player team (In online the game will send a packet to the server)
 *
 * @param i
 */
void SetTeam(int i)
{
    if (applyRules)
    {
        AllPlayers[0].Team = i;
        UpdateBottomScreenFrameCount += 8;
    }
    else if (!applyRules)
    {
        SetWaitForTeamResponse(true);
        AllButtons[1].isHidden = true;
        AllButtons[2].isHidden = true;
        SetTempTeam(i);
        SetSendTeam(true);
    }
}

/**
 * @brief Change the controls page in the settings controls menu
 *
 * @param isLeft
 */
void changeControlsPage(int isLeft)
{
    int nextIndex = currentSelectionMap;
    nextIndex += (Left) ? 1 : -1;
    controlsPage = wrapi(nextIndex, 0, CONTROLS_PAGE_COUNT);
    
    // Refresh the menu
    initControlsChangeMenu();
}

/**
 * @brief Show party event notification
 *
 * @param event Event type see TextEnum in ui.h
 */
void showPartyEventText(int event)
{
    switch(event)
    {
        case TERRORISTS_WIN:
            strncpy(gameEventNotificationText, "Terrorists win", 30);
            PlayBasicSound(SFX_TERRORISTSWIN);
            break;
        case COUNTER_TERRORISTS_WIN:
            strncpy(gameEventNotificationText, "Counter-Terrorists win", 30);
            PlayBasicSound(SFX_COUNTERTERRORISTWIN);
            break;
        case BOMB_PLANTED:
            strncpy(gameEventNotificationText, "Bomb has been planted", 30);
            PlayBasicSound(SFX_BOMBHASBEENPLANTED);
            break;
        case BOMB_DEFUSED:
            strncpy(gameEventNotificationText, "Bomb has been defused", 30);
            PlayBasicSound(SFX_BOMBHASBEENDEFUSED);
            break;
    }

    // Set the timer
    textToShowTimer = 360; // 6sec * 60 frames
}

/**
 * @brief Show kill text between two players
 *
 * @param killerIndex Killer index
 * @param deadPlayerIndex Dead player index
 */
void showKillText(int killerIndex, int deadPlayerIndex)
{
    sprintf(killText, "{%d%s {2killed {%d%s", (int)AllPlayers[killerIndex].Team, AllPlayers[killerIndex].name, (int)AllPlayers[deadPlayerIndex].Team, AllPlayers[deadPlayerIndex].name);

    KillTextShowTimer = 240;
    notificationType = 0;
}

/**
 * @brief Show connected notification
 *
 * @param connectedPlayerIndex Player index
 */
void showConnectedText(int connectedPlayerIndex)
{
    sprintf(killText, "%s connected", AllPlayers[connectedPlayerIndex].name);

    KillTextShowTimer = 240;
    notificationType = 1;
}

/**
 * @brief Show disconnected notification
 *
 * @param disconnectedPlayerIndex Player index
 */
void showDisconnectedText(int disconnectedPlayerIndex)
{
    sprintf(killText, "%s disconnected", AllPlayers[disconnectedPlayerIndex].name);

    KillTextShowTimer = 240;
    notificationType = 1;
}

/**
 * @brief Start input scan for the controls menu
 *
 * @param inputIndex Input index to scan
 */
void startScanForInput(int inputIndex)
{
    stopScanForInput();

    if (inputs[inputIndex].value != -1) // Unassign input
    {
        inputs[inputIndex].value = -1;
        inputs[inputIndex].nameIndex = 12;
    }
    else if (inputs[inputIndex].nameIndex != 13) // if the input to scan is not in scan mode, start scan for it
    {
        scanForInput = true;
        currentInputScanned = inputIndex;
        inputs[inputIndex].nameIndex = 13;
    }
}

/**
 * @brief Stop input scan for the controls menu
 *
 */
void stopScanForInput()
{
    // Disable current scanning input
    if (inputs[currentInputScanned].nameIndex == 14 || inputs[currentInputScanned].nameIndex == 13)
    {
        inputs[currentInputScanned].nameIndex = 12;
    }

    scanForInput = false;
}

/**
 * @brief Set keyboard mode button text
 *
 */
void updateKeyboardModeButton()
{
    AllButtons[2].text = (isAzerty) ? "Azerty" : "Qwerty";
}

/**
 * @brief Draw keyboard
 *
 */
void drawKeyboardInput()
{
    NE_2DDrawQuad(0, 0, ScreenWidth, ScreenHeightFixed, 3, RGB15(3, 3, 3)); // Background
    switch (currentMenu) 
    {
        case CHANGENAME:
            NE_TextPrint(0,        // Font slot
                     11, 2,    // Coordinates x(column), y(row)
                     NE_White, // Color
                     "Change name");

            char editedName[21];

            sprintf(editedName, "%s", tempText);

            int nameLength = strlen(editedName);
            for (int i = nameLength; i < maxInputLength; i++)
            {
                sprintf(editedName + nameLength, "_");
                nameLength++;
            }

            NE_TextPrint(0,        // Font slot
                        10, 12,   // Coordinates x(column), y(row)
                        NE_White, // Color
                        editedName);
            break;
        case ENTER_CODE:
            NE_TextPrint(0,        // Font slot
                        11, 2,    // Coordinates x(column), y(row)
                        NE_White, // Color
                        "Enter code");

            char code[21];

            sprintf(code, "%s", tempText);

            int codeLength = strlen(code);
            for (int i = codeLength; i < maxInputLength; i++)
            {
                sprintf(code + codeLength, "_");
                codeLength++;
            }

            NE_TextPrint(0,        // Font slot
                        14, 12,   // Coordinates x(column), y(row)
                        NE_White, // Color
                        code);
            break;
    }
}

/**
 * @brief Draw top screen
 *
 */
void drawTopScreenUI()
{
    Player *selectPlayer = &AllPlayers[CurrentCameraPlayer];

    // Set view in 2D mode
    NE_2DViewInit();

    // Only for debug
    int TextYOffset = 0;
    if (!isDebugTopScreen)
        TextYOffset = -3;

    for (int i = 1; i < MaxPlayer; i++)
        if (AllPlayers[i].mapVisivilityTimer > 0)
            AllPlayers[i].mapVisivilityTimer--;

    if (isShowingKeyBoard)
    {
        drawKeyboardInput();
    }
    else if (isShowingMap)
    {
        NE_2DDrawQuad(0, 0, ScreenWidth, ScreenHeightFixed, 3, RGB15(0, 0, 0)); // Background

        // Calculate map position
        float mapXPos = map(selectPlayer->position.x, -46, 57.5, -170, 170);
        float mapYPos = map(selectPlayer->position.z, -41, 67, -177, 177);

        // Set map position and draw sprite
        NE_SpriteSetPos(TopScreenSprites[1], ScreenCenterWidth - 170 / 2 + (mapXPos * zWithoutYForMap - mapYPos * xWithoutYForMap) / 2, ScreenCenterHeight - 177 / 2 + 2 + (mapYPos * zWithoutYForMap + mapXPos * xWithoutYForMap) / 2);
        NE_SpriteSetRot(TopScreenSprites[1], (int)selectPlayer->Angle);
        NE_SpriteDraw(TopScreenSprites[1]);

        for (int i = 1; i < MaxPlayer; i++)
        {
            // Draw other player if he is alive, visible or in the same team as the local player or if the local player is dead in a cusual party mode
            if (AllPlayers[i].Id != UNUSED && AllPlayers[i].Team != SPECTATOR && !AllPlayers[i].IsDead && (AllPlayers[i].Team == localPlayer->Team || AllPlayers[i].mapVisivilityTimer > 0 || (currentPartyMode == 1 && localPlayer->IsDead)))
            {
                // Calculate other players points
                float xPos2 = map(AllPlayers[i].position.x, -46, 57.5, -170, 170);
                float ypos2 = map(AllPlayers[i].position.z, -41, 67, -177, 177);
                float xPoint = ScreenCenterWidth - ((xPos2 - mapXPos) * zWithoutYForMap + (ypos2 - mapYPos) * -xWithoutYForMap) / 2.0;
                float yPoint = ScreenCenterHeight + ((ypos2 - mapYPos) * -zWithoutYForMap + (xPos2 - mapXPos) * -xWithoutYForMap) / 2.0;
                float finalXPoint1 = xPoint - 4, finalYPoint1 = yPoint - 2;
                float finalXPoint2 = xPoint + 2, finalYPoint2 = yPoint + 4;

                // Set point color and draw it
                if (AllPlayers[i].Team == AllPlayers[0].Team)
                    NE_2DDrawTexturedQuadColor(finalXPoint1, finalYPoint1, finalXPoint2, finalYPoint2, 1, BottomScreenSpritesMaterials[2], RGB15(0, 31, 31)); // Friends points
                else
                    NE_2DDrawTexturedQuadColor(finalXPoint1, finalYPoint1, finalXPoint2, finalYPoint2, 1, BottomScreenSpritesMaterials[2], RGB15(31, 0, 0)); // Enemies points
            }
        }

        if (bombDropped)
        {
            float bombXPos = map(droppedBombPositionAndRotation.x, -46, 57.5, -170, 170);
            float bombYpos = map(droppedBombPositionAndRotation.z, -41, 67, -177, 177);
            float bombXPoint = ScreenCenterWidth - ((bombXPos - mapXPos) * zWithoutYForMap + (bombYpos - mapYPos) * -xWithoutYForMap) / 2.0;
            float bombYPoint = ScreenCenterHeight + ((bombYpos - mapYPos) * -zWithoutYForMap + (bombXPos - mapXPos) * -xWithoutYForMap) / 2.0;
            float bombFinalXPoint1 = bombXPoint - 4, bombFinalYPoint1 = bombYPoint - 2;
            float bombFinalXPoint2 = bombXPoint + 4, bombFinalYPoint2 = bombYPoint + 4;

            NE_2DDrawTexturedQuadColor(bombFinalXPoint1, bombFinalYPoint1, bombFinalXPoint2, bombFinalYPoint2, 1, TopScreenSpritesMaterials[5], RGB15(31, 0, 0)); // Bomb
        }

        NE_2DDrawTexturedQuadColor(ScreenCenterWidth - 4, ScreenCenterHeight - 2, ScreenCenterWidth + 2, ScreenCenterHeight + 4, 1, BottomScreenSpritesMaterials[2], RGB15(0, 31, 0)); // Background
    }
    else
    {
        // Draw teams indicators
        if (!isInTutorial && currentMap != TUTORIAL)
        {
            int counterCount = 0;
            int terroristCount = 0;
            for (int i = 0; i < MaxPlayer; i++)
            {
                if (AllPlayers[i].Team == SPECTATOR || AllPlayers[i].Id == UNUSED)
                    continue;

                // Add an offset (right or left position for the point)
                int xoffset = 28 + terroristCount * 8;

                // Set grey color for dead players
                int color = ColorTerroristsTeam;
                if (AllPlayers[i].Team == COUNTERTERRORISTS)
                {
                    color = ColorCounterTeam;
                    xoffset = -32 + counterCount * -8;
                    counterCount++;
                }
                else
                {
                    terroristCount++;
                }

                // If the player is dead, set the color to grey
                if (AllPlayers[i].IsDead)
                    color = RGB15(7, 7, 7);

                // Draw point
                NE_2DDrawQuad(ScreenCenterWidth + xoffset, 4, ScreenCenterWidth + 3 + xoffset, 7, 1, color);
                NE_2DDrawQuad(ScreenCenterWidth - 1 + xoffset, 5, ScreenCenterWidth - 1 + 5 + xoffset, 6, 1, color);
                NE_2DDrawQuad(ScreenCenterWidth + 1 + xoffset, 3, ScreenCenterWidth + 2 + xoffset, 8, 1, color);
            }
        }

        // if (true)
        if (isDebugTopScreen)
        {
            // DEBUG show player position
            char PositionText[30];
            sprintf(PositionText, "%f %f %f", localPlayer->position.x, localPlayer->position.y, localPlayer->position.z);
            NE_TextPrint(0,        // Font slot
                         1, 0,     // Coordinates x(column), y(row)
                         NE_White, // Color
                         PositionText);

            // DEBUG show CPU usage
            char CPU[40];

            // sprintf(CPU, "CPU: %d%%, Mem: %d%%", NE_GetCPUPercent(), NE_TextureFreeMemPercent());
            // sprintf(CPU, "%0.3f %0.3f %0.3f", debugValue1, debugValue2, debugValue3);
            // sprintf(CPU, "FREE MEM %d", GetFreeSize());
            sprintf(CPU, "CON %d", Connection);
            // sprintf(CPU, "%0.3f %0.3f %0.3f", debugValue1, debugValue2, debugValue3);
            //  sprintf(CPU, "%d %d", AllGuns[selectPlayer->AllGunsInInventory[selectPlayer->currentGunInInventory]].isDualGun, selectPlayer->AllGunsInInventory[selectPlayer->currentGunInInventory]);

            NE_TextPrint(0,        // Font slot
                         1, 1,     // Coordinates x(column), y(row)
                         NE_White, // Color
                         CPU);

            char CPU2[120] = "";
            for (int i = 0; i < MaxPlayer; i++)
            {
                sprintf(CPU2 + strlen(CPU2), "%s %d %d\n", AllPlayers[i].name, AllPlayers[i].Health, AllPlayers[i].armor);
                //  sprintf(CPU2 + strlen(CPU2), "%s %d %d\n", AllPlayers[i].name, AllPlayers[i].Health, AllPlayers[i].armor);
                // sprintf(CPU2 + strlen(CPU2), "%s t%d i%d i%d\n", AllPlayers[i].name, AllPlayers[i].Team, AllPlayers[i].Id, AllPlayers[i].client.id);
                // sprintf(CPU2 + strlen(CPU2), "%s %d %d\n", AllPlayers[i].name, AllPlayers[i].target, AllPlayers[i].lastSeenTarget);
            }

            NE_TextPrint(0,        // Font slot
                         0, 9,     // Coordinates x(column), y(row)
                         NE_White, // Color
                         CPU2);

            // DEBUG show map zone to show
            char Occlusion[20];
            sprintf(Occlusion, "Zone to show: ");
            for (int i = 0; i < allMaps[currentMap].AllZones[localPlayer->CurrentOcclusionZone].ZoneCount; i++)
                sprintf(Occlusion + strlen(Occlusion), "%d ", allMaps[currentMap].AllZones[localPlayer->CurrentOcclusionZone].visibleMapPart[i]);

            NE_TextPrint(0,        // Font slot
                         1, 2,     // Coordinates x(column), y(row)
                         NE_White, // Color
                         Occlusion);
        }

        if (!BombPlanted)
        {
            if (!allPartyModes[currentPartyMode].infiniteTimer && (!localPlayer->IsDead || roundState == TRAINING) && !partyFinished)
            {
                char Timer[30];
                int timerXOffset = 0;
                if (roundState == TRAINING && Connection != OFFLINE)
                {
                    if (PlayerCount != 1)
                    {
                        char forceLaunchText[31];
                        sprintf(forceLaunchText, "Start to vote force launch %d/%d", playerWantToStart, playerWantToStartLimite);
                        int backgroundSize = strlen(forceLaunchText) * 8;
                        int backgroundOffset = 8; // 1*8
                        NE_PolyFormat(13, 0, NE_LIGHT_0, NE_CULL_BACK, NE_MODULATION);
                        NE_2DDrawQuad(backgroundOffset - 1, (6 + TextYOffset) * 8 - 1, backgroundOffset + backgroundSize, (7 + TextYOffset) * 8 + 1, 1, RGB15(3, 3, 3));
                        NE_PolyFormat(31, 0, NE_LIGHT_0, NE_CULL_BACK, NE_MODULATION);

                        NE_TextPrint(0,                  // Font slot
                                     1, 6 + TextYOffset, // Coordinates x(column), y(row)
                                     NE_White,           // Color
                                     forceLaunchText);
                    }

                    sprintf(Timer, "Waiting for players ");

                    NE_PolyFormat(13, 0, NE_LIGHT_0, NE_CULL_BACK, NE_MODULATION);
                    NE_2DDrawQuad(ScreenCenterWidth - 105, 8, ScreenCenterWidth + 104, 16, 1, RGB15(3, 3, 3));
                    NE_PolyFormat(31, 0, NE_LIGHT_0, NE_CULL_BACK, NE_MODULATION);
                    timerXOffset = 10;
                }
                else
                {
                    NE_PolyFormat(13, 0, NE_LIGHT_0, NE_CULL_BACK, NE_MODULATION);
                    NE_2DDrawQuad(ScreenCenterWidth - 25, 8, ScreenCenterWidth + 24, 16, 1, RGB15(3, 3, 3));
                    NE_PolyFormat(31, 0, NE_LIGHT_0, NE_CULL_BACK, NE_MODULATION);
                    strncpy(Timer, "", 30);
                }

                // Show party timer if bomb is not planted
                sprintf(Timer + strlen(Timer), "%02d::%02d", PartyMinutes, PartySeconds);

                if (isDebugTopScreen)
                    sprintf(Timer + strlen(Timer), " State: %d", roundState);

                NE_TextPrint(0,                                  // Font slot
                             13 - timerXOffset, 4 + TextYOffset, // Coordinates x(column), y(row)
                             NE_White,                           // Color
                             Timer);
            }

            // Show planting bomb animation
            if (localPlayer->bombTimer < bombPlantingTime && localPlayer->bombTimer > 0 && !localPlayer->IsDead)
            {
                // Draw Background
                NE_PolyFormat(20, 0, NE_LIGHT_0, NE_CULL_BACK, NE_MODULATION);
                NE_2DDrawQuad(ScreenWidthDivided4, 155, ScreenWidth - mapInt(localPlayer->bombTimer, -2, bombPlantingTime - 0, ScreenWidthDivided4 * 3, ScreenWidthDivided4), 170, 1, RGB15(3, 3, 3));
                NE_PolyFormat(24, 0, NE_LIGHT_0, NE_CULL_BACK, NE_MODULATION);
                NE_2DDrawQuad(84, 140, 176, 155, 1, RGB15(3, 3, 3));
                NE_PolyFormat(31, 0, NE_LIGHT_0, NE_CULL_BACK, NE_MODULATION);

                // Show planting text
                char plantingText[12];
                sprintf(plantingText, "Planting...");
                NE_TextPrint(0,        // Font slot
                             11, 18,   // Coordinates x(column), y(row)
                             NE_White, // Color
                             plantingText);
            }
        }
        else
        {
            // Show defusing bomb animation
            if (localPlayer->bombTimer < bombDefuseTime && localPlayer->bombTimer > 0 && !localPlayer->IsDead)
            {
                // Draw Background
                NE_PolyFormat(20, 0, NE_LIGHT_0, NE_CULL_BACK, NE_MODULATION);
                NE_2DDrawQuad(ScreenWidthDivided4, 155, ScreenWidth - mapInt(localPlayer->bombTimer, -2, bombDefuseTime, ScreenWidth / 4 * 3, ScreenWidthDivided4), 170, 1, RGB15(3, 3, 3));
                NE_PolyFormat(24, 0, NE_LIGHT_0, NE_CULL_BACK, NE_MODULATION);
                NE_2DDrawQuad(84, 140, 176, 155, 1, RGB15(3, 3, 3));
                NE_PolyFormat(31, 0, NE_LIGHT_0, NE_CULL_BACK, NE_MODULATION);

                // Show defusing text
                char defusingText[12];
                sprintf(defusingText, "Defusing...");
                NE_TextPrint(0,        // Font slot
                             11, 18,   // Coordinates x(column), y(row)
                             NE_White, // Color
                             defusingText);
            }
        }

        if (!allPartyModes[currentPartyMode].noScore)
        {
            NE_PolyFormat(13, 0, NE_LIGHT_0, NE_CULL_BACK, NE_MODULATION);
            NE_2DDrawQuad(ScreenCenterWidth - 25, 0, ScreenCenterWidth + 24, 8, 1, RGB15(3, 3, 3));
            NE_PolyFormat(31, 0, NE_LIGHT_0, NE_CULL_BACK, NE_MODULATION);
            // Show party score
            char PartyScore[15];
            //  CountersT score
            sprintf(PartyScore, "%02d  ", CounterScore);

            NE_TextPrint(0,                   // Font slot
                         13, 3 + TextYOffset, // Coordinates x(column), y(row)
                         ColorCounterTeam,    // Color
                         PartyScore);

            // Terrosists score
            sprintf(PartyScore, "%02d", TerroristsScore);

            NE_TextPrint(0,                   // Font slot
                         17, 3 + TextYOffset, // Coordinates x(column), y(row)
                         ColorTerroristsTeam, // Color
                         PartyScore);
        }

        if (Connection != OFFLINE && showPing)
        {
            char PING[8];
            sprintf(PING, "Ping %d", ping);
            // DEBUG for player show ping
            int backgroundSize = strlen(PING) * 8;
            int backgroundOffset = 0; // 1 * 8
            NE_PolyFormat(13, 0, NE_LIGHT_0, NE_CULL_BACK, NE_MODULATION);
            NE_2DDrawQuad(backgroundOffset, 0, backgroundOffset + backgroundSize, (1) * 8 + 1, 1, RGB15(3, 3, 3));
            NE_PolyFormat(31, 0, NE_LIGHT_0, NE_CULL_BACK, NE_MODULATION);

            NE_TextPrint(0,        // Font slot
                         0, 0,     // Coordinates x(column), y(row)
                         NE_White, // Color
                         PING);
        }

        // Show event text
        if (textToShowTimer > 0)
        {
            int backgroundSize = strlen(gameEventNotificationText) * 8;
            int backgroundOffset = 1;
            NE_PolyFormat(13, 0, NE_LIGHT_0, NE_CULL_BACK, NE_MODULATION);
            NE_2DDrawQuad(backgroundOffset, (7 + TextYOffset) * 8 - 1, backgroundOffset + backgroundSize, (8 + TextYOffset) * 8 + 1, 1, RGB15(3, 3, 3));
            NE_PolyFormat(31, 0, NE_LIGHT_0, NE_CULL_BACK, NE_MODULATION);

            NE_TextPrint(0,                  // Font slot
                         0, 7 + TextYOffset, // Coordinates x(column), y(row)
                         NE_White,           // Color
                         gameEventNotificationText);
        }

        // Show kill text
        if (KillTextShowTimer > 0)
        {
            int currentLine = 0;
            if (notificationType == 0)
            {
                char DamageText[50];
                sprintf(DamageText, killText);

                char *ptr = strtok(DamageText, "{");

                while (ptr != NULL)
                {
                    if (strncmp(ptr, "1", 1) == 0)
                    {
                        NE_TextPrint(0,                            // Font slot
                                     currentLine, 8 + TextYOffset, // Coordinates x(column), y(row)
                                     ColorCounterTeamLiter,        // Color
                                     ptr + 1);
                    }
                    else if (strncmp(ptr, "0", 1) == 0)
                    {
                        NE_TextPrint(0,                            // Font slot
                                     currentLine, 8 + TextYOffset, // Coordinates x(column), y(row)
                                     ColorTerroristsTeam,          // Color
                                     ptr + 1);
                    }
                    else
                    {
                        NE_TextPrint(0,                            // Font slot
                                     currentLine, 8 + TextYOffset, // Coordinates x(column), y(row)
                                     NE_White,                     // Color
                                     ptr + 1);
                    }

                    currentLine += strlen(ptr) - 1;
                    ptr = strtok(NULL, "{");
                }
            }
            else if (notificationType == 1)
            {
                NE_TextPrint(0,                  // Font slot
                             0, 8 + TextYOffset, // Coordinates x(column), y(row)
                             RGB15(31, 31, 31),  // Color
                             killText);

                currentLine = strlen(killText);
            }

            int backgroundSize = currentLine * 8;
            int backgroundOffset = 0;
            NE_PolyFormat(13, 0, NE_LIGHT_0, NE_CULL_BACK, NE_MODULATION);
            NE_2DDrawQuad(backgroundOffset, (8 + TextYOffset) * 8 - 1, backgroundOffset + backgroundSize, (9 + TextYOffset) * 8 + 1, 1, RGB15(3, 3, 3));
            NE_PolyFormat(31, 0, NE_LIGHT_0, NE_CULL_BACK, NE_MODULATION);
        }

        if (redHealthTextCounter != 0)
        {
            redHealthTextCounter--;
        }

        if (showShootFriendMessage != 0)
        {
            showShootFriendMessage--;
            NE_PolyFormat(13, 0, NE_LIGHT_0, NE_CULL_BACK, NE_MODULATION);
            NE_2DDrawQuad(31, 160, 224, 168, 1, RGB15(11, 0, 0));
            NE_PolyFormat(31, 0, NE_LIGHT_0, NE_CULL_BACK, NE_MODULATION);

            NE_TextPrint(0,        // Font slot
                         4, 20,    // Coordinates x(column), y(row)
                         NE_White, // Color
                         "Don't shoot your friends!");
        }

        if (selectPlayer->Team != SPECTATOR)
        {
            if (!selectPlayer->IsDead)
            {
                if (CurrentCameraPlayer != 0)
                {
                    int selectedPlayerNameLength = strlen(selectPlayer->name);
                    int backgroundSize = selectedPlayerNameLength * 8;
                    int textPosition = maxCharAtScreenDivided2 - selectedPlayerNameLength / 2;
                    int backgroundOffset = textPosition * 8;
                    NE_PolyFormat(13, 0, NE_LIGHT_0, NE_CULL_BACK, NE_MODULATION);
                    NE_2DDrawQuad(backgroundOffset, 168, backgroundOffset + backgroundSize, 176, 1, RGB15(3, 3, 3));
                    NE_PolyFormat(31, 0, NE_LIGHT_0, NE_CULL_BACK, NE_MODULATION);

                    char ViewedBotName[21] = "";
                    sprintf(ViewedBotName, "%s", selectPlayer->name);
                    NE_TextPrint(0,                // Font slot
                                 textPosition, 21, // Coordinates x(column), y(row)
                                 NE_White,         // Color
                                 ViewedBotName);
                }

                // Show gun ammo
                if (selectPlayer->currentGunInInventory == 1 || selectPlayer->currentGunInInventory == 2)
                {
                    char AmmoText[16];
                    sprintf(AmmoText, "%d/%d", selectPlayer->AllAmmoMagazine[selectPlayer->currentGunInInventory - 1].AmmoCount, selectPlayer->AllAmmoMagazine[selectPlayer->currentGunInInventory - 1].TotalAmmoCount);

                    int backgroundSize = strlen(AmmoText) * 8;
                    int backgroundOffset = 8;
                    NE_PolyFormat(13, 0, NE_LIGHT_0, NE_CULL_BACK, NE_MODULATION);
                    NE_2DDrawQuad(backgroundOffset - 1, 175, backgroundOffset + backgroundSize, 184, 1, RGB15(3, 3, 3));
                    NE_PolyFormat(31, 0, NE_LIGHT_0, NE_CULL_BACK, NE_MODULATION);

                    NE_TextPrint(0,        // Font slot
                                 1, 22,    // Coordinates x(column), y(row)
                                 NE_White, // Color
                                 AmmoText);
                }

                // Show health
                char HealthText[16];
                sprintf(HealthText, "+%d arm:%d", selectPlayer->Health, selectPlayer->armor);

                int backgroundSize = strlen(HealthText) * 8;
                int backgroundOffset = 8;
                NE_PolyFormat(13, 0, NE_LIGHT_0, NE_CULL_BACK, NE_MODULATION);
                NE_2DDrawQuad(backgroundOffset - 1, 183, backgroundOffset + backgroundSize, 193, 1, RGB15(3, 3, 3));
                NE_PolyFormat(31, 0, NE_LIGHT_0, NE_CULL_BACK, NE_MODULATION);

                // Print white text
                if (redHealthTextCounter == 0)
                {
                    NE_TextPrint(0,        // Font slot
                                 1, 23,    // Coordinates x(column), y(row)
                                 NE_White, // Color
                                 HealthText);
                }
                else // Print red text after taking damage
                {
                    int ColorAmount = 31 - redHealthTextCounter / 2;
                    NE_TextPrint(0,                                   // Font slot
                                 1, 23,                               // Coordinates x(column), y(row)
                                 RGB15(31, ColorAmount, ColorAmount), // Color
                                 HealthText);
                }
            }

            if (CurrentScopeLevel == 0) // Draw gun if player doesn't aim with his gun
            {
                // Draw cross air
                NE_SpriteSetParams(TopScreenSprites[0], cursorTransparency * 31, 0, NE_White);
                NE_SpriteDraw(TopScreenSprites[0]);
                NE_PolyFormat(31, 0, NE_LIGHT_0, NE_CULL_BACK, NE_MODULATION);

                //  Draw gun
                if (!selectPlayer->IsDead)
                {
                    // Set new gun position with head bobbing
                    if (selectPlayer->AllGunsInInventory[selectPlayer->currentGunInInventory] >= GunCount || !AllGuns[selectPlayer->AllGunsInInventory[selectPlayer->currentGunInInventory]].isKnife) // TODO Remove first condition?
                    {
                        rightGunX = GunPositionX + selectPlayer->rightGunXRecoil + sinf(selectPlayer->BobbingOffset) * 4;
                        rightGunY = GunPositionY + 4 + selectPlayer->rightGunYRecoil + cosf(selectPlayer->BobbingOffset * 2) * 6;
                        leftGunX = -selectPlayer->leftGunXRecoil + sinf(selectPlayer->BobbingOffset) * 4;
                        leftGunY = GunPositionY + 4 + selectPlayer->leftGunYRecoil + cosf(selectPlayer->BobbingOffset * 2) * 6;
                    }
                    else
                    {
                        rightGunX = GunPositionX - selectPlayer->rightGunXRecoil + GunMaxRecoil * 1.15 + sinf(selectPlayer->BobbingOffset) * 4;
                        rightGunY = GunPositionY + 4 - selectPlayer->rightGunYRecoil + GunMaxRecoil * 1.4 + cosf(selectPlayer->BobbingOffset * 2) * 6;
                    }

                    if (selectPlayer->isReloading && selectPlayer->AllGunsInInventory[selectPlayer->currentGunInInventory] < GunCount)
                    {
                        float valueToAdd = sinf((float)selectPlayer->GunReloadWaitCount / (float)AllGuns[selectPlayer->AllGunsInInventory[selectPlayer->currentGunInInventory]].ReloadTime * M_PI) * 70;
                        rightGunY += valueToAdd;
                        leftGunY += valueToAdd;
                    }
                    int lightCoef = 31 * selectPlayer->lightCoef;
                    NE_2DDrawTexturedQuadColor(rightGunX, rightGunY, rightGunX + 96, rightGunY + 96, 1, TopScreenSpritesMaterials[1], RGB15(lightCoef, lightCoef, lightCoef));
                    if (selectPlayer->AllGunsInInventory[selectPlayer->currentGunInInventory] < GunCount && AllGuns[selectPlayer->AllGunsInInventory[selectPlayer->currentGunInInventory]].isDualGun)
                    {
                        NE_PolyFormat(31, 0, NE_LIGHT_0, NE_CULL_FRONT, NE_MODULATION); // Polygon is inverted for mirroring, so we need to invert the culling
                        NE_2DDrawTexturedQuadColor(leftGunX + 96, leftGunY, leftGunX, leftGunY + 96, 1, TopScreenSpritesMaterials[1], RGB15(lightCoef, lightCoef, lightCoef));
                        NE_PolyFormat(31, 0, NE_LIGHT_0, NE_CULL_BACK, NE_MODULATION);
                    }
                }

                // Draw gun muzzle flash
                if (selectPlayer->AllGunsInInventory[selectPlayer->currentGunInInventory] < GunCount && ShowMuzzle > 0 && !AllGuns[selectPlayer->AllGunsInInventory[selectPlayer->currentGunInInventory]].isKnife)
                {
                    NE_PolyFormat(10 * ShowMuzzle, 0, NE_LIGHT_0, NE_CULL_BACK, NE_MODULATION);
                    if (selectPlayer->isRightGun)
                        NE_2DDrawTexturedQuad(rightGunX + AllGuns[selectPlayer->AllGunsInInventory[selectPlayer->currentGunInInventory]].xMuzzleFlashOffset, rightGunY + AllGuns[selectPlayer->AllGunsInInventory[selectPlayer->currentGunInInventory]].yMuzzleFlashOffset, rightGunX + 40 + AllGuns[selectPlayer->AllGunsInInventory[selectPlayer->currentGunInInventory]].xMuzzleFlashOffset, rightGunY + 42 + AllGuns[selectPlayer->AllGunsInInventory[selectPlayer->currentGunInInventory]].yMuzzleFlashOffset, 2, TopScreenSpritesMaterials[2]); // Gun muzzle flash
                    else
                        NE_2DDrawTexturedQuad(leftGunX + AllGuns[selectPlayer->AllGunsInInventory[selectPlayer->currentGunInInventory]].xMuzzleFlashOffset + 30, leftGunY + AllGuns[selectPlayer->AllGunsInInventory[selectPlayer->currentGunInInventory]].yMuzzleFlashOffset, leftGunX + 70 + AllGuns[selectPlayer->AllGunsInInventory[selectPlayer->currentGunInInventory]].xMuzzleFlashOffset, leftGunY + 42 + AllGuns[selectPlayer->AllGunsInInventory[selectPlayer->currentGunInInventory]].yMuzzleFlashOffset, 2, TopScreenSpritesMaterials[2]); // Gun muzzle flash
                }
            }
            else // Draw scope screen when aiming
            {
                NE_2DDrawQuad(0, 0, 46, 196, 1, RGB15(1, 1, 1));                                        // Left border
                NE_2DDrawQuad(ScreenWidth - 46, 0, ScreenWidth, 196, 1, RGB15(1, 1, 1));                // Right border
                NE_2DDrawQuad(0, 0, ScreenWidth, 12, 1, RGB15(1, 1, 1));                                // Top border
                NE_2DDrawQuad(0, ScreenHeight - 12, ScreenWidth, ScreenHeightFixed, 1, RGB15(1, 1, 1)); // Bottom border

                NE_PolyFormat(10, 0, NE_LIGHT_0, NE_CULL_BACK, NE_MODULATION);                      // Set alpha
                NE_2DDrawQuad(ScreenCenterWidth, 0, ScreenCenterWidth, 196, 1, NE_Black);           // Top to bottom line
                NE_2DDrawQuad(0, ScreenCenterHeight, ScreenWidth, ScreenCenterHeight, 1, NE_Black); // Left to Right line

                NE_PolyFormat(31, 0, NE_LIGHT_0, NE_CULL_BACK, NE_MODULATION); // Set alpha
                // Draw scope image
                NE_2DDrawTexturedQuad(ScreenCenterWidth - 82, ScreenCenterHeight - 84, ScreenCenterWidth + 82, ScreenCenterHeight + 84, 1, TopScreenSpritesMaterials[3]); // Circle
            }
        }

        float totalDis = 0;
        int totalEndAnimation = 0;
        for (int i = 0; i < GrenadeCount; i++)
        {
            if (grenades[i] != NULL && grenades[i]->GrenadeType == SMOKE && grenades[i]->Timer == 0)
            {
                // Calculate distances
                float smokeDistance = sqrtf(powf(selectPlayer->PlayerModel->x - grenades[i]->Model->x, 2.0) + powf(selectPlayer->PlayerModel->y - grenades[i]->Model->y, 2.0) + powf(selectPlayer->PlayerModel->z - grenades[i]->Model->z, 2.0)) / 26000.0; // fFor smoke detection

                // Set a minimum limit to the smoke detection distance
                if (smokeDistance > 1)
                {
                    smokeDistance = 1;
                }
                // Invert distance (more distance decrease the value)
                smokeDistance = map(1 - smokeDistance, 0, 0.2, 0, 1);
                totalDis += smokeDistance;
                totalEndAnimation += grenades[i]->EffectTimer;

                if (totalDis >= 1)
                {
                    totalDis = 1;
                    isInFullSmoke = true;
                    break;
                }
            }
        }

        // If player is in a smoke area
        if (totalDis > 0)
        {
            if (totalEndAnimation > 20)
            {
                totalEndAnimation = 20;
            }

            // Calculate smoke screen alpha
            totalEndAnimation = map(totalEndAnimation, 0, 20, 31, 0);
            int finalAlpha = 31 * totalDis - totalEndAnimation;
            if (finalAlpha < 0)
                finalAlpha = 0;

            NE_PolyFormat(finalAlpha, 1, NE_LIGHT_0, NE_CULL_BACK, NE_MODULATION); // Set alpha
            // Draw smoke screen
            NE_2DDrawQuad(0, 0, ScreenWidth, ScreenHeightFixed, 0, RGB15(9, 9, 9));
        }
    }
    showDialog();
    for (int i = 0; i < MaxPlayer; i++)
    {
        Player *player = &AllPlayers[i];

        // If player is flashed, show white screen
        if (player->flashed)
        {
            if (player->IsDead || player->Id == NO_PLAYER)
            {
                player->flashed = false;
                player->flashAnimation = 0;
                continue;
            }

            player->flashAnimation += flashAnimationSpeed;

            int alpha = 31;
            if (player->flashAnimation < flashAnimationSpeed * 5.0)
            {
                alpha = (int)((player->flashAnimation / (flashAnimationSpeed * 5.0)) * 31.0);
            }
            else if (player->flashAnimation >= flashAnimationSpeed * 120.0)
            {
                alpha = map(player->flashAnimation, flashAnimationSpeed * 120.0, 1, 31, 0);
            }
            if (alpha < 1)
                alpha = 1;

            if (!isShowingMap && !isShowingKeyBoard && selectPlayer == player)
            {
                NE_PolyFormat(alpha, 1, NE_LIGHT_0, NE_CULL_BACK, NE_MODULATION); // Set alpha
                NE_2DDrawQuad(0, 0, ScreenWidth, ScreenHeightFixed, 0, RGB15(31, 31, 31));
            }

            if (player->flashAnimation >= 1)
            {
                player->flashed = false;
                player->flashAnimation = 0;
            }
        }
    }
    if (KillTextShowTimer > 0)
        KillTextShowTimer--;

    if (textToShowTimer > 0)
        textToShowTimer--;

    // Reduce muzzle timer
    if (ShowMuzzle > 0)
        ShowMuzzle--;
}

/**
 * @brief Draw bottom screen UI
 *
 */
void drawBottomScreenUI()
{
    // Set view in 2D mode
    NE_2DViewInit();

    if (isShowingKeyBoard)
    {
        drawKeyBoard();
    }
    else
    {
        // Draw quit button
        NE_SpriteDraw(BottomScreenSprites[0]);

        // Draw menu background
        NE_2DDrawQuad(0, 0, 256, 196, 20, RGB15(3, 3, 3));

        // Draw the menu
        renderFunction();

        // Draw menu elements
        drawButtons();
        drawCheckboxs();
        drawSliders();
    }
    // Change screen update mode (30 fps with 2 screens update or 60 fps with main game screen update)
    if (NeedChangeScreen)
    {
        NeedChangeScreen = false;
        AlwaysUpdateBottomScreen = !AlwaysUpdateBottomScreen;
        UpdateBottomScreenFrameCount = 4;
    }
}

/**
 * @brief Init in game menu
 *
 */
void initGameMenu()
{
    SetTwoScreenMode(false);

    startChangeMenu(GAME);

    renderFunction = &drawGameMenu;

    lastOpenedMenu = &initGameMenu;

    setQuitButton(false);

    // Show score button
    char* scoreButtonText = (localPlayer->Team == SPECTATOR) ? "Choose Team" : "Show Score";
    addButton(0, 80, 104, 100, 24, &initScoreMenu, NULL_PARAMETER, 12, 14, scoreButtonText);

    // Open shop categories button
    addButton(1, 80, 72, 100, 24, &initShopCategoriesMenu, NULL_PARAMETER, 12, 10, "Open Shop");

    // Settings button
    addButton(2, 80, 136, 100, 24, &initSettingsMenu, NULL_PARAMETER, 12, 18, "Settings");

    // Quit game button
    addButton(3, 80, 168, 100, 24, &initQuitMenu, NULL_PARAMETER, 12, 22, "Quit Game");

    // Game pad button
    addButton(4, 80, 40, 100, 24, &initControllerMenu, NULL_PARAMETER, 13, 6, "Gamepad");
    
    AllButtons[0].isHidden = isInTutorial;
    bool isSpectator = AllPlayers[0].Team == SPECTATOR;
    AllButtons[1].isHidden = isSpectator || isInTutorial;
    AllButtons[4].isHidden = isSpectator;

    SetButtonToShow(5);
}

/**
 * @brief Init score menu
 *
 */
void initScoreMenu()
{
    SetTwoScreenMode(false);

    startChangeMenu(SCORE_BOARD);

    renderFunction = &drawScoreMenu;

    lastOpenedMenu = &initGameMenu;

    setQuitButton(true);

    if (AllPlayers[0].Team == -1)
    {
        // Join counter button
        addButton(0, 18, 169, 90, 20, &SetTeam, COUNTERTERRORISTS, 6, 22, "Join");

        // Join terrorists button
        addButton(1, 148, 169, 90, 20, &SetTeam, TERRORISTS, 22, 22, "Join");

        SetButtonToShow(2);
    }
    else
        SetButtonToShow(0);

    showFinalScore = false;
}

/**
 * @brief Init final score menu
 *
 */
void initFinalScoreMenu()
{
    SetTwoScreenMode(false);

    startChangeMenu(FINAL_SCORE);

    renderFunction = &drawScoreMenu;

    lastOpenedMenu = &initMainMenu;

    setQuitButton(true);

    totalFinishedParty++;

    if ((CounterScore > TerroristsScore && localPlayer->Team == COUNTERTERRORISTS) || (TerroristsScore > CounterScore && localPlayer->Team == TERRORISTS))
    {
        totalWins++;
    }

    uiTimer = 8;
    actionOfUiTimer = SAVE;

    showFinalScore = true;
}

/**
 * @brief Init shop categories menu
 *
 */
void initShopCategoriesMenu()
{
    if (!localPlayer->IsDead && (shopDisableTimer != 0 || !allPartyModes[currentPartyMode].limitedShopByZoneAndTimer) && isInShopZone)
    {
        SetTwoScreenMode(false);

        startChangeMenu(SHOPCATEGORIES);

        renderFunction = &drawShopCategoriesMenu;

        lastOpenedMenu = &initGameMenu;

        setQuitButton(true);

        int numOfButtons = 6;
        for (int buttonIndex = 0; buttonIndex < numOfButtons; buttonIndex++)
        {
            // Set all categories buttons
            int xPos = (ScreenWidth / 2) * (buttonIndex / 3);
            int yPos = ((198 - 23) / 3) * (buttonIndex % 3) + 23;
            int xSize = ScreenWidth / 2;
            int ySize = (198 - 23) / 3;
            addButton(buttonIndex, xPos, yPos, xSize, ySize, &OpenShopCategory, buttonIndex, 0, 0, "");
        }
        SetButtonToShow(numOfButtons);
    }
}

/**
 * @brief Init settings menu
 *
 */
void initSettingsMenu()
{
    SetTwoScreenMode(true);

    startChangeMenu(SETTINGS);

    renderFunction = &drawSettingsMenu;

    if (Connection == UNSELECTED)
        lastOpenedMenu = &initMainMenu;
    else
        lastOpenedMenu = &initGameMenu;

    setQuitButton(true);

    // Set rumble checkbox
    AllCheckBoxs[0].xPos = 58;
    AllCheckBoxs[0].yPos = 56;
    AllCheckBoxs[0].xSize = 20;
    AllCheckBoxs[0].ySize = 20;
    AllCheckBoxs[0].value = &useRumble;

    // Set show ping checkbox
    AllCheckBoxs[1].xPos = 180;
    AllCheckBoxs[1].yPos = 56;
    AllCheckBoxs[1].xSize = 20;
    AllCheckBoxs[1].ySize = 20;
    AllCheckBoxs[1].value = &showPing;

    // Set change name button
    addButton(0, 15, 88, 100, 24, &initChangeNameMenu, NULL_PARAMETER, 3, 12, "Change name");

    // Set change controls button
    addButton(1, 140, 88, 100, 24, &initControlsSettingsMenu, NULL_PARAMETER, 20, 12, "Controls");
    addButton(2, 140, 137, 100, 23, &changeKeyboardMode, NULL_PARAMETER, 21, 18, "");
    updateKeyboardModeButton();

    AllSliders[0].xPos = 20;
    AllSliders[0].yPos = 158;
    AllSliders[0].xSize = 100;
    AllSliders[0].min = 0;
    AllSliders[0].max = 1;
    AllSliders[0].step = 0.1;
    AllSliders[0].value = &cursorTransparency;

    SetSliderToShow(1);
    SetButtonToShow(3);
    SetCheckBoxToShow(2);
}

/**
 * @brief Init change name menu
 *
 */
void initChangeNameMenu()
{
    SetTwoScreenMode(false);

    startChangeMenu(CHANGENAME);
    onCloseMenu = &unloadChangeNameMenu;
    haveToCallOnCloseMenu = true;
    renderFunction = &drawChangeNameMenu;

    setQuitButton(true);

    isShowingKeyBoard = true;
    returnToMenuOnCancel = SETTINGS;
    returnToMenuOnSucces = SETTINGS;
    keyboardAction = KEYBOARD_ACTION_CHANGE_NAME;
    onKeyboardCloseSucces = &ChangeMenu;
    onKeyboardCloseCancel = &ChangeMenu;
    maxInputLength = 13;
    minInputLength = 1;
    SetCanChangeCase(true);

    isUpperCase = false;
    strncpy(tempText, localPlayer->name, 21);
}

/**
 * @brief Init quit menu
 *
 */
void initQuitMenu()
{
    SetTwoScreenMode(false);

    startChangeMenu(QUIT);

    renderFunction = &drawQuitMenu;

    setQuitButton(false);

    // No button
    addButton(0, ScreenWidth - 120, 90, 80, 20, &initGameMenu, NULL_PARAMETER, 21, 13, "No");

    // Yes button
    addButton(1, 40, 98, 80, 80, 20, &QuitParty, NULL_PARAMETER, 9, 13, "Yes");

    SetButtonToShow(2);
}

/**
 * @brief Init shop menu
 *
 */
void initShopMenu()
{
    SetTwoScreenMode(false);

    startChangeMenu(SHOP);

    renderFunction = &drawShopMenu;

    onCloseMenu = &unloadShopMenu;
    haveToCallOnCloseMenu = true;

    lastOpenedMenu = &initShopCategoriesMenu;

    CreateShopMenuNE();
 
    setQuitButton(true);

    // Buy button
    addButton(0, 88, 170, 80, 20, &BuyWeapon, 0, 14, 22, "Buy!");
    // Left button
    addButton(1, 48, 170, 30, 20, &ChangeShopElement, 1, 7, 22, "<-");
    // Right button
    addButton(2, 178, 170, 30, 20, &ChangeShopElement, 0, 23, 22, "->");

    SetButtonToShow(3);
}

/**
 * @brief Init game pad menu
 *
 */
void initControllerMenu()
{
    SetTwoScreenMode(false);

    startChangeMenu(CONTROLLER);

    renderFunction = &drawControllerMenu;

    onCloseMenu = &unloadControllerMenu;
    haveToCallOnCloseMenu = true;
    lastOpenedMenu = &initGameMenu;

    setQuitButton(true);

    BottomScreenSpritesMaterials[7] = NE_MaterialCreate();
    Palettes[14] = NE_PaletteCreate();
    NE_MaterialTexLoadBMPtoRGB256(BottomScreenSpritesMaterials[7], Palettes[14], (void *)JumpArrow_bin, 1);

    BottomScreenSpritesMaterials[8] = NE_MaterialCreate();
    Palettes[15] = NE_PaletteCreate();
    NE_MaterialTexLoadBMPtoRGB256(BottomScreenSpritesMaterials[8], Palettes[15], (void *)reload_bin, 1);

    int buttonXPosition = (isLeftControls) ? 218 : 2;
    int mapButtonXPosition = (isLeftControls) ? 193 : 2;
    int mapTextXPosition = (isLeftControls) ? 25 : 1;
    int textXPosition = (isLeftControls) ? 29 : 2;

    // Jump button
    addButton(0, buttonXPosition, 30, 36, 36, &SetNeedJump, 0, 0, 0, "");

    // Reload button
    addButton(1, buttonXPosition, 72, 36, 36, &startReloadGun, 0, 0, 0, "");

    // Right button
    addButton(2, buttonXPosition, 114, 36, 36, &ChangeGunInInventoryForLocalPlayer, 0, textXPosition, 16, ">");

    // Left button
    addButton(3, buttonXPosition, 156, 36, 36, &ChangeGunInInventoryForLocalPlayer, 1, textXPosition, 21, "<");

    // Map button
    addButton(4, mapButtonXPosition, 2, 36, 20, &changeMapState, 0, mapTextXPosition, 1, "Map");
    AllButtons[4].isHidden = currentMap == TUTORIAL;
    SetButtonToShow(5);
}

/**
 * @brief Init main menu
 *
 */
void initMainMenu()
{
    SetTwoScreenMode(true);

    startChangeMenu(MAIN);

    renderFunction = &drawMainMenu;

    lastOpenedMenu = &initMainMenu;

    SendLeave = (Connection != OFFLINE && Connection != UNSELECTED);
    Connection = UNSELECTED;

    setQuitButton(false);

    removeAllPlayers();

    if (isInTutorial)
    {
        endTutorial();
        uiTimer = 8;
        actionOfUiTimer = SAVE;
    }

    addButton(0, 40, 40, ScreenWidth - 80, 24, &initSelectionMapImageMenu, NULL_PARAMETER, 0, 10, 6, "Single Player");
    addButton(1, 40, 87, ScreenWidth - 80, 24, &initJoinCreatePartyMenu, NULL_PARAMETER, 11, 12, "Multiplayer");
    addButton(2, 40, 135, ScreenWidth - 80, 24, &initSettingsMenu, NULL_PARAMETER, 12, 18, "Settings");
    addButton(3, 76, 170, ScreenWidth - 160, 20, &initStatsMenu, NULL_PARAMETER, 13, 22, "Stats");

    launchMusic();

    SetButtonToShow(4);
}

/**
 * @brief Init chat menu (TODO)
 *
 */
void initChatMenu()
{
    // To Fill
}

/**
 * @brief Init controls settings menu
 *
 */
void initControlsSettingsMenu()
{

    SetTwoScreenMode(true);

    startChangeMenu(CONTROLSSETTINGS);

    renderFunction = &drawControlsSettingsMenu;

    lastOpenedMenu = &initSettingsMenu;

    setQuitButton(true);

    // Set rumble checkbox
    AllCheckBoxs[0].xPos = 58;
    AllCheckBoxs[0].yPos = 56;
    AllCheckBoxs[0].xSize = 20;
    AllCheckBoxs[0].ySize = 20;
    AllCheckBoxs[0].value = &isLeftControls;

    // Set change controls button
    addButton(0, 140, 40, 100, 32, &initControlsChangeMenu, NULL_PARAMETER, 17, 6, "");

    AllSliders[0].xPos = 140;
    AllSliders[0].yPos = 148;
    AllSliders[0].xSize = 100;
    AllSliders[0].min = MIN_SENSITIVITY;
    AllSliders[0].max = MAX_SENSITIVITY;
    AllSliders[0].step = 0.1;
    AllSliders[0].value = &sensitivity;

    AllSliders[1].xPos = 20;
    AllSliders[1].yPos = 148;
    AllSliders[1].xSize = 100;
    AllSliders[1].min = MIN_SENSITIVITY;
    AllSliders[1].max = MAX_SENSITIVITY;
    AllSliders[1].step = 0.1;
    AllSliders[1].value = &buttonsSensitivity;

    SetSliderToShow(2);
    SetButtonToShow(1);
    SetCheckBoxToShow(1);
}

/**
 * @brief Init controls change menu
 *
 */
void initControlsChangeMenu()
{
    SetTwoScreenMode(true);

    startChangeMenu(CONTROLSCHANGE);

    renderFunction = &drawControlsChangeMenu;

    lastOpenedMenu = &initControlsSettingsMenu;

    onCloseMenu = &unloadControlsChangeMenu;
    haveToCallOnCloseMenu = true;

    setQuitButton(true);

    // Set change controls button
    addButton(0, 15, 167, 100, 24, &changeControlsPage, 1, 7, 22, "<-");
    addButton(1, 140, 167, 100, 24, &changeControlsPage, 0, 23, 22, "->");

    // Set change controls button
    switch (controlsPage)
    {
        case 0:
            addButton(2, 15, 40, 100, 24, &startScanForInput, FIRE_BUTTON, 6, 4, "Fire");
            addButton(3, 140, 40, 100, 24, &startScanForInput, JUMP_BUTTON, 22, 4, "Jump");
            addButton(4, 15, 80, 100, 24, &startScanForInput, LEFT_BUTTON, 4, 9, "Move Left.");
            addButton(5, 140, 80, 100, 24, &startScanForInput, RIGHT_BUTTON, 19, 9, "Move Right");
            addButton(6, 15, 120, 100, 24, &startScanForInput, UP_BUTTON, 3, 14, "Move Forward");
            addButton(7, 140, 120, 100, 24, &startScanForInput, DOWN_BUTTON, 18, 14, "Move Backward");
            SetButtonToShow(8);
            break;
        case 1:
            addButton(2, 15, 40, 100, 24, &startScanForInput, LOOK_LEFT_BUTTON, 4, 4, "Look Left");
            addButton(3, 140, 40, 100, 24, &startScanForInput, LOOK_RIGHT_BUTTON, 19, 4, "Look Right");
            addButton(4, 15, 80, 100, 24, &startScanForInput, LOOK_UP_BUTTON, 5, 9, "Look Up.");
            addButton(5, 140, 80, 100, 24, &startScanForInput, LOOK_DOWN_BUTTON, 19, 9, "Look Down");
            addButton(6, 15, 120, 100, 24, &startScanForInput, DEFUSE_BUTTON, 5, 14, "Defuse");
            addButton(7, 140, 120, 100, 24, &startScanForInput, SCOPE_BUTTON, 21, 14, "Scope");
            SetButtonToShow(8);
            break;
        case 2:
            addButton(2, 15, 40, 100, 24, &startScanForInput, LEFT_GUN, 1, 4, "Previous Weapon");
            addButton(3, 140, 40, 100, 24, &startScanForInput, RIGHT_GUN, 18, 4, "Next Weapon");
            SetButtonToShow(4);
            break;

    }
}

/**
 * @brief Init map slection with image menu
 *
 */
void initSelectionMapImageMenu()
{
    SetTwoScreenMode(true);

    startChangeMenu(MAP_SELECTION_IMAGE);

    renderFunction = &drawSelectionMapImageMenu;

    lastOpenedMenu = &initMainMenu;

    onCloseMenu = &unloadSelectionMapImageMenu;
    haveToCallOnCloseMenu = true;

    setQuitButton(true);

    BottomScreenSpritesMaterials[6] = NE_MaterialCreate();
    Palettes[10] = NE_PaletteCreate();
    NE_MaterialTexLoadBMPtoRGB256(BottomScreenSpritesMaterials[6], Palettes[10], allMaps[currentSelectionMap].image, 1);

    // Set change controls button
    addButton(0, 35, 164, 40, 32, &ChangeMap, 0, 6, 22, "<-");
    addButton(1, 181, 164, 40, 32, &ChangeMap, 1, 24, 22, "->");
    addButton(2, 88, 164, 60, 32, &AskGameMode, NULL_PARAMETER, 14, 22, "Start");

    SetButtonToShow(3);
}

/**
 * @brief Init map selection as basic list (not implemented, maybe to remove)
 *
 */
void initSelectionMapListMenu()
{
    SetTwoScreenMode(true);

    startChangeMenu(MAP_SELECTION_LIST);

    renderFunction = &drawSelectionMapListMenu;

    lastOpenedMenu = &initSelectionMapImageMenu;

    setQuitButton(true);

    // Set change controls button
    addButton(0, 40, 40, 100, 32, &initControlsChangeMenu, NULL_PARAMETER, 5, 6, "<-");
    addButton(1, 140, 40, 100, 32, &initControlsChangeMenu, NULL_PARAMETER, 17, 6, "->");

    SetButtonToShow(2);
}

/**
 * @brief Init online error menu
 *
 */
void initPartyModeSelectionMenu()
{
    SetTwoScreenMode(false);

    startChangeMenu(PARTY_MODE_SELECTION_LIST);

    renderFunction = &drawPartyModeSelectionMenu;
    lastOpenedMenu = &initSelectionMapImageMenu;

    // Single player button
    addButton(0, 40, 56, ScreenWidth - 80, 24, &StartSinglePlayer, 1, 13, 8, "Casual");

    // Multiplayer button
    addButton(1, 40, 119, ScreenWidth - 80, 24, &StartSinglePlayer, 0, 11, 16, "Competitive");
    // isHidden = false

    SetButtonToShow(2);
    setQuitButton(true);
}

/**
 * @brief Init multiplayer menu
 *
 */
void initJoinCreatePartyMenu()
{
    SetTwoScreenMode(true);

    startChangeMenu(JOIN_CREATE_PARTY);

    renderFunction = &drawJoinCreatePartyMenu;

    lastOpenedMenu = &initMainMenu;

    setQuitButton(true);

    // Set change controls button
    addButton(0, 36, 44, 190, 32, &JoinParty, JOIN_RANDOM_PARTY, 7, 7, "Join a random party");
    addButton(1, 36, 92, 190, 32, &initEnterCodeMenu, NULL_PARAMETER, 6, 13, "Join a private party");
    addButton(2, 36, 140, 190, 32, &JoinParty, CREATE_PRIVATE_PARTY, 5, 19, "Create a private party")

    SetButtonToShow(3);
}

/**
 * @brief Init enter code menu
 *
 */
void initEnterCodeMenu()
{
    SetTwoScreenMode(false);

    startChangeMenu(ENTER_CODE);

    renderFunction = &drawEnterCodeMenu;

    isShowingKeyBoard = true;
    returnToMenuOnCancel = JOIN_CREATE_PARTY;
    returnToMenuOnSucces = 2;
    keyboardAction = KEYBOARD_ACTION_ENTER_CODE;

    onKeyboardCloseCancel = &ChangeMenu;
    onKeyboardCloseSucces = &JoinParty;

    maxInputLength = 5;
    minInputLength = 5;
    SetCanChangeCase(false);
    isUpperCase = true;
    strncpy(tempText, "", 21);
}

/**
 * @brief Init online error menu
 *
 */
void initOnlineErrorMenu()
{
    SetTwoScreenMode(false);

    startChangeMenu(ONLINE_ERROR);

    renderFunction = &drawOnlineErrorMenu;
    lastOpenedMenu = &initMainMenu;

    setQuitButton(true);
}

void initStatsMenu()
{
    SetTwoScreenMode(false);

    startChangeMenu(STATS);

    renderFunction = &drawStatsMenu;
    lastOpenedMenu = &initMainMenu;

    setQuitButton(true);
}

/**
 * @brief Draw game menu
 *
 */
void drawGameMenu()
{
    // Print texts
    NE_TextPrint(0,        // Font slot
                 12, 1,    // Coordinates x(column), y(row)
                 NE_White, // Color
                 "Main menu");

    if (isPrivate)
    {
        char PartyCodeText[30] = "Party code: ";
        sprintf(PartyCodeText + strlen(PartyCodeText), "%s ", partyCode);
        printLongText(10, 23, 3, PartyCodeText);
    }
}

/**
 * @brief Draw score menu
 *
 */
void drawScoreMenu()
{
    if (!WaitForTeamResponse && localPlayer->Team == SPECTATOR)
        ButtonToShow = 2;
    else if (!WaitForTeamResponse) /////////////////////////////////////////////////////////////////////////////////////////////CHECK THIS
        ButtonToShow = 0;

    // Draw white bars
    NE_2DDrawQuad(0, 25, 256, 28, 19, RGB15(31, 31, 31));
    NE_2DDrawQuad(126, 28, 130, 180, 19, RGB15(31, 31, 31));
    if (!showFinalScore)
    {
        NE_TextPrint(0,        // Font slot
                     11, 1,    // Coordinates x(column), y(row)
                     NE_White, // Color
                     "Score table");
    }
    else
    {
        NE_TextPrint(0,        // Font slot
                     9, 1,     // Coordinates x(column), y(row)
                     NE_White, // Color
                     "Party finished");
    }

    // Counter terrorists text
    char CounterScoreText[30] = "Counter t";
    int offset = 0;
    if (!allPartyModes[currentPartyMode].noScore)
    {
        sprintf(CounterScoreText + strlen(CounterScoreText), ": %d ", CounterScore);
    }
    else
    {
        offset = 2;
    }
    NE_TextPrint(0,             // Font slot
                 1 + offset, 4, // Coordinates x(column), y(row)
                 NE_White,      // Color
                 CounterScoreText);

    // Show each counter terrorists player
    int Count1 = 0;
    for (int i = 0; i < MaxPlayer; i++)
    {
        if (AllPlayers[i].Team == COUNTERTERRORISTS)
        {
            int y = 6 + Count1 * 4;

            char playerName[PLAYER_MAX_LENGTH];

            sprintf(playerName, AllPlayers[i].name);
            NE_TextPrint(0,        // Font slot
                         1, y,     // Coordinates x(column), y(row)
                         NE_White, // Color
                         playerName);

            char playerValues[PLAYER_MAX_LENGTH];
            sprintf(playerValues, "Kills:  %d", AllPlayers[i].KillCount);
            NE_TextPrint(0,                 // Font slot
                         1, y + 1,          // Coordinates x(column), y(row)
                         RGB15(17, 17, 17), // Color
                         playerValues);
            sprintf(playerValues, "Deaths: %d", AllPlayers[i].DeathCount);
            NE_TextPrint(0,                 // Font slot
                         1, y + 2,          // Coordinates x(column), y(row)
                         RGB15(17, 17, 17), // Color
                         playerValues);

            Count1++;
        }
    }

    // Counter terrorists text
    char TerroristsScoreText[30] = "Terrorists";
    if (!allPartyModes[currentPartyMode].noScore)
    {
        sprintf(TerroristsScoreText + strlen(TerroristsScoreText), ": %d ", TerroristsScore);
    }

    NE_TextPrint(0,              // Font slot
                 17 + offset, 4, // Coordinates x(column), y(row)
                 NE_White,       // Color
                 TerroristsScoreText);

    // Show each terrorists player
    int Count2 = 0;
    for (int i = 0; i < MaxPlayer; i++)
    {
        if (AllPlayers[i].Team == TERRORISTS)
        {
            int y = 6 + Count2 * 4;

            char playerName[PLAYER_MAX_LENGTH];
            // sprintf(playerName, "id: %d", AllPlayers[i].Id);
            sprintf(playerName, AllPlayers[i].name);
            NE_TextPrint(0,        // Font slot
                         17, y,    // Coordinates x(column), y(row)
                         NE_White, // Color
                         playerName);

            char playerValues[PLAYER_MAX_LENGTH];
            sprintf(playerValues, "Kills:  %d", AllPlayers[i].KillCount);
            NE_TextPrint(0,                 // Font slot
                         17, y + 1,         // Coordinates x(column), y(row)
                         RGB15(17, 17, 17), // Color
                         playerValues);
            sprintf(playerValues, "Deaths: %d", AllPlayers[i].DeathCount);
            NE_TextPrint(0,                 // Font slot
                         17, y + 2,         // Coordinates x(column), y(row)
                         RGB15(17, 17, 17), // Color
                         playerValues);

            Count2++;
        }
    }
}

/**
 * @brief Draw shop categories menu
 *
 */
void drawShopCategoriesMenu()
{
    // Draw white bars
    NE_2DDrawQuad(ScreenCenterWidth - 1, 23, ScreenCenterWidth + 1, ScreenHeightFixed, 0, RGB15(31, 31, 31));
    NE_2DDrawQuad(0, (198 - 23) / 3 - 1 + 23, ScreenWidth, (198 - 23) / 3 + 2 + 23, 0, RGB15(31, 31, 31));
    NE_2DDrawQuad(0, (198 - 23) / 3 * 2 - 1 + 23, ScreenWidth, (198 - 23) / 3 * 2 + 2 + 23, 0, RGB15(31, 31, 31));

    // Show texts
    NE_TextPrint(0,        // Font slot
                 14, 1,    // Coordinates x(column), y(row)
                 NE_White, // Color
                 "Shop");

    char MoneyText[30];
    sprintf(MoneyText, "%d$", localPlayer->Money);
    NE_TextPrint(0,        // Font slot
                 1, 1,     // Coordinates x(column), y(row)
                 NE_White, // Color
                 MoneyText);

    NE_TextPrint(0,        // Font slot
                 4, 6,     // Coordinates x(column), y(row)
                 NE_White, // Color
                 "PISTOLS");

    NE_TextPrint(0,        // Font slot
                 5, 13,    // Coordinates x(column), y(row)
                 NE_White, // Color
                 "HEAVY");

    NE_TextPrint(0,        // Font slot
                 6, 21,    // Coordinates x(column), y(row)
                 NE_White, // Color
                 "SMG");

    NE_TextPrint(0,        // Font slot
                 21, 6,    // Coordinates x(column), y(row)
                 NE_White, // Color
                 "RIFLES");

    NE_TextPrint(0,        // Font slot
                 20, 13,   // Coordinates x(column), y(row)
                 NE_White, // Color
                 "EQUIPMENT");

    NE_TextPrint(0,        // Font slot
                 20, 21,   // Coordinates x(column), y(row)
                 NE_White, // Color
                 "GRENADES");
}

/**
 * @brief Draw settings menu
 *
 */
void drawSettingsMenu()
{
    NE_TextPrint(0,        // Font slot
                 12, 1,    // Coordinates x(column), y(row)
                 NE_White, // Color
                 "Settings");

    NE_TextPrint(0,        // Font slot
                 18, 16,   // Coordinates x(column), y(row)
                 NE_White, // Color
                 "Keyboard mode");

    printLongText(1, 16, 3, "Use Rumble Pak (causes crash on DSi/3DS)");
    printLongText(17, 32, 4, "Show ping");

    char gamePadSensitivityText[29];
    sprintf(gamePadSensitivityText, "Crosshair transparency: %d%%", (int)(*AllSliders[0].value * 100));
    printLongText(1, 16, 15, gamePadSensitivityText);
}

/**
 * @brief Draw quit menu
 *
 */
void drawQuitMenu()
{
    if (isInTutorial)
    {
        NE_TextPrint(0,        // Font slot
                     7, 3,     // Coordinates x(column), y(row)
                     NE_White, // Color
                     "Skip the tutorial?");
    }
    else
    {
        NE_TextPrint(0,        // Font slot
                     4, 3,     // Coordinates x(column), y(row)
                     NE_White, // Color
                     "Are you sure want to quit\n            the game?");
    }
}

/**
 * @brief Draw shop menu
 *
 */
void drawShopMenu()
{
    NE_2DDrawQuad(160, 26, 255, 144, 1, RGB15(4, 4, 4));                                   // Draw gun details background
    NE_2DDrawTexturedQuad(26, 26, 118 + 26, 118 + 26, 1, BottomScreenSpritesMaterials[6]); // Draw gun

    // Title
    NE_TextPrint(0,        // Font slot
                 14, 1,    // Coordinates x(column), y(row)
                 NE_White, // Color
                 "Shop");

    char MoneyText[30];
    sprintf(MoneyText, "%d$", localPlayer->Money);
    NE_TextPrint(0,        // Font slot
                 1, 1,     // Coordinates x(column), y(row)
                 NE_White, // Color
                 MoneyText);

    if (GetShopCategory() < 4) // If current shop category is guns
    {
        // Gun name
        printLongConstChar(21, 31, 4, AllGuns[SelectedGunShop].name);

        // Gun damage
        NE_TextPrint(0,        // Font slot
                     21, 7,    // Coordinates x(column), y(row)
                     NE_White, // Color
                     "Damage");
        char DamageText[4];

        sprintf(DamageText, "%d", AllGuns[SelectedGunShop].Damage);
        NE_TextPrint(0,        // Font slot
                     21, 8,    // Coordinates x(column), y(row)
                     NE_White, // Color
                     DamageText);

        // Gun fire rate
        NE_TextPrint(0,        // Font slot
                     21, 11,   // Coordinates x(column), y(row)
                     NE_White, // Color
                     "Fire rate");
        char FireRateText[10];
        sprintf(FireRateText, "%d RPM", (int)(1.0 / (AllGuns[SelectedGunShop].fireRate / 3600.0)));
        NE_TextPrint(0,        // Font slot
                     21, 12,   // Coordinates x(column), y(row)
                     NE_White, // Color
                     FireRateText);

        // Gun price
        NE_TextPrint(0,        // Font slot
                     21, 15,   // Coordinates x(column), y(row)
                     NE_White, // Color
                     "Price");
        char PriceText[7];
        sprintf(PriceText, "%d$", AllGuns[SelectedGunShop].Price);
        NE_TextPrint(0,        // Font slot
                     21, 16,   // Coordinates x(column), y(row)
                     NE_White, // Color
                     PriceText);
    }
    else if (GetShopCategory() == 4) // If current shop category is equipements
    {
        // Gun name
        printLongConstChar(21, 31, 4, allEquipments[SelectedGunShop - GunCount - shopGrenadeCount].name);

        // Gun price
        NE_TextPrint(0,        // Font slot
                     21, 15,   // Coordinates x(column), y(row)
                     NE_White, // Color
                     "Price");

        char PriceText[7];
        sprintf(PriceText, "%d$", allEquipments[SelectedGunShop - GunCount - shopGrenadeCount].Price);
        NE_TextPrint(0,        // Font slot
                     21, 16,   // Coordinates x(column), y(row)
                     NE_White, // Color
                     PriceText);

        // Print description
        printLongConstChar(21, 31, 7, allEquipments[SelectedGunShop - GunCount - shopGrenadeCount].description);
    }
    else if (GetShopCategory() == 5) // If current shop category is grenades
    {
        // Gun name
        printLongConstChar(21, 31, 4, AllGrenades[SelectedGunShop - GunCount].name);

        // Gun price
        NE_TextPrint(0,        // Font slot
                     21, 15,   // Coordinates x(column), y(row)
                     NE_White, // Color
                     "Price");

        char PriceText[7];
        sprintf(PriceText, "%d$", AllGrenades[SelectedGunShop - GunCount].Price);
        NE_TextPrint(0,        // Font slot
                     21, 16,   // Coordinates x(column), y(row)
                     NE_White, // Color
                     PriceText);

        // Print description
        printLongConstChar(21, 31, 7, AllGrenades[SelectedGunShop - GunCount].description);
    }
}

/**
 * @brief Draw gamepad menu
 *
 */
void drawControllerMenu()
{
    if (!isLeftControls)
    {
        NE_2DDrawQuad(40, 30, 255, 194, 1, RGB15(4, 4, 4));                                        // touch pad area
        NE_2DDrawTexturedQuad(4, 32, 4 + 36 - 4, 30 + 36 - 2, 1, BottomScreenSpritesMaterials[7]); // Jump button
        NE_2DDrawTexturedQuad(4, 74, 4 + 36 - 4, 72 + 36 - 2, 1, BottomScreenSpritesMaterials[8]); // Reload button
    }
    else
    {
        NE_2DDrawQuad(0, 30, 215, 194, 1, RGB15(4, 4, 4));                                             // touch pad area
        NE_2DDrawTexturedQuad(220, 32, 220 + 36 - 4, 30 + 36 - 2, 1, BottomScreenSpritesMaterials[7]); // Jump button
        NE_2DDrawTexturedQuad(220, 74, 220 + 36 - 4, 72 + 36 - 2, 1, BottomScreenSpritesMaterials[8]); // Reload button
    }

    // Print texts
    NE_TextPrint(0,        // Font slot
                 13, 1,    // Coordinates x(column), y(row)
                 NE_White, // Color
                 "Gamepad");
}

/**
 * @brief Draw main menu
 *
 */
void drawMainMenu()
{

    // Print texts
    NE_TextPrint(0,        // Font slot
                 10, 1,    // Coordinates x(column), y(row)
                 NE_White, // Color
                 GAME_NAME);

    NE_TextPrint(0,        // Font slot
                 1, 22,    // Coordinates x(column), y(row)
                 NE_White, // Color
                 GAME_VERSION);

    NE_TextPrint(0,        // Font slot
                 24, 22,   // Coordinates x(column), y(row)
                 NE_White, // Color
                 DEV_NAME);
}

/**
 * @brief Draw change name menu
 *
 */
void drawChangeNameMenu()
{
    // Nothing here
}

/**
 * @brief Draw chat menu (TODO)
 *
 */
void drawChatMenu()
{
    // Nothing here
}

/**
 * @brief Draw controls settings menu
 *
 */
void drawControlsSettingsMenu()
{
    // Print texts
    NE_TextPrint(0,        // Font slot
                 12, 1,    // Coordinates x(column), y(row)
                 NE_White, // Color
                 "Controls");

    printLongText(2, 15, 4, "Left-handed gamepad");
    printLongText(20, 28, 6, "Change controls");

    // Print texts
    NE_TextPrint(0,        // Font slot
                 13, 11,   // Coordinates x(column), y(row)
                 NE_White, // Color
                 "Camera");

    char gamePadSensitivityText[26];
    sprintf(gamePadSensitivityText, "Gamepad sensitivity: %0.1f", *AllSliders[0].value);
    printLongText(16, 32, 13, gamePadSensitivityText);

    char buttonsSensitivityText[33];
    sprintf(buttonsSensitivityText, "Button sensitivity: %0.1f", *AllSliders[1].value);
    printLongText(2, 16, 13, buttonsSensitivityText);
}

/**
 * @brief Draw controls change menu
 *
 */
void drawControlsChangeMenu()
{
    // Print texts
    NE_TextPrint(0,        // Font slot
                 9, 1,     // Coordinates x(column), y(row)
                 NE_White, // Color
                 "Change controls");

    if (ButtonToShow >= 3)
        // Print texts
        NE_TextPrint(0,        // Font slot
                     3, 6,     // Coordinates x(column), y(row)
                     NE_White, // Color
                     inputsNames[inputs[0 + (controlsPage * 6)].nameIndex]);

    if (ButtonToShow >= 4)
        // Print texts
        NE_TextPrint(0,        // Font slot
                     19, 6,    // Coordinates x(column), y(row)
                     NE_White, // Color
                     inputsNames[inputs[1 + (controlsPage * 6)].nameIndex]);

    if (ButtonToShow >= 5)
        // Print texts
        NE_TextPrint(0,        // Font slot
                     3, 11,    // Coordinates x(column), y(row)
                     NE_White, // Color
                     inputsNames[inputs[2 + (controlsPage * 6)].nameIndex]);

    if (ButtonToShow >= 6)
        // Print texts
        NE_TextPrint(0,        // Font slot
                     19, 11,   // Coordinates x(column), y(row)
                     NE_White, // Color
                     inputsNames[inputs[3 + (controlsPage * 6)].nameIndex]);

    if (ButtonToShow >= 7)
        // Print texts
        NE_TextPrint(0,        // Font slot
                     3, 16,    // Coordinates x(column), y(row)
                     NE_White, // Color
                     inputsNames[inputs[4 + (controlsPage * 6)].nameIndex]);

    if (ButtonToShow >= 8)
        // Print texts
        NE_TextPrint(0,        // Font slot
                     19, 16,   // Coordinates x(column), y(row)
                     NE_White, // Color
                     inputsNames[inputs[5 + (controlsPage * 6)].nameIndex]);
}

/**
 * @brief Draw map selection with image menu
 *
 */
void drawSelectionMapImageMenu()
{
    // Print texts
    NE_TextPrint(0,        // Font slot
                 11, 1,    // Coordinates x(column), y(row)
                 NE_White, // Color
                 "Select map");

    NE_2DDrawTexturedQuad(0 + 32, 20, 192 + 32, 139 + 20, 3, BottomScreenSpritesMaterials[6]); // Draw map image

    // Text background
    NE_PolyFormat(15, 0, NE_LIGHT_0, NE_CULL_BACK, NE_MODULATION);
    NE_2DDrawQuad(0 + 32, 20, 192 + 32, 16 + 20, 2, RGB15(0, 0, 0));
    NE_PolyFormat(31, 0, NE_LIGHT_0, NE_CULL_BACK, NE_MODULATION);

    // Print map name
    printLongConstChar(4, 31 - 4, 3, allMaps[currentSelectionMap].name);
}

/**
 * @brief Draw map selection with list menu (not implemented, maybe to remove)
 *
 */
void drawSelectionMapListMenu()
{
    // Print texts
    NE_TextPrint(0,        // Font slot
                 11, 1,    // Coordinates x(column), y(row)
                 NE_White, // Color
                 "Select map");
}

/**
 * @brief Draw online error menu
 *
 */
void drawPartyModeSelectionMenu()
{
    NE_TextPrint(0,        // Font slot
                 7, 1,     // Coordinates x(column), y(row)
                 NE_White, // Color
                 "Select a game mode");
}

/**
 * @brief Draw multiplayer menu
 *
 */
void drawJoinCreatePartyMenu()
{
    // Print texts
    NE_TextPrint(0,        // Font slot
                 11, 1,    // Coordinates x(column), y(row)
                 NE_White, // Color
                 "Multiplayer");

    NE_TextPrint(0,        // Font slot
                 4, 23,    // Coordinates x(column), y(row)
                 NE_White, // Color
                 DISCORD_LINK);
}

/**
 * @brief Draw enter code menu
 *
 */
void drawEnterCodeMenu()
{
    // Nothing here
}

/**
 * @brief Draw online error menu
 *
 */
void drawOnlineErrorMenu()
{
    NE_TextPrint(0,        // Font slot
                 11, 1,    // Coordinates x(column), y(row)
                 NE_White, // Color
                 "Information:");

    printLongText(2, 30, 5, errorText);
}

void drawStatsMenu()
{
    NE_TextPrint(0,        // Font slot
                 11, 1,    // Coordinates x(column), y(row)
                 NE_White, // Color
                 "Statistics:");

    char botKillCountText[26];
    sprintf(botKillCountText, "Bots killed: %d", totalBotsKillCount);
    printLongText(0, 33, 5, botKillCountText);

    char onlineKillCountText[26];
    sprintf(onlineKillCountText, "Online players killed: %d", totalOnlinePlayersKillCount);
    printLongText(0, 33, 8, onlineKillCountText);

    char timerText[26];
    sprintf(timerText, "Time played: %02dh:%02dm", totalPlayedHours, totalPlayedMinutes);
    printLongText(0, 33, 11, timerText);

    char deathCountText[26];
    sprintf(deathCountText, "Death count: %d", totalDeathCount);
    printLongText(0, 33, 14, deathCountText);

    char partyCountText[26];
    sprintf(partyCountText, "Finished parties: %d", totalFinishedParty);
    printLongText(0, 33, 17, partyCountText);

    char winCountText2[26];
    sprintf(winCountText2, "Victories: %d", totalWins);
    printLongText(0, 33, 20, winCountText2);
}

/**
 * @brief Draw every buttons of the menu
 *
 */
void drawButtons()
{
    for (int i = 0; i < ButtonToShow; i++)
    {
        if (!AllButtons[i].isHidden)
        {
            int ButtonXPosition = AllButtons[i].xPos, ButtonYPosition = AllButtons[i].yPos, ButtonXSize = AllButtons[i].xSize, ButtonYSize = AllButtons[i].ySize;
            NE_2DDrawQuad(ButtonXPosition, ButtonYPosition, ButtonXPosition + ButtonXSize, ButtonYPosition + ButtonYSize, 1, RGB15(5, 5, 5)); // Button background

            NE_TextPrint(0,                                              // Font slot
                         AllButtons[i].xTextPos, AllButtons[i].yTextPos, // Coordinates x(column), y(row)
                         NE_White,                                       // Color
                         AllButtons[i].text);
        }
    }
}

/**
 * @brief Draw every checkboxs of the menu
 *
 */
void drawCheckboxs()
{
    for (int i = 0; i < checkBoxToShow; i++)
    {
        int CheckBoxXPosition = AllCheckBoxs[i].xPos, CheckBoxYPosition = AllCheckBoxs[i].yPos, CheckBoxXSize = AllCheckBoxs[i].xSize, CheckBoxYSize = AllCheckBoxs[i].ySize;
        NE_2DDrawTexturedQuadColor(CheckBoxXPosition - 3, CheckBoxYPosition - 3, CheckBoxXPosition + CheckBoxXSize + 3, CheckBoxYPosition + CheckBoxYSize + 3, 2, BottomScreenSpritesMaterials[5], RGB15(4, 4, 4)); // Border
        NE_2DDrawTexturedQuadColor(CheckBoxXPosition - 1, CheckBoxYPosition - 1, CheckBoxXPosition + CheckBoxXSize + 1, CheckBoxYPosition + CheckBoxYSize + 1, 1, BottomScreenSpritesMaterials[5], RGB15(5, 5, 5)); // Background
        if (*AllCheckBoxs[i].value)
            NE_2DDrawTexturedQuadColor(CheckBoxXPosition, CheckBoxYPosition, CheckBoxXPosition + CheckBoxXSize, CheckBoxYPosition + CheckBoxYSize, 0, BottomScreenSpritesMaterials[4], RGB15(31, 31, 31)); // CheckMark
    }
}

/**
 * @brief Draw every sliders of the menu
 *
 */
void drawSliders()
{
    for (int i = 0; i < sliderToShow; i++)
    {
        int SliderXPosition = AllSliders[i].xPos, SliderYPosition = AllSliders[i].yPos, SliderXSize = AllSliders[i].xSize;
        NE_2DDrawQuad(SliderXPosition - 1, SliderYPosition - 2, SliderXPosition + SliderXSize + 1, SliderYPosition + 2, 2, RGB15(5, 5, 5));

        int sliderHandXPos = (int)map(*AllSliders[i].value, AllSliders[i].min, AllSliders[i].max, AllSliders[i].xPos, AllSliders[i].xPos + AllSliders[i].xSize);
        NE_2DDrawTexturedQuadColor(sliderHandXPos - 10, SliderYPosition - 10, sliderHandXPos + 10, SliderYPosition + 10, 1, BottomScreenSpritesMaterials[5], RGB15(7, 7, 7));
    }
}

/**
 * @brief Unload the shop menu
 *
 */
void unloadShopMenu()
{
    // Delete shop gun preview image
    NE_MaterialDelete(BottomScreenSpritesMaterials[6]);
    NE_PaletteDelete(Palettes[10]);
}

/**
 * @brief Unload the gamepad menu
 *
 */
void unloadControllerMenu()
{
    closeMap();

    // Delete button images
    NE_MaterialDelete(BottomScreenSpritesMaterials[7]);
    NE_PaletteDelete(Palettes[14]);
    NE_MaterialDelete(BottomScreenSpritesMaterials[8]);
    NE_PaletteDelete(Palettes[15]);
}

/**
 * @brief Unload the selection map with image menu
 *
 */
void unloadSelectionMapImageMenu()
{
    // Delete shop gun preview image
    NE_MaterialDelete(BottomScreenSpritesMaterials[6]);
    NE_PaletteDelete(Palettes[10]);
}

/**
 * @brief Unload change name menu
 *
 */
void unloadChangeNameMenu()
{
    SendPlayerName = true;
}

/**
 * @brief Unloads controls change menu
 *
 */
void unloadControlsChangeMenu()
{
    stopScanForInput();
}

/**
 * @brief Initiates a ui button.
 *
 */
void addButton(int buttonIndex, int xPos, int yPos, int xSize, int ySize, void(*function)(), int parameter, int xTextPos, int yTextPos, char* text)
{
    AllButtons[buttonIndex].xPos = xPos;
    AllButtons[buttonIndex].yPos = yPos;
    AllButtons[buttonIndex].xSize = xSize;
    AllButtons[buttonIndex].ySize = ySize;
    AllButtons[buttonIndex].OnClick = function;
    if (parameter != NULL_PARAMETER) // If the function has a value to pass as a parameter
        AllButtons[buttonIndex].parameter = parameter;
    AllButtons[buttonIndex].xTextPos = xTextPos;
    AllButtons[buttonIndex].yTextPos = yTextPos;
    AllButtons[buttonIndex].text = text;
}

/**
 * @brief Creates Nitro Engine materials for shop menu.
 *

 */
void CreateShopMenuNE() {
    NE_PaletteDelete(Palettes[10]);
    NE_MaterialDelete(BottomScreenSpritesMaterials[6]);
    BottomScreenSpritesMaterials[6] = NE_MaterialCreate();
    Palettes[10] = NE_PaletteCreate();
    switch (ShopCategory)
    {
        case PISTOLS:
        case HEAVY:
        case SGM:
            NE_MaterialTexLoadBMPtoRGB256(BottomScreenSpritesMaterials[6], Palettes[10], AllGuns[GetSelectedGunShop()].texture, 1);
            break;
        case EQUIPMENT:
            NE_MaterialTexLoadBMPtoRGB256(BottomScreenSpritesMaterials[6], Palettes[10], GetAllGrenades()[GetSelectedGunShop() - GunCount - shopGrenadeCount].texture, 1);
            break;
        case GRENADE:
            NE_MaterialTexLoadBMPtoRGB256(BottomScreenSpritesMaterials[6], Palettes[10], GetAllGrenades()[GetSelectedGunShop() - GunCount].texture, 1);
            break;
    }
}
