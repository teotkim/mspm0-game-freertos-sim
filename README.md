# MSPM0 Game — FreeRTOS Host Simulation

A Linux FreeRTOS POSIX simulation of selected logic from an MSPM0G3507 game originally developed by Teo Kim and Kell Harpole.

## What it runs

An input task sends scripted button and potentiometer readings through a FreeRTOS queue at nominal 33 ms intervals. A game task receives them and updates the language menu, astronaut position, moving green enemy, collision, and first battle turn. The program checks the resulting game state and exits with PASS or FAIL.

## Build and run

From the repository root:

```sh
git submodule update --init --recursive
cmake -S . -B build
cmake --build build
./build/game_sim
Scope
This is a host-side simulation of part of the game. It does not build the original LCD, ADC, sound, or other board drivers, and it does not demonstrate FreeRTOS running on MSPM0 hardware. Enemy damage uses a fixed value so the test is repeatable.
