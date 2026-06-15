#include "battle_core.h"

#include <string.h>

#define BATTLE_NO_PELLET 0xffU

static uint16_t Battle_Rand(BattleState *state, uint16_t max)
{
    state->rng_state = state->rng_state * 1664525UL + 1013904223UL;
    if (max == 0) {
        return 0;
    }
    return (uint16_t)((state->rng_state >> 16) % max);
}

uint8_t Battle_IsReverse(BattleDir from, BattleDir to)
{
    return (uint8_t)((from == BATTLE_DIR_UP && to == BATTLE_DIR_DOWN) ||
                     (from == BATTLE_DIR_DOWN && to == BATTLE_DIR_UP) ||
                     (from == BATTLE_DIR_LEFT && to == BATTLE_DIR_RIGHT) ||
                     (from == BATTLE_DIR_RIGHT && to == BATTLE_DIR_LEFT));
}

static void Battle_Advance(BattleDir dir, int16_t *x, int16_t *y)
{
    if (dir == BATTLE_DIR_UP) {
        (*y)--;
    } else if (dir == BATTLE_DIR_DOWN) {
        (*y)++;
    } else if (dir == BATTLE_DIR_LEFT) {
        (*x)--;
    } else {
        (*x)++;
    }
}

static uint8_t Battle_InWorld(int16_t x, int16_t y)
{
    return (uint8_t)(x >= 0 && x < BATTLE_WORLD_COLS &&
                     y >= 0 && y < BATTLE_WORLD_ROWS);
}

static void Battle_SetSnake(BattleSnake *snake, uint8_t head_x,
                            uint8_t head_y, BattleDir dir)
{
    uint16_t i;
    uint16_t score = snake->score;

    memset(snake, 0, sizeof(*snake));
    snake->len = 6;
    snake->score = score;
    snake->alive = 1;
    snake->dir = dir;
    snake->next_dir = dir;

    for (i = 0; i < snake->len; i++) {
        snake->y[i] = head_y;
        if (dir == BATTLE_DIR_RIGHT) {
            snake->x[i] = (uint8_t)(head_x - i);
        } else if (dir == BATTLE_DIR_LEFT) {
            snake->x[i] = (uint8_t)(head_x + i);
        } else if (dir == BATTLE_DIR_DOWN) {
            snake->x[i] = head_x;
            snake->y[i] = (uint8_t)(head_y - i);
        } else {
            snake->x[i] = head_x;
            snake->y[i] = (uint8_t)(head_y + i);
        }
    }
}

static uint8_t Battle_SnakeHasCell(const BattleSnake *snake, uint8_t x,
                                   uint8_t y, uint16_t limit)
{
    uint16_t i;

    if (!snake->alive) {
        return 0;
    }
    if (limit > snake->len) {
        limit = snake->len;
    }

    for (i = 0; i < limit; i++) {
        if (snake->x[i] == x && snake->y[i] == y) {
            return 1;
        }
    }
    return 0;
}

static uint8_t Battle_PelletAt(const BattleState *state, uint8_t x, uint8_t y)
{
    uint8_t i;

    for (i = 0; i < BATTLE_MAX_PELLETS; i++) {
        if (state->pellets[i].active &&
            state->pellets[i].x == x && state->pellets[i].y == y) {
            return i;
        }
    }
    return BATTLE_NO_PELLET;
}

static uint8_t Battle_CellFree(const BattleState *state, uint8_t x, uint8_t y)
{
    if (Battle_SnakeHasCell(&state->snakes[0], x, y, state->snakes[0].len) ||
        Battle_SnakeHasCell(&state->snakes[1], x, y, state->snakes[1].len)) {
        return 0;
    }
    return (uint8_t)(Battle_PelletAt(state, x, y) == BATTLE_NO_PELLET);
}

static uint8_t Battle_FindFreePelletSlot(BattleState *state)
{
    uint8_t i;

    for (i = 0; i < BATTLE_MAX_PELLETS; i++) {
        if (!state->pellets[i].active) {
            return i;
        }
    }
    return (uint8_t)Battle_Rand(state, BATTLE_MAX_PELLETS);
}

static uint8_t Battle_PlacePellet(BattleState *state, BattlePelletType type,
                                  uint8_t value, uint8_t x, uint8_t y)
{
    uint8_t slot = Battle_FindFreePelletSlot(state);

    state->pellets[slot].active = 1;
    state->pellets[slot].x = x;
    state->pellets[slot].y = y;
    state->pellets[slot].type = type;
    state->pellets[slot].value = value;
    state->pellets[slot].color = (uint8_t)Battle_Rand(state, 6);
    return slot;
}

static void Battle_PlaceRandomPellet(BattleState *state)
{
    uint16_t tries;
    uint8_t x;
    uint8_t y;

    for (tries = 0; tries < 500; tries++) {
        x = (uint8_t)Battle_Rand(state, BATTLE_WORLD_COLS);
        y = (uint8_t)Battle_Rand(state, BATTLE_WORLD_ROWS);
        if (Battle_CellFree(state, x, y)) {
            Battle_PlacePellet(state, BATTLE_PELLET_NORMAL, 1, x, y);
            return;
        }
    }
}

uint16_t Battle_ActivePelletCount(const BattleState *state)
{
    uint8_t i;
    uint16_t count = 0;

    for (i = 0; i < BATTLE_MAX_PELLETS; i++) {
        if (state->pellets[i].active) {
            count++;
        }
    }
    return count;
}

static void Battle_MaintainPellets(BattleState *state)
{
    uint8_t attempts = 0;

    while (Battle_ActivePelletCount(state) < BATTLE_TARGET_PELLETS &&
           attempts < BATTLE_MAX_PELLETS) {
        Battle_PlaceRandomPellet(state);
        attempts++;
    }
}

void Battle_ResetInput(BattleInput *input)
{
    memset(input, 0, sizeof(*input));
}

void Battle_Init(BattleState *state, uint32_t seed)
{
    uint8_t i;
    static const uint8_t start_x[BATTLE_PLAYER_COUNT] = {14, 34, 52, 22, 64};
    static const uint8_t start_y[BATTLE_PLAYER_COUNT] = {40, 40, 22, 62, 58};

    memset(state, 0, sizeof(*state));
    state->rng_state = (seed == 0) ? 0x13572468UL : seed;
    for (i = 0; i < BATTLE_PLAYER_COUNT; i++) {
        Battle_SetSnake(&state->snakes[i], start_x[i], start_y[i],
                        (i & 1U) ? BATTLE_DIR_LEFT : BATTLE_DIR_RIGHT);
    }
    Battle_MaintainPellets(state);
}

static void Battle_DropSnakePellets(BattleState *state, uint8_t player)
{
    BattleSnake *snake = &state->snakes[player];
    uint16_t i;

    for (i = 0; i < snake->len; i += 2) {
        Battle_PlacePellet(state, BATTLE_PELLET_CORPSE, 2,
                           snake->x[i], snake->y[i]);
    }
}

static void Battle_KillSnake(BattleState *state, uint8_t player)
{
    BattleSnake *snake = &state->snakes[player];

    if (!snake->alive) {
        return;
    }

    Battle_DropSnakePellets(state, player);
    snake->alive = 0;
    snake->len = 0;
    snake->respawn_ms = BATTLE_RESPAWN_MS;
    snake->move_acc_ms = 0;
    snake->boost_on = 0;
    state->last_events |= (player == 0) ? BATTLE_EVENT_DEATH_P1 :
                                         BATTLE_EVENT_DEATH_P2;
}

static uint8_t Battle_RespawnCellSafe(const BattleState *state, uint8_t x,
                                      uint8_t y)
{
    uint8_t px;
    uint8_t py;

    for (py = (uint8_t)(y > 3 ? y - 3 : 0);
         py <= y + 3 && py < BATTLE_WORLD_ROWS; py++) {
        for (px = (uint8_t)(x > 3 ? x - 3 : 0);
             px <= x + 3 && px < BATTLE_WORLD_COLS; px++) {
            if (!Battle_CellFree(state, px, py)) {
                return 0;
            }
        }
    }
    return 1;
}

static void Battle_TryRespawn(BattleState *state, uint8_t player)
{
    uint16_t tries;
    uint8_t x;
    uint8_t y;
    BattleDir dir;

    for (tries = 0; tries < 300; tries++) {
        x = (uint8_t)(4 + Battle_Rand(state, BATTLE_WORLD_COLS - 8));
        y = (uint8_t)(4 + Battle_Rand(state, BATTLE_WORLD_ROWS - 8));
        if (Battle_RespawnCellSafe(state, x, y)) {
            dir = (player == 0) ? BATTLE_DIR_RIGHT : BATTLE_DIR_LEFT;
            Battle_SetSnake(&state->snakes[player], x, y, dir);
            state->last_events |= (player == 0) ? BATTLE_EVENT_RESPAWN_P1 :
                                                 BATTLE_EVENT_RESPAWN_P2;
            return;
        }
    }
}

static void Battle_SetWinner(BattleState *state)
{
    if (state->snakes[0].score > state->snakes[1].score) {
        state->winner = 1;
    } else if (state->snakes[1].score > state->snakes[0].score) {
        state->winner = 2;
    } else {
        state->winner = 3;
    }
}

static uint8_t Battle_WillEat(const BattleState *state, int16_t x, int16_t y)
{
    if (!Battle_InWorld(x, y)) {
        return BATTLE_NO_PELLET;
    }
    return Battle_PelletAt(state, (uint8_t)x, (uint8_t)y);
}

static uint8_t Battle_HitsOtherBody(const BattleSnake *other, uint8_t x,
                                    uint8_t y, uint8_t other_moving,
                                    uint8_t other_growing)
{
    uint16_t limit = other->len;

    if (!other->alive || other->len == 0) {
        return 0;
    }
    if (other_moving && !other_growing && limit > 0) {
        limit--;
    }
    return Battle_SnakeHasCell(other, x, y, limit);
}

static void Battle_MoveSnake(BattleSnake *snake, uint8_t nx, uint8_t ny,
                             uint16_t new_len)
{
    uint16_t old_len = snake->len;
    uint8_t tail_x = snake->x[old_len - 1];
    uint8_t tail_y = snake->y[old_len - 1];
    int16_t i;

    if (new_len > BATTLE_MAX_SNAKE_LEN) {
        new_len = BATTLE_MAX_SNAKE_LEN;
    }
    if (new_len < 1) {
        new_len = 1;
    }

    for (i = (int16_t)new_len - 1; i > 0; i--) {
        if ((uint16_t)(i - 1) < old_len) {
            snake->x[i] = snake->x[i - 1];
            snake->y[i] = snake->y[i - 1];
        } else {
            snake->x[i] = tail_x;
            snake->y[i] = tail_y;
        }
    }

    snake->x[0] = nx;
    snake->y[0] = ny;
    snake->len = new_len;
}

static uint8_t Battle_BoostCost(BattleState *state, uint8_t player,
                                uint8_t old_tail_x, uint8_t old_tail_y)
{
    BattleSnake *snake = &state->snakes[player];

    if (!snake->boost_on || snake->len <= BATTLE_MIN_BOOST_LEN) {
        snake->boost_cost_counter = 0;
        return 0;
    }

    snake->boost_cost_counter++;
    if (snake->boost_cost_counter < BATTLE_BOOST_COST_MOVES) {
        return 0;
    }

    snake->boost_cost_counter = 0;
    Battle_PlacePellet(state, BATTLE_PELLET_NORMAL, 1, old_tail_x, old_tail_y);
    state->last_events |= (player == 0) ? BATTLE_EVENT_BOOST_P1 :
                                         BATTLE_EVENT_BOOST_P2;
    return 1;
}

static void Battle_ApplyMove(BattleState *state, uint8_t player, int16_t nx,
                             int16_t ny, uint8_t pellet_index)
{
    BattleSnake *snake = &state->snakes[player];
    uint16_t new_len = snake->len;
    uint8_t old_tail_x = snake->x[snake->len - 1];
    uint8_t old_tail_y = snake->y[snake->len - 1];
    uint8_t cost;

    if (pellet_index != BATTLE_NO_PELLET) {
        BattlePellet *pellet = &state->pellets[pellet_index];
        new_len = (uint16_t)(new_len + pellet->value);
        snake->score = (uint16_t)(snake->score + pellet->value);
        pellet->active = 0;
        state->last_events |= (player == 0) ? BATTLE_EVENT_EAT_P1 :
                                             BATTLE_EVENT_EAT_P2;
    }

    cost = Battle_BoostCost(state, player, old_tail_x, old_tail_y);
    if (cost && new_len > BATTLE_MIN_BOOST_LEN) {
        new_len--;
    }

    Battle_MoveSnake(snake, (uint8_t)nx, (uint8_t)ny, new_len);
}

static void Battle_Step(BattleState *state, const uint8_t *move)
{
    int16_t nx[BATTLE_PLAYER_COUNT];
    int16_t ny[BATTLE_PLAYER_COUNT];
    uint8_t pellet[BATTLE_PLAYER_COUNT];
    uint8_t grow[BATTLE_PLAYER_COUNT];
    uint8_t dead[BATTLE_PLAYER_COUNT];
    uint8_t consumed[BATTLE_MAX_PELLETS];
    uint8_t i;
    uint8_t j;

    memset(dead, 0, sizeof(dead));
    memset(grow, 0, sizeof(grow));
    memset(consumed, 0, sizeof(consumed));

    for (i = 0; i < BATTLE_PLAYER_COUNT; i++) {
        BattleSnake *snake = &state->snakes[i];
        nx[i] = snake->alive ? snake->x[0] : 0;
        ny[i] = snake->alive ? snake->y[0] : 0;
        pellet[i] = BATTLE_NO_PELLET;
        if (move[i] && snake->alive) {
            snake->dir = snake->next_dir;
            Battle_Advance(snake->dir, &nx[i], &ny[i]);
            pellet[i] = Battle_WillEat(state, nx[i], ny[i]);
            grow[i] = (uint8_t)(pellet[i] != BATTLE_NO_PELLET);
            if (!Battle_InWorld(nx[i], ny[i])) {
                dead[i] = 1;
            }
        }
    }

    for (i = 0; i < BATTLE_PLAYER_COUNT; i++) {
        if (!move[i] || dead[i] || !state->snakes[i].alive) {
            continue;
        }
        for (j = 0; j < BATTLE_PLAYER_COUNT; j++) {
            if (i == j) {
                continue;
            }
            if (Battle_HitsOtherBody(&state->snakes[j], (uint8_t)nx[i],
                                     (uint8_t)ny[i], move[j], grow[j])) {
                dead[i] = 1;
                break;
            }
        }
    }

    for (i = 0; i < BATTLE_PLAYER_COUNT; i++) {
        if (!move[i] || dead[i] || !state->snakes[i].alive) {
            continue;
        }
        for (j = (uint8_t)(i + 1); j < BATTLE_PLAYER_COUNT; j++) {
            if (move[j] && !dead[j] && state->snakes[j].alive &&
                nx[i] == nx[j] && ny[i] == ny[j]) {
                dead[i] = 1;
                dead[j] = 1;
            }
        }
    }

    for (i = 0; i < BATTLE_PLAYER_COUNT; i++) {
        if (dead[i]) {
            Battle_KillSnake(state, i);
        }
    }

    for (i = 0; i < BATTLE_PLAYER_COUNT; i++) {
        if (move[i] && !dead[i] && state->snakes[i].alive) {
            if (pellet[i] != BATTLE_NO_PELLET) {
                if (consumed[pellet[i]]) {
                    pellet[i] = BATTLE_NO_PELLET;
                } else {
                    consumed[pellet[i]] = 1;
                }
            }
            Battle_ApplyMove(state, i, nx[i], ny[i], pellet[i]);
        }
    }

    Battle_MaintainPellets(state);
}

void Battle_Update(BattleState *state, const BattleInput *input, uint16_t dt_ms)
{
    uint8_t i;
    uint8_t move[BATTLE_PLAYER_COUNT];
    uint8_t any_move;
    BattleSnake *snake;
    uint16_t interval;

    state->last_events = BATTLE_EVENT_NONE;

    if (state->match_over) {
        return;
    }

    state->elapsed_ms += dt_ms;
#if BATTLE_MATCH_MS != 0
    if (state->elapsed_ms >= BATTLE_MATCH_MS) {
        state->match_over = 1;
        state->elapsed_ms = BATTLE_MATCH_MS;
        Battle_SetWinner(state);
        state->last_events |= BATTLE_EVENT_MATCH_END;
        return;
    }
#endif

    for (i = 0; i < BATTLE_PLAYER_COUNT; i++) {
        snake = &state->snakes[i];
        if (!snake->alive) {
            if (snake->respawn_ms > dt_ms) {
                snake->respawn_ms = (uint16_t)(snake->respawn_ms - dt_ms);
            } else {
                snake->respawn_ms = 0;
                Battle_TryRespawn(state, i);
            }
            continue;
        }

        if (input->set_dir[i] &&
            !Battle_IsReverse(snake->dir, input->dir[i])) {
            snake->next_dir = input->dir[i];
        }
        snake->boost_on = (uint8_t)(input->boost[i] &&
                                    snake->len > BATTLE_MIN_BOOST_LEN);
        snake->move_acc_ms = (uint16_t)(snake->move_acc_ms + dt_ms);
    }

    do {
        any_move = 0;
        for (i = 0; i < BATTLE_PLAYER_COUNT; i++) {
            move[i] = 0;
        }
        for (i = 0; i < BATTLE_PLAYER_COUNT; i++) {
            snake = &state->snakes[i];
            if (!snake->alive) {
                continue;
            }
            interval = snake->boost_on ? BATTLE_BOOST_STEP_MS :
                                         BATTLE_NORMAL_STEP_MS;
            if (snake->move_acc_ms >= interval) {
                snake->move_acc_ms = (uint16_t)(snake->move_acc_ms - interval);
                move[i] = 1;
                any_move = 1;
            }
        }
        if (any_move) {
            Battle_Step(state, move);
        }
    } while (any_move);
}
