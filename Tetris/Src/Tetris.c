#include "Tetris.h"
#include "stm32f4xx_hal.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

static Stone   currentStone;
static long    points = 0;

static int BUTTON_LEFT_LAST  = 0;
static int BUTTON_RIGHT_LAST = 0;
static int BUTTON_ROT_LAST   = 0;
static int BUTTON_DOWN_LAST  = 0;

/* ── UART helpers ────────────────────────────────────────────── */

static void uart_print(const char *s)
{
    HAL_UART_Transmit(&huart2, (uint8_t *)s, strlen(s), HAL_MAX_DELAY);
}

/* Non-blocking single-char read. Returns 0xFF if nothing available. */
static uint8_t uart_getc(void)
{
    if (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_RXNE))
        return (uint8_t)(huart2.Instance->DR & 0xFF);
    return 0xFF;
}

/*
 * Returns: 'L' 'R' 'U' 'D' ' ' or 0
 * Handles raw chars AND VT100 arrow escape sequences.
 */
static char read_input(void)
{
    uint8_t c = uart_getc();
    if (c == 0xFF) return 0;

    /* VT100 escape sequence: ESC [ A/B/C/D */
    if (c == 0x1B) {
        uint8_t c2 = 0xFF, c3 = 0xFF;
        uint32_t t = HAL_GetTick();
        while (c2 == 0xFF && (HAL_GetTick() - t) < 5)  c2 = uart_getc();
        while (c3 == 0xFF && (HAL_GetTick() - t) < 10) c3 = uart_getc();
        if (c2 == '[') {
            if (c3 == 'A') return 'U';  /* up    = rotate */
            if (c3 == 'B') return 'D';  /* down  = soft drop */
            if (c3 == 'C') return 'R';  /* right */
            if (c3 == 'D') return 'L';  /* left  */
        }
        return 0;
    }

    switch (c) {
        case 'a': case 'A': return 'L';
        case 'd': case 'D': return 'R';
        case 'w': case 'W': return 'U';
        case 's': case 'S': return 'D';
        case ' ':           return ' ';
        default:            return 0;
    }
}

/* ── Score display ───────────────────────────────────────────── */

static void show_score(void)
{
    char buf[8];
    snprintf(buf, sizeof(buf), "%-6ld", points);
    Lcd_cursor(&hlcd, 0, 10);
    Lcd_string(&hlcd, buf);
}

static void add_points(long p)
{
    points += p;
    show_score();
}

/* ── Pseudo-random (same XOR-shift used in Stone.c) ─────────── */

static uint32_t te_rand(uint32_t lo, uint32_t hi)
{
    static uint32_t s = 0;
    if (!s) s = HAL_GetTick() | 1;
    s ^= s << 13; s ^= s >> 17; s ^= s << 5;
    return lo + (s % (hi - lo));
}

/* ── Row deletion ────────────────────────────────────────────── */

static void Te_DeleteRow(int row)
{
    for (int i = 0; i < 15; i++) {
        int sw = i & 1;
        Dp_DrawLine(1, row, 8, 1,  sw);
        Dp_DrawLine(0,  0, 10, 1, !sw);
        Dp_DrawLine(0, 15, 10, 1, !sw);
        Dp_DrawLine(0,  0, 16, 0, !sw);
        Dp_DrawLine(9,  0, 16, 0, !sw);
        Dp_Draw();
        HAL_Delay(90);
    }
    Dp_DrawLine(1, row, 8, 1, 0);
    Dp_DrawLine(0,  0, 10, 1, 1);
    Dp_DrawLine(0, 15, 10, 1, 1);
    Dp_DrawLine(0,  0, 16, 0, 1);
    Dp_DrawLine(9,  0, 16, 0, 1);
    Dp_Draw();

    for (int zeile = row; zeile > 2; zeile--)
        for (int x = 1; x < TE_WIDTH + 1; x++)
            Dp_SetPixel(x, zeile, Dp_GetPixel(x, zeile - 1));
}

static void Te_CheckRows(void)
{
    uint8_t wins = 0;
    for (int zeile = TE_HEIGHT; zeile > 0; zeile--) {
        int full = 1;
        for (int x = 1; x < TE_WIDTH; x++)
            if (!Dp_GetPixel(x, zeile)) { full = 0; break; }
        if (full) { wins++; Te_DeleteRow(zeile); zeile++; }
    }
    if (wins)
        add_points((long)te_rand(9,15) * (10 + (long)pow(3.0, wins) * 10));
}

/* ── Game Over / Reload ──────────────────────────────────────── */

static void Te_Reload(void)
{
    for (int row = 1; row < TE_HEIGHT + 1; row++) {
        Dp_DrawLine(1, row, 8, 1, 1); Dp_Draw(); HAL_Delay(60);
    }
    HAL_Delay(400);
    for (int row = TE_HEIGHT; row >= 1; row--) {
        Dp_DrawLine(1, row, 8, 1, 0); Dp_Draw(); HAL_Delay(60);
    }
    currentStone = St_InitStone();
    points = 0;
    Lcd_cursor(&hlcd, 0, 10); Lcd_string(&hlcd, "0     ");
    Lcd_cursor(&hlcd, 1, 3);  Lcd_string(&hlcd, "             ");
}

/* ── Public API ──────────────────────────────────────────────── */

void Te_Init(void)
{
    currentStone = St_InitStone();
}

void Te_Draw(void)
{
    St_Print(currentStone, 1);
    Dp_Draw();
    St_Print(currentStone, 0);
}

void Te_Update(uint32_t frameCount)
{
    /* ── Gravity: drop one row every 40 frames (~40 ms) ───────── */
    if (frameCount % 40 == 0) {
        currentStone.y++;
        if (St_HitTest(currentStone)) {
            currentStone.y--;

            /* Lock piece into the board */
            St_Print(currentStone, 1);
            currentStone = St_InitStone();
            Dp_Draw();

            /* Game over? */
            if (St_HitTest(currentStone)) {
                uart_print("\r\nGame Over! Press any key...\r\n");
                Lcd_cursor(&hlcd, 1, 3); Lcd_string(&hlcd, "Game Over!");
                while (uart_getc() == 0xFF) {}
                Te_Reload();
                return;
            }

            /* Score + flash only when a full row is cleared */
            Te_CheckRows();
            return;
        }
    }

    /* Re-arm down-key repeat every 10 frames */
    if (frameCount % 10 == 0) BUTTON_DOWN_LAST = 0;

    /* ── Read input ───────────────────────────────────────────── */
    char key = read_input();

    /* LEFT */
    if (key == 'L') {
        if (!BUTTON_LEFT_LAST) {
            BUTTON_LEFT_LAST = 1;
            currentStone.x--;
            if (St_HitTest(currentStone)) currentStone.x++;
        }
    } else BUTTON_LEFT_LAST = 0;

    /* RIGHT */
    if (key == 'R') {
        if (!BUTTON_RIGHT_LAST) {
            BUTTON_RIGHT_LAST = 1;
            currentStone.x++;
            if (St_HitTest(currentStone)) currentStone.x--;
        }
    } else BUTTON_RIGHT_LAST = 0;

    /* ROTATE */
    if (key == 'U') {
        if (!BUTTON_ROT_LAST) {
            BUTTON_ROT_LAST = 1;
            St_Rotate(&currentStone);
            if (St_HitTest(currentStone)) St_BackRotate(&currentStone);
        }
    } else BUTTON_ROT_LAST = 0;

    /* SOFT DROP */
    if (key == 'D') {
        if (!BUTTON_DOWN_LAST) {
            BUTTON_DOWN_LAST = 1;
            currentStone.y++;
            if (St_HitTest(currentStone)) currentStone.y--;
        }
    } else BUTTON_DOWN_LAST = 0;

    /* HARD DROP */
    if (key == ' ') {
        while (!St_HitTest(currentStone)) currentStone.y++;
        currentStone.y--;
    }
}
