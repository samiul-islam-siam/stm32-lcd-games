# Helicopter Game 🚁
A Helicopter-style obstacle avoidance game running on a 16x2 LCD, built with an STM32 Nucleo board using STM32CubeMX and STM32CubeIDE.


## Features

- Animated helicopter sprites
- Random obstacle generation
- Progressive difficulty system
- High score tracking during runtime
- Home screen, gameplay, and game over states
- Collision detection with explosion animation

## Hardware

- STM32 Nucleo board
- LCD 1602 (HD44780 compatible, 16x2)
- Push button (4 pin)
- 10kΩ potentiometer (LCD contrast)
- 220Ω resistor (LCD backlight)
- Breadboard and jumper wires

## Circuit Connections

### LCD 1602

| LCD Pin | LCD Signal | STM32 Pin                  |
|---------|------------|----------------------------|
| 1       | VSS        | GND                        |
| 2       | VDD        | 5V                         |
| 3       | VO         | Potentiometer middle pin   |
| 4       | RS         | PB5 (D4)                   |
| 5       | RW         | GND                        |
| 6       | EN         | PB4 (D5)                   |
| 7–10    | D0–D3      | Not connected (4-bit mode) |
| 11      | D4         | PC7 (D9)                   |
| 12      | D5         | PB6 (D10)                  |
| 13      | D6         | PA7 (D11)                  |
| 14      | D7         | PA6 (D12)                  |
| 15      | A (BL+)    | 3.3V                       |
| 16      | K (BL−)    | GND                        |

> All LCD GPIO pins are configured as output mode.

> Connect VO directly to GND if a potentiometer is unavailable.

### Push Button

| Button Pin | Connect To |
|------------|------------|
| One side   | PA0        |
| Other side | 3.3V       |

> PA0 is configured as input with internal pull-down.

> Button reads HIGH when pressed.

### Potentiometer (Contrast)

| Pot Pin | Connect To     |
|---------|----------------|
| Left    | GND            |
| Middle  | LCD Pin 3 (VO) |
| Right   | 3.3V           |

## How to Play

- Hold the button to fly upward
- Release the button to fall downward
- Avoid incoming walls and obstacles
- Score increases as you survive longer
- Game speed gradually increases over time
- Collision ends the game
- Press the button on the Game Over screen to return to the menu and play again

### Controls

| Action    | Button State   |
|-----------|----------------|
| Fly Up    | Hold Button    |
| Fall Down | Release Button |

### Difficulty Tuning

Edit these constants in `main.c` to adjust gameplay difficulty:

| Constant          | Default | Effect                     |
|-------------------|---------|----------------------------|
| `MAX_WALL_RATE`   | 350 ms  | Initial obstacle speed     |
| `MIN_WALL_RATE`   | 140 ms  | Maximum game speed         |
| `FRAME_RATE`      | 125 ms  | Helicopter animation speed |
| `DEATH_HOLD`      | 1500 ms | Death screen duration      |
| `DEATH_FLASH`     | 150 ms  | Explosion animation speed  |

### High Score

- High score is stored in RAM during runtime
- High score resets when power is removed or the board is reset

