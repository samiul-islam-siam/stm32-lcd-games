# Pacman Rider Game
A Pacman-inspired arcade game running on a 16x2 LCD, built with an STM32 Nucleo board using STM32CubeMX and STM32CubeIDE.

## How to Play

- Press the button to move Pacman between the top and bottom lanes
- Collect hearts ❤️ to earn points
- Avoid ghosts 👻 at all costs
- Game speed increases as your score grows
- Collision with a ghost ends the game
- Try to beat your high score

## Features

- Animated Pacman sprite
- Heart collection system
- Random ghost and heart spawning
- Progressive difficulty scaling
- Runtime high score tracking
- Intro, gameplay, and game over screens
- LCD transition animations
- Dual control support (Button + UART keyboard)

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

### UART Control (Optional)

**UART Settings**

- Baud Rate: 9600
- Data Bits: 8
- Stop Bits: 1
- Parity: None

> Any key press in a serial terminal acts as the move button.

### Potentiometer (Contrast)

| Pot Pin  | Connect To     |
|----------|----------------|
| Left     | GND            |
| Middle   | LCD Pin 3 (VO) |
| Right    | 3.3V           |

## Controls

| Action         | Input                   |
|----------------|-------------------------|
| Move Pacman    | Button Press            |
| Move Pacman    | Any UART Key            |
| Continue Menus | Button Press / UART Key |


## Scoring System

| Event           | Points    |
|-----------------|-----------|
| Collect Heart   | +1        |
| Hit Ghost       | Game Over |

## Difficulty Tuning

Edit these variables in `main.c`:

| Variable          | Default         | Effect                  |
|-------------------|-----------------|-------------------------|
| `gameSpeed`       | 600 ms          | Initial movement speed  |
| `ghostOdds`       | 6               | Ghost spawn probability |
| `gameSpeed -= 30` | Every 10 points | Speed increase rate     |
| `ghostOdds--`     | Every 10 points | Difficulty progression  |

## High Score

- High score is stored in RAM only
- High score resets when power is removed
- Can be upgraded to Flash memory or Backup SRAM for persistence

