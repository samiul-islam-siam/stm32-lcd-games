#ifndef DISPLAY_H_
#define DISPLAY_H_

#include "lcd.h"
#include <string.h>

#define DP_HEIGHT 16
#define DP_WIDTH  10

/* hlcd is defined in main.c, used everywhere */
extern Lcd_HandleTypeDef hlcd;

typedef struct {
    uint8_t grid[8];
} ZeichenFlaeche;

extern ZeichenFlaeche zeichen[2][2];

static inline void Dp_SetPixel(int x, int y, int isOn)
{
    if (isOn)
        zeichen[x/5][y/8].grid[y%8] |=  (1u << (4 - x%5));
    else
        zeichen[x/5][y/8].grid[y%8] &= ~(1u << (4 - x%5));
}

static inline int Dp_GetPixel(int x, int y)
{
    return (zeichen[x/5][y/8].grid[y%8] >> (4 - x%5)) & 1;
}

void Dp_DrawLine(int x, int y, int length, int hori, int isSet);
void Dp_Init(void);
void Dp_Draw(void);

#endif /* DISPLAY_H_ */
