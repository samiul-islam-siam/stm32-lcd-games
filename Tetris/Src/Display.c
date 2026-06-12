#include "Display.h"

ZeichenFlaeche zeichen[2][2];

void Dp_Init(void)
{
    memset(zeichen, 0, sizeof(zeichen));
    Dp_DrawLine(0,  0, 10, 1, 1);   /* top    */
    Dp_DrawLine(0, 15, 10, 1, 1);   /* bottom */
    Dp_DrawLine(0,  0, 16, 0, 1);   /* left   */
    Dp_DrawLine(9,  0, 16, 0, 1);   /* right  */
}

void Dp_Draw(void)
{
    /*
     * Custom char slot mapping:
     *   slot 0 → zeichen[0][0]  col 0, upper half (y 0..7)
     *   slot 1 → zeichen[0][1]  col 0, lower half (y 8..15)
     *   slot 2 → zeichen[1][0]  col 1, upper half
     *   slot 3 → zeichen[1][1]  col 1, lower half
     *
     * Lcd_define_char saves to CGRAM.
     * Lcd_cursor(row, col) → row 0 = top LCD row, row 1 = bottom LCD row.
     * Lcd_write_char outputs the char code at current cursor position.
     */
    Lcd_define_char(&hlcd, 0, zeichen[0][0].grid);
    Lcd_define_char(&hlcd, 1, zeichen[0][1].grid);
    Lcd_define_char(&hlcd, 2, zeichen[1][0].grid);
    Lcd_define_char(&hlcd, 3, zeichen[1][1].grid);

    Lcd_cursor(&hlcd, 0, 0); Lcd_write_char(&hlcd, 0);  /* top-left char    */
    Lcd_cursor(&hlcd, 1, 0); Lcd_write_char(&hlcd, 1);  /* bottom-left char */
    Lcd_cursor(&hlcd, 0, 1); Lcd_write_char(&hlcd, 2);  /* top-right char   */
    Lcd_cursor(&hlcd, 1, 1); Lcd_write_char(&hlcd, 3);  /* bottom-right char*/
}

void Dp_DrawLine(int x, int y, int length, int hori, int isSet)
{
    for (int offset = 0; offset < length; offset++) {
        if (hori) Dp_SetPixel(x + offset, y, isSet);
        else      Dp_SetPixel(x, y + offset, isSet);
    }
}
