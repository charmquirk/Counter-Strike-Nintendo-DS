// SPDX-License-Identifier: MIT
//
// Copyright (c) 2021-2022, Fewnity - Grégory Machefer
//
// This file is part of Counter Strike Nintendo DS Multiplayer Edition (CS:DS)

#include "../main.h"
#include "sounds.h"
#include "movements.h"
#include "grenade.h"
#include "gun.h"
#include "ui.h"
#include "collisions.h"
#include "network.h"
#include "party.h"
#include "player.h"
#include "security.h"
#include "camera.h"

#include <dswifi9.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

// Current ping of the nintendo ds
int ping;

// Socket
int my_socket;

// Retain if the game needs to send a packet
int SendPositionData = 3;
bool SendPosition;
bool SendShoot;
bool SendLeave;
bool SendGrenade;
bool SendBombPlacing;
bool SendBombPlace;
bool SendBombDefused;
bool SendPing;
bool SendTeam;
bool WaitForTeamResponse;
bool SendBuyWeapon;
bool SendWallHit;
bool SendSelectedGun;
bool SendPlayerName;
bool SendVoteStartNow;
bool SendKeyResponse;
bool SendGetDroppedBomb;
bool SendReloaded;

bool firstConnection = true; // Retain if the player is just connected

int frameCount = 0; // For security, we need to retain the number of frame shown
int timeOut = 0; // Timeout timer for the connection

enum JoinType partyOption = JOIN_RANDOM_PARTY; // Join type to send to the server

char partyCode[PARTY_CODE_LENGTH]; // Party code to send to the server or to receive from the server

bool isPrivate; // Is the party private?

char Values[1024] = "";     // Store all the values received from the wifi and waiting for treatment
char TempValues[1024] = ""; // Store values that can't be treated yet

char outgoingData[1024];    // Array of requests to send to the server from this client. For unused networking system.
char incomingData[1024];    // Array of requests to process on this client. For unused networking system.


void initNetwork(int option)
{
    // See Wifi_CheckInit to replace my_socket == 0 by !Wifi_CheckInit()
    // Call Wifi_InitDefault only once (by checking if the socket was already used), or the wifi will not work after that
    if (my_socket == 0 && !Wifi_InitDefault(WFC_CONNECT))
    {
        Connection = UNSELECTED;
        initMainMenu();
    }
    else // Then connect to server
    {
        my_socket = socket(AF_INET, SOCK_STREAM, 0);
        char* url;
        switch (Connection)
        {
            case DEBUG_IP_1:
                url = DEBUG_IP_1_STRING;
                break;
            case DEBUG_IP_2:
                url = DEBUG_IP_2_STRING;
                break;
            case ONLINE_SERVER_IP:
                url = ONLINE_SERVER_IP_STRING;
                break;
            case LOCAL: // Local mode (Android phone with the android server version (unreleased))
                break;
        }
        if (Connection > OFFLINE)
            connectToServer(url, url[0] == '\0', my_socket, option);
        
        Connection = UNSELECTED;
    }
}

/**
 * @brief Connect the game to a server, with an url.
 *
 *
 * @param url Url of the server
 * @param LocalMode Used to connect on a localserver on an android phone
 * @param my_socket socket
 * @param option (JOIN_RANDOM_PARTY, JOIN_PARTY_CODE, JOIN_PRIVATE_PARTY)
 */
void connectToServer(char *url, bool LocalMode, int my_socket, enum JoinType option)
{
    partyOption = option;
    struct hostent *myhost;
    if (LocalMode)
    {
        // Get ip adress of the phone
        struct in_addr gateway;
        Wifi_GetIPInfo(&gateway, NULL, NULL, NULL);

        // Find the IP address of the server, with gethostbyname
        char GatewayChar[22] = "";
        sprintf(GatewayChar, "%s", inet_ntoa(gateway));
        myhost = gethostbyname(GatewayChar);
    }
    else // Find the IP address of the server, with gethostbyname
        myhost = gethostbyname(url);

    // Tell the socket to connect to the IP address we found, on port 6003 or 1080 for android phone
    struct sockaddr_in sain;
    sain.sin_family = AF_INET;
    int port = (LocalMode) ? LOCAL_SERVER_PORT : SERVER_PORT;
    sain.sin_port = htons(port);
    sain.sin_addr.s_addr = *((unsigned long *)(myhost->h_addr_list[0]));

    // Connect to the server
    connect(my_socket, (struct sockaddr *)&sain, sizeof(sain));

    // Set socked in non block mode
    int blockmode = 1;
    ioctl(my_socket, FIONBIO, &blockmode);
    resetNetworkVariables();

    // Launch the read data service
    ReadServerData();
}

/**
 * @brief Reset network send values variables (call before connecting to a new server)
 *
 */
void resetNetworkVariables()
{
    SendShoot = false;
    SendLeave = false;
    SendGrenade = false;
    SendBombPlacing = false;
    SendBombPlace = false;
    SendBombDefused = false;
    SendPing = false;
    SendTeam = false;
    WaitForTeamResponse = false;
    SendBuyWeapon = false;
    SendWallHit = false;
    SendSelectedGun = false;
    SendPlayerName = false;
    SendVoteStartNow = false;
    SendKeyResponse = false;
    SendGetDroppedBomb = false;
    SendReloaded = false;
}

void treatData()
{
    int StartPosition, EndPosition;
    // printf("a:%s\n", Values);

    // If a complete packet is detected
    while ((StartPosition = strstr(Values, "{") - Values + 1) > 0 && (EndPosition = strstr(Values + StartPosition, "}") - Values) > 0)
    {
        char currentPacket[256] = "";
        strncpy(currentPacket, Values + StartPosition, EndPosition - StartPosition);

        //   Start spliting incoming data
        char *ptr = strtok(currentPacket, ";");
        int SplitCount = 0;
        char arr[10][64] = {0};

        // Split data
        while (ptr != NULL)
        {
            strcpy(arr[SplitCount], ptr);
            SplitCount++;
            ptr = strtok(NULL, ";");
        }

        // Check packet info
        if (strcmp(arr[REQUEST_NAME_INDEX], "POS") == 0) // Player position update
        {
            // Get target player id
            int PlayerIdInt = intParse(arr[1]);

            float XFloat = floatParse(arr[2]);
            float YFloat = floatParse(arr[3]);
            float ZFloat = floatParse(arr[4]);
            // Change scale
            XFloat /= 4096.0;
            YFloat /= 4096.0;
            ZFloat /= 4096.0;

            int AngleInt = intParse(arr[5]);
            int CameraAngleInt = intParse(arr[6]);

            // If new position is for the local player
            if (localPlayer->Id == PlayerIdInt)
            {
                // Set new position and teleport player
                localPlayer->position.x = XFloat;
                localPlayer->position.y = YFloat;
                localPlayer->position.z = ZFloat;
                localPlayer->Angle = AngleInt;
                localPlayer->PlayerPhysic->xspeed = 0;
                localPlayer->PlayerPhysic->yspeed = 0;
                localPlayer->PlayerPhysic->zspeed = 0;
                NE_ModelSetCoord(localPlayer->PlayerModel, localPlayer->position.x, localPlayer->position.y, localPlayer->position.z);
                ForceUpdateLookRotation(localPlayer->cameraAngle);
            }
            else
            {
                // Find player with unique ID to update his informations
                for (int i = 1; i < MaxPlayer; i++)
                    if (AllPlayers[i].Id == PlayerIdInt)
                    {
                        // If player has no position, teleport the player to new updated position (0 default position)
                        if (AllPlayers[i].lerpDestination.x == 0)
                        {
                            AllPlayers[i].position.x = XFloat;
                            AllPlayers[i].position.y = YFloat;
                            AllPlayers[i].position.z = ZFloat;
                            AllPlayers[i].Angle = AngleInt;
                        }

                        // Set player destination
                        AllPlayers[i].lerpDestination.x = XFloat;
                        AllPlayers[i].lerpDestination.y = YFloat;
                        AllPlayers[i].lerpDestination.z = ZFloat;

                        AllPlayers[i].AngleDestination = AngleInt;

                        // if (CurrentCameraPlayer == i)
                        // CameraAngleY = CameraAngleInt;
                        AllPlayers[i].cameraAngle = CameraAngleInt;
                        break;
                    }
            }
        }
        else if (strcmp(arr[REQUEST_NAME_INDEX], "WALLHIT") == 0) // Wall hit position update for hit flash at wall animation
        {
            int flashIndex = 0;
            for (int i = 0; i < FLASH_MODELS_COUNT; i++)
            {
                if (ShowWallHitFlash[i] == 0)
                {
                    flashIndex = i;
                    break;
                }
            }

            // Set animation timer
            ShowWallHitFlash[flashIndex] = 3;

            float XFloat = floatParse(arr[1]);
            float YFloat = floatParse(arr[2]);
            float ZFloat = floatParse(arr[3]);

            // Change scale
            XFloat /= 4096.0;
            YFloat /= 4096.0;
            ZFloat /= 4096.0;

            // Set effect position
            NE_ModelSetCoord(flashModels[flashIndex], XFloat, YFloat, ZFloat);

            // Get wall hit flash direction
            Vector2 Direction1D;
            Direction1D.y = YFloat - AllPlayers[CurrentCameraPlayer].position.y - CameraOffsetY + y;
            Direction1D.x = 1;
            normalize2D(&Direction1D);

            Vector3 Direction;
            Direction.x = XFloat - AllPlayers[CurrentCameraPlayer].position.x;
            Direction.y = YFloat - AllPlayers[CurrentCameraPlayer].position.y;
            Direction.z = ZFloat - AllPlayers[CurrentCameraPlayer].position.z;
            normalize(&Direction);

            // Set wall hit angle
            int FinalAngleY = atan2f(Direction.x, Direction.z) * 512.0 / (M_TWOPI) + 384;
            int FinalAngleZ = atan2f(Direction1D.y, 1) * 512.0 / (M_TWOPI) + 128;
            NE_ModelSetRot(flashModels[flashIndex], 0, FinalAngleY, FinalAngleZ);

            // Make 3D sound
            int Panning, Volume;
            Vector4 SoundPos;
            SoundPos.x = XFloat;
            SoundPos.y = YFloat;
            SoundPos.z = ZFloat;
            GetPanningByPosition(&Panning, &Volume, SoundPos, xWithoutYForAudio, zWithoutYForAudio, 0.15);
            Play3DSound(SFX_RIC, Volume, Panning, NULL);
        }
        else if (strcmp(arr[REQUEST_NAME_INDEX], "SHOOT") == 0) // Shoot from a player
        {
            // Get player id
            int ParsedPlayerId = intParse(arr[1]);

            // Get gun id
            int ParsedGunId = intParse(arr[2]);

            // Make a sound
            int Panning, Volume;
            GetPanning(ParsedPlayerId, &Panning, &Volume, xWithoutYForAudio, zWithoutYForAudio, AllGuns[ParsedGunId].MaxSoundDistance);
            Player *player = NULL;
            int index = UNUSED;
            for (int i = 0; i < MaxPlayer; i++)
            {
                if (AllPlayers[i].Id == ParsedPlayerId)
                {
                    player = &AllPlayers[i];
                    index = i;
                    break;
                }
            }
            if (player != NULL)
            {
                if (player->currentGunInInventory == 1 || player->currentGunInInventory == 2)
                    player->AllAmmoMagazine[player->currentGunInInventory - 1].AmmoCount--;

                if (CurrentCameraPlayer == index)
                {
                    setGunRecoil(player);
                }

                if (ParsedGunId < GunCount)
                    Play3DSound(AllGuns[ParsedGunId].gunSound, Volume, Panning, player);
            }
        }
        else if (strcmp(arr[REQUEST_NAME_INDEX], "TimerA") == 0) // Timer changes
        {
            // Parse seconds and minutes texts to int
            PartyMinutes = intParse(arr[1]);
            PartySeconds = intParse(arr[2]);

            if (firstConnection)
            {
                BombSeconds = PartySeconds;
            }
        }
        else if (strcmp(arr[REQUEST_NAME_INDEX], "PING") == 0) // Get ping request from server
        {
            // Parse ping text to int
            ping = intParse(arr[1]);
            SendPing = true;
        }
        else if (strcmp(arr[REQUEST_NAME_INDEX], "CURGUN") == 0) // Set current gun in inventory
        {
            int PlayerIdInt = intParse(arr[1]);

            // Find player to set health
            for (int i = 0; i < MaxPlayer; i++)
            {
                if (AllPlayers[i].Id == PlayerIdInt)
                {
                    int newGun = intParse(arr[2]);
                    AllPlayers[i].currentGunInInventory = newGun;
                    if (i == CurrentCameraPlayer)
                        UpdateGunTexture();

                    break;
                }
            }
        }
        else if (strcmp(arr[REQUEST_NAME_INDEX], "HITSOUND") == 0) // Make a hit sound
        {
            int PlayerId = intParse(arr[2]);
            int HitType = intParse(arr[3]);

            int Panning, Volume;
            if (PlayerId != localPlayer->Id)
                GetPanning(PlayerId, &Panning, &Volume, xWithoutYForAudio, zWithoutYForAudio, 0.10);
            else
            {
                Panning = 128;
                Volume = 255;

                xOffset = (rand() % ScreenShakeAmount + ScreenShakeMinAmount) / 100.0;
                if (rand() % 2 == 0)
                    xOffset = -xOffset;

                yOffset = (rand() % ScreenShakeAmount + ScreenShakeMinAmount) / 100.0;
                if (rand() % 2 == 0)
                    yOffset = -yOffset;

                redHealthTextCounter = 62;
            }

            Player *player = NULL;
            for (int i = 0; i < MaxPlayer; i++)
            {
                if (AllPlayers[i].Id == PlayerId)
                {
                    player = &AllPlayers[i];
                    break;
                }
            }

            switch (HitType)
            {
                case 0:
                    Play3DSound(SFX_FLESH_IMPACT, Volume, Panning, player); // Check with kevlar
                    break;
                case 1:
                    Play3DSound(SFX_HEADSHOT1, Volume, Panning, player);
                    break;
                case 2:
                    Play3DSound(SFX_FLESH_IMPACT, Volume, Panning, player);
                    break;
                default:
                    Play3DSound(SFX_KNIFE_HIT_PLAYER, Volume, Panning, player);
                    break;
            }
        }
        else if (strcmp(arr[REQUEST_NAME_INDEX], "SETID") == 0) // Add player
        {
            int ParsedId = intParse(arr[1]);

            // Spawn player
            AddNewPlayer(ParsedId, true, false);
        }
        else if (strcmp(arr[REQUEST_NAME_INDEX], "SETMAP") == 0) // Set the map
        {
            // Parse map id text to int
            int newMapId = intParse(arr[1]);

            UnLoadMap(currentMap);
            currentMap = newMapId;
            LoadMap(currentMap);
        }
        else if (strcmp(arr[REQUEST_NAME_INDEX], "SETMODE") == 0) // Set the party mode
        {
            // Parse mode id text to int
            currentPartyMode = intParse(arr[1]);
        }
        else if (strcmp(arr[REQUEST_NAME_INDEX], "TEAM") == 0) // Change team
        {
            // Get player id
            int ParsedPlayerId = intParse(arr[1]);

            // Get Team value
            int ParsedIsCounter = intParse(arr[2]);

            // Find player and affect new value
            for (int i = 0; i < MaxPlayer; i++)
                if (AllPlayers[i].Id == ParsedPlayerId)
                {
                    AllPlayers[i].Team = ParsedIsCounter;
                    UpdatePlayerTexture(i);
                    break;
                }

            // Update screen if team screen is opened
            switch (currentMenu)
            {
                case SCORE_BOARD:
                    UpdateBottomScreenFrameCount += 8;
                    break;
                case GAME:
                    initGameMenu();
                    break;
            }

            // TODO check this
            if (localPlayer->Id == ParsedPlayerId)
            {
                if (roundState != TRAINING)
                    changeCameraPlayerView(false);

                AllButtons[0].isHidden = ParsedIsCounter == -1;
                AllButtons[1].isHidden = false;
                AllButtons[2].isHidden = false;
                WaitForTeamResponse = false;
            }
        }
        else if (strcmp(arr[REQUEST_NAME_INDEX], "BOMBPLACE") == 0) // Bomb place update
        {
            // Convert chars into ints
            int XInt = intParse(arr[1]);
            int YInt = intParse(arr[2]);
            int ZInt = intParse(arr[3]);
            int AngleInt = intParse(arr[4]);
            int dropped = intParse(arr[5]);

            if (dropped == 1)
            {
                // Change values scale and set bomb position and angle value
                droppedBombPositionAndRotation.x = XInt / 4096.0;
                droppedBombPositionAndRotation.y = YInt / 4096.0;
                droppedBombPositionAndRotation.z = ZInt / 4096.0;
                droppedBombPositionAndRotation.r = AngleInt;

                // Set bomb 3D model position and angle value
                NE_ModelSetCoord(Model[7], droppedBombPositionAndRotation.x, droppedBombPositionAndRotation.y, droppedBombPositionAndRotation.z);
                // NE_ModelSetCoord(Model[10], droppedBombPositionAndRotation.x, droppedBombPositionAndRotation.y, droppedBombPositionAndRotation.z);
                Model[7]->rz = droppedBombPositionAndRotation.r;

                SetBombTakingZone(droppedBombPositionAndRotation.x, droppedBombPositionAndRotation.z, &bombDefuseZone); // Set zone for taking the bomb
                bombDropped = true;
            }
            else
            {
                // Change values scale and set bomb position and angle value
                BombPosition.x = XInt / 4096.0;
                BombPosition.y = YInt / 4096.0;
                BombPosition.z = ZInt / 4096.0;
                BombPosition.r = AngleInt;

                // Set bomb 3D model position and angle value
                NE_ModelSetCoord(Model[7], BombPosition.x, BombPosition.y, BombPosition.z);
                NE_ModelSetCoord(Model[10], BombPosition.x, BombPosition.y, BombPosition.z);
                Model[7]->rz = BombPosition.r;

                BombSeconds = allPartyModes[currentPartyMode].bombWaitingSecondsDuration;

                bombBipTimer = 120;
                BombPlanted = true;
                SetBombDefuseZone(BombPosition.x, BombPosition.z, &bombDefuseZone);
            }
        }
        else if (strcmp(arr[REQUEST_NAME_INDEX], "VOTERESULT") == 0) // Get the current vote values
        {
            int VoteTypeInt = intParse(arr[1]);

            if (VoteTypeInt == 0)
            {
                playerWantToStart = intParse(arr[2]);
                playerWantToStartLimite = intParse(arr[3]);
            }
        }
        else if (strcmp(arr[REQUEST_NAME_INDEX], "CONFIRM") == 0) // Confirm shop gun buy, or set player gun from server command
        {
            // Convert chars into ints
            int ConfirmTypeInt = intParse(arr[1]);
            int ConfirmArgumentInt = intParse(arr[2]);
            int ConfirmInventoryIndexInt = intParse(arr[3]);
            int ConfirmResultInt = intParse(arr[4]); // 0 Error, 1 Okay

            if (ConfirmTypeInt) // Set gun in inventory
            {
                switch (ConfirmResultInt) // Not finished in source code. Ignore empty entries.
                {
                    case 0: // Error
                        break;
                    case 1: // Okay
                        DisableAim();
                        SetGunInInventory(ConfirmArgumentInt, ConfirmInventoryIndexInt);
                        setSelectedGunInInventory(0, ConfirmInventoryIndexInt);
                        break;
                    case 2: // Not an error
                        break;
                }
            }
        }
        else if (strcmp(arr[REQUEST_NAME_INDEX], "SETMONEY") == 0) // Set player money
        {
            // Parse money text to int
            int ParsedMoney = intParse(arr[1]);
            setPlayerMoney(0, ParsedMoney);
        }
        else if (strcmp(arr[REQUEST_NAME_INDEX], "SCORE") == 0) // Party score changes
        {
            // Parse scores texts to int
            CounterScore = intParse(arr[1]);
            TerroristsScore = intParse(arr[2]);

            // Update screen if team screen is opened
            if (currentMenu == SCORE_BOARD)
                UpdateBottomScreenFrameCount += 8;
        }
        else if (strcmp(arr[REQUEST_NAME_INDEX], "SCRBOARD") == 0) // Score board score changes
        {
            // Get player id
            int ParsedPlayerId = intParse(arr[1]);

            // Find player with unique ID to update his informations
            for (int i = 0; i < MaxPlayer; i++)
                if (AllPlayers[i].Id == ParsedPlayerId)
                {
                    AllPlayers[i].KillCount = intParse(arr[2]);
                    AllPlayers[i].DeathCount = intParse(arr[3]);
                    break;
                }

            // Update screen if team screen is opened
            if (currentMenu == SCORE_BOARD)
                UpdateBottomScreenFrameCount += 8;
        }
        else if (strcmp(arr[REQUEST_NAME_INDEX], "SETSHOPZONE") == 0) // Update the shop zone
        {
            shopDisableTimer = SHOP_DISABLE_TIMER;
            setShopZone(localPlayer);
        }
        else if (strcmp(arr[REQUEST_NAME_INDEX], "PartyRound") == 0) // Round state changes
        {
            // Parse text to int
            int tempRoundState = intParse(arr[1]);

            roundState = tempRoundState;

            // Reset some values on new round
            if (roundState == WAIT_START)
            {
                onNewRoundStart();
                NE_SpriteVisible(TopScreenSprites[0], true);

                StopReloading(0);
                ResetGunsAmmo(0);
                if (CurrentCameraPlayer != 0)
                {
                    SetCurrentCameraPlayer(0);
                }
                for (int i = 0; i < MaxPlayer; i++)
                    AllPlayers[i].IsDead = FALSE;

                deathCameraAnimation = 0;
                deathCameraYOffset = 0;
                redHealthTextCounter = 0;
            }
        }
        else if (strcmp(arr[REQUEST_NAME_INDEX], "LEAVE") == 0) // When a player leave
        {
            // Get player id
            int ParsedPlayerId = intParse(arr[1]);

            // Find player
            for (int i = 0; i < MaxPlayer; i++)
                if (AllPlayers[i].Id == ParsedPlayerId)
                {
                    AllPlayers[i].Id = UNUSED;
                    PlayerCount--;
                    NE_ModelDelete(AllPlayers[i].PlayerModel);
                    if (i == 0)
                    {
                        NE_PhysicsDelete(AllPlayers[i].PlayerPhysic);
                    }
                    else
                    {
                        showDisconnectedText(i);
                    }
                    AllPlayers[i].KillCount = 0;
                    AllPlayers[i].Team = SPECTATOR;
                    AllPlayers[i].DeathCount = 0;
                    if (currentMenu == SCORE_BOARD)
                        initScoreMenu();

                    if (CurrentCameraPlayer == i)
                    {
                        SetCurrentCameraPlayer(0);
                    }

                    break;
                }
        }
        else if (strcmp(arr[REQUEST_NAME_INDEX], "SETHEALTH") == 0) // Set player health
        {
            int PlayerIdInt = intParse(arr[1]);

            // Find player to set health
            for (int i = 0; i < MaxPlayer; i++)
            {
                if (AllPlayers[i].Id == PlayerIdInt)
                {
                    int newHealth = intParse(arr[2]);
                    setPlayerHealth(i, newHealth);
                    AllPlayers[i].armor = intParse(arr[3]);

                    int haveHeadset = intParse(arr[4]);
                    AllPlayers[i].haveHeadset = haveHeadset == 1 ? true : false;
                    break;
                }
            }
        }
        else if (strcmp(arr[REQUEST_NAME_INDEX], "SETBOMB") == 0) // Set/remove the bomb of a player
        {
            // Get player id
            int ParsedPlayerId = intParse(arr[1]);

            // Get gun id
            int haveBomb = intParse(arr[2]);

            for (int i = 0; i < MaxPlayer; i++)
            {
                if (AllPlayers[i].Id == ParsedPlayerId && haveBomb == 1)
                {
                    AllPlayers[i].haveBomb = true;
                    bombDropped = false;
                    SetGunInInventoryForNonLocalPlayer(i, 28, 8);
                }
                else
                {
                    AllPlayers[i].haveBomb = false;
                    SetGunInInventoryForNonLocalPlayer(i, EMPTY, 8);
                }
            }
        }
        else if (strcmp(arr[REQUEST_NAME_INDEX], "RELOADED") == 0) // Reload the gun of a player
        {
            // Get player id
            int ParsedPlayerId = intParse(arr[1]);

            int Reset = intParse(arr[2]);
            switch (Reset)
            {
                case 0:
                case 1:
                    ResetGunsAmmo(ParsedPlayerId);
                    break;
                case 2:
                    int GunId = intParse(arr[4]);
                    ResetGunAmmo(ParsedPlayerId, GunId);
                    break;
            }
        }
        else if (strcmp(arr[REQUEST_NAME_INDEX], "SETNAME") == 0) // Get a player name
        {
            // Get player id
            int ParsedPlayerId = intParse(arr[1]);

            for (int i = 0; i < MaxPlayer; i++)
            {
                if (AllPlayers[i].Id == ParsedPlayerId)
                {
                    strcpy(AllPlayers[i].name, arr[2]);
                    if (i != 0 && !firstConnection)
                        showConnectedText(i);
                    break;
                }
            }

            // Update screen if team screen is opened
            if (currentMenu == 2)
                UpdateBottomScreenFrameCount += 8;
        }
        else if (strcmp(arr[REQUEST_NAME_INDEX], "SETCODE") == 0) // Get the party code
        {
            strcpy(partyCode, arr[1]);
            isPrivate = true;
        }
        else if (strcmp(arr[REQUEST_NAME_INDEX], "ERROR") == 0) // Show text from server
        {
            int errorId = intParse(arr[1]);
            char* newText;
            switch (errorId)
            {
                case Ban:
                    newText = "You are banned!";
                    break;
                case WrongVersion:
                    newText = "You are using an old version of the game, please update your game.";
                    break;
                case MacAddressMissing:
                    newText = "Your Nintendo DS doesn't have a MAC address.";
                    break;
                case WrongSecurityKey:
                    newText = "You are using a modified game.";
                    break;
                case ServerFull:
                    newText = "The server is currently full, please try again later.";
                    break;
                case ServerStopped:
                    newText = "The server is currently under maintenance.";
                    break;
                case SaveCorrupted:
                    newText = "Your save is corrupted.";
                    break;
                case IncorrectCode:
                    newText = "The code is incorrect.";
                    break;
                case KickTeamKill:
                    newText = "You have been kicked (Team kill).";
                    break;
                default:
                    newText = "An error has occurred!";
                    break;
            }
            strcpy(errorText, newText);
            initOnlineErrorMenu();
        }
        else if (strcmp(arr[REQUEST_NAME_INDEX], "TEXT") == 0) // Show text from server
        {
            int ParsedTextId = intParse(arr[1]);

            showPartyEventText(ParsedTextId);
        }
        else if (strcmp(arr[REQUEST_NAME_INDEX], "TEXTPLAYER") == 0) // Show text about a player from server
        {
            int ParsedPlayerId = intParse(arr[1]);

            int KillerIdInt = intParse(arr[2]);
            // int TextIdInt = intParse(arr[3]); unused

            int killerPlayerIndex = NO_PLAYER;
            int killedPlayerIndex = NO_PLAYER;

            if (KillerIdInt != NO_PLAYER)
            {
                // Find player with unique ID to update his informations
                for (int i = 0; i < MaxPlayer; i++)
                {
                    if (AllPlayers[i].Id == KillerIdInt)
                    {
                        killerPlayerIndex = i;
                        break;
                    }
                }
            }

            if (ParsedPlayerId != NO_PLAYER)
            {
                for (int i = 0; i < MaxPlayer; i++)
                {
                    if (AllPlayers[i].Id == ParsedPlayerId)
                    {
                        killedPlayerIndex = i;
                        break;
                    }
                }
            }

            if (killerPlayerIndex != NO_PLAYER && killerPlayerIndex != NO_PLAYER)
                showKillText(killerPlayerIndex, killedPlayerIndex);
        }
        else if (strcmp(arr[REQUEST_NAME_INDEX], "BOMBPLACING") == 0) // Bomb placing sound
        {
            int ParsedPlayerId = intParse(arr[1]);

            int Panning, Volume;
            GetPanning(ParsedPlayerId, &Panning, &Volume, xWithoutYForAudio, zWithoutYForAudio, 0.10);

            Play3DSound(SFX_BOMBPLANTING, Volume, Panning, GetPlayer(ParsedPlayerId));
        }
        else if (strcmp(arr[REQUEST_NAME_INDEX], "BOMBDEFUSE") == 0) // Bomb defuse sound
        {
            BombDefused = true;
        }
        else if (strcmp(arr[REQUEST_NAME_INDEX], "ADDRANGE") == 0) // Add multiples non local players
        {
            for (int i = 1; i < SplitCount; i++)
            {
                int FoundId = intParse(arr[i]);
                AddNewPlayer(FoundId, false, false);
            }
        }
        else if (strcmp(arr[REQUEST_NAME_INDEX], "ENDGAME") == 0) // End the party
        {
            partyFinished = true;
            initFinalScoreMenu();
        }
        else if (strcmp(arr[REQUEST_NAME_INDEX], "ENDUPDATE") == 0) // End the update party sequence
        {
            firstConnection = false;
        }
        else if (strcmp(arr[REQUEST_NAME_INDEX], "INVTORY") == 0) // Get the inventory of a player
        {
            int ParsedPlayerId = intParse(arr[1]);

            int FoundPlayer = UNUSED;
            for (int i = 0; i < MaxPlayer; i++)
            {
                if (AllPlayers[i].Id == ParsedPlayerId)
                {
                    FoundPlayer = i;
                    for (int i2 = 2; i2 < SplitCount; i2++)
                    {
                        int FoundGunId = intParse(arr[i2]);
                        SetGunInInventoryForNonLocalPlayer(i, FoundGunId, i2 - 2);
                    }
                    break;
                }
            }
            // If the current player camera is looked by the local player, update the gun texture
            if (FoundPlayer == CurrentCameraPlayer)
            {
                UpdateGunTexture();
            }
        }
        else if (strcmp(arr[REQUEST_NAME_INDEX], "AMMO") == 0) // Get the ammo count of a player
        {
            int ParsedPlayerId = intParse(arr[1]);
            int FoundPlayer = UNUSED;
            for (int playerIndex = 0; playerIndex < MaxPlayer; playerIndex++)
            {
                if (AllPlayers[playerIndex].Id == ParsedPlayerId)
                {
                    FoundPlayer = playerIndex;
                    for (int i2 = 0; i2 < 2; i2++)
                    {
                        int AmmoCountInt = intParse(arr[2 + i2 * 2]);
                        int TotalAmmoCountInt = intParse(arr[2 + i2 * 2 + 1]);

                        AllPlayers[FoundPlayer].AllAmmoMagazine[i2].AmmoCount = AmmoCountInt;
                        AllPlayers[FoundPlayer].AllAmmoMagazine[i2].TotalAmmoCount = TotalAmmoCountInt;
                    }
                    break;
                }
            }
        }
        else if (strcmp(arr[REQUEST_NAME_INDEX], "KEY") == 0) // Get the key from the server
        {
            SendKeyResponse = true;
            serverKey = intParse(arr[1]);
        }
        else if (strcmp(arr[REQUEST_NAME_INDEX], "GRENADE") == 0) // Spawn a player's grenade
        {
            // Convert chars into ints
            float XDirection = floatParse(arr[1]);
            float YDirection = floatParse(arr[2]);
            float ZDirection = floatParse(arr[3]);
            int XInt = intParse(arr[4]);
            int YInt = intParse(arr[5]);
            int ZInt = intParse(arr[6]);
            int GrenadeType = intParse(arr[7]);

            PhysicalGrenade *newGrenade = CreateGrenade(GrenadeType, 0);
            if (newGrenade != NULL)
            {
                lanchGrenade(newGrenade, XDirection, YDirection, ZDirection, XInt, YInt, ZInt);
            }
        }

        // Clear "TempValues"
        for (int i = 0; i < sizeof(TempValues); i++)
            TempValues[i] = '\0';
        // Values[strlen(Values)] = '\0';
        //  Add all characters after current data packet to "TempValues"
        strcat(TempValues, Values + EndPosition + 1);
        // Copy "TempValues" to "Values"
        strcpy(Values, TempValues);
    }
}

/**
 * @brief Read data from the game server
 *
 */
void ReadServerData()
{
    // All temp variable for incoming data
    int recvd_len = 0;
    char incoming_buffer[64];

    // Read a maximum of 64 char in one loop
    while ((recvd_len = recv(my_socket, incoming_buffer, 63, 0)) != 0) // if recv returns 0, the socket has been closed. (Sometimes yes, sometimes not, lol)
    {
        if (recvd_len > 0)
        {
            // printf("\n{%d %s}\n", recvd_len, wirelessData);
            timeOut = 0;
            incoming_buffer[recvd_len] = 0; // null-terminate
            // add incoming_buffer to Values
            strncat(Values, incoming_buffer, recvd_len);
            treatData();
        }
        else if (recvd_len == -1)
        {
            // If the recv() returns -1 for too long, the socket has been closed.
            timeOut++;
            if (timeOut == 400)
                break;
        }

        if (localPlayer->Id != UNUSED)
        {
            // Play game code
            GameLoop();
        }

        UpdateEngine();
        sendDataToServer();
    }
    shutdown(my_socket, 0); // good practice to shutdown the socket.
    closesocket(my_socket);
    Connection = UNSELECTED;
    if (!partyFinished && currentMenu != ONLINE_ERROR)
    {
        initMainMenu();
    }
    partyFinished = false;
    isPrivate = false;
    firstConnection = true;
    OnPartyQuit();
}

/**
 * @brief Send data to the server
 *
 */
void sendDataToServer()
{
    // Check if position need to be updated for other player
    if (localPlayer->position.x != OldxPos || localPlayer->position.y != OldyPos || localPlayer->position.z != OldzPos)
    {
        OldxPos = localPlayer->position.x;
        OldyPos = localPlayer->position.y;
        OldzPos = localPlayer->position.z;
        SendPosition = true;
    }

    char InfoToSend[1024] = "";
    // Send shoot data for sound/animation for clients
    if (SendLeave)
    {
        sprintf(InfoToSend + strlen(InfoToSend), "{%d}", LEAVE);
        SendLeave = false;
    }

    // Send position one in two
    if (SendPositionData > 0)
        SendPositionData--;

    // If local player need to be updated for other player (59 bytes)
    if (localPlayer->Id != -1 && SendPosition && SendPositionData == 0)
    {
        sprintf(InfoToSend + strlen(InfoToSend), "{%d;%d;%d;%d;%d;%0.0f}", POS, localPlayer->PlayerModel->x, localPlayer->PlayerModel->y, localPlayer->PlayerModel->z, (int)localPlayer->Angle, localPlayer->cameraAngle);

        SendPosition = false;
        SendPositionData = 4;
    }

    // Send grenade launch
    if (SendGrenade)
    {
        sprintf(InfoToSend + strlen(InfoToSend), "{%d;%f;%f;%f}", GRENADE, x, y, z);
        SendGrenade = false;
    }

    // Send bomb placement data
    if (SendBombPlace)
    {
        SendBombPlace = false;
        sprintf(InfoToSend + strlen(InfoToSend), "{%d;%d;%d;%d;%d}", BOMBPLACE, (int)(BombPosition.x * 4096.0), (int)(BombPosition.y * 4096.0), (int)(BombPosition.z * 4096.0), BombPosition.r);
    }

    // Send bomb defuse data
    if (SendBombDefused)
    {
        SendBombDefused = false;
        sprintf(InfoToSend + strlen(InfoToSend), "{%d}", BOMBDEFUSE);
    }

    // Send bomb planting data
    if (SendBombPlacing)
    {
        SendBombPlacing = false;
        sprintf(InfoToSend + strlen(InfoToSend), "{%d}", BOMBPLACING);
    }

    // Send team data
    if (SendTeam)
    {
        SendTeam = false;
        sprintf(InfoToSend + strlen(InfoToSend), "{%d;%d}", TEAM, tempTeam);
        tempTeam = -1;
    }

    // Send ping reply
    if (SendPing)
    {
        SendPing = false;
        sprintf(InfoToSend + strlen(InfoToSend), "{%d}", PING);
    }

    // Send buy weapon request data
    if (SendBuyWeapon)
    {
        SendBuyWeapon = false;
        sprintf(InfoToSend + strlen(InfoToSend), "{%d;%d}", BUY, GetSelectedGunShop());
    }

    // Send gun reloaded data
    if (SendReloaded)
    {
        SendReloaded = false;
        sprintf(InfoToSend + strlen(InfoToSend), "{%d}", RELOADED);
    }

    // Send selected gun
    if (SendSelectedGun)
    {
        SendSelectedGun = false;
        sprintf(InfoToSend + strlen(InfoToSend), "{%d;%d}", CURGUN, localPlayer->currentGunInInventory);
    }

    // Send shoot data for sound/animation for clients
    if (SendShoot)
    {
        sprintf(InfoToSend + strlen(InfoToSend), "{%d}", SHOOT);
        SendShoot = false;
    }

    // Check if the game needs to send hitted clients
    bool sendHitClient = false;
    for (int i = 0; i < FLASH_MODELS_COUNT; i++)
    {
        if (hittedClient[i] != NO_PLAYER)
        {
            sendHitClient = true;
            break;
        }
    }

    // Send hit data
    if (sendHitClient)
    {
        sprintf(InfoToSend + strlen(InfoToSend), "{%d", HIT);
        for (int i = 0; i < FLASH_MODELS_COUNT; i++)
        {
            if (hittedClient[i] != -1)
            {
                sprintf(InfoToSend + strlen(InfoToSend), ";%d;%d;%d;%f", AllPlayers[hittedClient[i]].Id, localPlayer->IsHeadShot[i], localPlayer->IsLegShot[i], hitDistance[i]);
                hittedClient[i] = NO_PLAYER;
            }
        }
        sprintf(InfoToSend + strlen(InfoToSend), "}");
    }

    // Send vote
    if (SendVoteStartNow)
    {
        SendVoteStartNow = false;
        sprintf(InfoToSend + strlen(InfoToSend), "{%d;0}", VOTE);
    }

    // Send wall hit data
    if (SendWallHit)
    {
        SendWallHit = false;
        for (int i = 0; i < getPlayerCurrentGun(localPlayer).bulletCountPerShoot; i++)
        {
            sprintf(InfoToSend + strlen(InfoToSend), "{%d;%d;%d;%d}", WALLHIT, WallHitXPos[i], WallHitYPos[i], WallHitZPos[i]);
        }
    }

    // Send local player name
    if (SendPlayerName)
    {
        SendPlayerName = false;
        sprintf(InfoToSend + strlen(InfoToSend), "{%d;%s}", SETNAME, localPlayer->name);
    }

    // Send security key response
    if (SendKeyResponse)
    {
        SendKeyResponse = false;

        u8 macAddress[6];
        Wifi_GetData(WIFIGETDATA_MACADDRESS, 6, macAddress);

        sprintf(InfoToSend + strlen(InfoToSend), "{%d;%d;%02X%02X%02X%02X%02X%02X;%s;%s}", KEY, getKeyResponse(serverKey), macAddress[0], macAddress[1], macAddress[2], macAddress[3], macAddress[4], macAddress[5], localPlayer->name, GAME_VERSION);

        if (partyOption == JOIN_PRIVATE_PARTY)
            sprintf(InfoToSend + strlen(InfoToSend), "{%d;%d;%s}", PARTY, partyOption, partyCode);
        else
            sprintf(InfoToSend + strlen(InfoToSend), "{%d;%d}", PARTY, partyOption);
    }

    // Send get bomb dropped event
    if (SendGetDroppedBomb)
    {
        SendGetDroppedBomb = false;
        sprintf(InfoToSend + strlen(InfoToSend), "{%d}", GETBOMB);
    }

    // Get string data length
    int InfoLength = strlen(InfoToSend);
    if (InfoLength <= 0)
        return;

    // Send the current frame
    char tmp[20];
    sprintf(tmp, "{%d;%d}", FRAME, frameCount);
    send(my_socket, tmp, strlen(tmp), 0); // Send data to server
    send(my_socket, InfoToSend, InfoLength, 0); // Send data to server
    // sendto(my_socket, InfoToSend, InfoLength, 0, (struct sockaddr *)&sain, sizeof(sain)); //UDP
}

/**
 * @brief Parse a string to an int
 *
 * @param charToParse
 * @return int
 */
int intParse(char *charToParse)
{
    int result = 0;
    sscanf(charToParse, "%d", &result);
    return result;
}

/**
 * @brief Parse a string to a float
 *
 * @param charToParse
 * @return int
 */
float floatParse(char *charToParse)
{
    float result = 0;
    sscanf(charToParse, "%f", &result);
    return result;
}

/**
 * @brief Get player from id
 *
 * @param id
 * @return Player*
 */
Player *GetPlayer(int id)
{
    for (int i = 0; i < MaxPlayer; i++)
    {
        if (AllPlayers[i].Id == id)
            return &AllPlayers[i];
    }
    return NULL;
}

/**
 * @brief Queue a request to be sent to and processed by the server
 *  
 * @param id Request type
 * @param data Request data
 * 
*/
void QueueRequest(int type, char* data)
{
    char requestString[1024];
    sprintf(requestString, "%d %d", type, data);
    outgoingData[sizeof(outgoingData)] = requestString;
}

/**
* @brief Collect data from the client to send to the server
*
*/
void SendDataToServer()
{
    if (sizeof(outgoingData) <= 0)
        printf("No queued requests!");
        return;
    
    int outgoingRequestType;           // The number representing the request type
    char outgoingRequestData[1024];     // The data related to the request.
    char* endptr;                       // For strtol error checking

    printf("Sending requests to server...");
    for (int i = 0; i < sizeof(outgoingData), i++)
    {
        // Check if the string is too long
        if (strlen(outgoingData[i]) >= sizeof(outgoingRequestData)) {
            printf("String %d is too long to process!\n", i);
            continue;
        }
        
        // Check if the string is too short
        strncpy(outgoingRequestData, outgoingData[i], sizeof(outgoingRequestData));
        outgoingRequestData[sizeof(outgoingRequestData)-1] = '\0'; // Ensure null termination
        if (outgoingRequestData[0] == '\0' || strlen(outgoingRequestData) < 2) {
            printf("String %d was too short or empty!\n", i);
            continue;
        }

        // Check for a valid request type
        outgoingRequestType = strtol(outgoingRequestData, &endptr, 10);
        if (endptr == outgoingRequestData) {
            printf("String %d has no digits!\n", i);
            continue;
        }

        send(my_socket, outgoingRequestData, strlen(outgoingRequestData), 0);
        printf("Sending a request to server! Outgoing Data: %d", outgoingRequestData);
    }
    memset(outgoingData, 0, sizeof(outgoingData)); // Clear the outgoing data array
}

/**
 * @brief Receive and process data from the server on the client
 * 
 */
void ReceiveDataFromServer()
{
    if (sizeof(incomingData) <= 0)
        printf("No incoming data!");
        return;
    
    int incomingRequestType;            // The number representing the request type
    char incomingRequestData[1024];     // The data related to the request
    char* endptr;                       // For strtol error checking

    printf("Reading incoming data from server...");
    for (int i = 0; i < sizeof(incomingData); i++)
    {
        // Ensure we don't overflow our buffer
        if (strlen(incomingData[i]) >= sizeof(incomingRequestData)) {
            printf("String %d is too long to process!\n", i);
            continue;
        }
        
        strncpy(incomingRequestData, incomingData[i], sizeof(incomingRequestData));
        incomingRequestData[sizeof(incomingRequestData)-1] = '\0'; // Ensure null termination

        if (incomingRequestData[0] == '\0' || strlen(incomingRequestData) < 2) {
            printf("String %d was too short or empty!\n", i);
            continue;
        }
        
        incomingRequestType = strtol(incomingRequestData, &endptr, 10);
        if (endptr == incomingRequestData) {
            printf("String %d has no digits!\n", i);
            continue;
        }
        
        switch (incomingRequestType)
        {
            case PING: // Get ping request from server
                ping = intParse(arr[1]); // Parse ping text to int
                SendPing = true;
                break;
            
            case STPEP:
                break;
            
            case WALLHIT: // Wall hit position update for hit flash at wall animation
                int flashIndex = 0;
                for (int i = 0; i < FLASH_MODELS_COUNT; i++)
                {
                    if (ShowWallHitFlash[i] == 0)
                    {
                        flashIndex = i;
                        break;
                    }
                }

                // Set animation timer
                ShowWallHitFlash[flashIndex] = 3;

                float XFloat = floatParse(arr[1]);
                float YFloat = floatParse(arr[2]);
                float ZFloat = floatParse(arr[3]);

                // Change scale
                XFloat /= 4096.0;
                YFloat /= 4096.0;
                ZFloat /= 4096.0;

                // Set effect position
                NE_ModelSetCoord(flashModels[flashIndex], XFloat, YFloat, ZFloat);

                // Get wall hit flash direction
                Vector2 Direction1D;
                Direction1D.y = YFloat - AllPlayers[CurrentCameraPlayer].position.y - CameraOffsetY + y;
                Direction1D.x = 1;
                normalize2D(&Direction1D);

                Vector3 Direction;
                Direction.x = XFloat - AllPlayers[CurrentCameraPlayer].position.x;
                Direction.y = YFloat - AllPlayers[CurrentCameraPlayer].position.y;
                Direction.z = ZFloat - AllPlayers[CurrentCameraPlayer].position.z;
                normalize(&Direction);

                // Set wall hit angle
                int FinalAngleY = atan2f(Direction.x, Direction.z) * 512.0 / (M_TWOPI) + 384;
                int FinalAngleZ = atan2f(Direction1D.y, 1) * 512.0 / (M_TWOPI) + 128;
                NE_ModelSetRot(flashModels[flashIndex], 0, FinalAngleY, FinalAngleZ);

                // Make 3D sound
                int Panning, Volume;
                Vector4 SoundPos;
                SoundPos.x = XFloat;
                SoundPos.y = YFloat;
                SoundPos.z = ZFloat;
                GetPanningByPosition(&Panning, &Volume, SoundPos, xWithoutYForAudio, zWithoutYForAudio, 0.15);
                Play3DSound(SFX_RIC, Volume, Panning, NULL);
                break;
            
            case BOMBPLACE: // Bomb place update
                // Convert chars into ints
                int XInt = intParse(arr[1]);
                int YInt = intParse(arr[2]);
                int ZInt = intParse(arr[3]);
                int AngleInt = intParse(arr[4]);
                int dropped = intParse(arr[5]);

                if (dropped)
                {
                    // Change values scale and set bomb position and angle value
                    droppedBombPositionAndRotation.x = XInt / 4096.0;
                    droppedBombPositionAndRotation.y = YInt / 4096.0;
                    droppedBombPositionAndRotation.z = ZInt / 4096.0;
                    droppedBombPositionAndRotation.r = AngleInt;

                    // Set bomb 3D model position and angle value
                    NE_ModelSetCoord(Model[7], droppedBombPositionAndRotation.x, droppedBombPositionAndRotation.y, droppedBombPositionAndRotation.z);
                    // NE_ModelSetCoord(Model[10], droppedBombPositionAndRotation.x, droppedBombPositionAndRotation.y, droppedBombPositionAndRotation.z);
                    Model[7]->rz = droppedBombPositionAndRotation.r;

                    SetBombTakingZone(droppedBombPositionAndRotation.x, droppedBombPositionAndRotation.z, &bombDefuseZone); // Set zone for taking the bomb
                    bombDropped = true;
                }
                else
                {
                    // Change values scale and set bomb position and angle value
                    BombPosition.x = XInt / 4096.0;
                    BombPosition.y = YInt / 4096.0;
                    BombPosition.z = ZInt / 4096.0;
                    BombPosition.r = AngleInt;

                    // Set bomb 3D model position and angle value
                    NE_ModelSetCoord(Model[7], BombPosition.x, BombPosition.y, BombPosition.z);
                    NE_ModelSetCoord(Model[10], BombPosition.x, BombPosition.y, BombPosition.z);
                    Model[7]->rz = BombPosition.r;

                    BombSeconds = allPartyModes[currentPartyMode].bombWaitingSecondsDuration;

                    bombBipTimer = 120;
                    BombPlanted = true;
                    SetBombDefuseZone(BombPosition.x, BombPosition.z, &bombDefuseZone);
                }            
                break;
            
            case BOMBPLACING: // Bomb placing sound
                int ParsedPlayerId = intParse(arr[1]);
                int Panning, Volume;
                GetPanning(ParsedPlayerId, &Panning, &Volume, xWithoutYForAudio, zWithoutYForAudio, 0.10);

                Play3DSound(SFX_BOMBPLANTING, Volume, Panning, GetPlayer(ParsedPlayerId));
                break;
            
            case BOMBDEFUSE: // Bomb defuse sound
                BombDefused = true;
                break;

            case CURGUN: // Set current gun in inventory
                int PlayerIdInt = intParse(arr[1]);

                // Find player to set health
                for (int i = 0; i < MaxPlayer; i++)
                {
                    if (AllPlayers[i].Id != PlayerIdInt)
                        continue; // Skip player
                    
                    int newGun = intParse(arr[2]);
                    AllPlayers[i].currentGunInInventory = newGun;
                    if (i == CurrentCameraPlayer)
                        UpdateGunTexture();

                    break;
                }
                break;
            
            case LEAVE: // When a player leave
                // Get player id
                int ParsedPlayerId = intParse(arr[1]);

                // Find player
                for (int i = 0; i < MaxPlayer; i++)
                {
                    if (AllPlayers[i].Id != ParsedPlayerId)
                        continue; // Skip player
                    
                    AllPlayers[i].Id = UNUSED;
                    PlayerCount--;
                    NE_ModelDelete(AllPlayers[i].PlayerModel);
                    if (i == 0)
                    {
                        NE_PhysicsDelete(AllPlayers[i].PlayerPhysic);
                    }
                    else
                    {
                        showDisconnectedText(i);
                    }
                    AllPlayers[i].KillCount = 0;
                    AllPlayers[i].Team = SPECTATOR;
                    AllPlayers[i].DeathCount = 0;
                    if (currentMenu == SCORE_BOARD)
                        initScoreMenu();

                    if (CurrentCameraPlayer == i)
                    {
                        SetCurrentCameraPlayer(0);
                    }

                    break;
                }
                break;
            
            case VOTE:
                break;
            
            case GRENADE:
                // Convert chars into ints
                float XDirection = floatParse(arr[1]);
                float YDirection = floatParse(arr[2]);
                float ZDirection = floatParse(arr[3]);
                int XInt = intParse(arr[4]);
                int YInt = intParse(arr[5]);
                int ZInt = intParse(arr[6]);
                int GrenadeType = intParse(arr[7]);

                PhysicalGrenade *newGrenade = CreateGrenade(GrenadeType, 0);
                if (newGrenade != NULL)
                {
                    lanchGrenade(newGrenade, XDirection, YDirection, ZDirection, XInt, YInt, ZInt);
                }
                break;  
                                                                                           
            case POS: // Update player position
                int PlayerIdInt = intParse(arr[1]); // Get target player id

                float XFloat = floatParse(arr[2]);
                float YFloat = floatParse(arr[3]);
                float ZFloat = floatParse(arr[4]);
                // Change scale
                XFloat /= 4096.0;
                YFloat /= 4096.0;
                ZFloat /= 4096.0;

                int AngleInt = intParse(arr[5]);
                int CameraAngleInt = intParse(arr[6]);

                // If new position is for the local player
                if (localPlayer->Id == PlayerIdInt)
                {
                    // Set new position and teleport player
                    localPlayer->position.x = XFloat;
                    localPlayer->position.y = YFloat;
                    localPlayer->position.z = ZFloat;
                    localPlayer->Angle = AngleInt;
                    localPlayer->PlayerPhysic->xspeed = 0;
                    localPlayer->PlayerPhysic->yspeed = 0;
                    localPlayer->PlayerPhysic->zspeed = 0;
                    NE_ModelSetCoord(localPlayer->PlayerModel, localPlayer->position.x, localPlayer->position.y, localPlayer->position.z);
                    ForceUpdateLookRotation(localPlayer->cameraAngle);
                }
                else
                {
                    // Find player with unique ID to update his informations
                    for (int i = 1; i < MaxPlayer; i++)
                    {
                        if (AllPlayers[i].Id != PlayerIdInt)
                            continue; // Skip player

                        // If player has no position, teleport the player to new updated position (0 default position)
                        if (AllPlayers[i].lerpDestination.x == 0)
                        {
                            AllPlayers[i].position.x = XFloat;
                            AllPlayers[i].position.y = YFloat;
                            AllPlayers[i].position.z = ZFloat;
                            AllPlayers[i].Angle = AngleInt;
                        }

                        // Set player destination
                        AllPlayers[i].lerpDestination.x = XFloat;
                        AllPlayers[i].lerpDestination.y = YFloat;
                        AllPlayers[i].lerpDestination.z = ZFloat;

                        AllPlayers[i].AngleDestination = AngleInt;

                        // if (CurrentCameraPlayer == i)
                        // CameraAngleY = CameraAngleInt;
                        AllPlayers[i].cameraAngle = CameraAngleInt;
                        break;
                    }
                }
                break;
            
            case SETNAME: // Get a player name
                // Get player id
                int ParsedPlayerId = intParse(arr[1]);

                for (int i = 0; i < MaxPlayer; i++)
                {
                    if (AllPlayers[i].Id == ParsedPlayerId)
                    {
                        strcpy(AllPlayers[i].name, arr[2]);
                        if (i != 0 && !firstConnection)
                            showConnectedText(i);
                        break;
                    }
                }

                // Update screen if team screen is opened
                if (currentMenu == 2)
                    UpdateBottomScreenFrameCount += 8;
                break;

            case SHOOT: // Shoot from a player
                int ParsedPlayerId = intParse(arr[1]); // Get player id
                int ParsedGunId = intParse(arr[2]); // Get gun id

                // Make a sound
                int Panning, Volume;
                GetPanning(ParsedPlayerId, &Panning, &Volume, xWithoutYForAudio, zWithoutYForAudio, AllGuns[ParsedGunId].MaxSoundDistance);
                Player *player = NULL;
                int index = UNUSED;
                for (int i = 0; i < MaxPlayer; i++)
                {
                    if (AllPlayers[i].Id != ParsedPlayerId)
                        continue; // Skip player
                    
                    player = &AllPlayers[i];
                    index = i;
                    break;
            }
                if (player != NULL)
                {
                    if (player->currentGunInInventory == 1 || player->currentGunInInventory == 2)
                        player->AllAmmoMagazine[player->currentGunInInventory - 1].AmmoCount--;

                    if (CurrentCameraPlayer == index)
                    {
                        setGunRecoil(player);
                    }

                    if (ParsedGunId < GunCount)
                        Play3DSound(AllGuns[ParsedGunId].gunSound, Volume, Panning, player);
                }
                break;
            
            case HIT:
                break;
            
            case PARTY:
                break;
            
            case BUY:
                break;
            
            case TEAM: // Change team
                // Get player id
                int ParsedPlayerId = intParse(arr[1]);

                // Get Team value
                int ParsedIsCounter = intParse(arr[2]);

                // Find player and affect new value
                for (int i = 0; i < MaxPlayer; i++)
                    if (AllPlayers[i].Id == ParsedPlayerId)
                    {
                        AllPlayers[i].Team = ParsedIsCounter;
                        UpdatePlayerTexture(i);
                        break;
                    }

                // Update screen if team screen is opened
                switch (currentMenu)
                {
                    case SCORE_BOARD:
                        UpdateBottomScreenFrameCount += 8;
                        break;
                    case GAME:
                        initGameMenu();
                        break;
                }

                // TODO check this
                if (localPlayer->Id == ParsedPlayerId)
                {
                    if (roundState != TRAINING)
                        changeCameraPlayerView(false);

                    AllButtons[0].isHidden = ParsedIsCounter == -1;
                    AllButtons[1].isHidden = false;
                    AllButtons[2].isHidden = false;
                    WaitForTeamResponse = false;
                }
                break;                

            case KEY:
                SendKeyResponse = true;
                serverKey = intParse(arr[1]);
                break;
                
            case SETID: // Add player
                int ParsedId = intParse(arr[1]);

                // Spawn player
                AddNewPlayer(ParsedId, true, false);
                break;
            
            case TIMER: // Timer changes. Previously known as "TimerA"
                // Parse seconds and minutes texts to int
                PartyMinutes = intParse(arr[1]);
                PartySeconds = intParse(arr[2]);

                if (firstConnection)
                    BombSeconds = PartySeconds;
                break;

            case SETMAP: // Set the map
                int newMapId = intParse(arr[1]); // Parse map id text to int

                UnLoadMap(currentMap);
                currentMap = newMapId;
                LoadMap(currentMap);
                break;

            case SETMODE: // Set the party mode
                // Parse mode id text to int
                currentPartyMode = intParse(arr[1]);
                break;

            case VOTERESULT: // Get the current vote values
                int VoteTypeInt = intParse(arr[1]);

                if (VoteTypeInt == 0)
                {
                    playerWantToStart = intParse(arr[2]);
                    playerWantToStartLimite = intParse(arr[3]);
                }
                break;

            case CONFIRM: // Confirm shop gun buy, or set player gun from server command
                // Convert chars into ints
                int ConfirmTypeInt = intParse(arr[1]);
                int ConfirmArgumentInt = intParse(arr[2]);
                int ConfirmInventoryIndexInt = intParse(arr[3]);
                int ConfirmResultInt = intParse(arr[4]); // 0 Error, 1 Okay

                if (ConfirmTypeInt) // Set gun in inventory
                {
                    switch (ConfirmResultInt) // Not finished in source code. Ignore empty entries.
                    {
                        case 0: // Error
                            break;
                        case 1: // Okay
                            DisableAim();
                            SetGunInInventory(ConfirmArgumentInt, ConfirmInventoryIndexInt);
                            setSelectedGunInInventory(0, ConfirmInventoryIndexInt);
                            break;
                        case 2: // Not an error
                            break;
                    }
                }
                break;

            case SETMONEY: // Set player money
                // Parse money text to int
                int ParsedMoney = intParse(arr[1]);
                setPlayerMoney(0, ParsedMoney);
                break;

            case SCORE: // Party score changes
                // Parse scores texts to int
                CounterScore = intParse(arr[1]);
                TerroristsScore = intParse(arr[2]);

                // Update screen if team screen is opened
                if (currentMenu == SCORE_BOARD)
                    UpdateBottomScreenFrameCount += 8;
                break;

            case SCRBOARD: // Score board score changes
                 // Get player id
                int ParsedPlayerId = intParse(arr[1]);

                // Find player with unique ID to update his informations
                for (int i = 0; i < MaxPlayer; i++)
                    if (AllPlayers[i].Id == ParsedPlayerId)
                    {
                        AllPlayers[i].KillCount = intParse(arr[2]);
                        AllPlayers[i].DeathCount = intParse(arr[3]);
                        break;
                    }

                // Update screen if team screen is opened
                if (currentMenu == SCORE_BOARD)
                    UpdateBottomScreenFrameCount += 8;
                break;

            case SETSHOPZONE: // Update the shop zone
                shopDisableTimer = SHOP_DISABLE_TIMER;
                setShopZone(localPlayer);            
                break;
            
            case PARTYROUND: // Round state changes
                // Parse text to int
                int tempRoundState = intParse(arr[1]);

                roundState = tempRoundState;

                // Reset some values on new round
                if (roundState == WAIT_START)
                {
                    onNewRoundStart();
                    NE_SpriteVisible(TopScreenSprites[0], true);

                    StopReloading(0);
                    ResetGunsAmmo(0);
                    if (CurrentCameraPlayer != 0)
                    {
                        SetCurrentCameraPlayer(0);
                    }
                    for (int i = 0; i < MaxPlayer; i++)
                        AllPlayers[i].IsDead = FALSE;

                    deathCameraAnimation = 0;
                    deathCameraYOffset = 0;
                    redHealthTextCounter = 0;
                }
                break;

            case SETHEALTH: // Set player health
                int PlayerIdInt = intParse(arr[1]);

                // Find player to set health
                for (int i = 0; i < MaxPlayer; i++)
                {
                    if (AllPlayers[i].Id != PlayerIdInt)
                        continue: // Skip player

                    int newHealth = intParse(arr[2]);
                    setPlayerHealth(i, newHealth);
                    AllPlayers[i].armor = intParse(arr[3]);

                    int haveHeadset = intParse(arr[4]);
                    AllPlayers[i].haveHeadset = haveHeadset;
                    break;
                }
                break;
                
            case SETBOMB: // Set/remove the bomb of a player
                // Get player id
                int ParsedPlayerId = intParse(arr[1]);

                // Get gun id
                int haveBomb = intParse(arr[2]);

                for (int i = 0; i < MaxPlayer; i++)
                {
                    if (AllPlayers[i].Id == ParsedPlayerId && haveBomb == 1)
                    {
                        AllPlayers[i].haveBomb = true;
                        bombDropped = false;
                        SetGunInInventoryForNonLocalPlayer(i, 28, 8);
                    }
                    else
                    {
                        AllPlayers[i].haveBomb = false;
                        SetGunInInventoryForNonLocalPlayer(i, EMPTY, 8);
                    }
                }
                break;                

            case SETCODE: // Get the party code
                strcpy(partyCode, arr[1]);
                isPrivate = true;
                break;  
                
            case HITSOUND: // Make a hit sound
                int PlayerId = intParse(arr[2]);
                int HitType = intParse(arr[3]);

                int Panning, Volume;
                if (PlayerId != localPlayer->Id)
                    GetPanning(PlayerId, &Panning, &Volume, xWithoutYForAudio, zWithoutYForAudio, 0.10);
                else
                {
                    Panning = 128;
                    Volume = 255;

                    xOffset = (rand() % ScreenShakeAmount + ScreenShakeMinAmount) / 100.0;
                    if (rand() % 2 == 0)
                        xOffset = -xOffset;

                    yOffset = (rand() % ScreenShakeAmount + ScreenShakeMinAmount) / 100.0;
                    if (rand() % 2 == 0)
                        yOffset = -yOffset;

                    redHealthTextCounter = 62;
                }

                Player *player = NULL;
                for (int i = 0; i < MaxPlayer; i++)
                {
                    if (AllPlayers[i].Id == PlayerId)
                    {
                        player = &AllPlayers[i];
                        break;
                    }
                }

                switch (HitType)
                {
                    case 0:
                        Play3DSound(SFX_FLESH_IMPACT, Volume, Panning, player); // Check with kevlar
                        break;
                    case 1:
                        Play3DSound(SFX_HEADSHOT1, Volume, Panning, player);
                        break;
                    case 2:
                        Play3DSound(SFX_FLESH_IMPACT, Volume, Panning, player);
                        break;
                    default:
                        Play3DSound(SFX_KNIFE_HIT_PLAYER, Volume, Panning, player);
                        break;
                }
                break;  
                
            case ERROR: // Show text from server
                int errorId = intParse(arr[1]);
                char* newText;
                switch (errorId)
                {
                    case Ban:
                        newText = "You are banned!";
                        break;
                    case WrongVersion:
                        newText = "You are using an old version of the game, please update your game.";
                        break;
                    case MacAddressMissing:
                        newText = "Your Nintendo DS doesn't have a MAC address.";
                        break;
                    case WrongSecurityKey:
                        newText = "You are using a modified game.";
                        break;
                    case ServerFull:
                        newText = "The server is currently full, please try again later.";
                        break;
                    case ServerStopped:
                        newText = "The server is currently under maintenance.";
                        break;
                    case SaveCorrupted:
                        newText = "Your save is corrupted.";
                        break;
                    case IncorrectCode:
                        newText = "The code is incorrect.";
                        break;
                    case KickTeamKill:
                        newText = "You have been kicked (Team kill).";
                        break;
                    default:
                        newText = "An error has occurred!";
                        break;
                }
                strcpy(errorText, newText);
                initOnlineErrorMenu();
                break;  
                
            case TEXT: // Show text from server
                int ParsedTextId = intParse(arr[1]);
                showPartyEventText(ParsedTextId);
                break;  
                
            case TEXTPLAYER: // Show text about a player from server
                int ParsedPlayerId = intParse(arr[1]);
                int KillerIdInt = intParse(arr[2]);
                // int TextIdInt = intParse(arr[3]); unused

                int killerPlayerIndex = NO_PLAYER;
                int killedPlayerIndex = NO_PLAYER;

                if (KillerIdInt != NO_PLAYER)
                {
                    // Find player with unique ID to update his informations
                    for (int i = 0; i < MaxPlayer; i++)
                    {
                        if (AllPlayers[i].Id == KillerIdInt)
                        {
                            killerPlayerIndex = i;
                            break;
                        }
                    }
                }

                if (ParsedPlayerId != NO_PLAYER)
                {
                    for (int i = 0; i < MaxPlayer; i++)
                    {
                        if (AllPlayers[i].Id == ParsedPlayerId)
                        {
                            killedPlayerIndex = i;
                            break;
                        }
                    }
                }

                if (killerPlayerIndex != NO_PLAYER && killerPlayerIndex != NO_PLAYER)
                    showKillText(killerPlayerIndex, killedPlayerIndex);
                break;  
                
            case ADDRANGE: // Add multiples non local players
                for (int i = 1; i < SplitCount; i++) // **FIX
                {
                    int FoundId = intParse(arr[i]);
                    AddNewPlayer(FoundId, false, false);
                }
                break;  
                
            case ENDGAME: // End the party
                partyFinished = true;
                initFinalScoreMenu();
                break;  
                
            case ENDUPDATE: // End the update party sequence
                firstConnection = false;
                break;  
                
            case INVTORY: // Get the inventory of a player
                int ParsedPlayerId = intParse(arr[1]);
                int FoundPlayer = UNUSED;

                for (int i = 0; i < MaxPlayer; i++)
                {
                    if (AllPlayers[i].Id != ParsedPlayerId)
                        continue; // Skip player

                    FoundPlayer = i;
                    for (int i2 = 2; i2 < SplitCount; i2++) // **FIX
                    {
                        int FoundGunId = intParse(arr[i2]);
                        SetGunInInventoryForNonLocalPlayer(i, FoundGunId, i2 - 2);
                    }
                    break;
                }
                // If the current player camera is looked by the local player, update the gun texture
                if (FoundPlayer == CurrentCameraPlayer)
                {
                    UpdateGunTexture();
                }           
                break;

            case GETBOMB:
                break;
                
            case FRAME:
                break;
                
            case RELOADED: // Reload the gun of a player
                // Get player id
                int ParsedPlayerId = intParse(arr[1]);

                int Reset = intParse(arr[2]);
                switch (Reset)
                {
                    case 0:
                    case 1:
                        ResetGunsAmmo(ParsedPlayerId);
                        break;
                    case 2:
                        int GunId = intParse(arr[4]);
                        ResetGunAmmo(ParsedPlayerId, GunId);
                        break;
                }
                break;
                
            case STATUS:
                break;
            
            case AMMO: // Not previously added as a request type
                int ParsedPlayerId = intParse(arr[1]);
                int FoundPlayer = UNUSED;
                for (int playerIndex = 0; playerIndex < MaxPlayer; playerIndex++)
                {
                    if (AllPlayers[playerIndex].Id != ParsedPlayerId)
                        continue; // Skip player

                    FoundPlayer = playerIndex;
                    for (int j = 0; j < 2; j++)
                    {
                        int AmmoCountInt = intParse(arr[2 + j * 2]);
                        int TotalAmmoCountInt = intParse(arr[2 + j * 2 + 1]);

                        AllPlayers[FoundPlayer].AllAmmoMagazine[j].AmmoCount = AmmoCountInt;
                        AllPlayers[FoundPlayer].AllAmmoMagazine[j].TotalAmmoCount = TotalAmmoCountInt;
                    }
                    break;
                }
                break;
                            
            default:
                printf("Unknown request type: %d\n", incomingRequestType);
                break;
        }
    }
    memset(incomingData, 0, sizeof(incomingData)); // Clear the incoming data array
}
