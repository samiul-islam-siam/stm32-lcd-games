# Jumping Jack 🏃
A side-scrolling endless runner game inspired by classic arcade runners, built for a 16x2 LCD using an STM32 Nucleo-F446RE board with STM32CubeMX and STM32CubeIDE.

## How to Play

- Press the button to jump over obstacles
- Time your jumps carefully to avoid collisions
- Survive as long as possible to increase your score
- Score is displayed on the top-right corner of the LCD
- Collision with any obstacle ends the game
- Press the button on the Game Over screen to restart
<!--
## Features

- Animated running and jumping character
- Randomly generated obstacles
- Upper and lower terrain obstacles
- Real-time score tracking
- Start screen and Game Over screen
- Collision detection system
- Smooth scrolling terrain animation
-->
## Hardware

- STM32 Nucleo-F446RE board
- LCD 1602 (HD44780 compatible, 16x2)
- Push button (4 pin)
- 10kΩ potentiometer (LCD contrast)
- 220Ω resistor (LCD backlight)
- Breadboard and jumper wires

## Circuit Connections

### LCD 1602

| LCD Pin | LCD Signal | STM32 Pin                  |
|----------|------------|----------------------------|
| 1         | VSS        | GND                        |
| 2         | VDD        | 5V                         |
| 3         | VO         | Potentiometer middle pin   |
| 4         | RS         | PB5                        |
| 5         | RW         | GND                        |
| 6         | EN         | PB4                        |
| 7–10      | D0–D3      | Not connected (4-bit mode) |
| 11        | D4         | PC7                        |
| 12        | D5         | PB6                        |
| 13        | D6         | PA7                        |
| 14        | D7         | PA6                        |
| 15        | A (BL+)    | 3.3V                       |
| 16        | K (BL−)    | GND                        |

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

## Controls

| Action | Button |
|---------|--------|
| Jump | Press |
| Restart After Game Over | Press |
<!--
## Game Screens

### Start Screen

Displays:

```
JUMPING JACK
  BTN:>
```

- Waits for a button press to begin the game. -->

### Gameplay

- Character continuously runs forward.
- Obstacles scroll from right to left.
- Random ground and upper obstacles are generated.
- Score increases as distance traveled increases.
- Jump over obstacles to survive.
<!--
### Game Over Screen

Displays:

```
GAME OVER
Score: XXXX
BTN:>
```

- Shows final score.
- Waits for a button press to restart.
-->
## Game Mechanics

### Character States

- Running Animation (2 frames)
- Jump Start
- Mid-Air Jump
- Landing
- Upper Platform Running

### Obstacle Types

- Lower terrain blocks
- Upper terrain blocks
- Random empty gaps between obstacle groups

### Scoring

- Score increases based on distance traveled.
- Longer survival results in a higher score.

## Difficulty Tuning

Edit these values in `main.c` to modify gameplay:

| Parameter | Location | Effect |
|------------|----------|---------|
| `HAL_Delay(100)` | Main loop | Game speed |
| `newTerrainDuration` | Terrain generation | Obstacle spacing |
| `rng(3)` | Obstacle selection | Upper/lower obstacle frequency |
<!--
## Development Environment

- STM32CubeMX
- STM32CubeIDE
- STM32 HAL Drivers
- STM32 Nucleo-F446RE
-->
