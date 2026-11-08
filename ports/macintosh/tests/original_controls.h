/* Source handlers extracted without algorithm changes from preserved SDL baseline. */
int original_getSkierGroundSpriteFromMousePosition(short param_1, short param_2) {
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
int original_getSkierInAirSpriteFromMousePosition(short param_1, short param_2) {
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
    uint32_t ActorframeNo;

    if (!playerActor) {
        handleGameReset();
        return;
    }
    ActorframeNo = playerActor->frameNo;
    if (ActorframeNo != 11) {
        if (playerActor->isInAir == 0) {
            playerActor->inAirCounter = 4;
            ActorframeNo = 0xd;
        } else if (ActorframeNo != 17) {
            switch (ActorframeNo) {
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
    if ((ActorframeNo != playerActor->frameNo) && (setActorFrameNo(playerActor, ActorframeNo), redrawRequired != 0)) {
        drawWindow(mainWindowDC, &windowClientRect);
        redrawRequired = FALSE;
    }
}
void original_handleKeydownMessage(SDL_Event* e) {
    short sVar1;
    uint32_t actorframeNo;

    switch (e->key.keysym.sym) {
    case SDLK_ESCAPE:
        
        SDL_MinimizeWindow(hSkiMainWnd);
        return;
    case SDLK_F3:
        togglePausedState(); 
        return;
    case SDLK_RETURN:
        if (playerActor != (Actor*)0x0) {
            return;
        }
        handleGameReset(); 
        return;
    case SDLK_F2:
        handleGameReset(); 
        return;
    default:
        break;
    }

    if (playerActor == NULL) {
        return;
    }
    actorframeNo = playerActor->frameNo;
    sVar1 = playerActor->isInAir;
    if ((actorframeNo != 0xb) && (actorframeNo != 0x11)) {
        switch (e->key.keysym.sym) {
        case SDLK_LEFT:
            
            ski_assert(actorframeNo < 0x16, 0xf63);

            actorframeNo = playerTurnFrameNoTbl[actorframeNo].leftFrameNo;
            if (actorframeNo == 7) {
                
                
                
                
                
                playerActor->HorizontalVelMaybe = max_(playerActor->HorizontalVelMaybe - 8, -8);
            }
            break;

        case SDLK_RIGHT:
            
            ski_assert(actorframeNo < 0x16, 3947);

            actorframeNo = playerTurnFrameNoTbl[actorframeNo].rightFrameNo;
            if (actorframeNo == 8) {
                
                
                
                
                

                playerActor->HorizontalVelMaybe = min_(playerActor->HorizontalVelMaybe + 8, 8);
            }
            break;

        case SDLK_DOWN:
            if (sVar1 == 0) {
                actorframeNo = 0;
                break;
            }
            switch (actorframeNo) {
            case 0xd:
                actorframeNo = 0x13;
                break;
            case 0x14:
                actorframeNo = 0xe;
                break;
            case 0x15:
                actorframeNo = 0xf;
                break;
            case 0x12:
                actorframeNo = 0xd;
                break;
            case 0x13:
                actorframeNo = 0x12;
                break;
            }
            break;

        case SDLK_UP:
            switch (actorframeNo) {
            case 0xd:
                
                actorframeNo = 0x12;
                break;
            case 0x13:
                
                actorframeNo = 0xd;
                break;
            case 0xe:
                actorframeNo = 0x14;
                break;
            case 0xf:
                actorframeNo = 0x15;
                break;
            case 3:
            case 7:
            case 0xc:
                if (playerActor->verticalVelocityMaybe == 0) {
                    actorframeNo = 9;
                    playerActor->verticalVelocityMaybe = -4;
                }
                break;
            case 6:
            case 8:
                if (playerActor->verticalVelocityMaybe == 0) {
                    actorframeNo = 10;
                    playerActor->verticalVelocityMaybe = -4;
                }
                break;
            case 0x12:
                
                actorframeNo = 0x13;
                break;
            }
            break;

        case SDLK_KP_7:
            if (sVar1 == 0) {
                actorframeNo = 3;
            }
            break;

        case 0x21:
        case 0x69:
        case SDLK_KP_9:
            
            if (sVar1 == 0) {
                actorframeNo = 6;
            }
            break;

        case 0x23:
        case 0x61:
            
            if (sVar1 == 0) {
                actorframeNo = 1;
            }
            break;

        case 0x22:
        case 99:
            
            if (sVar1 == 0) {
                actorframeNo = 4;
            }
            break;

        case 0x2d:
        case 0x60:
            
            if (sVar1 == 0) {
                playerActor->inAirCounter = 2;
                actorframeNo = 0xd;
                if (4 < playerActor->verticalVelocityMaybe) {
                    playerActor->verticalVelocityMaybe = playerActor->verticalVelocityMaybe + -4;
                }
            }
        }
    }

    if ((actorframeNo != playerActor->frameNo) && (setActorFrameNo(playerActor, actorframeNo), redrawRequired != 0)) {
        drawWindow(mainWindowDC, &windowClientRect);
        redrawRequired = 0;
    }
}