/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Pacman Rider for STM32F446RE (HAL version)
  *
  * LCD 16x2, 4-bit parallel:
  *   RS → PB5,  EN → PB4
  *   D4 → PC7,  D5 → PB6,  D6 → PA7,  D7 → PA6
  *
  * Button → PA0 (PULLDOWN, active-HIGH, polled) = move pacman
  *
  * UART2 keyboard control (PA2=TX, PA3=RX, 9600 baud):
  *   Space / Enter / any key = move pacman (same as button)
  *
  * No EEPROM on STM32F446RE bare-metal HAL without FatFS,
  * so high score is kept in RAM (resets on power-off).
  * If you want persistence, enable Flash emulation or backup SRAM.
  ******************************************************************************
  */
/* USER CODE END Header */

#include "main.h"

/* USER CODE BEGIN Includes */
#include "lcd.h"
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
/* USER CODE END Includes */

/* USER CODE BEGIN PD */
#define MAX_SPRITES      10
#define STATE_INTRO       0
#define STATE_PLAY        1
#define STATE_GAMEOVER    2
#define TYPE_NONE         0
#define TYPE_HEART        1
#define TYPE_GHOST        2
#define TOP               0
#define BOTTOM            1

/* Custom char slot indices */
#define SPRITE_PACMAN_OPEN    0
#define SPRITE_PACMAN_CLOSED  1
#define SPRITE_GHOST          2
#define SPRITE_HEART          3
#define SPRITE_SMILEY         4
/* slots 5,6,7 used by animation only */
/* USER CODE END PD */

/* USER CODE BEGIN PV */
static Lcd_HandleTypeDef lcd;
UART_HandleTypeDef huart2;

/* ── Sprite bitmaps (identical to Arduino original) ─────────── */
static const uint8_t spriteBitmaps[8][8] = {
    {0x07,0x0F,0x1E,0x1C,0x1C,0x1E,0x0F,0x07},  /* 0: pacman open   */
    {0x00,0x0F,0x1F,0x1F,0x1E,0x1F,0x0F,0x00},  /* 1: pacman closed */
    {0x19,0x1F,0x15,0x1F,0x11,0x1F,0x1D,0x0C},  /* 2: ghost         */
    {0x00,0x00,0x00,0x0A,0x15,0x11,0x0A,0x04},  /* 3: heart         */
    {0x00,0x0A,0x00,0x00,0x11,0x0E,0x00,0x00},  /* 4: smiley        */
    {0x1F,0x1F,0x1F,0x1F,0x1F,0x00,0x00,0x00},  /* 5: anim top      */
    {0x00,0x00,0x00,0x1F,0x1F,0x1F,0x1F,0x1F},  /* 6: anim bottom   */
    {0x1F,0x00,0x00,0x00,0x00,0x00,0x00,0x00},  /* 7: anim line     */
};

/* ── Sprite struct ───────────────────────────────────────────── */
typedef struct { int x, y, type; } Sprite;
static Sprite sprites[MAX_SPRITES];

/* ── Game variables ──────────────────────────────────────────── */
static int      state;
static int      score;
static int      highScore   = 0;   /* RAM only – resets on power-off */
static int      gameSpeed;         /* ms between moves */
static int      ghostOdds;
static int      pacmanX, pacmanY;
static uint8_t  mouthState;
static uint8_t  smile;

static uint32_t timeToMove;
static uint32_t timeToAnimate;
static uint32_t timeToDebounce;

/* ── LCG random (no stdlib rand needed) ─────────────────────── */
static uint32_t rng_state = 73529;
static int rng_range(int lo, int hi)   /* returns lo..hi-1 */
{
    rng_state = rng_state * 1664525u + 1013904223u;
    int range = hi - lo;
    if (range <= 0) return lo;
    return lo + (int)((rng_state >> 16) % (uint32_t)range);
}
/* USER CODE END PV */

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);

/* USER CODE BEGIN 0 */

/* ════════════════════════════════════════════════════════════════
 * Hardware helpers
 * ════════════════════════════════════════════════════════════════ */
static uint8_t UART_ReadChar(char *c)
{
    if (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_RXNE)) {
        *c = (char)(huart2.Instance->DR & 0xFF);
        return 1;
    }
    return 0;
}

/* Returns 1 on debounced press (button OR any UART key) */
static int checkButton(void)
{
    uint32_t now = HAL_GetTick();
    if (now > timeToDebounce) {
        uint8_t pressed = 0;

        /* Physical button (PULLDOWN, active-HIGH) */
        if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) == GPIO_PIN_SET)
            pressed = 1;

        /* Any UART character also counts */
        char c = 0;
        if (UART_ReadChar(&c)) {
            /* Swallow ESC sequences so arrow keys don't fire twice */
            if (c == 0x1B) {
                uint32_t t = HAL_GetTick();
                char c2 = 0, c3 = 0;
                while (!UART_ReadChar(&c2) && (HAL_GetTick()-t) < 5)  {}
                while (!UART_ReadChar(&c3) && (HAL_GetTick()-t) < 10) {}
            }
            pressed = 1;
        }

        if (pressed) {
            timeToDebounce = now + 300;
            return 1;
        }
    }
    return 0;
}

static void waitButton(void)
{
    /* Drain any pending input first so a held key doesn't skip pages */
    HAL_Delay(200);
    char dummy = 0;
    while (UART_ReadChar(&dummy)) {}
    while (!checkButton()) HAL_Delay(50);
}

/* ── LCD build ──────────────────────────────────────────────── */
static Lcd_HandleTypeDef lcd_build(void)
{
    static Lcd_PortType dp[4]   = { GPIOC, GPIOB, GPIOA, GPIOA };
    static Lcd_PinType  dpin[4] = { GPIO_PIN_7, GPIO_PIN_6,
                                    GPIO_PIN_7, GPIO_PIN_6 };
    return Lcd_create(dp, dpin,
                      GPIOB, GPIO_PIN_5,
                      GPIOB, GPIO_PIN_4,
                      LCD_4_BIT_MODE);
}

/* Load all 8 sprite bitmaps into CGRAM */
static void loadSprites(void)
{
    for (int i = 0; i < 8; i++)
        Lcd_define_char(&lcd, i, (uint8_t*)spriteBitmaps[i]);
}

/* ════════════════════════════════════════════════════════════════
 * Game logic  (direct port of Arduino functions)
 * ════════════════════════════════════════════════════════════════ */
static void initVars(void)
{
    for (int i = 0; i < MAX_SPRITES; i++)
        sprites[i] = (Sprite){0, 0, TYPE_NONE};

    timeToMove     = 0;
    timeToAnimate  = 0;
    timeToDebounce = 0;
    score          = 0;
    gameSpeed      = 600;
    ghostOdds      = 6;
    pacmanX        = 1;
    pacmanY        = TOP;
    mouthState     = 0;
    smile          = 0;
}

static int at(int x, int y)
{
    for (int i = 0; i < MAX_SPRITES; i++)
        if (sprites[i].x == x && sprites[i].y == y &&
            sprites[i].type != TYPE_NONE)
            return sprites[i].type;
    return TYPE_NONE;
}

static void createSprite(int s, int x, int y, int type)
{
    sprites[s].x    = x;
    sprites[s].y    = y;
    sprites[s].type = type;
}

static void hideSprite(int s)
{
    Lcd_cursor(&lcd, sprites[s].y, sprites[s].x);
    Lcd_write_char(&lcd, 0x20);   /* space */
}

static void deleteSprite(int s)
{
    hideSprite(s);
    sprites[s] = (Sprite){0, 0, TYPE_NONE};
}

static void moveSprite(int s, int x, int y)
{
    if (x < 0 || x > 15) {
        deleteSprite(s);
    } else {
        hideSprite(s);
        sprites[s].x = x;
        sprites[s].y = y;
    }
}

static void drawSprite(int s)
{
    int type = sprites[s].type;
    if (type == TYPE_NONE) return;
    int x = sprites[s].x;
    int y = sprites[s].y;
    if (x == pacmanX && y == pacmanY) return;  /* pacman occupies this cell */
    Lcd_cursor(&lcd, y, x);
    switch (type) {
        case TYPE_GHOST: Lcd_write_char(&lcd, SPRITE_GHOST); break;
        case TYPE_HEART: Lcd_write_char(&lcd, SPRITE_HEART); break;
        default:         Lcd_write_char(&lcd, 0x20);         break;
    }
}

static void hidePacman(void)
{
    Lcd_cursor(&lcd, pacmanY, pacmanX);
    Lcd_write_char(&lcd, 0x20);
}

static void drawPacman(void)
{
    uint32_t now = HAL_GetTick();
    if (now > timeToAnimate || smile) {
        uint32_t wait = 350;
        Lcd_cursor(&lcd, pacmanY, pacmanX);
        if (smile) {
            Lcd_write_char(&lcd, SPRITE_SMILEY);
            wait  = 600;
            smile = 0;
        } else if (mouthState) {
            Lcd_write_char(&lcd, SPRITE_PACMAN_OPEN);
        } else {
            Lcd_write_char(&lcd, SPRITE_PACMAN_CLOSED);
        }
        mouthState    = !mouthState;
        timeToAnimate = now + wait;
    }
}

static void drawScreen(void)
{
    for (int i = 0; i < MAX_SPRITES; i++) drawSprite(i);
    drawPacman();
}

static int okayToSpawnGhost(int pos)
{
    if (at(15, pos)  != TYPE_NONE)  return 0;
    if (at(15, !pos) == TYPE_GHOST) return 0;
    if (pos == TOP    && at(14, BOTTOM) == TYPE_GHOST) return 0;
    if (pos == BOTTOM && at(14, TOP)    == TYPE_GHOST) return 0;
    return 1;
}

static int okayToSpawnHeart(int pos)
{
    return (at(15, pos) == TYPE_NONE) ? 1 : 0;
}

static void spawn(int type)
{
    int x = 15;
    int y = rng_range(0, 2);
    for (int i = 0; i < MAX_SPRITES; i++) {
        if (sprites[i].type == TYPE_NONE) {
            if ((type == TYPE_GHOST && okayToSpawnGhost(y)) ||
                (type == TYPE_HEART && okayToSpawnHeart(y)))
                createSprite(i, x, y, type);
            return;
        }
    }
}

static void moveLeft(void)
{
    for (int i = 0; i < MAX_SPRITES; i++)
        if (sprites[i].type != TYPE_NONE)
            moveSprite(i, sprites[i].x - 1, sprites[i].y);
}

static void eatHeart(void)
{
    for (int i = 0; i < MAX_SPRITES; i++) {
        if (sprites[i].x    == pacmanX &&
            sprites[i].y    == pacmanY &&
            sprites[i].type == TYPE_HEART) {
            smile = 1;
            deleteSprite(i);
            return;
        }
    }
}

static void increaseScore(void)
{
    score++;
    if (!(score % 10)) {
        if (gameSpeed > 60) gameSpeed -= 30;
        if (ghostOdds > 1)  ghostOdds--;
    }
}

static int collision(void)
{
    return at(pacmanX, pacmanY);
}

/* Opening/closing curtain animation identical to Arduino version */
static void animation(int direction)
{
    /* direction=1: open (play start), direction=0: close (game over) */
    const uint8_t animOpen[6]  = {0xFF, 0xFF, 5, 6, 7, 0x5F};
    const uint8_t animClose[6] = {7, 0x5F, 5, 6, 0xFF, 0xFF};
    const uint8_t *anim = direction ? animOpen : animClose;

    Lcd_clear(&lcd);
    loadSprites();   /* CGRAM wiped by clear on some controllers */

    for (int frame = 0; frame < 3; frame++) {
        /* Top row */
        Lcd_cursor(&lcd, 0, 0);
        for (int i = 0; i < 16; i++)
            Lcd_write_char(&lcd, anim[frame * 2]);
        /* Bottom row */
        Lcd_cursor(&lcd, 1, 0);
        for (int i = 0; i < 16; i++)
            Lcd_write_char(&lcd, anim[frame * 2 + 1]);

        HAL_Delay(300);
        Lcd_clear(&lcd);
        loadSprites();
    }
}

/* ════════════════════════════════════════════════════════════════
 * Game states
 * ════════════════════════════════════════════════════════════════ */
static void intro(void)
{
    Lcd_clear(&lcd);
    Lcd_cursor(&lcd, 0, 3); Lcd_string(&lcd, "WELCOME TO");
    Lcd_cursor(&lcd, 1, 1); Lcd_string(&lcd, "PACMAN RIDER");
    waitButton();

    Lcd_clear(&lcd);
    Lcd_cursor(&lcd, 0, 0); Lcd_string(&lcd, "Press the button");
    Lcd_cursor(&lcd, 1, 1); Lcd_string(&lcd, "to move pacman");
    waitButton();

    Lcd_clear(&lcd);
    Lcd_cursor(&lcd, 0, 3); Lcd_string(&lcd, "COLLECT: ");
    Lcd_write_char(&lcd, SPRITE_HEART);
    Lcd_cursor(&lcd, 1, 4); Lcd_string(&lcd, "AVOID: ");
    Lcd_write_char(&lcd, SPRITE_GHOST);
    waitButton();

    Lcd_clear(&lcd);
    Lcd_cursor(&lcd, 0, 1); Lcd_string(&lcd, "ARE YOU READY?");
    Lcd_cursor(&lcd, 1, 0); Lcd_string(&lcd, "Press the button");
    waitButton();

    animation(1);
    state = STATE_PLAY;
}

static void play(void)
{
    drawScreen();
    uint32_t now = HAL_GetTick();

    if (checkButton()) {
        hidePacman();
        pacmanY = !pacmanY;
    }

    if (now > timeToMove) {
        moveLeft();
        if (!rng_range(0, ghostOdds)) spawn(TYPE_GHOST);
        if (!rng_range(0, 3))         spawn(TYPE_HEART);
        timeToMove = now + (uint32_t)gameSpeed;
    }

    int c = collision();
    if (c == TYPE_HEART) {
        eatHeart();
        increaseScore();
    } else if (c == TYPE_GHOST) {
        state = STATE_GAMEOVER;
    }
}

static void gameover(void)
{
    animation(0);

    Lcd_cursor(&lcd, 0, 3); Lcd_string(&lcd, "GAME OVER");
    waitButton();

    Lcd_clear(&lcd);
    Lcd_cursor(&lcd, 0, 2); Lcd_string(&lcd, "YOUR SCORE:");
    Lcd_cursor(&lcd, 1, 7); Lcd_int(&lcd, score);
    waitButton();

    if (score > highScore) {
        highScore = score;
        Lcd_clear(&lcd);
        Lcd_cursor(&lcd, 0, 1); Lcd_string(&lcd, "NEW HIGHSCORE!");
        waitButton();
    }

    Lcd_clear(&lcd);
    Lcd_cursor(&lcd, 0, 3); Lcd_string(&lcd, "TRY AGAIN");
    Lcd_cursor(&lcd, 1, 2); Lcd_string(&lcd, "Highscore: ");
    Lcd_int(&lcd, highScore);
    waitButton();

    initVars();
    loadSprites();
    state = STATE_PLAY;
    Lcd_clear(&lcd);
    loadSprites();
}

/* USER CODE END 0 */

/* ════════════════════════════════════════════════════════════════
 * main
 * ════════════════════════════════════════════════════════════════ */
int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART2_UART_Init();

    lcd = lcd_build();
    loadSprites();

    /* Seed LCG from SysTick counter */
    rng_state ^= HAL_GetTick();

    initVars();
    state = STATE_INTRO;

    while (1)
    {
        switch (state) {
            case STATE_INTRO:    intro();    break;
            case STATE_PLAY:     play();     break;
            case STATE_GAMEOVER: gameover(); break;
        }
    }
}

/* ════════════════════════════════════════════════════════════════
 * Peripheral init
 * ════════════════════════════════════════════════════════════════ */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

    RCC_OscInitStruct.OscillatorType      = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState            = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState        = RCC_PLL_NONE;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) Error_Handler();

    RCC_ClkInitStruct.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                     | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_HSI;
    RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK) Error_Handler();
}

static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6 | GPIO_PIN_7, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_7, GPIO_PIN_RESET);

    /* PA0 – button, PULLDOWN, polled */
    GPIO_InitStruct.Pin  = GPIO_PIN_0;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLDOWN;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* PA6=D7, PA7=D6 */
    GPIO_InitStruct.Pin   = GPIO_PIN_6 | GPIO_PIN_7;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* PB4=EN, PB5=RS, PB6=D5 */
    GPIO_InitStruct.Pin   = GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* PC7=D4 */
    GPIO_InitStruct.Pin   = GPIO_PIN_7;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
}

static void MX_USART2_UART_Init(void)
{
    __HAL_RCC_USART2_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin       = GPIO_PIN_2 | GPIO_PIN_3;
    GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull      = GPIO_NOPULL;
    GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF7_USART2;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    huart2.Instance          = USART2;
    huart2.Init.BaudRate     = 9600;
    huart2.Init.WordLength   = UART_WORDLENGTH_8B;
    huart2.Init.StopBits     = UART_STOPBITS_1;
    huart2.Init.Parity       = UART_PARITY_NONE;
    huart2.Init.Mode         = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart2) != HAL_OK) Error_Handler();
}

void Error_Handler(void)
{
    __disable_irq();
    while (1) {}
}
