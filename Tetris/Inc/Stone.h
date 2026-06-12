#ifndef STONE_H_
#define STONE_H_

#include "Display.h"
#include <stdint.h>

#define TE_WIDTH  (DP_WIDTH  - 2)
#define TE_HEIGHT (DP_HEIGHT - 2)

typedef enum {
    TStone = 0,
    LStone = 1,
    IStone = 2,
    SStone = 3,
    OStone = 4,
    ZStone = 5,
} Stone_Type;

typedef struct {
    uint8_t field[4];
    int8_t  x;
    int8_t  y;
    int8_t  rot;
    Stone_Type type;
} Stone;

void  Stone_SeedRand(uint32_t seed);
Stone St_InitStone(void);
int   St_HitTest(Stone st);
void  St_Print(Stone st, int printIt);
void  St_Rotate(Stone *st);
void  St_BackRotate(Stone *st);
void  St_SetRot(Stone *st, int8_t rot);

#endif /* STONE_H_ */
