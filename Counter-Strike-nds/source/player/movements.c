// SPDX-License-Identifier: MIT
//
// Copyright (c) 2021-2022, Fewnity - Grégory Machefer
//
// This file is part of Counter Strike Nintendo DS Multiplayer Edition (CS:DS)

#include "movements.h"
//#include "main.h"
#include "ai.h"
#include "ui.h"
#include "party.h"
#include "input.h"
#include "sounds.h"
#include "network.h"
#include <math.h>

int fovCheckAngle = 80;

// Is the player on stairs?
bool isOnStairs;
// Is the player descending stairs?
bool isInDownStairs;
// Whether or not the player is grounded.
bool isOnFloor;
// Whether or not the player can jump. Can jump if the player is not dead, the round has started, or the player is on the ground (and not on some stairs).
bool CanJump;
// Whether or not the key or ui button for jumping is pressed.
bool WantsJump;
// Coyote Timer used for jumping. Can jump while timer is above 0.
int JumpTimer;
// CanJumpRealTimer is used wait a little bit of time before the player can jump
int CanJumpRealTimer = 2;
// Ask jump from UI button
bool NeedJump;

bool GetCanJump(){
    return CanJump = !(roundState == WAIT_START && localPlayer->IsDead) && JumpTimer > 0 && isOnFloor;
}

void SetNeedJump()
{
	if (CanMove)
		NeedJump = true;
}

/**
 * @brief Update movement. Processes every frame.
 *
 */
void UpdateMovement() {
    CanMove = !(roundState == WAIT_START && localPlayer->IsDead);
    isOnFloor = !isInDownStairs && localPlayer->PlayerPhysic->yspeed == 0;
    UpdateJump();
    UpdateWalk();
}

/**
 * @brief Player jumps... or doesn't.
 *
 */
void UpdateJump() {
    isInDownStairs = false;
    // Check if the player is on a stairs
    CheckStairs(&CanJump, &isInDownStairs);

    WantsJump = isKeyDown(JUMP_BUTTON) || NeedJump;
    if (isOnFloor) 
    {
        JumpTimer = CoyoteTime;
    }
    else if (JumpTimer > 0)
    {
        JumpTimer--;
        frameCountDuringAir++;
    }

    CanJump = CanMove && JumpTimer > 0 && isOnFloor;
    if (CanJump && WantsJump)
    {
        NeedJump = false;
        JumpTimer = 0;
        frameCountDuringAir = 0;
        localPlayer->PlayerPhysic->yspeed = JumpForce;
    }

    if ((CanJumpRealTimer == 0 || isInDownStairs) && isOnFloor && frameCountDuringAir > 20) // Make jump land sound if the player was more than 0,33 secs in the air
    {
        frameCountDuringAir = 0;
        Play2DSound(SFX_LAND, 140);
        // NeedJump = false;
    }
}


/**
 * @brief Player walks according to their speed and direction.
 *
 */
void UpdateWalk()
{
    // Reset player speed
    localPlayer->PlayerPhysic->xspeed = 0;
    localPlayer->PlayerPhysic->zspeed = 0;
    movementVector = {0, 0};

    if (inputVector.x == 0 && inputVector.y == 0)
        return;

    // Player movements
    gunWalkSpeed = defaultWalkSpeed;
    if (getPlayerCurrentGunIndex(&localPlayer) < GunCount)
        gunWalkSpeed = getPlayerCurrentGun(&localPlayer).WalkSpeed;

    // Get player's rotation angle in radians
    float angleRad = localPlayer->Angle / 512.0 * M_TWOPI;
    float cosAngle = cosf(angleRad);
    float sinAngle = sinf(angleRad);

    // Rotate input vector according to player's rotation
    float rotatedX = inputVector.x * cosAngle - inputVector.y * sinAngle;
    float rotatedY = inputVector.x * sinAngle + inputVector.y * cosAngle;
    movementVector = (rotatedX * gunWalkSpeed, rotatedY * gunWalkSpeed);

    // Apply speed and set movement
    localPlayer->PlayerPhysic->xspeed = movementVector.x;
    localPlayer->PlayerPhysic->zspeed = movementVector.y;

    // Gun headbobing
    if (isOnFloor)
        ApplyGunWalkAnimation(0);
}

/**
 * @brief Add angle to local player
 *
 * @param xAngleToAdd X angle to add
 * @param yAngleToAdd Y angle to add
 */
void AddAnglesToPlayer(float xAngleToAdd, float yAngleToAdd)
{
    float AngleSpeed = 1;

    // If the player is scoping, the angle speed is lower
    if (GetCurrentScopeLevel() == 1)
        AngleSpeed = 0.5;
    else if (GetCurrentScopeLevel() == 2)
        AngleSpeed = 0.25;

    // Muliply with the sensitivity for the angle speed
    AngleSpeed *= sensitivity;

    // Add angle to the player
    if (xAngleToAdd != 0)
    {
        localPlayer->Angle += AngleSpeed * xAngleToAdd;
        SetNeedUpdateViewRotation(true);
        SetSendPosition(true);
    }

    // Add angle to the camera
    if (yAngleToAdd != 0)
    {
        float NewyAngle = GetCameraAngleY() + AngleSpeed * yAngleToAdd;
        if (NewyAngle > 10 && NewyAngle < 246)
        {
            SetCameraAngleY(NewyAngle);
            SetNeedUpdateViewRotation(true);
            SetSendPosition(true);
        }
    }
}

/**
 * @brief Rotate player with physicals key
 *
 * @param NeedUpdateViewRotation
 * @param SendPosition
 * @param CameraAngleY
 * @param CurrentScopeLevel
 */
void RotatePlayer(bool *NeedUpdateViewRotation, bool *SendPosition, float *CameraAngleY)
{
    float AngleSpeed;

    // If the player is scoping, the angle speed is lower
    switch (GetCurrentScopeLevel())
    {
        case 1:
            AngleSpeed *= 0.5;
        case 2:
            AngleSpeed *= 0.25;
        default:
            AngleSpeed = 1 * buttonsSensitivity;
    }
        

    // Change player rotation
    int playerRotationDirection = isKey(LOOK_LEFT_BUTTON) - isKey(LOOK_RIGHT_BUTTON);
    if (playerRotationDirection != 0)
    {
        localPlayer->Angle += AngleSpeed * playerRotationDirection;
        *NeedUpdateViewRotation = true;
        *SendPosition = true;
    }


    // Change camera rotation
    int cameraRotationDirection = isKey(LOOK_DOWN_BUTTON) - isKey(LOOK_UP_BUTTON);
    int nextCameraAngleY;
    if (cameraRotationDirection != 0)
    {
        nextCameraAngleY += *CameraAngleY + (AngleSpeed * cameraRotationDirection);
        if (nextCameraAngleY > 9 && nextCameraAngleY < 245)
        {
            *CameraAngleY = nextCameraAngleY;
            *NeedUpdateViewRotation = true;
        }
        
    }
}

int updateRate = 0;

/**
 * @brief Force to update the view rotation
 *
 * @param CameraAngleY Camera angle
 */
void ForceUpdateLookRotation(float CameraAngleY)
{
    updateRate = 0;
    UpdateLookRotation(CameraAngleY);
}

void stepSound(int playerIndex)
{
    Player *player = &AllPlayers[playerIndex];

    if (playerIndex == 0) // do a 2d sound
    {
        DoStepSound(110, 128, 0);
    }
    else // Do a 3d sound
    {
        int Panning, Volume;
        GetPanning(player->Id, &Panning, &Volume, xWithoutYForAudio, zWithoutYForAudio, 0.11);
        DoStepSound(Volume, Panning, playerIndex);
    }
}

/**
 * @brief Apply animation when the player is moving
 *
 * @param playerIndex Player index
 */
void ApplyGunWalkAnimation(int playerIndex)
{
    Player *player = &AllPlayers[playerIndex];

    player->BobbingOffset += BobbingSpeed;

    // Do sound every half bobbing cycle
    if (!player->HasBobbed && player->BobbingOffset >= M_PI)
    {
        stepSound(playerIndex);

        player->HasBobbed = true;
    }
    else if (player->BobbingOffset >= M_TWOPI)
    {
        player->BobbingOffset = 0;
        stepSound(playerIndex);

        player->HasBobbed = false;
    }
}

/**
 * @brief Update look values for local player
 *
 * @param CameraAngleY Camera angle
 */
void UpdateLookRotation(float CameraAngleY)
{
    // Math formula to get a point position on sphere from the middle of the sphere with 2 angle
    float TempS = AllPlayers[GetCurrentCameraPlayer()].Angle / 512.0 * M_TWOPI;
    float TempT = (384 - CameraAngleY) / 512.0 * M_TWOPI;

    double cosTempT = cos(TempT);
    double cosTempS = cos(TempS);
    double SinTempS = sin(TempS);

    x = SinTempS * cosTempT;
    y = -sin(TempT);
    z = cosTempS * cosTempT;

    xWithoutY = -SinTempS;
    zWithoutY = -cosTempS;

    // Values for map
    if (isShowingMap)
    {
        float TempSForMap = (AllPlayers[GetCurrentCameraPlayer()].Angle) / 512.0 * M_TWOPI;
        xWithoutYForMap = -sin(TempSForMap);
        zWithoutYForMap = -cos(TempSForMap);
    }

    // Values for audio and occlusions
    if (updateRate == 0)
    {
        float TempSside1 = (AllPlayers[GetCurrentCameraPlayer()].Angle - 80) / 512.0 * M_TWOPI;
        float TempSside2 = (AllPlayers[GetCurrentCameraPlayer()].Angle + 80) / 512.0 * M_TWOPI;

        float TempSForAudio = (AllPlayers[GetCurrentCameraPlayer()].Angle - 128) / 512.0 * M_TWOPI;

        xWithoutYForAudio = -sin(TempSForAudio);
        zWithoutYForAudio = -cos(TempSForAudio);

        xWithoutYForOcclusionSide1 = -sin(TempSside1);
        zWithoutYForOcclusionSide1 = -cos(TempSside1);
        xWithoutYForOcclusionSide2 = -sin(TempSside2);
        zWithoutYForOcclusionSide2 = -cos(TempSside2);

        updateRate = 2;
    }
    updateRate--;
}

/**
 * @brief  Update look values for non local player
 *
 * @param CameraAngleY Camera angle
 * @param angle Angle of the player
 * @param x out x value
 * @param y out y value
 * @param z out z value
 */
void UpdateLookRotationAI(float CameraAngleY, float angle, float *x, float *y, float *z)
{
    // Math formula to get a point position on sphere from the middle of the sphere with 2 angle
    float TempS = angle / 512.0 * M_TWOPI;
    float TempT = (384 - CameraAngleY) / 512.0 * M_TWOPI;

    double cosTempT = cos(TempT);
    double cosTempS = cos(TempS);
    double SinTempS = sin(TempS);

    *x = SinTempS * cosTempT;
    *y = -sin(TempT);
    *z = cosTempS * cosTempT;
}

/**
 * @brief Update look values FOR culling for non local player
 *
 * @param playerIndex Player index
 * @param angle Angle of the player
 * @param xSide1 out x value for side 1
 * @param zSide1 out z value for side 1
 * @param xSide2 out x value for side 2
 * @param zSide2 out z value for side 2
 */
void GetRotationForCullingAI(int playerIndex, float angle, float *xSide1, float *zSide1, float *xSide2, float *zSide2)
{
    float TempSside1 = (AllPlayers[playerIndex].Angle - 100) / 512.0 * M_TWOPI;
    float TempSside2 = (AllPlayers[playerIndex].Angle + 100) / 512.0 * M_TWOPI;

    *xSide1 = -sin(TempSside1);
    *zSide1 = -cos(TempSside1);

    *xSide2 = -sin(TempSside2);
    *zSide2 = -cos(TempSside2);
}

float Speed = 0.05;
/**
 * @brief Normalize vector3
 *
 * @param p Vector3
 */
void normalize(Vector3 *p)
{
    float w = sqrtf(p->x * p->x + p->y * p->y + p->z * p->z);
    p->x /= w;
    p->y /= w;
    p->z /= w;
}

/**
 * @brief Normalize vector2 2D
 *
 * @param p Vector2
 */
void normalize2D(Vector2 *p)
{
    float w = sqrtf(p->x * p->x + p->y * p->y);
    p->x /= w;
    p->y /= w;
}

/**
 * @brief Normalize vector2 1D
 *
 * @param p Vector2
 */
void normalize1D(Vector2 *p)
{
    float w = sqrtf(p->x * p->x);
    p->x /= w;
}

/**
 * @brief Set non local player position
 *
 */
void SetOnlinelPlayersPositions()
{
    // Loop using "AllPlayers" array for updating non local player position smoothly
    for (int i = 1; i < MaxPlayer; i++)
    {
        Player *player = &AllPlayers[i];

        if (player->Id == UNUSED || player->IsDead)
            continue;

        // If a player is spawned
        if (!player->isAi)
        {
            float Speed = 0.1;
            float Dis = fabs(player->position.x - player->lerpDestination.x) + fabs(player->position.y - player->lerpDestination.y) + fabs(player->position.z - player->lerpDestination.z);

            if (Dis < 4.0)
            {
                // Smoothing player position
                if (player->position.x != player->lerpDestination.x)                                                    // If player x position is not the same as x destination
                    player->position.x = player->position.x + Speed * (player->lerpDestination.x - player->position.x); // Move player with lerp system

                if (player->position.y != player->lerpDestination.y)                                                    // If player y position is not the same as y destination
                    player->position.y = player->position.y + Speed * (player->lerpDestination.y - player->position.y); // Move player with lerp system

                if (player->position.z != player->lerpDestination.z)                                                    // If player y position is not the same as z destination
                    player->position.z = player->position.z + Speed * (player->lerpDestination.z - player->position.z); // Move player with lerp system

                if (abs(player->AngleDestination - player->Angle) > 90)
                {
                    player->Angle = player->AngleDestination;
                }
                else
                {
                    // Smoothing player angle
                    float AngleAddAmout = Speed * (player->AngleDestination - player->Angle);
                    player->Angle = player->Angle + AngleAddAmout;
                }

                if (Dis > 0.05)
                    ApplyGunWalkAnimation(i);
            }
            else
            {
                player->position.x = player->lerpDestination.x;
                player->position.y = player->lerpDestination.y;
                player->position.z = player->lerpDestination.z;

                player->Angle = player->AngleDestination;
            }

            // Set new player position and angle
            NE_ModelSetCoord(player->PlayerModel, player->position.x, player->position.y, player->position.z);
            NE_ModelSetCoord(player->PlayerShadow, player->position.x, player->position.y - 0.845, player->position.z);
            player->PlayerModel->ry = player->Angle;
        }
        else if (player->PathCount != 0 && (player->target == NO_PLAYER || player->tooFar) && roundState != WAIT_START)
        {
            float Speed = 0.087;
            int CurPath = player->CurrentPath;
            int Ok = 0;

            Vector3 Direction;
            Direction.x = GetWaypoints()[player->Path[CurPath]].x - player->position.x;
            Direction.y = GetWaypoints()[player->Path[CurPath]].y - 0.1 - player->position.y;
            Direction.z = GetWaypoints()[player->Path[CurPath]].z - player->position.z;
            Vector2 Direction2D;
            Direction2D.x = GetWaypoints()[player->Path[CurPath]].x - GetWaypoints()[player->Path[CurPath - 1]].x;
            Direction2D.y = GetWaypoints()[player->Path[CurPath]].z - GetWaypoints()[player->Path[CurPath - 1]].z;
            normalize(&Direction);
            normalize2D(&Direction2D);

            // Smoothing player position
            if (player->position.x - 0.1 > GetWaypoints()[player->Path[CurPath]].x || player->position.x + 0.1 < GetWaypoints()[player->Path[CurPath]].x)
                player->position.x += Direction.x * Speed;
            else
                Ok++;

            // TODO remplace -0.1 by a player offset vector 3
            if (player->position.y - 0.1 > GetWaypoints()[player->Path[CurPath]].y - 0.1 || player->position.y + 0.1 < GetWaypoints()[player->Path[CurPath]].y - 0.1)
                player->position.y += Direction.y * Speed;

            if (player->position.z - 0.1 > GetWaypoints()[player->Path[CurPath]].z || player->position.z + 0.1 < GetWaypoints()[player->Path[CurPath]].z)
                player->position.z += Direction.z * Speed;
            else
                Ok++;

            if (Ok == 2) // if the player reach the waypoint
            {
                if (CurPath < player->PathCount - 1)
                {
                    // Go to next waypoint
                    player->CurrentPath++;
                }
                else
                {
                    // if the player is at the end of the path
                    if (applyRules)
                    {
                        player->LastWayPoint = player->Path[CurPath];
                        // Plant/defuse the bomb if needed
                        if (player->haveBomb || (player->Team == COUNTERTERRORISTS && BombPlanted && !BombDefused && bombPlantedAt == player->LastWayPoint))
                        {
                            if (player->LastWayPoint == 14 || player->LastWayPoint == 29) // TOTO Replace 14 and 29 by bomb point define
                            {
                                player->isPlantingBomb = true;
                                if (!BombDefused && BombPlanted) // Set timer
                                    player->bombTimer = bombDefuseTime;
                                else if (!BombPlanted) // Set timer
                                    player->bombTimer = bombPlantingTime;
                            }
                        }
                    }
                    player->PathCount = 0;
                }
            }
            else
            {
                // Move player
                ApplyGunWalkAnimation(i);
                NE_ModelSetCoord(player->PlayerModel, player->position.x, player->position.y, player->position.z);
                NE_ModelSetCoord(player->PlayerShadow, player->position.x, player->position.y - 0.845, player->position.z);

                float FinalAngle = atan2f(Direction2D.x, Direction2D.y) * 512.0 / (M_TWOPI) + 256.0;
                player->Angle = FinalAngle;
                player->PlayerModel->ry = player->Angle;
            }
        }
        else //?
        {
            Vector3 positionToGo;

            if (player->tooFar)
            {
                Player *targetPlayer = &AllPlayers[player->target];
                positionToGo.x = targetPlayer->position.x;
                positionToGo.y = targetPlayer->position.y;
                positionToGo.z = targetPlayer->position.z;
            }
            else if (player->searchForDroppedBomb)
            {
                positionToGo.x = droppedBombPositionAndRotation.x;
                positionToGo.y = droppedBombPositionAndRotation.y + 0.845;
                positionToGo.z = droppedBombPositionAndRotation.z;
            }
            else
            {
                player->Angle = player->AngleDestination;
                player->PlayerModel->ry = player->AngleDestination;
                continue;
            }

            float Speed = 0.087;
            int Ok = 0;

            Vector3 Direction;
            Direction.x = positionToGo.x - player->position.x;
            Direction.y = positionToGo.y - player->position.y;
            Direction.z = positionToGo.z - player->position.z;

            normalize(&Direction);

            // Smoothing player position
            if (player->position.x - 0.7 > positionToGo.x || player->position.x + 0.7 < positionToGo.x)
                player->position.x += Direction.x * Speed;
            else
                Ok++;

            if (player->position.y - 0.1 > positionToGo.y || player->position.y + 0.1 < positionToGo.y)
                player->position.y += Direction.y * Speed;

            if (player->position.z - 0.7 > positionToGo.z || player->position.z + 0.7 < positionToGo.z)
                player->position.z += Direction.z * Speed;
            else
                Ok++;

            NE_ModelSetCoord(player->PlayerModel, player->position.x, player->position.y, player->position.z);
            NE_ModelSetCoord(player->PlayerShadow, player->position.x, player->position.y - 0.845, player->position.z);

            player->Angle = player->AngleDestination;
            player->PlayerModel->ry = player->AngleDestination;
        }
    }
}