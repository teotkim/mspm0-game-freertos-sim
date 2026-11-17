/*
 * FreeRTOS-Tutorials
 * Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * SPDX-License-Identifier: MIT-0
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this
 * software and associated documentation files (the "Software"), to deal in the Software
 * without restriction, including without limitation the rights to use, copy, modify,
 * merge, publish, distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
 * PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * https://www.FreeRTOS.org
 * https://github.com/FreeRTOS
 *
 */

/* Standard includes. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

enum { STATE_BLACK = 2, STATE_OVERWORLD = 4, STATE_BATTLE = 5 };
enum { BUTTON_RIGHT = 1, BUTTON_DOWN = 2, BUTTON_LEFT = 4, BUTTON_UP = 8 };

typedef struct {
    uint32_t pot;
    uint32_t buttons;
} GameInput;

typedef struct {
    int screen, x, y, language, language_lock;
    int green_x, green_dir, green_alive, green_follower;
    int enemy_hp, player_points, player_hp;
    int selected_attack, battle_turn, battle_delay, battle_lock;
    int player_turn_count;
} GameState;

static QueueHandle_t input_queue;

static void start_green_battle(GameState *game)
{
    game->screen = STATE_BATTLE;
    game->enemy_hp = 15;
    game->player_points = 6;
    game->player_hp = 20;
    game->selected_attack = 0;
    game->battle_turn = 0;
    game->battle_delay = 0;
    game->battle_lock = 0;
    game->player_turn_count = 0;
}

static void game_step(GameState *game, GameInput input)
{
    if (game->screen == STATE_BLACK) {
        game->language = input.pot < 2048 ? 0 : 1;
        if (input.buttons == 0)
            game->language_lock = 0;

        if (!game->language_lock && input.buttons == BUTTON_UP) {
            game->language_lock = 1;
            game->screen = STATE_OVERWORLD;
        }

    } else if (game->screen == STATE_OVERWORLD) {
        if (input.buttons == BUTTON_LEFT)       game->x -= 5;
        else if (input.buttons == BUTTON_UP)    game->y -= 5;
        else if (input.buttons == BUTTON_RIGHT) game->x += 5;
        else if (input.buttons == BUTTON_DOWN)  game->y += 5;

        if (game->x < 0) game->x = 0;
        if (game->x > 128 - 24) game->x = 128 - 24;
        if (game->y < 24 - 1) game->y = 24 - 1;
        if (game->y > 159) game->y = 159;

        /* Green enemy's patrol from the original game. */
        if (game->green_alive && !game->green_follower) {
            game->green_x += game->green_dir * 2;
            if (game->green_x <= 84) {
                game->green_x = 84;
                game->green_dir = 1;
            }
            if (game->green_x >= 100) {
                game->green_x = 100;
                game->green_dir = -1;
            }

            /* Check overlap of the 24x24 astronaut and 16x16 enemy. */
            if (game->x <= game->green_x + 15 &&
                game->x + 23 >= game->green_x &&
                game->y - 23 <= 140 &&
                game->y >= 125) {
                start_green_battle(game);
            }
        }

    } else if (game->screen == STATE_BATTLE) {
        game->selected_attack = input.pot < 2048 ? 0 : 1;
        if (input.buttons == 0)
            game->battle_lock = 0;

        if (game->battle_turn == 0 &&
            !game->battle_lock && input.buttons == BUTTON_UP) {
            game->battle_lock = 1;

            int special_ready = (game->player_turn_count % 2) == 1;
            int cost = game->selected_attack == 0 ? 4 :
                       special_ready ? 7 : 6;
            int damage = game->selected_attack == 0 ? 2 :
                         special_ready ? 8 : 4;

            if (game->player_points >= cost) {
                game->player_points -= cost;
                game->enemy_hp -= damage;
                if (game->enemy_hp < 0)
                    game->enemy_hp = 0;
                game->player_turn_count++;
            }

            if (game->enemy_hp == 0) {
                game->green_follower = 1;
                game->screen = STATE_OVERWORLD;
            } else {
                game->battle_turn = 1;
                game->battle_delay = 30;
            }

        } else if (game->battle_turn == 1) {
            if (game->battle_delay > 0) {
                game->battle_delay--;
            } else {
                /* Repeatable substitute for the original 3 + Random(3). */
                game->player_hp -= 3;
                if (game->player_hp < 0)
                    game->player_hp = 0;

                game->player_points += 5;
                if (game->player_points > 20)
                    game->player_points = 20;

                game->battle_turn = 0;
            }
        }
    }
}

static void send_frame(TickType_t *next_tick, uint32_t buttons)
{
    GameInput input = {1500, buttons};

    if (xQueueSend(input_queue, &input, portMAX_DELAY) != pdPASS)
        exit(1);

    vTaskDelayUntil(next_tick, pdMS_TO_TICKS(33));
}

static void input_task(void *unused)
{
    (void)unused;
    TickType_t next_tick = xTaskGetTickCount();

    send_frame(&next_tick, 0);          /* language screen */
    send_frame(&next_tick, BUTTON_UP);  /* enter overworld */

    for (int i = 0; i < 11; ++i)
        send_frame(&next_tick, BUTTON_RIGHT);

    send_frame(&next_tick, BUTTON_UP);  /* first attack */
    send_frame(&next_tick, BUTTON_UP);  /* held: no second attack */

    for (int i = 0; i < 30; ++i)
        send_frame(&next_tick, 0);      /* wait through enemy turn */

    vTaskDelete(NULL);
}

static void game_task(void *unused)
{
    (void)unused;
    GameState game = {
        .screen = STATE_BLACK,
        .x = 12,
        .y = 148,
        .green_x = 88,
        .green_dir = 1,
        .green_alive = 1
    };
    GameInput input;

    for (int frame = 0; frame < 45; ++frame) {
        if (xQueueReceive(input_queue, &input, portMAX_DELAY) != pdPASS)
            exit(1);

        game_step(&game, input);

        if (frame < 15 || frame >= 43) {
            printf("frame=%d state=%d x=%d y=%d green=%d "
                   "enemyHP=%d playerHP=%d points=%d delay=%d\n",
                   frame, game.screen, game.x, game.y, game.green_x,
                   game.enemy_hp, game.player_hp, game.player_points,
                   game.battle_delay);
        }
    }

    if (game.screen != STATE_BATTLE ||
        game.x != 67 ||
        game.green_x != 90 ||
        game.enemy_hp != 13 ||
        game.player_hp != 17 ||
        game.player_points != 7 ||
        game.player_turn_count != 1 ||
        game.battle_turn != 0 ||
        game.battle_delay != 0) {
        fputs("FAIL: battle result differed from expected result\n", stderr);
        exit(1);
    }

    puts("PASS: collision, button lock, and delayed enemy turn");
    exit(0);
}

int main(void)
{
    input_queue = xQueueCreate(4, sizeof(GameInput));

    if (input_queue == NULL ||
        xTaskCreate(input_task, "Input", 1024, NULL, 1, NULL) != pdPASS ||
        xTaskCreate(game_task, "Game", 1024, NULL, 2, NULL) != pdPASS) {
        fputs("Failed to create FreeRTOS queue or tasks\n", stderr);
        return 1;
    }

    vTaskStartScheduler();
    return 1;
}