/* Same legacy grouping/order; only enumerate visible candidates. */
static void drawWindowFast(HDC hdc, RECT* windowRect) {
    RECT* rect1;
    RECT* ptVar3;
    uint32_t uVar4;
    Actor* pAVar6;
    Actor* pAVar7;

    
    ski_assert(windowRect, 1273);

    for (pAVar7 = actorListPtr; pAVar7 != NULL; pAVar7 = pAVar7->next) {
        
        if ((pAVar7->flags & (FLAG_1 | FLAG_2 | FLAG_8)) == 0) {
            pAVar6 = pAVar7->linkedActor;
            if (pAVar6 != NULL && (pAVar6->flags & FLAG_1) != 0 && (pAVar6->flags & FLAG_2) != 0 && pAVar7->spriteIdx2 == pAVar6->spriteIdx2) {
                if ((pAVar6->flags & FLAG_4) != 0) {
                    ptVar3 = &pAVar6->someRect;
                } else {
                    ptVar3 = updateActorSpriteRect(pAVar6);
                }
                if ((pAVar7->flags & FLAG_4) != 0) {
                    rect1 = &pAVar7->someRect;
                } else {
                    rect1 = updateActorSpriteRect(pAVar7);
                }
                if (areRectanglesEqual(rect1, ptVar3)) {
                    pAVar7->flags |= FLAG_1;
                    pAVar6->flags = pAVar6->flags &= 0xfffffffe;
                    actorSetFlag8IfFlag1IsUnset(pAVar6);
                }
            }
        }
    }
    for (pAVar7 = actorListPtr; pAVar7 != NULL; pAVar7 = pAVar7->next) {
        if ((pAVar7->flags & FLAG_8) != 0) {
            pAVar7->flags &= 0xffffffef;
        } else {
            if ((pAVar7->flags & FLAG_4) != 0) {
                ptVar3 = &pAVar7->someRect;
            } else {
                ptVar3 = updateActorSpriteRect(pAVar7);
            }
            uVar4 = doRectsOverlap(ptVar3, windowRect);
            
            pAVar7->flags = (pAVar7->flags & 0xffffffef) | ((uVar4 & 1) << 4);
            if ((uVar4 & 1) != 0) {
                (pAVar7->rect).left = ptVar3->left;
                (pAVar7->rect).top = ptVar3->top;
                (pAVar7->rect).right = ptVar3->right;
                (pAVar7->rect).bottom = ptVar3->bottom;
                pAVar7->actorPtr = NULL;
            }
        }
    }

    {Actor* visible[100];int count=0,vi,vj;
    for(pAVar7=actorListPtr;pAVar7;pAVar7=pAVar7->next)if(pAVar7->flags&FLAG_10)visible[count++]=pAVar7;
    for(vi=0;vi<count;vi++){pAVar7=visible[vi];if(pAVar7->flags&FLAG_10){
        pAVar6=pAVar7->linkedActor;
        if(pAVar6&&(pAVar6->flags&FLAG_10)&&doRectsOverlap(&pAVar7->rect,&pAVar6->rect))actorClearFlag10(pAVar7,pAVar6);
        for(vj=0;vj<vi;vj++){pAVar6=visible[vj];if((pAVar6->flags&FLAG_10)&&doRectsOverlap(&pAVar7->rect,&pAVar6->rect)){
            actorClearFlag10(pAVar7,pAVar6);
            /* Match the legacy loop's restart at actorListPtr->next. */
            vj=visible[0]==actorListPtr?0:-1;
        }}
    }}
    }

    for (pAVar7 = actorListPtr; pAVar7 != NULL; pAVar7 = pAVar7->next) {
        if ((pAVar7->flags & FLAG_10) != 0) {
            drawActor(hdc, pAVar7);
        }
    }

    for (pAVar7 = actorListPtr; pAVar7 != NULL; pAVar7 = pAVar7->next) {
        if ((pAVar7->flags & FLAG_2) != 0) {
            actorSetFlag8IfFlag1IsUnset(pAVar7);
        }
    }
    removeFlag8ActorsFromList();
}