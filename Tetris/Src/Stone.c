#include "Stone.h"
#include "stm32f4xx_hal.h"
#include <stdlib.h>

static uint32_t rng_state = 1;

static uint32_t simple_rand(void)
{
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return rng_state;
}

static uint32_t rand_range(uint32_t lo, uint32_t hi)
{
    return lo + (simple_rand() % (hi - lo));
}

void Stone_SeedRand(uint32_t seed)
{
    rng_state = seed ? seed : 1;
}

void St_SetRot(Stone *st, int8_t rot)
{
    st->rot = rot;
    switch (st->type)
    {
    case TStone:
        switch (rot) {
        case 0: st->field[0]=0x00; st->field[1]=0x04; st->field[2]=0x0E; st->field[3]=0x00; break;
        case 1: st->field[0]=0x00; st->field[1]=0x04; st->field[2]=0x06; st->field[3]=0x04; break;
        case 2: st->field[0]=0x00; st->field[1]=0x00; st->field[2]=0x0E; st->field[3]=0x04; break;
        case 3: st->field[0]=0x00; st->field[1]=0x04; st->field[2]=0x0C; st->field[3]=0x04; break;
        }
        break;
    case LStone:
        switch (rot) {
        case 0: st->field[0]=0x00; st->field[1]=0x02; st->field[2]=0x0E; st->field[3]=0x00; break;
        case 1: st->field[0]=0x00; st->field[1]=0x04; st->field[2]=0x04; st->field[3]=0x06; break;
        case 2: st->field[0]=0x00; st->field[1]=0x00; st->field[2]=0x0E; st->field[3]=0x08; break;
        case 3: st->field[0]=0x00; st->field[1]=0x0C; st->field[2]=0x04; st->field[3]=0x04; break;
        }
        break;
    case IStone:
        switch (rot) {
        case 0: case 2:
            st->field[0]=0x04; st->field[1]=0x04; st->field[2]=0x04; st->field[3]=0x04; break;
        case 1: case 3:
            st->field[0]=0x00; st->field[1]=0x00; st->field[2]=0x0F; st->field[3]=0x00; break;
        }
        break;
    case SStone:
        switch (rot) {
        case 0: case 2:
            st->field[0]=0x00; st->field[1]=0x0C; st->field[2]=0x06; st->field[3]=0x00; break;
        case 1:
            st->field[0]=0x00; st->field[1]=0x04; st->field[2]=0x0C; st->field[3]=0x08; break;
        case 3:
            st->field[0]=0x00; st->field[1]=0x02; st->field[2]=0x06; st->field[3]=0x04; break;
        }
        break;
    case OStone:
        st->field[0]=0x00; st->field[1]=0x0C; st->field[2]=0x0C; st->field[3]=0x00;
        break;
    case ZStone:
        switch (rot) {
        case 0: case 2:
            st->field[0]=0x00; st->field[1]=0x06; st->field[2]=0x0C; st->field[3]=0x00; break;
        case 1:
            st->field[0]=0x00; st->field[1]=0x08; st->field[2]=0x0C; st->field[3]=0x04; break;
        case 3:
            st->field[0]=0x00; st->field[1]=0x04; st->field[2]=0x06; st->field[3]=0x02; break;
        }
        break;
    default: break;
    }
}

int St_HitTest(Stone st)
{
    for (int8_t bx = 0; bx < 5; bx++) {
        for (int8_t by = 0; by < 4; by++) {
            if ((st.field[by] >> bx) & 1) {
                int8_t xT = st.x + bx;
                int8_t yT = st.y + by;
                if (xT < 1 || xT > TE_WIDTH || yT < 1 || yT > TE_HEIGHT)
                    return 1;
                if (Dp_GetPixel(xT, yT))
                    return 1;
            }
        }
    }
    return 0;
}

void St_Print(Stone st, int printIt)
{
    for (int8_t bx = 0; bx < 5; bx++) {
        for (int8_t by = 0; by < 4; by++) {
            if ((st.field[by] >> bx) & 1) {
                int8_t xT = st.x + bx;
                int8_t yT = st.y + by;
                if (xT < 1 || xT > TE_WIDTH || yT < 1 || yT > TE_HEIGHT)
                    continue;
                Dp_SetPixel(xT, yT, printIt);
            }
        }
    }
}

void St_Rotate(Stone *st)     { St_SetRot(st, (st->rot + 1) % 4); }
void St_BackRotate(Stone *st) { St_SetRot(st, st->rot == 0 ? 3 : st->rot - 1); }

Stone St_InitStone(void)
{
    Stone st;
    st.x   = TE_WIDTH / 2 - 1;
    st.y   = 1;
    st.rot = 0;
    st.type = (Stone_Type)(rand_range(0, 6));
    St_SetRot(&st, 0);
    return st;
}
