#ifndef __BATTLE_CORE_H_
#define __BATTLE_CORE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define BATTLE_WORLD_COLS      80
#define BATTLE_WORLD_ROWS      80
#define BATTLE_PLAYER_COUNT    5
#define BATTLE_AI_COUNT        4
#define BATTLE_MAX_SNAKE_LEN   512
#define BATTLE_MAX_PELLETS     192
#define BATTLE_TARGET_PELLETS  128
#define BATTLE_MATCH_MS        0UL
#define BATTLE_RESPAWN_MS      2000U
#define BATTLE_NORMAL_STEP_MS  95U
#define BATTLE_BOOST_STEP_MS   55U
#define BATTLE_MIN_BOOST_LEN   7U
#define BATTLE_BOOST_COST_MOVES 4U

typedef enum {
    BATTLE_DIR_UP = 0,
    BATTLE_DIR_DOWN,
    BATTLE_DIR_LEFT,
    BATTLE_DIR_RIGHT
} BattleDir;

typedef enum {
    BATTLE_PELLET_NORMAL = 0,
    BATTLE_PELLET_CORPSE
} BattlePelletType;

typedef enum {
    BATTLE_EVENT_NONE       = 0x0000,
    BATTLE_EVENT_EAT_P1     = 0x0001,
    BATTLE_EVENT_EAT_P2     = 0x0002,
    BATTLE_EVENT_DEATH_P1   = 0x0004,
    BATTLE_EVENT_DEATH_P2   = 0x0008,
    BATTLE_EVENT_RESPAWN_P1 = 0x0010,
    BATTLE_EVENT_RESPAWN_P2 = 0x0020,
    BATTLE_EVENT_MATCH_END  = 0x0040,
    BATTLE_EVENT_BOOST_P1   = 0x0080,
    BATTLE_EVENT_BOOST_P2   = 0x0100
} BattleEvent;

typedef struct {
    uint8_t x;
    uint8_t y;
    uint8_t active;
    uint8_t value;
    uint8_t color;
    BattlePelletType type;
} BattlePellet;

typedef struct {
    uint8_t x[BATTLE_MAX_SNAKE_LEN];
    uint8_t y[BATTLE_MAX_SNAKE_LEN];
    uint16_t len;
    uint16_t score;
    uint16_t move_acc_ms;
    uint16_t respawn_ms;
    uint8_t alive;
    uint8_t boost_on;
    uint8_t boost_cost_counter;
    BattleDir dir;
    BattleDir next_dir;
} BattleSnake;

typedef struct {
    BattleSnake snakes[BATTLE_PLAYER_COUNT];
    BattlePellet pellets[BATTLE_MAX_PELLETS];
    uint32_t rng_state;
    uint32_t elapsed_ms;
    uint16_t last_events;
    uint8_t match_over;
    uint8_t winner; /* 0: none/running, 1: P1, 2: P2, 3: draw */
} BattleState;

typedef struct {
    uint8_t set_dir[BATTLE_PLAYER_COUNT];
    BattleDir dir[BATTLE_PLAYER_COUNT];
    uint8_t boost[BATTLE_PLAYER_COUNT];
} BattleInput;

void Battle_Init(BattleState *state, uint32_t seed);
void Battle_ResetInput(BattleInput *input);
void Battle_Update(BattleState *state, const BattleInput *input, uint16_t dt_ms);
uint8_t Battle_IsReverse(BattleDir from, BattleDir to);
uint16_t Battle_ActivePelletCount(const BattleState *state);

#ifdef __cplusplus
}
#endif

#endif
