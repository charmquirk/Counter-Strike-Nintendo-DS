// SPDX-License-Identifier: MIT
//
// Copyright (c) 2021-2022, Fewnity - Grégory Machefer
//
// This file is part of Counter Strike Nintendo DS Multiplayer Edition (CS:DS)

#ifndef NETWORK_H_ /* Include guard */
#define NETWORK_H_

#define PARTY_CODE_LENGTH 5
enum ErrorType
{
    Ban,
    WrongVersion,
    MacAddressMissing,
    WrongSecurityKey,
    ServerFull,
    ServerStopped,
    SaveCorrupted,
    IncorrectCode,
    KickTeamKill
};

enum JoinType
{
    JOIN_RANDOM_PARTY,
    CREATE_PRIVATE_PARTY,
    JOIN_PRIVATE_PARTY
};

enum RequestType
{
    PING,
    STPEP,
    WALLHIT,
    BOMBPLACE,
    BOMBPLACING,
    BOMBDEFUSE,
    CURGUN,
    LEAVE,
    VOTE,
    GRENADE,
    POS,
    SETNAME,
    SHOOT,
    HIT,
    PARTY,
    BUY,
    TEAM,
    KEY,
    SETID,
    TIMER,
    SETMAP,
    SETMODE,
    VOTERESULT,
    CONFIRM,
    SETMONEY,
    SCORE,
    SCRBOARD,
    SETSHOPZONE,
    PARTYROUND,
    SETHEALTH,
    SETBOMB,
    SETCODE ,
    HITSOUND,
    ERROR,
    TEXT,
    TEXTPLAYER,
    ADDRANGE,
    ENDGAME,
    ENDUPDATE,
    INVTORY,
    GETBOMB,
    FRAME,
    RELOADED,
    STATUS,
};

#define REQUEST_NAME_INDEX 0

#define DEBUG_IP_1_STRING ""
#define DEBUG_IP_2_STRING ""
#define ONLINE_SERVER_IP_STRING "cs.fewnity.com"
#define SERVER_PORT 6003
#define LOCAL_SERVER_PORT 1080 // For the android server version (but this is not used)

extern int ping;
extern char partyCode[PARTY_CODE_LENGTH];
extern bool isPrivate;

// Socket
extern int my_socket;

// Write part
extern bool SendPosition;
extern int SendPositionData;
extern bool SendShoot;
extern bool SendGrenade;
extern bool SendBombPlacing;
extern bool SendBombPlace;
extern bool SendBombDefused;
extern bool SendPing;
extern bool SendLeave;
extern bool SendTeam;
extern bool WaitForTeamResponse;
extern bool SendBuyWeapon;
extern bool SendWallHit;
extern bool SendSelectedGun;
extern bool SendPlayerName;
extern bool SendVoteStartNow;
extern bool SendGetDroppedBomb;
extern bool SendReloaded;

extern int timeOut;
extern int frameCount;

void SetPlayer();
void initNetwork(int option);
void connectToServer(char *url, bool LocalMode, int my_socket, enum JoinType option);
void ReadServerListData();
void ReadServerData();
void connectFromServerList(int unused);
Player *GetPlayer(int id);
void resetNetworkVariables();
void treatData();
void sendDataToServer();
int intParse(char *charToParse);
float floatParse(char *charToParse);

#endif // NETWORK_H_