/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Jumping Jack Game
  ******************************************************************************
  */
/* USER CODE END Header */

#include "main.h"

/* USER CODE BEGIN Includes */
#include "lcd.h"
#include <string.h>
#include <stdint.h>
/* USER CODE END Includes */

/* USER CODE BEGIN PD */
#define SPRITE_RUN1                1
#define SPRITE_RUN2                2
#define SPRITE_JUMP                3
#define SPRITE_JUMP_UPPER         '.'
#define SPRITE_JUMP_LOWER          4
#define SPRITE_TERRAIN_EMPTY      ' '
#define SPRITE_TERRAIN_SOLID       5
#define SPRITE_TERRAIN_SOLID_RIGHT 6
#define SPRITE_TERRAIN_SOLID_LEFT  7

#define HERO_HORIZONTAL_POSITION   1
#define TERRAIN_WIDTH             16
#define TERRAIN_EMPTY              0
#define TERRAIN_LOWER_BLOCK        1
#define TERRAIN_UPPER_BLOCK        2

#define HERO_POSITION_OFF          0
#define HERO_POSITION_RUN_LOWER_1  1
#define HERO_POSITION_RUN_LOWER_2  2
#define HERO_POSITION_JUMP_1       3
#define HERO_POSITION_JUMP_2       4
#define HERO_POSITION_JUMP_3       5
#define HERO_POSITION_JUMP_4       6
#define HERO_POSITION_JUMP_5       7
#define HERO_POSITION_JUMP_6       8
#define HERO_POSITION_JUMP_7       9
#define HERO_POSITION_JUMP_8      10
#define HERO_POSITION_RUN_UPPER_1 11
#define HERO_POSITION_RUN_UPPER_2 12

/* Game states */
#define STATE_START     0
#define STATE_PLAYING   1
#define STATE_GAMEOVER  2
/* USER CODE END PD */

/* USER CODE BEGIN PV */
static Lcd_HandleTypeDef lcd;
static char terrainUpper[TERRAIN_WIDTH + 1];
static char terrainLower[TERRAIN_WIDTH + 1];
/* USER CODE END PV */

void SystemClock_Config(void);
static void MX_GPIO_Init(void);

/* USER CODE BEGIN 0 */

/* ── LCG random ─────────────────────────────────────────────── */
static uint32_t rng_state = 12345;
static uint32_t rng(uint32_t max)
{
    rng_state = rng_state * 1664525u + 1013904223u;
    return (rng_state >> 16) % max;
}

/* ── Custom character bitmaps ───────────────────────────────── */
static uint8_t graphics[7][8] = {
    {0x0C,0x0C,0x00,0x0E,0x1C,0x0C,0x1A,0x13},  /* 1: Run1      */
    {0x0C,0x0C,0x00,0x0C,0x0C,0x0C,0x0C,0x0E},  /* 2: Run2      */
    {0x0C,0x0C,0x00,0x1E,0x0D,0x1F,0x10,0x00},  /* 3: Jump      */
    {0x1E,0x0D,0x1F,0x10,0x00,0x00,0x00,0x00},  /* 4: JumpLower */
    {0x1F,0x1F,0x1F,0x1F,0x1F,0x1F,0x1F,0x1F},  /* 5: Ground    */
    {0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03},  /* 6: GndRight  */
    {0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18},  /* 7: GndLeft   */
};

/* ── initializeGraphics ─────────────────────────────────────── */
static void initializeGraphics(void)
{
    for (uint8_t i = 0; i < 7; i++)
        Lcd_define_char(&lcd, i + 1, graphics[i]);

    for (int i = 0; i < TERRAIN_WIDTH; i++) {
        terrainUpper[i] = SPRITE_TERRAIN_EMPTY;
        terrainLower[i] = SPRITE_TERRAIN_EMPTY;
    }
}

/* ── advanceTerrain ─────────────────────────────────────────── */
static void advanceTerrain(char *terrain, char newTerrain)
{
    for (int i = 0; i < TERRAIN_WIDTH; i++) {
        char current = terrain[i];
        char next    = (i == TERRAIN_WIDTH - 1) ? newTerrain : terrain[i + 1];
        switch (current) {
            case SPRITE_TERRAIN_EMPTY:
                terrain[i] = (next == SPRITE_TERRAIN_SOLID)
                             ? SPRITE_TERRAIN_SOLID_RIGHT : SPRITE_TERRAIN_EMPTY;
                break;
            case SPRITE_TERRAIN_SOLID:
                terrain[i] = (next == SPRITE_TERRAIN_EMPTY)
                             ? SPRITE_TERRAIN_SOLID_LEFT : SPRITE_TERRAIN_SOLID;
                break;
            case SPRITE_TERRAIN_SOLID_RIGHT:
                terrain[i] = SPRITE_TERRAIN_SOLID;
                break;
            case SPRITE_TERRAIN_SOLID_LEFT:
                terrain[i] = SPRITE_TERRAIN_EMPTY;
                break;
        }
    }
}

/* ── drawHero ───────────────────────────────────────────────── */
static uint8_t drawHero(uint8_t position, unsigned int score)
{
    uint8_t collide = 0;
    char upperSave  = terrainUpper[HERO_HORIZONTAL_POSITION];
    char lowerSave  = terrainLower[HERO_HORIZONTAL_POSITION];
    char upper, lower;

    switch (position) {
        case HERO_POSITION_OFF:
            upper = lower = SPRITE_TERRAIN_EMPTY; break;
        case HERO_POSITION_RUN_LOWER_1:
            upper = SPRITE_TERRAIN_EMPTY; lower = SPRITE_RUN1; break;
        case HERO_POSITION_RUN_LOWER_2:
            upper = SPRITE_TERRAIN_EMPTY; lower = SPRITE_RUN2; break;
        case HERO_POSITION_JUMP_1:
        case HERO_POSITION_JUMP_8:
            upper = SPRITE_TERRAIN_EMPTY; lower = SPRITE_JUMP; break;
        case HERO_POSITION_JUMP_2:
        case HERO_POSITION_JUMP_7:
            upper = SPRITE_JUMP_UPPER; lower = SPRITE_JUMP_LOWER; break;
        case HERO_POSITION_JUMP_3:
        case HERO_POSITION_JUMP_4:
        case HERO_POSITION_JUMP_5:
        case HERO_POSITION_JUMP_6:
            upper = SPRITE_JUMP; lower = SPRITE_TERRAIN_EMPTY; break;
        case HERO_POSITION_RUN_UPPER_1:
            upper = SPRITE_RUN1; lower = SPRITE_TERRAIN_EMPTY; break;
        case HERO_POSITION_RUN_UPPER_2:
            upper = SPRITE_RUN2; lower = SPRITE_TERRAIN_EMPTY; break;
        default:
            upper = lower = SPRITE_TERRAIN_EMPTY; break;
    }

    if (upper != SPRITE_TERRAIN_EMPTY) {
        terrainUpper[HERO_HORIZONTAL_POSITION] = upper;
        collide = (upperSave != SPRITE_TERRAIN_EMPTY) ? 1 : 0;
    }
    if (lower != SPRITE_TERRAIN_EMPTY) {
        terrainLower[HERO_HORIZONTAL_POSITION] = lower;
        collide |= (lowerSave != SPRITE_TERRAIN_EMPTY) ? 1 : 0;
    }

    uint8_t digits = (score > 9999) ? 5 :
                     (score > 999)  ? 4 :
                     (score > 99)   ? 3 :
                     (score > 9)    ? 2 : 1;

    terrainUpper[TERRAIN_WIDTH] = '\0';
    terrainLower[TERRAIN_WIDTH] = '\0';

    char temp = terrainUpper[16 - digits];
    terrainUpper[16 - digits] = '\0';
    Lcd_cursor(&lcd, 0, 0);
    Lcd_string(&lcd, terrainUpper);
    terrainUpper[16 - digits] = temp;

    Lcd_cursor(&lcd, 1, 0);
    Lcd_string(&lcd, terrainLower);

    Lcd_cursor(&lcd, 0, 16 - digits);
    Lcd_int(&lcd, (int)score);

    terrainUpper[HERO_HORIZONTAL_POSITION] = upperSave;
    terrainLower[HERO_HORIZONTAL_POSITION] = lowerSave;

    return collide;
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

/* ── Button: debounced rising-edge detect ───────────────────── */
/* Call every loop iteration. Returns 1 only on the single tick  */
/* when the button transitions from released → pressed.          */
static uint8_t buttonJustPressed(void)
{
    static uint8_t prev = 0;
    uint8_t curr = (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) == GPIO_PIN_SET) ? 1 : 0;
    uint8_t edge = (curr && !prev);
    prev = curr;
    return edge;
}

/* USER CODE END 0 */

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();

    lcd = lcd_build();
    initializeGraphics();

    uint8_t      gameState         = STATE_START;
    uint8_t      heroPos           = HERO_POSITION_RUN_LOWER_1;
    uint8_t      newTerrainType    = TERRAIN_EMPTY;
    uint8_t      newTerrainDuration = 1;
    unsigned int distance          = 0;
    unsigned int finalScore        = 0;

    /* ── Starting Screen ──────── */
    Lcd_clear(&lcd);
    Lcd_cursor(&lcd, 0, 1);
    Lcd_string(&lcd, "JUMPING JACK");
    Lcd_cursor(&lcd, 1, 2);
    Lcd_string(&lcd, "BTN:>");

    while (1)
    {
        uint8_t pressed = buttonJustPressed();

        /* ════════════════════════════════════════════════════════
         * STATE: START – static screen, wait for button press
         * ════════════════════════════════════════════════════════ */
        if (gameState == STATE_START)
        {
            if (pressed)
            {
                /* Reset everything and start playing */
                initializeGraphics();
                heroPos             = HERO_POSITION_RUN_LOWER_1;
                newTerrainType      = TERRAIN_EMPTY;
                newTerrainDuration  = 1;
                distance            = 0;
                gameState           = STATE_PLAYING;
            }
            HAL_Delay(20);   /* small poll delay, no blinking */
            continue;
        }

        /* ════════════════════════════════════════════════════════
         * STATE: GAME OVER – static screen, wait for button press
         * ════════════════════════════════════════════════════════ */
        if (gameState == STATE_GAMEOVER)
        {
            if (pressed)
            {
                /* Restart */
                initializeGraphics();
                heroPos             = HERO_POSITION_RUN_LOWER_1;
                newTerrainType      = TERRAIN_EMPTY;
                newTerrainDuration  = 1;
                distance            = 0;
                gameState           = STATE_PLAYING;
            }
            HAL_Delay(20);
            continue;
        }

        /* ════════════════════════════════════════════════════════
         * STATE: PLAYING
         * ════════════════════════════════════════════════════════ */

        /* Advance terrain */
        advanceTerrain(terrainLower,
            newTerrainType == TERRAIN_LOWER_BLOCK
                ? SPRITE_TERRAIN_SOLID : SPRITE_TERRAIN_EMPTY);
        advanceTerrain(terrainUpper,
            newTerrainType == TERRAIN_UPPER_BLOCK
                ? SPRITE_TERRAIN_SOLID : SPRITE_TERRAIN_EMPTY);

        /* Generate new terrain segment */
        if (--newTerrainDuration == 0) {
            if (newTerrainType == TERRAIN_EMPTY) {
                newTerrainType      = (rng(3) == 0) ? TERRAIN_UPPER_BLOCK
                                                    : TERRAIN_LOWER_BLOCK;
                newTerrainDuration  = 2 + rng(10);
            } else {
                newTerrainType      = TERRAIN_EMPTY;
                newTerrainDuration  = 10 + rng(10);
            }
        }

        /* Jump on button press */
        if (pressed) {
            if (heroPos <= HERO_POSITION_RUN_LOWER_2)
                heroPos = HERO_POSITION_JUMP_1;
        }

        /* Draw and check collision */
        if (drawHero(heroPos, distance >> 3))
        {
            /* ── Collision: show Game Over screen once, then wait ── */
            finalScore = distance >> 3;
            Lcd_clear(&lcd);
            Lcd_cursor(&lcd, 0, 3);
            Lcd_string(&lcd, "GAME  OVER");
            Lcd_cursor(&lcd, 1, 1);
            Lcd_string(&lcd, "Score:");
            Lcd_cursor(&lcd, 1, 8);
            Lcd_int(&lcd, (int)finalScore);
            Lcd_cursor(&lcd, 1, 11);
            Lcd_string(&lcd, "BTN:>");

            /* Flush any button press that caused death */
            HAL_Delay(300);
            /* Reset edge-detect state so death-press doesn't restart */
            buttonJustPressed();

            gameState = STATE_GAMEOVER;
        }
        else
        {
            /* Advance hero animation */
            if (heroPos == HERO_POSITION_RUN_LOWER_2 ||
                heroPos == HERO_POSITION_JUMP_8)
            {
                heroPos = HERO_POSITION_RUN_LOWER_1;
            }
            else if (heroPos >= HERO_POSITION_JUMP_3 &&
                     heroPos <= HERO_POSITION_JUMP_5 &&
                     terrainLower[HERO_HORIZONTAL_POSITION] != SPRITE_TERRAIN_EMPTY)
            {
                heroPos = HERO_POSITION_RUN_UPPER_1;
            }
            else if (heroPos >= HERO_POSITION_RUN_UPPER_1 &&
                     terrainLower[HERO_HORIZONTAL_POSITION] == SPRITE_TERRAIN_EMPTY)
            {
                heroPos = HERO_POSITION_JUMP_5;
            }
            else if (heroPos == HERO_POSITION_RUN_UPPER_2)
            {
                heroPos = HERO_POSITION_RUN_UPPER_1;
            }
            else
            {
                heroPos++;
            }
            distance++;
        }

        HAL_Delay(100);
    }
}

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
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0 | GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_7, GPIO_PIN_RESET);

    /* PA0 – button, PULLDOWN, polled */
    GPIO_InitStruct.Pin  = GPIO_PIN_0;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLDOWN;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* PA6, PA7 – LCD D7, D6 */
    GPIO_InitStruct.Pin   = GPIO_PIN_6 | GPIO_PIN_7;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* PB0, PB4=EN, PB5=RS, PB6=D5 */
    GPIO_InitStruct.Pin   = GPIO_PIN_0 | GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* PC7 – LCD D4 */
    GPIO_InitStruct.Pin   = GPIO_PIN_7;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
}

void Error_Handler(void)
{
    __disable_irq();
    while (1) {}
}
