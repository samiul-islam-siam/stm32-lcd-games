/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Helicopter Game for STM32F446RE (HAL version)
  *
  * LCD 16x2, 4-bit parallel:
  *   RS → PB5,  EN → PB4
  *   D4 → PC7,  D5 → PB6,  D6 → PA7,  D7 → PA6
  *
  * Button → PA0 (PULLDOWN, active-HIGH)
  *   HOLD button = fly up (row 0)
  *   RELEASE     = fall down (row 1)
  *
  * High score stored in RAM (resets on power-off).
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
/* Custom character slot indices for CGRAM */
#define SPRITE_TAIL0  0
#define SPRITE_TAIL1  1
#define SPRITE_BODY0  2
#define SPRITE_BODY1  3
/* slot 4 unused */
#define SPRITE_WALLB  5
#define SPRITE_WALLT  6
#define SPRITE_EXPL   7

/* Game state machine values */
#define GAME_HOME     0
#define GAME_PLAY     1
#define GAME_OVER     2

/* Timing constants (all in milliseconds) */
#define FRAME_RATE          125UL   /* ms per animation frame            */
#define MAX_WALL_RATE       350UL   /* ms between wall advances (slow)   */
#define MIN_WALL_RATE       140UL   /* ms between wall advances (fast)   */
#define DEATH_HOLD         1500UL   /* ms to show death animation        */
#define DEATH_FLASH         150UL   /* ms per explosion flash cycle      */
/* USER CODE END PD */

/* USER CODE BEGIN PV */
static Lcd_HandleTypeDef lcd;

/* ── Sprite bitmaps (5x8 custom characters for CGRAM) ───────── */
static uint8_t sprite_tail0[8] = {0x00,0x00,0x00,0x11,0x0F,0x05,0x00,0x01};
static uint8_t sprite_tail1[8] = {0x00,0x00,0x00,0x05,0x0F,0x11,0x00,0x01};
static uint8_t sprite_body0[8] = {0x00,0x04,0x04,0x1E,0x15,0x1E,0x14,0x1F};
static uint8_t sprite_body1[8] = {0x00,0x1F,0x04,0x1E,0x15,0x1E,0x14,0x1F};
static uint8_t sprite_wallt[8] = {0x00,0x04,0x09,0x1E,0x09,0x04,0x00,0x00};
static uint8_t sprite_wallb[8] = {0x00,0x04,0x04,0x0A,0x0E,0x0A,0x0E,0x0A};
static uint8_t sprite_expl[8]  = {0x04,0x14,0x0D,0x0A,0x0A,0x16,0x05,0x04};

/* ── Animation sequencer ─────────────────────────────────────── */
typedef struct {
    const int *frames;   /* pointer to frame index array  */
    int        count;    /* total number of frames        */
    int        current;  /* current frame index           */
} Anim;

static const int seq_tail[] = { SPRITE_TAIL0, SPRITE_TAIL1 };
static const int seq_body[] = { SPRITE_BODY0, SPRITE_BODY1 };

static Anim anim_tail = { seq_tail, 2, 0 };
static Anim anim_body = { seq_body, 2, 0 };

/* Advance animation to next frame (wraps around) */
static void anim_next(Anim *a)
{
    a->current = (a->current + 1) % a->count;
}

/* Return the sprite slot index for the current frame */
static int anim_cur(Anim *a)
{
    return a->frames[a->current];
}

/* ── Game state variables ────────────────────────────────────── */
static int            game_mode     = GAME_HOME;
static uint8_t        first         = 1;          /* first-tick init flag      */
static uint8_t        button_state  = 1;          /* 1=released, 0=pressed     */
static unsigned long  score         = 0;
static unsigned long  highScore     = 0;          /* RAM only, resets on power */
static uint8_t        new_highscore = 0;          /* flag: beat high score?    */

/* 16-bit bitmaps: bit N = wall present in column N of that row */
static uint16_t walls_top = 0x0000;
static uint16_t walls_bot = 0x0000;

/* Timing state */
static unsigned long wall_advance_rate = MAX_WALL_RATE;
static unsigned long wall_advance_next = 0;
static unsigned long frame_next        = 0;
static unsigned long next_read         = 0;

/* Helicopter occupies leftmost 2 columns */
static const uint16_t mask_heli = 0x0003;

/* ── LCG pseudo-random number generator ─────────────────────── */
static uint32_t rng_state = 73529;
static uint32_t rng_next(void)
{
    rng_state = rng_state * 1664525u + 1013904223u;
    return rng_state;
}
/* USER CODE END PV */

void SystemClock_Config(void);
static void MX_GPIO_Init(void);

/* USER CODE BEGIN 0 */

/* ════════════════════════════════════════════════════════════════
 * Button reading
 *
 * button_state:
 *   0 = button held (fly UP,  helicopter on row 0)
 *   1 = released    (fall DOWN, helicopter on row 1)
 *
 * Physical button PA0: HIGH when pressed (external PULLDOWN).
 * Polled every 50 ms to debounce.
 * ════════════════════════════════════════════════════════════════ */
static void read_button(void)
{
    /* Throttle reads to once every 50 ms for debounce */
    if (HAL_GetTick() < next_read) return;
    next_read = HAL_GetTick() + 50;

    /* PA0 is HIGH when button is pressed (PULLDOWN config) */
    uint8_t hw = (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) == GPIO_PIN_SET) ? 1 : 0;

    /* button_state=0 means flying (pressed), 1 means falling (released) */
    button_state = hw ? 0 : 1;
}

/* ════════════════════════════════════════════════════════════════
 * LCD pin mapping and sprite loader
 * ════════════════════════════════════════════════════════════════ */
static Lcd_HandleTypeDef lcd_build(void)
{
    /* Data pins D4-D7 mapped to PC7, PB6, PA7, PA6 respectively */
    static Lcd_PortType dp[4]   = { GPIOC, GPIOB, GPIOA, GPIOA };
    static Lcd_PinType  dpin[4] = { GPIO_PIN_7, GPIO_PIN_6,
                                    GPIO_PIN_7, GPIO_PIN_6 };
    return Lcd_create(dp, dpin,
                      GPIOB, GPIO_PIN_5,   /* RS pin */
                      GPIOB, GPIO_PIN_4,   /* EN pin */
                      LCD_4_BIT_MODE);
}

/* Load all custom characters into LCD CGRAM slots */
static void loadSprites(void)
{
    Lcd_define_char(&lcd, SPRITE_TAIL0, sprite_tail0);
    Lcd_define_char(&lcd, SPRITE_TAIL1, sprite_tail1);
    Lcd_define_char(&lcd, SPRITE_BODY0, sprite_body0);
    Lcd_define_char(&lcd, SPRITE_BODY1, sprite_body1);
    Lcd_define_char(&lcd, SPRITE_WALLB, sprite_wallb);
    Lcd_define_char(&lcd, SPRITE_WALLT, sprite_wallt);
    Lcd_define_char(&lcd, SPRITE_EXPL,  sprite_expl);
}

/* ════════════════════════════════════════════════════════════════
 * set_game_mode – transition between HOME / PLAY / OVER
 * Clears screen, reloads sprites, resets input state.
 * ════════════════════════════════════════════════════════════════ */
static void set_game_mode(int mode)
{
    button_state = 1;   /* treat as released on every transition */
    game_mode    = mode;
    first        = 1;   /* signal each state handler to initialise */
    Lcd_clear(&lcd);
    loadSprites();
}

/* ════════════════════════════════════════════════════════════════
 * HOME screen
 * Displays title and best score, animates helicopter sprite.
 * Any button press starts the game.
 * ════════════════════════════════════════════════════════════════ */
static void game_home(uint8_t update)
{
    if (first) {
        first = 0;
        score = highScore;   /* show current best on home screen */
        Lcd_clear(&lcd);
        loadSprites();
    }

    read_button();

    /* button_state==0 means pressed → start game */
    if (!button_state) {
        set_game_mode(GAME_PLAY);
        return;
    }

    if (update) {
        Lcd_cursor(&lcd, 0, 0);
        Lcd_string(&lcd, "Helicopter! ");
        Lcd_write_char(&lcd, (uint8_t)anim_cur(&anim_tail));
        Lcd_write_char(&lcd, (uint8_t)anim_cur(&anim_body));

        Lcd_cursor(&lcd, 1, 0);
        Lcd_string(&lcd, "Best: ");
        Lcd_int(&lcd, (int)score);
    }
}

/* ════════════════════════════════════════════════════════════════
 * PLAY state
 * Scrolls walls left, moves helicopter up/down, detects collision.
 * Score increments with every wall-advance tick.
 * Speed increases gradually until MIN_WALL_RATE is reached.
 * ════════════════════════════════════════════════════════════════ */
static void game_play(uint8_t update)
{
    /* First tick: reset all play state and let LCD settle */
    if (first) {
        first             = 0;
        score             = 0;
        new_highscore     = 0;
        walls_bot         = 0;
        walls_top         = 0;
        wall_advance_rate = MAX_WALL_RATE;
        wall_advance_next = HAL_GetTick() + MAX_WALL_RATE;
        Lcd_clear(&lcd);
        loadSprites();
        return;
    }

    unsigned long now = HAL_GetTick();

    /* ── Advance walls on timer ──────────────────────────────── */
    if (now >= wall_advance_next) {

        /* Gradually increase speed down to MIN_WALL_RATE */
        if (wall_advance_rate > MIN_WALL_RATE)
            wall_advance_rate--;

        wall_advance_next = now + wall_advance_rate;

        /* Shift all wall columns one step to the left */
        walls_top >>= 1;
        walls_bot >>= 1;
        score++;

        /* ── Spawn new wall at rightmost column ──────────────
         * Rules:
         *  1. Rightmost 6 columns must be fully clear (0xFC00)
         *     to guarantee reaction time for the player.
         *  2. Never place walls on both rows at the same column
         *     (would be undodgeable).
         *  3. 40% chance to skip spawning each cycle, creating
         *     natural breathing room between obstacles.
         * ──────────────────────────────────────────────────── */
        uint16_t combined = walls_top | walls_bot;

        if ((combined & 0xFC00) == 0) {
            uint32_t r = rng_next();

            /* Skip spawn 40% of the time (r%10 >= 6 means spawn) */
            if ((r % 10) < 6) {
                if (r & 1) {
                    /* Spawn top wall only if bottom is also clear */
                    if ((walls_bot & 0xFC00) == 0)
                        walls_top |= 0x8000;
                } else {
                    /* Spawn bottom wall only if top is also clear */
                    if ((walls_top & 0xFC00) == 0)
                        walls_bot |= 0x8000;
                }
            }
        }

        update = 1;   /* force a render this tick */
    }

    /* Skip rendering if nothing has changed */
    if (!update) return;

    /* ── Determine helicopter row ────────────────────────────── */
    /* button_state==0 (pressed) → row 0 (top/flying)           */
    /* button_state==1 (released) → row 1 (bottom/falling)      */
    uint8_t heli_row  = button_state ? 1 : 0;
    uint8_t clear_row = button_state ? 0 : 1;   /* row NOT occupied by heli */

    /* Erase helicopter columns on the row the heli just left */
    Lcd_cursor(&lcd, clear_row, 0);
    Lcd_write_char(&lcd, 0x20);
    Lcd_write_char(&lcd, 0x20);

    /* ── Render top-row walls ────────────────────────────────── */
    for (int n = 0; n < 16; n++) {
        if (walls_top & (1 << n)) {
            Lcd_cursor(&lcd, 0, n);
            Lcd_write_char(&lcd, SPRITE_WALLT);
        } else if (n >= 2) {
            /* Only erase cols 2+ to avoid overwriting helicopter */
            Lcd_cursor(&lcd, 0, n);
            Lcd_write_char(&lcd, 0x20);
        }
    }

    /* ── Render bottom-row walls ─────────────────────────────── */
    for (int n = 0; n < 16; n++) {
        if (walls_bot & (1 << n)) {
            Lcd_cursor(&lcd, 1, n);
            Lcd_write_char(&lcd, SPRITE_WALLB);
        } else if (n >= 2) {
            Lcd_cursor(&lcd, 1, n);
            Lcd_write_char(&lcd, 0x20);
        }
    }

    /* ── Collision detection ─────────────────────────────────── */
    /* Check if any wall bit overlaps the heli's 2-column mask   */
    uint16_t active_wall = (heli_row == 0) ? walls_top : walls_bot;

    Lcd_cursor(&lcd, heli_row, 0);

    if (mask_heli & active_wall) {
        /* ── Death animation: flash explosion sprite ─────────── */
        Lcd_write_char(&lcd, SPRITE_EXPL);
        Lcd_write_char(&lcd, SPRITE_EXPL);

        unsigned long death_stop = HAL_GetTick() + DEATH_HOLD;
        unsigned long flash_next = HAL_GetTick() + DEATH_FLASH;
        uint8_t       flash_on   = 1;

        while (HAL_GetTick() < death_stop) {
            if (HAL_GetTick() > flash_next) {
                flash_next = HAL_GetTick() + DEATH_FLASH;
                flash_on   = !flash_on;
                Lcd_cursor(&lcd, heli_row, 0);
                Lcd_write_char(&lcd, flash_on ? (uint8_t)SPRITE_EXPL : 0x20);
                Lcd_write_char(&lcd, flash_on ? (uint8_t)SPRITE_EXPL : 0x20);
            }
        }

        /* Update high score if beaten */
        if (score > highScore) {
            highScore     = score;
            new_highscore = 1;
        }

        set_game_mode(GAME_OVER);
    } else {
        /* Draw animated helicopter (tail + body) */
        Lcd_write_char(&lcd, (uint8_t)anim_cur(&anim_tail));
        Lcd_write_char(&lcd, (uint8_t)anim_cur(&anim_body));
    }
}

/* ════════════════════════════════════════════════════════════════
 * GAME OVER screen
 * Shows final score and new-high-score message if applicable.
 * Any button press returns to the home screen.
 * ════════════════════════════════════════════════════════════════ */
static void game_over(uint8_t update)
{
    if (first) {
        first = 0;
        Lcd_clear(&lcd);
        loadSprites();
    }

    read_button();

    /* Any press → return to home screen */
    if (!button_state) {
        set_game_mode(GAME_HOME);
        return;
    }

    if (update) {
        Lcd_cursor(&lcd, 0, 0);
        if (new_highscore)
            Lcd_string(&lcd, "New High Score!");
        else
            Lcd_string(&lcd, "Game Over       ");

        Lcd_cursor(&lcd, 1, 0);
        Lcd_string(&lcd, "Score: ");
        Lcd_int(&lcd, (int)score);
        Lcd_string(&lcd, "        ");   /* clear any leftover digits */
    }
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

    /* Build LCD handle and load custom sprites */
    lcd = lcd_build();
    loadSprites();

    /* Seed LCG with SysTick value for varied randomness */
    rng_state ^= HAL_GetTick() * 1664525u + 1013904223u;

    set_game_mode(GAME_HOME);

    while (1)
    {
        uint8_t update = 0;
        unsigned long now = HAL_GetTick();

        /* Advance animation frame every FRAME_RATE ms */
        if (now > frame_next) {
            anim_next(&anim_tail);
            anim_next(&anim_body);
            frame_next = now + FRAME_RATE;
            update = 1;   /* only re-render on new animation frame */
        }

        read_button();

        /* Dispatch to active game state handler */
        switch (game_mode) {
            case GAME_HOME: game_home(update); break;
            case GAME_PLAY: game_play(update); break;
            case GAME_OVER: game_over(update); break;
            default: set_game_mode(GAME_HOME); break;
        }
    }
}

/* ════════════════════════════════════════════════════════════════
 * Peripheral initialisation
 * ════════════════════════════════════════════════════════════════ */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

    /* Use internal HSI oscillator, no PLL */
    RCC_OscInitStruct.OscillatorType      = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState            = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState        = RCC_PLL_NONE;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) Error_Handler();

    /* All bus clocks derived directly from HSI (no dividers) */
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

    /* Enable GPIO clocks */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    /* Set LCD data/control pins to known low state */
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6|GPIO_PIN_7, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4|GPIO_PIN_5|GPIO_PIN_6, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_7, GPIO_PIN_RESET);

    /* PA0 – button input with internal pull-down; HIGH = pressed */
    GPIO_InitStruct.Pin  = GPIO_PIN_0;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLDOWN;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* PA6=D7, PA7=D6 – LCD data bits */
    GPIO_InitStruct.Pin   = GPIO_PIN_6|GPIO_PIN_7;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* PB4=EN, PB5=RS, PB6=D5 – LCD control and data */
    GPIO_InitStruct.Pin   = GPIO_PIN_4|GPIO_PIN_5|GPIO_PIN_6;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* PC7=D4 – LCD data bit */
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
