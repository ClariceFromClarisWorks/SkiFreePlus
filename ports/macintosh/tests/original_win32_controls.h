/* Direct Win32 decompilation reference, commit 7b473c51f79671c7fc54c7b14eb4e118687b2048. */
int __fastcall original_getSkierGroundSpriteFromMousePosition(short param_1,short param_2) {
    short uVar1;

    if (param_2 > 0) {
        if (param_1 == 0) {
            return 0;
        }
        uVar1 = (((int)param_2 << 2) / (int)param_1);
        if (uVar1 <= -0xc) {
            return 0;
        }
        if (uVar1 <= -6) {
            return 1;
        }
        if (uVar1 <= -3) {
            return 2;
        }
        if (uVar1 <= -1) {
            return 3;
        }
        if (uVar1 >= 0xc) {
            return 0;
        }
        if (uVar1 >= 6) {
            return 4;
        }
        if (uVar1 >= 3) {
            return 5;
        }
        if (uVar1 >= 1) {
            return 6;
        }
    }
    return param_1 < 0 ? 3 : 6;
}
int __fastcall original_getSkierInAirSpriteFromMousePosition(short param_1,short param_2) {
    if (param_1 < 0) {
        if (param_2 < 0) {
            return (param_2 < param_1) ? 16 : 14;
        }
        return (-param_2 < param_1) ? 13 : 14;
    }
    if (param_2 < 0) {
        return (-param_2 <= param_1) ? 15 : 16;
    }
    return (param_2 <= param_1) + 13;
}
void original_handleMouseClick() {
    UINT ActorframeNo;

    if (!playerActor) {
        handleGameReset();
        return;
    }
    ActorframeNo = playerActor->frameNo;
    if (ActorframeNo != 11) {
        if (playerActor->isInAir == 0) {
            playerActor->inAirCounter = 4;
            ActorframeNo = 0xd;
        }
        else if (ActorframeNo != 17) {
            switch(ActorframeNo) {
                case 0xd:
                    ActorframeNo = 0x12;
                    break;
                case 0x12:
                    ActorframeNo = 0x13;
                    break;
                case 0x13:
                    ActorframeNo = 0xd;
                    break;
                case 0xe:
                    ActorframeNo = 0x14;
                    break;
                case 0xf:
                    ActorframeNo = 0x15;
                    break;
            }
        }
    }
    if ((ActorframeNo != playerActor->frameNo) &&
        (setActorFrameNo(playerActor,ActorframeNo), redrawRequired != 0)) {
        drawWindow(mainWindowDC,&windowClientRect);
        redrawRequired = FALSE;
    }
}
void __fastcall original_handleKeydownMessage(UINT charCode) {
    short sVar1;
    UINT ActorframeNo;

    switch(charCode) {
        case VK_ESCAPE:
            ShowWindow(hSkiMainWnd,6);
            return;
        case VK_F3:
            togglePausedState(); 
            return;
        case VK_RETURN:
            if (playerActor != (Actor *)0x0) {
                return;
            }
            handleGameReset(); 
            return;
        case VK_F2:
            handleGameReset(); 
            return;
        default: break;
    }

    if (playerActor == (Actor *)0x0) {
        return;
    }
    ActorframeNo = playerActor->frameNo;
    sVar1 = playerActor->isInAir;
    if ((ActorframeNo != 0xb) && (ActorframeNo != 0x11)) {
        switch (charCode) {
            case 0x25:
            case 100:
                
                ski_assert(ActorframeNo < 0x16, 0xf63);

                ActorframeNo = playerTurnFrameNoTbl[ActorframeNo].leftFrameNo;
                if (ActorframeNo == 7) {





                    playerActor->HorizontalVelMaybe = max(playerActor->HorizontalVelMaybe - 8, -8);
                }
                break;
            case 0x27:
            case 0x66:
                
                ski_assert(ActorframeNo < 0x16, 3947);

                ActorframeNo = playerTurnFrameNoTbl[ActorframeNo].rightFrameNo;
                if (ActorframeNo == 8) {






                    playerActor->HorizontalVelMaybe = min(playerActor->HorizontalVelMaybe + 8, 8);
                }
                break;

            case 0x28:
            case 0x62:
                
                if (sVar1 == 0) {
                    ActorframeNo = 0;
                    break;
                }
                switch (ActorframeNo) {
                    case 0xd:
                        ActorframeNo = 0x13;
                        break;
                    case 0x14:
                        ActorframeNo = 0xe;
                        break;
                    case 0x15:
                        ActorframeNo = 0xf;
                        break;
                    case 0x12:
                        ActorframeNo = 0xd;
                        break;
                    case 0x13:
                        ActorframeNo = 0x12;
                        break;
                }
                break;

            case 0x26:
            case 0x68:
                
                switch (ActorframeNo) {
                    case 0xd:

                        ActorframeNo = 0x12;
                        break;
                    case 0x13:

                        ActorframeNo = 0xd;
                        break;
                    case 0xe:
                        ActorframeNo = 0x14;
                        break;
                    case 0xf:
                        ActorframeNo = 0x15;
                        break;
                    case 3:
                    case 7:
                    case 0xc:
                        if (playerActor->verticalVelocityMaybe == 0) {
                            ActorframeNo = 9;
                            playerActor->verticalVelocityMaybe = -4;
                        }
                        break;
                    case 6:
                    case 8:
                        if (playerActor->verticalVelocityMaybe == 0) {
                            ActorframeNo = 10;
                            playerActor->verticalVelocityMaybe = -4;
                        }
                        break;
                    case 0x12:

                        ActorframeNo = 0x13;
                        break;

                }
                break;

            case 0x24:
            case 0x67:
                
                if (sVar1 == 0) {
                    ActorframeNo = 3;
                }
                break;

            case 0x21:
            case 0x69:
                
                if (sVar1 == 0) {
                    ActorframeNo = 6;
                }
                break;

            case 0x23:
            case 0x61:
                
                if (sVar1 == 0) {
                    ActorframeNo = 1;
                }
                break;

            case 0x22:
            case 99:
                
                if (sVar1 == 0) {
                    ActorframeNo = 4;
                }
                break;

            case 0x2d:
            case 0x60:
                
                if (sVar1 == 0) {
                    playerActor->inAirCounter = 2;
                    ActorframeNo = 0xd;
                    if (4 < playerActor->verticalVelocityMaybe) {
                        playerActor->verticalVelocityMaybe = playerActor->verticalVelocityMaybe + -4;
                    }
                }
        }
    }

    if ((ActorframeNo != playerActor->frameNo) &&
        (setActorFrameNo(playerActor,ActorframeNo), redrawRequired != 0)) {
        drawWindow(mainWindowDC,&windowClientRect);
        redrawRequired = 0;
    }
}