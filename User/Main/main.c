#include "stm32f10x.h"
#include "hw_config.h"
#include "lcd.h"
#include "snake_uart.h"
#include "battle_core.h"

#define GRID_COLS       15
#define GRID_ROWS       20
#define CELL_W          13
#define CELL_H          12
#define BOARD_X         22
#define BOARD_Y         72
#define BATTLE_FRAME_MS_10FPS 100
#define BATTLE_FRAME_MS_15FPS 66
#define BATTLE_FRAME_MS_30FPS 33
#define BATTLE_FRAME_MS_DEFAULT BATTLE_FRAME_MS_15FPS
#define BATTLE_PAUSE_DEBOUNCE_MS 180
#define BATTLE_INTERP_SCALE 16
#define OPEN_DUO_ROWS   9
#define OPEN_DUO_P1_Y   BOARD_Y
#define OPEN_DUO_P2_Y   204
#define OPEN_WORLD_COLS 40
#define OPEN_WORLD_ROWS 56
#define MAX_SNAKE_LEN   (OPEN_WORLD_COLS * OPEN_WORLD_ROWS)
#define LEVEL_COUNT     5
#define MUSIC_TICK_MS   5
#define MUSIC_COUNT(a)  ((u16)(sizeof(a) / sizeof((a)[0])))

#define NOTE_REST       0
#define NOTE_C3         131
#define NOTE_CS3        139
#define NOTE_D3         147
#define NOTE_DS3        156
#define NOTE_E3         165
#define NOTE_F3         175
#define NOTE_FS3        185
#define NOTE_G3         196
#define NOTE_GS3        208
#define NOTE_A3         220
#define NOTE_AS3        233
#define NOTE_B3         247
#define NOTE_C4         262
#define NOTE_CS4        277
#define NOTE_D4         294
#define NOTE_DS4        311
#define NOTE_E4         330
#define NOTE_F4         349
#define NOTE_FS4        370
#define NOTE_G4         392
#define NOTE_GS4        415
#define NOTE_A4         440
#define NOTE_AS4        466
#define NOTE_B4         494
#define NOTE_C5         523
#define NOTE_CS5        554
#define NOTE_D5         587
#define NOTE_DS5        622
#define NOTE_E5         659
#define NOTE_F5         698
#define NOTE_FS5        740
#define NOTE_G5         784
#define NOTE_GS5        831
#define NOTE_A5         880
#define NOTE_AS5        932
#define NOTE_B5         988
#define NOTE_C6         1047
#define NOTE_CS6        1109
#define NOTE_D6         1175
#define NOTE_DS6        1245
#define NOTE_E6         1319
#define NOTE_F6         1397
#define NOTE_FS6        1480
#define NOTE_G6         1568
#define NOTE_A6         1760

#define KEY_UP_MASK     0x01
#define KEY_DOWN_MASK   0x02
#define KEY_LEFT_MASK   0x04
#define KEY_RIGHT_MASK  0x08
#define KEY_PAUSE_MASK  (KEY_UP_MASK | KEY_DOWN_MASK)

#define KNOB_LEFT_EVENT   1
#define KNOB_RIGHT_EVENT  2
#define KNOB_LOW_TH       1400
#define KNOB_HIGH_TH      2700
#define KNOB_CENTER_LOW   1700
#define KNOB_CENTER_HIGH  2400
#define KNOB_TURN_DELTA   420
#define VOLUME_MAX        5
#define VOLUME_DEFAULT    4
#define AUDIO_WAVE_POINTS 32
#define AUDIO_DAC_CENTER  2047
#define AUDIO_DAC_DHR12R2 ((u32)0x40007414)

#define FOOD_NORMAL     0
#define FOOD_POISON     1
#define FOOD_BONUS      2

#define STEP_ALIVE      1
#define STEP_DEAD       2
#define STEP_LEVEL_DONE 3
#define STEP_WIN        4
#define STEP_SELF_HIT   5
#define STEP_SELF_GAME_OVER 6

#define GAME_MODE_STAGE   0
#define GAME_MODE_CLASSIC 1
#define GAME_MODE_DUO     2
#define GAME_MODE_OPEN    3
#define GAME_MODE_OPEN_DUO 4
#define GAME_MODE_BATTLE  5
#define GAME_MODE_COUNT   6
#define RANKING_TOP_COUNT 5

#define HOME_NAVY       0x000C
#define HOME_BLUE       0x01D1
#define HOME_PANEL      0x1084
#define HOME_TEAL       0x05B8
#define HOME_GOLD       0xFD20
#define BATTLE_BG       0x0883
#define BATTLE_PANEL    0x10C4
#define BATTLE_GRID     0x1126
#define BATTLE_TEXT     0xE7BF
#define BATTLE_MUTED    0x8497

#define KNOB_LEVEL_STEP (4096 / LEVEL_COUNT)
#define KNOB_MODE_STEP  (4096 / GAME_MODE_COUNT)

#define SNAKE_FLASH_PAGE_SIZE   0x800UL
#define SNAKE_FLASH_STORE_ADDR  0x0803F800UL
#define SNAKE_FLASH_MAGIC       0x534E4B50UL
#define SNAKE_FLASH_VERSION     0x0003UL
#define SNAKE_FLASH_XOR_KEY     0xA55A3CC3UL
#define SNAKE_NO_INDEX          0xFFFFU
#define SNAKE_FLASH_KEY1        0x45670123UL
#define SNAKE_FLASH_KEY2        0xCDEF89ABUL
#define SNAKE_FLASH_TIMEOUT     0x000B0000UL

typedef enum {
    DIR_UP = 0,
    DIR_DOWN,
    DIR_LEFT,
    DIR_RIGHT
} SnakeDir;

typedef struct {
    u16 freq;
    u16 ms;
} MusicNote;

typedef struct {
    const MusicNote *notes;
    u16 count;
} LevelMusic;

typedef struct {
    u32 magic;
    u32 version;
    u32 high_score;
    u32 settings;
    u32 rankings[GAME_MODE_COUNT][RANKING_TOP_COUNT];
    u32 checksum;
} SnakePersistRecord;

typedef struct {
    short x1;
    short y1;
    short x2;
    short y2;
    u8 valid;
} BattleRenderBounds;

static const MusicNote music_level1[] = {
    {NOTE_C4, 260}, {NOTE_E4, 180}, {NOTE_G4, 260}, {NOTE_C5, 380},
    {NOTE_B4, 180}, {NOTE_G4, 220}, {NOTE_A4, 340}, {NOTE_REST, 70},
    {NOTE_D4, 240}, {NOTE_F4, 180}, {NOTE_A4, 260}, {NOTE_D5, 360},
    {NOTE_C5, 180}, {NOTE_A4, 220}, {NOTE_B4, 340}, {NOTE_REST, 70},
    {NOTE_E4, 240}, {NOTE_G4, 180}, {NOTE_C5, 300}, {NOTE_E5, 400},
    {NOTE_D5, 180}, {NOTE_C5, 220}, {NOTE_A4, 320}, {NOTE_REST, 80},
    {NOTE_F4, 220}, {NOTE_A4, 160}, {NOTE_D5, 300}, {NOTE_C5, 220},
    {NOTE_B4, 200}, {NOTE_G4, 220}, {NOTE_C5, 520}, {NOTE_REST, 120}
};

static const MusicNote music_level2[] = {
    {NOTE_D3, 520}, {NOTE_A3, 260}, {NOTE_D4, 520}, {NOTE_F4, 280},
    {NOTE_E4, 260}, {NOTE_C4, 360}, {NOTE_REST, 110},
    {NOTE_AS3, 460}, {NOTE_F4, 260}, {NOTE_AS4, 420}, {NOTE_A4, 220},
    {NOTE_G4, 340}, {NOTE_D4, 380}, {NOTE_REST, 100},
    {NOTE_C4, 360}, {NOTE_G3, 240}, {NOTE_C4, 260}, {NOTE_E4, 360},
    {NOTE_F4, 220}, {NOTE_E4, 260}, {NOTE_D4, 520}, {NOTE_REST, 120},
    {NOTE_D3, 240}, {NOTE_REST, 60}, {NOTE_D3, 240}, {NOTE_A3, 360},
    {NOTE_C4, 260}, {NOTE_F4, 360}, {NOTE_D4, 640}, {NOTE_REST, 160}
};

static const MusicNote music_level3[] = {
    {NOTE_E4, 360}, {NOTE_B4, 260}, {NOTE_FS5, 540}, {NOTE_REST, 80},
    {NOTE_G5, 180}, {NOTE_FS5, 180}, {NOTE_E5, 380}, {NOTE_B4, 300},
    {NOTE_CS4, 340}, {NOTE_GS4, 260}, {NOTE_DS5, 520}, {NOTE_REST, 90},
    {NOTE_E5, 180}, {NOTE_DS5, 180}, {NOTE_CS5, 360}, {NOTE_GS4, 320},
    {NOTE_A3, 320}, {NOTE_E4, 240}, {NOTE_B4, 480}, {NOTE_CS5, 220},
    {NOTE_E5, 300}, {NOTE_FS5, 520}, {NOTE_REST, 120},
    {NOTE_B3, 300}, {NOTE_FS4, 220}, {NOTE_CS5, 420}, {NOTE_B4, 240},
    {NOTE_GS4, 300}, {NOTE_E4, 620}, {NOTE_REST, 160}
};

static const MusicNote music_level4[] = {
    {NOTE_A3, 180}, {NOTE_REST, 40}, {NOTE_E4, 180}, {NOTE_A4, 220},
    {NOTE_C5, 180}, {NOTE_E5, 260}, {NOTE_D5, 140}, {NOTE_C5, 300},
    {NOTE_REST, 70},
    {NOTE_GS3, 180}, {NOTE_REST, 40}, {NOTE_E4, 180}, {NOTE_GS4, 220},
    {NOTE_B4, 180}, {NOTE_E5, 260}, {NOTE_D5, 140}, {NOTE_B4, 300},
    {NOTE_REST, 70},
    {NOTE_F3, 160}, {NOTE_C4, 160}, {NOTE_F4, 220}, {NOTE_A4, 180},
    {NOTE_C5, 300}, {NOTE_AS4, 150}, {NOTE_A4, 260}, {NOTE_REST, 60},
    {NOTE_E4, 160}, {NOTE_GS4, 160}, {NOTE_B4, 220}, {NOTE_E5, 340},
    {NOTE_GS5, 180}, {NOTE_E5, 180}, {NOTE_C5, 520}, {NOTE_REST, 130}
};

static const MusicNote music_level5[] = {
    {NOTE_C3, 180}, {NOTE_REST, 45}, {NOTE_C3, 180}, {NOTE_REST, 80},
    {NOTE_G3, 360}, {NOTE_DS4, 520}, {NOTE_D4, 260}, {NOTE_C4, 620},
    {NOTE_REST, 130},
    {NOTE_C3, 150}, {NOTE_REST, 40}, {NOTE_C3, 150}, {NOTE_REST, 60},
    {NOTE_G3, 300}, {NOTE_F4, 420}, {NOTE_DS4, 260}, {NOTE_D4, 620},
    {NOTE_REST, 110},
    {NOTE_C3, 130}, {NOTE_REST, 35}, {NOTE_C3, 130}, {NOTE_G3, 240},
    {NOTE_C4, 240}, {NOTE_G4, 480}, {NOTE_FS4, 180}, {NOTE_F4, 360},
    {NOTE_DS4, 300}, {NOTE_C4, 660}, {NOTE_REST, 140},
    {NOTE_C5, 180}, {NOTE_B4, 150}, {NOTE_AS4, 240}, {NOTE_G4, 320},
    {NOTE_DS5, 340}, {NOTE_D5, 300}, {NOTE_C5, 760}, {NOTE_REST, 180}
};

static const MusicNote music_home[] = {
    #include "music_home_data.inc"
};

static const LevelMusic level_music[LEVEL_COUNT] = {
    {music_level1, MUSIC_COUNT(music_level1)},
    {music_level2, MUSIC_COUNT(music_level2)},
    {music_level3, MUSIC_COUNT(music_level3)},
    {music_level4, MUSIC_COUNT(music_level4)},
    {music_level5, MUSIC_COUNT(music_level5)}
};

static const u16 audio_sine_base[AUDIO_WAVE_POINTS] = {
    2047, 2447, 2831, 3185, 3498, 3750, 3939, 4056,
    4095, 4056, 3939, 3750, 3495, 3185, 2831, 2447,
    2047, 1647, 1263, 909, 599, 344, 155, 38,
    0, 38, 155, 344, 599, 909, 1263, 1647
};

static u16 audio_sine_buffer[AUDIO_WAVE_POINTS];

static const char level_map[LEVEL_COUNT][GRID_ROWS][GRID_COLS + 1] = {
    {
        "...............",
        "...............",
        "...............",
        "...............",
        "...............",
        "...............",
        "...............",
        "...............",
        "...............",
        "...............",
        "...............",
        "...............",
        "...............",
        "...............",
        "...............",
        "...............",
        "...............",
        "...............",
        "...............",
        "..............."
    },
    {
        "...............",
        "...............",
        "....#####......",
        "...............",
        "...............",
        "...#.......#...",
        "...#.......#...",
        "...#.......#...",
        "...............",
        "...............",
        "...............",
        "...............",
        "......#####....",
        "...............",
        "...............",
        "..###.....###..",
        "...............",
        "...............",
        "...............",
        "..............."
    },
    {
        "...............",
        "..A............",
        "...............",
        ".....####......",
        "...............",
        "...............",
        "...............",
        "...............",
        "...............",
        "...............",
        "...............",
        "...............",
        "...............",
        "......####.....",
        "...............",
        "...............",
        "...............",
        "............B..",
        "...............",
        "..............."
    },
    {
        "...............",
        "...............",
        "...###.........",
        "...............",
        "...............",
        "...............",
        "...........###.",
        "...............",
        "...............",
        "...............",
        "...............",
        "...............",
        ".###...........",
        "...............",
        "...............",
        "...............",
        ".........###...",
        "...............",
        "...............",
        "..............."
    },
    {
        "...............",
        ".###.....###...",
        "...............",
        "...............",
        ".....###.......",
        "...............",
        "...............",
        "...#.......#...",
        "...#.......#...",
        "...............",
        "...............",
        "...#.......#...",
        "...#.......#...",
        "...............",
        ".......###.....",
        "...............",
        "...............",
        "...###.....###.",
        "...............",
        "..............."
    }
};

static const u8 level_target[LEVEL_COUNT] = {3, 4, 4, 6, 5};
static const u8 level_time_limit[LEVEL_COUNT] = {0, 0, 0, 0, 45};
static const char *level_name[LEVEL_COUNT] = {
    "BASIC",
    "WALL",
    "PORTAL",
    "ITEM",
    "TIME"
};

static const u16 home_author_glyphs[5][16] = {
    {
        0x0880, 0x1980, 0x11FC, 0x3380, 0x3280, 0x7480, 0x50F8, 0x1080,
        0x1080, 0x10FC, 0x1080, 0x1080, 0x1080, 0x0080, 0x0000, 0x0000
    },
    {
        0x0300, 0x0308, 0x1FD8, 0x0330, 0x0360, 0x7FFC, 0x0300, 0x0FF0,
        0x1810, 0x6FF0, 0x0810, 0x0810, 0x0FF0, 0x0000, 0x0000, 0x0000
    },
    {
        0x0000, 0x7FFC, 0x0100, 0x0100, 0x0100, 0x0100, 0x0100, 0x0100,
        0x0100, 0x0100, 0x0100, 0x0100, 0x0700, 0x0000, 0x0000, 0x0000
    },
    {
        0x08C0, 0x1910, 0x1308, 0x33F4, 0x3110, 0x5318, 0x5684, 0x11F8,
        0x1190, 0x1690, 0x1060, 0x10E0, 0x179C, 0x1400, 0x0000, 0x0000
    },
    {
        0x0000, 0x33F0, 0x1130, 0x01E0, 0x60E0, 0x3F3C, 0x0440, 0x03F8,
        0x1040, 0x2040, 0x27FC, 0x6040, 0x6040, 0x0040, 0x0000, 0x0000
    }
};

static u8 snake_x[MAX_SNAKE_LEN];
static u8 snake_y[MAX_SNAKE_LEN];
static u16 snake_len;
static u16 prev_snake_len;
static u8 prev_snake_head_x;
static u8 prev_snake_head_y;
static u8 prev_snake_tail_x;
static u8 prev_snake_tail_y;
static u8 snake2_x[MAX_SNAKE_LEN];
static u8 snake2_y[MAX_SNAKE_LEN];
static u16 snake2_len;
static u16 prev_snake2_len;
static u8 prev_snake2_head_x;
static u8 prev_snake2_head_y;
static u8 prev_snake2_tail_x;
static u8 prev_snake2_tail_y;
static SnakeDir dir2;
static SnakeDir next_dir2;
static u8 turn_pending2;
static u8 duo_winner;
static u8 food_x;
static u8 food_y;
static u8 prev_food_x;
static u8 prev_food_y;
static u8 food_type;
static u16 score;
static u16 score2;
static u16 high_score;
static u8 lives;
static u8 level_index;
static u8 level_score;
static u16 classic_target_len;
static u8 viewport_x;
static u8 viewport_y;
static u8 prev_viewport_x;
static u8 prev_viewport_y;
static u8 viewport2_x;
static u8 viewport2_y;
static u8 prev_viewport2_x;
static u8 prev_viewport2_y;
static u8 time_left;
static u16 time_acc_ms;
static const MusicNote *music_notes;
static u16 music_count;
static u16 music_index;
static u16 music_left_ms;
static u16 music_gap_ms;
static u16 music_freq;
static u8 audio_playing;
static u8 audio_wave_volume = 0xff;
static u32 rng_state = 0x13572468;
static SnakeDir dir;
static SnakeDir next_dir;
static u8 key_last_raw;
static u8 key_stable;
static u8 key_stable_count;
static u8 key_press_latch;
static u8 knob_event_latch;
static u8 knob_adc_channel = ADC_Channel_3;
static u16 knob_last_value;
static u8 sound_volume = VOLUME_DEFAULT;
static u8 turn_pending;
static u8 paused;
static u8 pause_lock;
static u8 restart_request;
static u8 return_home_request;
static u8 start_level;
static u8 game_mode;
static u8 persist_dirty;
static u8 force_board_clear;
static const char *status_msg = "READY";
static u16 ranking_scores[GAME_MODE_COUNT][RANKING_TOP_COUNT];
static BattleState battle_state;
static BattleInput battle_input;
static u8 battle_view_x;
static u8 battle_view_y;
static u8 battle_skin = 1;
static u16 battle_frame_ms = BATTLE_FRAME_MS_DEFAULT;
static u16 battle_pause_cooldown_ms;
static u8 battle_header_dirty = 1;
static u8 battle_frame_led;
static u8 battle_render_valid;
static u8 battle_force_full_render;
static u8 battle_render_view_x;
static u8 battle_render_view_y;
static BattleRenderBounds battle_prev_bounds[BATTLE_PLAYER_COUNT];

static void Snake_AudioStop(void);
static void Snake_AudioSet(u16 freq);
static void Snake_Beep(u16 ms);
static void Snake_SetStatus(const char *msg);
static void Snake_DrawHeader(void);
static void Snake_DrawPauseHint(void);
static const char *Snake_RankingModeText(u8 mode);
static void Snake_RankingResetAll(void);
static void Snake_RankingInsert(u8 mode, u16 score_value);
static void Snake_RecordModeResults(void);
static void Snake_PersistMarkDirty(void);
static u8 Snake_HandleRankingSerialInput(u8 *selected_mode);
static void Snake_DrawRankingTable(u8 selected_mode);
static void Snake_DrawRankingScreen(u8 selected_mode, short snake_x);
static void Snake_ShowRanking(void);
static void Snake_ClearPendingUartCommands(void);
static FLASH_Status Snake_FlashWaitReady(void);
static void Snake_FlashClearStatus(void);
static void Snake_FlashUnlock(void);
static void Snake_FlashLock(void);
static FLASH_Status Snake_FlashErasePage(u32 page_addr);
static FLASH_Status Snake_FlashProgramHalfWord(u32 address, u16 data);
static FLASH_Status Snake_FlashProgramWord(u32 address, u32 data);
static void Snake_PersistLoad(void);
static void Snake_PersistSave(void);
static void Snake_Render(void);
static void Snake_StartLevel(u8 lv);
static void Snake_UpdateBest(void);
static void Snake_BattleStart(void);
static void Snake_BattleLoop(void);

static u8 Snake_IsDuoMode(void)
{
    return (u8)(game_mode == GAME_MODE_DUO || game_mode == GAME_MODE_OPEN_DUO);
}

static u8 Snake_IsOpenMode(void)
{
    return (u8)(game_mode == GAME_MODE_OPEN || game_mode == GAME_MODE_OPEN_DUO);
}

static u8 Snake_IsOpenDuoMode(void)
{
    return (u8)(game_mode == GAME_MODE_OPEN_DUO);
}

static u8 Snake_IsBattleMode(void)
{
    return (u8)(game_mode == GAME_MODE_BATTLE);
}

static const char *Snake_RankingModeText(u8 mode)
{
    if (mode == GAME_MODE_CLASSIC) return "MODE CLASSIC";
    if (mode == GAME_MODE_DUO) return "MODE DUO";
    if (mode == GAME_MODE_OPEN) return "MODE OPEN";
    if (mode == GAME_MODE_OPEN_DUO) return "MODE OPENDUO";
    if (mode == GAME_MODE_BATTLE) return "MODE BATTLE";
    return "MODE STAGE";
}

static void Snake_RankingResetAll(void)
{
    u8 mode;
    u8 rank;

    for (mode = 0; mode < GAME_MODE_COUNT; mode++) {
        for (rank = 0; rank < RANKING_TOP_COUNT; rank++) {
            ranking_scores[mode][rank] = 0;
        }
    }
}

static void Snake_RankingInsert(u8 mode, u16 score_value)
{
    u8 i;
    u8 j;

    if (mode >= GAME_MODE_COUNT || score_value == 0) {
        return;
    }
    if (score_value > 999) {
        score_value = 999;
    }

    for (i = 0; i < RANKING_TOP_COUNT; i++) {
        if (score_value > ranking_scores[mode][i]) {
            for (j = (u8)(RANKING_TOP_COUNT - 1); j > i; j--) {
                ranking_scores[mode][j] = ranking_scores[mode][j - 1];
            }
            ranking_scores[mode][i] = score_value;
            Snake_PersistMarkDirty();
            return;
        }
    }
}

static void Snake_RecordModeResults(void)
{
    if (Snake_IsDuoMode()) {
        Snake_RankingInsert(game_mode, (u16)(score + score2));
    } else {
        Snake_RankingInsert(game_mode, score);
    }
}

static u32 Snake_PersistBuildSettings(void)
{
    return (u32)sound_volume | ((u32)game_mode << 8);
}

static u32 Snake_PersistBuildChecksum(const SnakePersistRecord *record)
{
    const u32 *words = (const u32 *)record;
    u16 i;
    u32 checksum = SNAKE_FLASH_XOR_KEY;

    for (i = 0; i < (u16)((sizeof(SnakePersistRecord) / sizeof(u32)) - 1); i++) {
        checksum ^= words[i];
        checksum = (checksum << 1) | (checksum >> 31);
    }

    return checksum;
}

static void Snake_PersistMarkDirty(void)
{
    persist_dirty = 1;
}

static void Snake_PersistRemember(void)
{
    persist_dirty = 0;
}

static FLASH_Status Snake_FlashWaitReady(void)
{
    u32 timeout = SNAKE_FLASH_TIMEOUT;

    while (((FLASH->SR & FLASH_SR_BSY) != 0) && (timeout != 0)) {
        timeout--;
    }

    if ((FLASH->SR & FLASH_SR_BSY) != 0) {
        return FLASH_TIMEOUT;
    }
    if ((FLASH->SR & FLASH_SR_PGERR) != 0) {
        return FLASH_ERROR_PG;
    }
    if ((FLASH->SR & FLASH_SR_WRPRTERR) != 0) {
        return FLASH_ERROR_WRP;
    }

    return FLASH_COMPLETE;
}

static void Snake_FlashClearStatus(void)
{
    FLASH->SR = FLASH_SR_EOP | FLASH_SR_PGERR | FLASH_SR_WRPRTERR;
}

static void Snake_FlashUnlock(void)
{
    if ((FLASH->CR & FLASH_CR_LOCK) != 0) {
        FLASH->KEYR = SNAKE_FLASH_KEY1;
        FLASH->KEYR = SNAKE_FLASH_KEY2;
    }
}

static void Snake_FlashLock(void)
{
    FLASH->CR |= FLASH_CR_LOCK;
}

static FLASH_Status Snake_FlashErasePage(u32 page_addr)
{
    FLASH_Status status = Snake_FlashWaitReady();

    if (status != FLASH_COMPLETE) {
        return status;
    }

    Snake_FlashClearStatus();
    FLASH->CR |= FLASH_CR_PER;
    FLASH->AR = page_addr;
    FLASH->CR |= FLASH_CR_STRT;

    status = Snake_FlashWaitReady();
    FLASH->CR &= (u32)(~FLASH_CR_PER);
    Snake_FlashClearStatus();
    return status;
}

static FLASH_Status Snake_FlashProgramHalfWord(u32 address, u16 data)
{
    FLASH_Status status = Snake_FlashWaitReady();

    if (status != FLASH_COMPLETE) {
        return status;
    }

    Snake_FlashClearStatus();
    FLASH->CR |= FLASH_CR_PG;
    *(volatile u16 *)address = data;

    status = Snake_FlashWaitReady();
    FLASH->CR &= (u32)(~FLASH_CR_PG);
    Snake_FlashClearStatus();

    if (status != FLASH_COMPLETE) {
        return status;
    }

    if (*(volatile u16 *)address != data) {
        return FLASH_ERROR_PG;
    }

    return FLASH_COMPLETE;
}

static FLASH_Status Snake_FlashProgramWord(u32 address, u32 data)
{
    FLASH_Status status;

    status = Snake_FlashProgramHalfWord(address, (u16)(data & 0xffffu));
    if (status != FLASH_COMPLETE) {
        return status;
    }

    return Snake_FlashProgramHalfWord(address + 2,
                                      (u16)((data >> 16) & 0xffffu));
}

static void Snake_PersistLoad(void)
{
    const SnakePersistRecord *record =
        (const SnakePersistRecord *)SNAKE_FLASH_STORE_ADDR;
    u32 settings;
    u8 saved_volume;
    u8 saved_mode;
    u8 mode;
    u8 rank;

    Snake_RankingResetAll();

    if (record->magic != SNAKE_FLASH_MAGIC ||
        record->version != SNAKE_FLASH_VERSION) {
        Snake_PersistRemember();
        return;
    }

    if (record->checksum != Snake_PersistBuildChecksum(record)) {
        Snake_PersistRemember();
        return;
    }

    settings = record->settings;
    saved_volume = (u8)(settings & 0xffu);
    saved_mode = (u8)((settings >> 8) & 0xffu);

    if (saved_volume > VOLUME_MAX) {
        saved_volume = VOLUME_DEFAULT;
    }
    if (saved_mode >= GAME_MODE_COUNT) {
        saved_mode = GAME_MODE_STAGE;
    }

    high_score = (u16)((record->high_score > 999u) ? 999u : record->high_score);
    sound_volume = saved_volume;
    game_mode = saved_mode;
    for (mode = 0; mode < GAME_MODE_COUNT; mode++) {
        for (rank = 0; rank < RANKING_TOP_COUNT; rank++) {
            ranking_scores[mode][rank] =
                (u16)((record->rankings[mode][rank] > 999u) ?
                999u : record->rankings[mode][rank]);
        }
    }
    Snake_PersistRemember();
}

static void Snake_PersistSave(void)
{
    SnakePersistRecord record;
    const SnakePersistRecord *saved =
        (const SnakePersistRecord *)SNAKE_FLASH_STORE_ADDR;
    FLASH_Status status;
    u32 settings;
    u16 i;
    u8 mode;
    u8 rank;
    u8 same = 1;

    if (!persist_dirty) {
        return;
    }

    settings = Snake_PersistBuildSettings();
    record.magic = SNAKE_FLASH_MAGIC;
    record.version = SNAKE_FLASH_VERSION;
    record.high_score = high_score;
    record.settings = settings;
    for (mode = 0; mode < GAME_MODE_COUNT; mode++) {
        for (rank = 0; rank < RANKING_TOP_COUNT; rank++) {
            record.rankings[mode][rank] = ranking_scores[mode][rank];
        }
    }
    record.checksum = Snake_PersistBuildChecksum(&record);

    for (i = 0; i < (u16)(sizeof(SnakePersistRecord) / sizeof(u32)); i++) {
        if (((const u32 *)saved)[i] != ((const u32 *)&record)[i]) {
            same = 0;
            break;
        }
    }

    if (same) {
        Snake_PersistRemember();
        return;
    }

    Snake_FlashUnlock();
    Snake_FlashClearStatus();

    status = Snake_FlashErasePage(SNAKE_FLASH_STORE_ADDR);
    if (status == FLASH_COMPLETE) {
        status = Snake_FlashProgramWord(SNAKE_FLASH_STORE_ADDR + 0,
                                        record.magic);
    }
    if (status == FLASH_COMPLETE) {
        status = Snake_FlashProgramWord(SNAKE_FLASH_STORE_ADDR + 4,
                                        record.version);
    }
    if (status == FLASH_COMPLETE) {
        status = Snake_FlashProgramWord(SNAKE_FLASH_STORE_ADDR + 8,
                                        record.high_score);
    }
    if (status == FLASH_COMPLETE) {
        status = Snake_FlashProgramWord(SNAKE_FLASH_STORE_ADDR + 12,
                                        record.settings);
    }
    for (mode = 0; mode < GAME_MODE_COUNT && status == FLASH_COMPLETE; mode++) {
        for (rank = 0; rank < RANKING_TOP_COUNT; rank++) {
            status = Snake_FlashProgramWord(
                SNAKE_FLASH_STORE_ADDR + 16 +
                ((mode * RANKING_TOP_COUNT + rank) * 4UL),
                record.rankings[mode][rank]);
        }
    }
    if (status == FLASH_COMPLETE) {
        status = Snake_FlashProgramWord(
            SNAKE_FLASH_STORE_ADDR + 16 +
            (GAME_MODE_COUNT * RANKING_TOP_COUNT * 4UL),
            record.checksum);
    }

    Snake_FlashLock();

    if (status == FLASH_COMPLETE) {
        Snake_PersistRemember();
    }
}

static u8 Snake_WorldCols(void)
{
    return Snake_IsOpenMode() ? OPEN_WORLD_COLS : GRID_COLS;
}

static u8 Snake_WorldRows(void)
{
    return Snake_IsOpenMode() ? OPEN_WORLD_ROWS : GRID_ROWS;
}

static u8 Snake_IsReverse(SnakeDir from, SnakeDir to)
{
    return (u8)((from == DIR_UP && to == DIR_DOWN) ||
                (from == DIR_DOWN && to == DIR_UP) ||
                (from == DIR_LEFT && to == DIR_RIGHT) ||
                (from == DIR_RIGHT && to == DIR_LEFT));
}

static u8 Snake_CommandToDir(SnakeUartCmdType type, SnakeDir *want)
{
    if (type == SNAKE_UART_CMD_UP || type == SNAKE_UART_CMD_P2_UP) {
        *want = DIR_UP;
    } else if (type == SNAKE_UART_CMD_DOWN || type == SNAKE_UART_CMD_P2_DOWN) {
        *want = DIR_DOWN;
    } else if (type == SNAKE_UART_CMD_LEFT || type == SNAKE_UART_CMD_P2_LEFT) {
        *want = DIR_LEFT;
    } else if (type == SNAKE_UART_CMD_RIGHT || type == SNAKE_UART_CMD_P2_RIGHT) {
        *want = DIR_RIGHT;
    } else {
        return 0;
    }

    return 1;
}

static u8 Snake_DuoKeypadToDir(u8 value, SnakeDir *want)
{
    if (value == 0) {
        *want = DIR_LEFT;   /* 1 */
    } else if (value == 1) {
        *want = DIR_DOWN;   /* 2 */
    } else if (value == 2) {
        *want = DIR_RIGHT;  /* 3 */
    } else if (value == 4) {
        *want = DIR_UP;     /* 5 */
    } else {
        return 0;
    }

    return 1;
}

static void Snake_SetP1Direction(SnakeDir want)
{
    if (paused || turn_pending || want == next_dir || Snake_IsReverse(dir, want)) {
        return;
    }

    next_dir = want;
    turn_pending = 1;
}

static void Snake_SetP2Direction(SnakeDir want)
{
    if (paused || turn_pending2 || want == next_dir2 ||
        Snake_IsReverse(dir2, want)) {
        return;
    }

    next_dir2 = want;
    turn_pending2 = 1;
}

static void Snake_TogglePause(void)
{
    paused = (u8)!paused;
    pause_lock = 1;
    key_press_latch = 0;
    if (paused) {
        Snake_AudioStop();
    } else if (music_left_ms != 0) {
        Snake_AudioSet(music_freq);
    }
    Snake_SetStatus(paused ? "PAUSED" : "RUNNING");
    Snake_DrawHeader();
    if (paused) {
        Snake_DrawPauseHint();
    } else {
        LCD_Fill(12, 148, LCD_W - 13, 186, BLACK);
        Snake_Render();
    }
    Snake_Beep(35);
}

static void Snake_SelectLevel(u8 next_level, const char *msg)
{
    Snake_StartLevel(next_level);
    paused = 1;
    Snake_AudioStop();
    Snake_SetStatus(msg);
    Snake_DrawHeader();
    Snake_Beep(35);
}

static void Snake_ApplySerialCommand(const SnakeUartCmd *cmd)
{
    SnakeDir want = next_dir;

    if (cmd->type == SNAKE_UART_CMD_PAUSE) {
        Snake_TogglePause();
        return;
    }

    if (cmd->type == SNAKE_UART_CMD_START) {
        Snake_TogglePause();
        return;
    }

    if (cmd->type == SNAKE_UART_CMD_RESET) {
        if (paused) {
            return_home_request = 1;
            Snake_SetStatus("EXIT HOME");
            Snake_DrawHeader();
            Snake_DrawPauseHint();
        } else {
            restart_request = 1;
            Snake_SetStatus("UART RESET");
            Snake_DrawHeader();
        }
        return;
    }

    if (cmd->type == SNAKE_UART_CMD_LEVEL) {
        if (Snake_IsDuoMode()) {
            if (Snake_DuoKeypadToDir(cmd->value, &want)) {
                Snake_SetP2Direction(want);
            }
        } else if (cmd->value < LEVEL_COUNT) {
            Snake_SelectLevel(cmd->value, "UART LEVEL");
        }
        return;
    }

    if (!Snake_CommandToDir(cmd->type, &want)) {
        return;
    }

    if (cmd->type == SNAKE_UART_CMD_P2_UP ||
        cmd->type == SNAKE_UART_CMD_P2_DOWN ||
        cmd->type == SNAKE_UART_CMD_P2_LEFT ||
        cmd->type == SNAKE_UART_CMD_P2_RIGHT) {
        Snake_SetP2Direction(want);
    } else {
        Snake_SetP1Direction(want);
    }
}

static void Snake_HandleSerialInput(void)
{
    SnakeUartCmd cmd;

    while (SnakeUart_PopCommand(&cmd)) {
        Snake_ApplySerialCommand(&cmd);
    }
}

static u8 Snake_HandleHomeSerialInput(u8 *selected_level, u8 *selected_mode,
                                      u8 *open_settings)
{
    SnakeUartCmd cmd;
    u8 should_start = 0;
    u8 selected = *selected_level;

    while (SnakeUart_PopCommand(&cmd)) {
        if (cmd.type == SNAKE_UART_CMD_LEVEL) {
            if (cmd.value < LEVEL_COUNT) {
                selected = cmd.value;
            }
        } else if (cmd.type == SNAKE_UART_CMD_LEFT) {
            selected = (selected == 0) ? (LEVEL_COUNT - 1) : (u8)(selected - 1);
        } else if (cmd.type == SNAKE_UART_CMD_RIGHT) {
            selected = (u8)((selected + 1) % LEVEL_COUNT);
        } else if (cmd.type == SNAKE_UART_CMD_START) {
            should_start = 1;
        } else if (cmd.type == SNAKE_UART_CMD_PAUSE) {
            *open_settings = 1;
        } else if (cmd.type == SNAKE_UART_CMD_UP) {
            *selected_mode = (u8)((*selected_mode == 0) ?
                (GAME_MODE_COUNT - 1) : (*selected_mode - 1));
        } else if (cmd.type == SNAKE_UART_CMD_DOWN) {
            *selected_mode = (u8)((*selected_mode + 1) % GAME_MODE_COUNT);
        }
    }

    *selected_level = selected;
    return should_start;
}

static u8 Snake_HandleSettingsSerialInput(void)
{
    SnakeUartCmd cmd;
    u8 should_return = 0;

    while (SnakeUart_PopCommand(&cmd)) {
        if (cmd.type == SNAKE_UART_CMD_LEFT) {
            if (sound_volume > 0) {
                sound_volume--;
            }
        } else if (cmd.type == SNAKE_UART_CMD_RIGHT) {
            if (sound_volume < VOLUME_MAX) {
                sound_volume++;
            }
        } else if (cmd.type == SNAKE_UART_CMD_START ||
                   cmd.type == SNAKE_UART_CMD_PAUSE ||
                   cmd.type == SNAKE_UART_CMD_RESET) {
            should_return = 1;
        }
    }

    return should_return;
}

static u16 Snake_Rand(u16 max)
{
    rng_state = rng_state * 1664525u + 1013904223u;
    return (u16)((rng_state >> 16) % max);
}

static u8 Snake_KeyReadRaw(void)
{
    u8 keys = 0;

    if (GPIO_ReadInputDataBit(GPIOD, GPIO_Pin_11) == 0) keys |= KEY_UP_MASK;
    if (GPIO_ReadInputDataBit(GPIOD, GPIO_Pin_12) == 0) keys |= KEY_DOWN_MASK;
    if (GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_13) == 0) keys |= KEY_LEFT_MASK;
    if (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_0) == 0)  keys |= KEY_RIGHT_MASK;

    return keys;
}

static void Snake_KeyReset(void)
{
    key_last_raw = Snake_KeyReadRaw();
    key_stable = key_last_raw;
    key_stable_count = 0;
    key_press_latch = 0;
    pause_lock = 0;
}

static void Snake_KeyScan(void)
{
    u8 raw = Snake_KeyReadRaw();

    if (raw == key_last_raw) {
        if (key_stable_count < 3) {
            key_stable_count++;
        }
    } else {
        key_last_raw = raw;
        key_stable_count = 0;
    }

    if (key_stable_count >= 2 && raw != key_stable) {
        u8 newly_pressed = (u8)(raw & (u8)(~key_stable));
        key_stable = raw;
        key_press_latch |= newly_pressed;
    }
}

static void Snake_ADCConfiguration(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    ADC_InitTypeDef ADC_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_AFIO |
                           RCC_APB2Periph_ADC1, ENABLE);
    RCC_ADCCLKConfig(RCC_PCLK2_Div6);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    ADC_DeInit(ADC1);
    ADC_InitStructure.ADC_Mode = ADC_Mode_Independent;
    ADC_InitStructure.ADC_ScanConvMode = DISABLE;
    ADC_InitStructure.ADC_ContinuousConvMode = DISABLE;
    ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;
    ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
    ADC_InitStructure.ADC_NbrOfChannel = 1;
    ADC_Init(ADC1, &ADC_InitStructure);

    ADC_Cmd(ADC1, ENABLE);
    ADC_ResetCalibration(ADC1);
    while (ADC_GetResetCalibrationStatus(ADC1)) {
    }
    ADC_StartCalibration(ADC1);
    while (ADC_GetCalibrationStatus(ADC1)) {
    }
    ADC_SoftwareStartConvCmd(ADC1, ENABLE);
}

static u16 Snake_ReadAdcChannel(u8 channel)
{
    u8 i;
    u32 sum = 0;

    for (i = 0; i < 4; i++) {
        ADC_RegularChannelConfig(ADC1, channel, 1,
                                 ADC_SampleTime_239Cycles5);
        ADC_SoftwareStartConvCmd(ADC1, ENABLE);
        while (ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC) == RESET) {
        }
        sum += ADC_GetConversionValue(ADC1);
    }

    return (u16)(sum / 4);
}

static u16 Snake_KnobRead(void)
{
    return Snake_ReadAdcChannel(knob_adc_channel);
}

static u8 Snake_KnobLevel(void)
{
    u8 lv = (u8)(Snake_KnobRead() / KNOB_LEVEL_STEP);

    if (lv >= LEVEL_COUNT) {
        lv = LEVEL_COUNT - 1;
    }

    return lv;
}

static void Snake_KnobReset(void)
{
    knob_last_value = Snake_KnobRead();
    knob_event_latch = 0;
}

static void Snake_KnobScan(void)
{
    u16 value = Snake_KnobRead();

    if ((u16)(value + KNOB_TURN_DELTA) < knob_last_value) {
        knob_event_latch = KNOB_LEFT_EVENT;
        knob_last_value = value;
    } else if (value > (u16)(knob_last_value + KNOB_TURN_DELTA)) {
        knob_event_latch = KNOB_RIGHT_EVENT;
        knob_last_value = value;
    }
}

static u8 Snake_KnobPopEvent(void)
{
    u8 event = knob_event_latch;

    knob_event_latch = 0;
    return event;
}

static void Snake_AudioUpdateWave(void)
{
    u8 i;
    s32 delta;
    u16 value;

    if (audio_wave_volume == sound_volume) {
        return;
    }

    for (i = 0; i < AUDIO_WAVE_POINTS; i++) {
        delta = (s32)audio_sine_base[i] - AUDIO_DAC_CENTER;
        value = (u16)(AUDIO_DAC_CENTER +
                      (delta * sound_volume) / VOLUME_MAX);
        audio_sine_buffer[i] = value;
    }

    audio_wave_volume = sound_volume;
}

static void Snake_AudioStop(void)
{
    TIM_Cmd(TIM3, DISABLE);
    DAC_SetChannel2Data(DAC_Align_12b_R, AUDIO_DAC_CENTER);
    audio_playing = 0;
}

static void Snake_AudioSet(u16 freq)
{
    u32 period;

    if (freq == NOTE_REST || freq == 0 || sound_volume == 0) {
        Snake_AudioStop();
        return;
    }

    Snake_AudioUpdateWave();

    period = (SystemCoreClock / (AUDIO_WAVE_POINTS * (u32)freq));
    if (period < 2) {
        period = 2;
    }

    TIM_Cmd(TIM3, DISABLE);
    TIM_SetAutoreload(TIM3, (u16)(period - 1));
    TIM_SetCounter(TIM3, 0);
    TIM_Cmd(TIM3, ENABLE);
    audio_playing = 1;
}

static void Snake_AudioInit(void)
{
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    GPIO_InitTypeDef GPIO_InitStructure;
    DAC_InitTypeDef DAC_InitStructure;
    DMA_InitTypeDef DMA_InitStructure;

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_DAC | RCC_APB1Periph_TIM3, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_AFIO, ENABLE);
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA2, ENABLE);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    Snake_AudioUpdateWave();

    DMA_DeInit(DMA2_Channel4);
    DMA_InitStructure.DMA_PeripheralBaseAddr = AUDIO_DAC_DHR12R2;
    DMA_InitStructure.DMA_MemoryBaseAddr = (u32)audio_sine_buffer;
    DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralDST;
    DMA_InitStructure.DMA_BufferSize = AUDIO_WAVE_POINTS;
    DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
    DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
    DMA_InitStructure.DMA_Mode = DMA_Mode_Circular;
    DMA_InitStructure.DMA_Priority = DMA_Priority_High;
    DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;
    DMA_Init(DMA2_Channel4, &DMA_InitStructure);
    DMA_Cmd(DMA2_Channel4, ENABLE);

    DAC_DeInit();
    DAC_InitStructure.DAC_Trigger = DAC_Trigger_T3_TRGO;
    DAC_InitStructure.DAC_WaveGeneration = DAC_WaveGeneration_None;
    DAC_InitStructure.DAC_LFSRUnmask_TriangleAmplitude = DAC_LFSRUnmask_Bit0;
    DAC_InitStructure.DAC_OutputBuffer = DAC_OutputBuffer_Disable;
    DAC_Init(DAC_Channel_2, &DAC_InitStructure);
    DAC_Cmd(DAC_Channel_2, ENABLE);
    DAC_DMACmd(DAC_Channel_2, ENABLE);
    DAC_SetChannel2Data(DAC_Align_12b_R, AUDIO_DAC_CENTER);

    TIM_DeInit(TIM3);
    TIM_InternalClockConfig(TIM3);

    TIM_TimeBaseStructure.TIM_Period = 1000 - 1;
    TIM_TimeBaseStructure.TIM_Prescaler = 0;
    TIM_TimeBaseStructure.TIM_ClockDivision = 0;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM3, &TIM_TimeBaseStructure);
    TIM_ARRPreloadConfig(TIM3, ENABLE);
    TIM_SelectOutputTrigger(TIM3, TIM_TRGOSource_Update);

    Snake_AudioStop();
}

static void Snake_PlayTone(u16 freq, u16 ms)
{
    u16 saved_freq = music_freq;

    if (ms == 0 || sound_volume == 0) {
        return;
    }

    Snake_AudioUpdateWave();
    Snake_AudioSet(freq);
    Delay_ms(ms);
    Snake_AudioSet(saved_freq);
}

static void Snake_MusicSelect(const MusicNote *notes, u16 count)
{
    music_notes = notes;
    music_count = count;
    music_index = 0;
    music_left_ms = 0;
    music_gap_ms = 0;
    music_freq = NOTE_REST;
    Snake_AudioStop();
}

static void Snake_MusicSetLevel(u8 lv)
{
    if (lv >= LEVEL_COUNT) {
        lv = LEVEL_COUNT - 1;
    }

    Snake_MusicSelect(level_music[lv].notes, level_music[lv].count);
}

static void Snake_MusicLoadNextNote(void)
{
    if (music_notes == 0 || music_count == 0) {
        music_freq = NOTE_REST;
        music_left_ms = 80;
        music_gap_ms = 0;
        Snake_AudioStop();
        return;
    }

    music_freq = music_notes[music_index].freq;
    music_left_ms = music_notes[music_index].ms;
    music_gap_ms = 0;
    if (music_freq != NOTE_REST) {
        music_gap_ms = (music_left_ms >= 300) ? 24 : 10;
    }
    Snake_AudioSet(music_freq);
    music_index++;
    if (music_index >= music_count) {
        music_index = 0;
    }

    if (music_left_ms == 0) {
        music_left_ms = MUSIC_TICK_MS;
    }
}

static void Snake_MusicTick(u16 ms)
{
    u16 chunk;

    while (ms > 0) {
        if (music_left_ms == 0) {
            Snake_MusicLoadNextNote();
        }

        chunk = ms;
        if (chunk > music_left_ms) {
            chunk = music_left_ms;
        }

        if (music_gap_ms != 0 && music_left_ms <= music_gap_ms) {
            Snake_AudioStop();
        }
        Delay_ms(chunk);
        music_left_ms = (u16)(music_left_ms - chunk);
        ms = (u16)(ms - chunk);
    }
}

static void Snake_Beep(u16 ms)
{
    Snake_PlayTone(NOTE_C6, ms);
}

static void Snake_BeepLevel(void)
{
    Snake_PlayTone(NOTE_G5, 90);
    Snake_PlayTone(NOTE_C6, 140);
}

static void Snake_ShowText(u16 x, u16 y, u8 size, const char *text,
                           u16 fc, u16 bc, u8 mode)
{
    POINT_COLOR = fc;
    BACK_COLOR = bc;
    LCD_ShowString(x, y, size, (u8 *)text, mode);
}

static void Snake_ShowTextCenter(u16 y, u8 size, const char *text,
                                 u16 fc, u16 bc, u8 mode)
{
    u16 len = 0;
    const char *p = text;

    while (*p >= ' ' && *p <= '~') {
        len++;
        p++;
    }

    Snake_ShowText((u16)((LCD_W - len * (size / 2)) / 2),
                   y, size, text, fc, bc, mode);
}

static void Snake_DrawHomeFrame(u16 x1, u16 y1, u16 x2, u16 y2, u16 color)
{
    POINT_COLOR = color;
    LCD_DrawRectangle(x1, y1, x2, y2);
    LCD_DrawRectangle((u16)(x1 + 2), (u16)(y1 + 2),
                      (u16)(x2 - 2), (u16)(y2 - 2));
}

static void Snake_DrawHomeGlyph(u16 x, u16 y, u8 glyph, u16 fc, u16 bc)
{
    u8 row;
    u8 col;
    u16 bits;

    for (row = 0; row < 16; row++) {
        bits = home_author_glyphs[glyph][row];
        for (col = 0; col < 16; col++) {
            GUI_DrawPoint((u16)(x + col), (u16)(y + row),
                          (bits & (u16)(0x8000 >> col)) ? fc : bc);
        }
    }
}

static void Snake_DrawHomeAuthor(u16 y)
{
    u8 i;
    u16 x = 77;

    for (i = 0; i < 5; i++) {
        Snake_DrawHomeGlyph((u16)(x + i * 18), y, i, LGRAY, HOME_NAVY);
    }
}

static void Snake_DrawHomeDecor(void)
{
    u8 i;

    POINT_COLOR = GRAYBLUE;
    for (i = 0; i < 6; i++) {
        LCD_DrawLine((u16)(i * 46), 76, (u16)(i * 46 + 24), 122);
    }
}

static void Snake_FillClippedRect(short x1, short y1, short x2, short y2, u16 color)
{
    if (x1 > x2 || y1 > y2) {
        return;
    }
    if (x2 < 0 || y2 < 0 || x1 >= LCD_W || y1 >= LCD_H) {
        return;
    }
    if (x1 < 0) x1 = 0;
    if (y1 < 0) y1 = 0;
    if (x2 >= LCD_W) x2 = (short)(LCD_W - 1);
    if (y2 >= LCD_H) y2 = (short)(LCD_H - 1);

    LCD_Fill((u16)x1, (u16)y1, (u16)x2, (u16)y2, color);
}

static void Snake_DrawHomeSnake(short snake_x, u8 clear_prev, short prev_snake_x)
{
    u8 i;
    const short sx[9] = {0, 16, 32, 48, 64, 80, 96, 112, 128};
    const short sy[9] = {104, 104, 104, 92, 92, 92, 104, 104, 104};
    short clear_left;
    short clear_right;

    if (clear_prev) {
        if ((snake_x - prev_snake_x > 80) || (prev_snake_x - snake_x > 80)) {
            Snake_FillClippedRect((short)(prev_snake_x - 2), 88,
                                  (short)(prev_snake_x + 144), 118,
                                  HOME_BLUE);
            Snake_FillClippedRect((short)(snake_x - 2), 88,
                                  (short)(snake_x + 144), 118,
                                  HOME_BLUE);
        } else {
            clear_left = (snake_x < prev_snake_x) ? snake_x : prev_snake_x;
            clear_right = ((snake_x + 142) > (prev_snake_x + 142)) ?
                          (snake_x + 142) : (prev_snake_x + 142);
            Snake_FillClippedRect((short)(clear_left - 2), 88,
                                  (short)(clear_right + 2), 118, HOME_BLUE);
        }
        Snake_DrawHomeDecor();
    }

    for (i = 0; i < 9; i++) {
        Snake_FillClippedRect((short)(snake_x + sx[i]), sy[i],
                              (short)(snake_x + sx[i] + 13),
                              (short)(sy[i] + 13),
                              (i == 8) ? YELLOW : GREEN);
        if (snake_x + sx[i] >= 0 && snake_x + sx[i] + 13 < LCD_W) {
            POINT_COLOR = BLACK;
            LCD_DrawRectangle((u16)(snake_x + sx[i]), (u16)sy[i],
                              (u16)(snake_x + sx[i] + 13),
                              (u16)(sy[i] + 13));
        }
    }

    Snake_FillClippedRect((short)(snake_x + 138), 108,
                          (short)(snake_x + 142), 112, RED);
    Snake_FillClippedRect((short)(snake_x + 132), 98,
                          (short)(snake_x + 134), 100, BLACK);
}

static void Snake_AnimateHomeSnake(short *snake_x, short *prev_snake_x,
                                   u8 *snake_valid, u8 *anim_tick)
{
    *anim_tick = (u8)(*anim_tick + 20);
    if (*anim_tick < 40) {
        return;
    }
    *anim_tick = 0;

    *prev_snake_x = *snake_x;
    *snake_x = (short)(*snake_x + 2);
    if (*snake_x > LCD_W) {
        *snake_x = -142;
    }
    Snake_DrawHomeSnake(*snake_x, *snake_valid, *prev_snake_x);
    *snake_valid = 1;
}

static void Snake_DrawHomeLevels(u8 selected)
{
    u8 i;
    u16 x;
    u16 color;

    LCD_Fill(18, 142, 221, 236, HOME_PANEL);
    Snake_DrawHomeFrame(18, 142, 221, 236, HOME_TEAL);

    Snake_ShowTextCenter(154, 16, "FIVE STAGE ROUTE", WHITE, HOME_PANEL, 1);
    Snake_ShowTextCenter(176, 12, "obstacles  speed  score", LGRAY, HOME_PANEL, 1);

    for (i = 0; i < LEVEL_COUNT; i++) {
        x = (u16)(30 + i * 36);
        if (i == 0) {
            color = GREEN;
        } else if (i == 1) {
            color = CYAN;
        } else if (i == 2) {
            color = HOME_GOLD;
        } else if (i == 3) {
            color = MAGENTA;
        } else {
            color = RED;
        }

        LCD_Fill(x, 200, (u16)(x + 24), 222, color);
        POINT_COLOR = WHITE;
        LCD_DrawRectangle(x, 200, (u16)(x + 24), 222);
        BACK_COLOR = color;
        POINT_COLOR = BLACK;
        LCD_ShowNum((u16)(x + 8), 204, (u32)(i + 1), 1, 16);
    }

    x = (u16)(30 + selected * 36);
    Snake_DrawHomeFrame((u16)(x - 4), 196, (u16)(x + 28), 226, HOME_GOLD);
}

static void Snake_DrawHomePrompt(u8 visible)
{
    LCD_Fill(22, 278, 217, 309, HOME_NAVY);
    Snake_DrawHomeFrame(22, 278, 217, 309, visible ? HOME_GOLD : HOME_BLUE);

    if (visible) {
        Snake_ShowTextCenter(286, 16, "PRESS ANY KEY", YELLOW, HOME_NAVY, 1);
    }
}

static void Snake_DrawHomeMode(u8 selected_mode)
{
    LCD_Fill(18, 236, 122, 270, HOME_PANEL);
    Snake_DrawHomeFrame(18, 236, 122, 270,
                        selected_mode == GAME_MODE_STAGE ? HOME_TEAL : HOME_GOLD);
    if (selected_mode == GAME_MODE_CLASSIC) {
        Snake_ShowText(31, 244, 12, "MODE CLASSIC", WHITE, HOME_PANEL, 1);
    } else if (selected_mode == GAME_MODE_DUO) {
        Snake_ShowText(43, 244, 12, "MODE DUO", WHITE, HOME_PANEL, 1);
    } else if (selected_mode == GAME_MODE_OPEN) {
        Snake_ShowText(39, 244, 12, "MODE OPEN", WHITE, HOME_PANEL, 1);
    } else if (selected_mode == GAME_MODE_OPEN_DUO) {
        Snake_ShowText(25, 244, 12, "MODE OPENDUO", WHITE, HOME_PANEL, 1);
    } else if (selected_mode == GAME_MODE_BATTLE) {
        Snake_ShowText(31, 244, 12, "MODE BATTLE", WHITE, HOME_PANEL, 1);
    } else {
        Snake_ShowText(37, 244, 12, "MODE STAGE", WHITE, HOME_PANEL, 1);
    }
}

static void Snake_DrawHomeSettingsButton(void)
{
    LCD_Fill(122, 236, 221, 270, HOME_PANEL);
    Snake_DrawHomeFrame(122, 236, 221, 270, HOME_TEAL);
    Snake_ShowText(132, 244, 12, "KEY4 SETTINGS", WHITE, HOME_PANEL, 1);
}

static void Snake_DrawVolumeBar(void)
{
    u8 i;
    u16 x;

    LCD_Fill(42, 154, 197, 210, BLACK);
    for (i = 0; i < VOLUME_MAX; i++) {
        x = (u16)(50 + i * 30);
        LCD_Fill(x, 176, (u16)(x + 20), 202,
                 (i < sound_volume) ? HOME_GOLD : HOME_PANEL);
        POINT_COLOR = WHITE;
        LCD_DrawRectangle(x, 176, (u16)(x + 20), 202);
    }

    POINT_COLOR = WHITE;
    BACK_COLOR = BLACK;
    LCD_ShowString(78, 154, 16, (u8 *)"Volume", 0);
    LCD_ShowNum(150, 154, sound_volume, 1, 16);
}

static void Snake_ShowSettings(void)
{
    u8 last_volume = 0xff;
    u8 raw;
    u16 knob_value;
    u16 last_knob_value;

    LCD_Clear(BLACK);
    LCD_Fill(0, 0, LCD_W - 1, 76, HOME_NAVY);
    Snake_ShowTextCenter(22, 16, "SETTINGS", WHITE, HOME_NAVY, 1);
    Snake_ShowTextCenter(48, 12, "A/D or knob volume", CYAN, HOME_NAVY, 1);
    Snake_DrawVolumeBar();
    Snake_ShowTextCenter(250, 12, "Space/P return", LGRAY, BLACK, 1);

    while (Snake_KeyReadRaw() != 0) {
        Delay_ms(20);
    }

    last_knob_value = Snake_KnobRead();

    while (1) {
        knob_value = Snake_KnobRead();
        if ((u16)(knob_value + KNOB_TURN_DELTA) < last_knob_value ||
            knob_value > (u16)(last_knob_value + KNOB_TURN_DELTA)) {
            sound_volume = (u8)((knob_value * (VOLUME_MAX + 1UL)) / 4096UL);
            if (sound_volume > VOLUME_MAX) {
                sound_volume = VOLUME_MAX;
            }
            last_knob_value = knob_value;
        }

        if (Snake_HandleSettingsSerialInput()) {
            Snake_PersistSave();
            LCD_Clear(BLACK);
            Snake_KeyReset();
            Snake_KnobReset();
            return;
        }

        if (sound_volume != last_volume) {
            Snake_PersistMarkDirty();
            Snake_DrawVolumeBar();
            last_volume = sound_volume;
            Snake_AudioUpdateWave();
            if (audio_playing && music_freq != NOTE_REST) {
                Snake_AudioSet(music_freq);
            }
            if (sound_volume != 0) {
                Snake_PlayTone(NOTE_C6, 25);
            }
        }

        raw = Snake_KeyReadRaw();
        if (raw != 0) {
            while (Snake_KeyReadRaw() != 0) {
                Delay_ms(20);
            }
            Snake_PersistSave();
            LCD_Clear(BLACK);
            Snake_KeyReset();
            Snake_KnobReset();
            return;
        }

        Delay_ms(40);
    }
}

static void Snake_ShowHome(void)
{
    u8 blink = 1;
    u8 i;
    u8 selected = Snake_KnobLevel();
    u8 selected_mode = game_mode;
    u8 last_selected = 0xff;
    u8 last_mode = 0xff;
    u16 adc_value;
    u16 last_adc_value;
    u8 raw_key;
    u8 open_settings;
    short home_snake_x = -142;
    short prev_home_snake_x = -142;
    u8 home_snake_valid = 0;
    u8 home_anim_tick = 0;

    LCD_Clear(BLACK);
    LCD_Fill(0, 0, LCD_W - 1, 78, HOME_NAVY);
    LCD_Fill(0, 78, LCD_W - 1, 124, HOME_BLUE);
    LCD_Fill(0, 124, LCD_W - 1, LCD_H - 1, BLACK);

    Snake_DrawHomeDecor();

    Snake_ShowTextCenter(20, 16, "SnakePilot", WHITE, HOME_NAVY, 1);
    Snake_ShowTextCenter(42, 12, "STM32 SMART SNAKE", CYAN, HOME_NAVY, 1);
    Snake_DrawHomeAuthor(61);

    Snake_DrawHomeSnake(home_snake_x, 0, prev_home_snake_x);
    home_snake_valid = 1;
    Snake_DrawHomeLevels(selected);
    Snake_DrawHomeMode(selected_mode);

    Snake_ShowTextCenter(250, 12, "K3 rank  K4 settings  Space start",
                         LGRAY, BLACK, 1);
    Snake_DrawHomeSettingsButton();
    Snake_DrawHomePrompt(blink);
    Snake_MusicSelect(music_home, MUSIC_COUNT(music_home));

    while (Snake_KeyReadRaw() != 0) {
        Snake_AnimateHomeSnake(&home_snake_x, &prev_home_snake_x,
                               &home_snake_valid, &home_anim_tick);
        Snake_MusicTick(20);
    }

    last_adc_value = Snake_KnobRead();

    while (1) {
        for (i = 0; i < 25; i++) {
            adc_value = Snake_KnobRead();
            if ((u16)(adc_value + KNOB_TURN_DELTA) < last_adc_value ||
                adc_value > (u16)(last_adc_value + KNOB_TURN_DELTA)) {
                selected = (u8)(adc_value / KNOB_LEVEL_STEP);
                if (selected >= LEVEL_COUNT) {
                    selected = LEVEL_COUNT - 1;
                }
                last_adc_value = adc_value;
            }

            open_settings = 0;
            if (Snake_HandleHomeSerialInput(&selected, &selected_mode,
                                            &open_settings)) {
                start_level = selected;
                if (game_mode != selected_mode) {
                    Snake_PersistMarkDirty();
                }
                game_mode = selected_mode;
                Snake_PersistSave();
                Snake_Beep(60);
                Delay_ms(120);
                LCD_Clear(BLACK);
                Snake_AudioStop();
                Snake_KeyReset();
                return;
            }

            if (open_settings) {
                Snake_AudioStop();
                Snake_ShowSettings();
                LCD_Clear(BLACK);
                Snake_ShowHome();
                return;
            }

            if (selected != last_selected) {
                Snake_DrawHomeLevels(selected);
                last_selected = selected;
            }

            if (selected_mode != last_mode) {
                Snake_DrawHomeMode(selected_mode);
                last_mode = selected_mode;
            }

            raw_key = Snake_KeyReadRaw();
            if (raw_key & KEY_RIGHT_MASK) {
                Snake_AudioStop();
                Snake_ShowSettings();
                LCD_Clear(BLACK);
                Snake_ShowHome();
                return;
            }
            if (raw_key & KEY_LEFT_MASK) {
                Snake_AudioStop();
                Snake_ShowRanking();
                LCD_Clear(BLACK);
                Snake_ShowHome();
                return;
            }

            if (raw_key != 0) {
                Snake_Beep(60);
                while (Snake_KeyReadRaw() != 0) {
                    Snake_AnimateHomeSnake(&home_snake_x, &prev_home_snake_x,
                                           &home_snake_valid, &home_anim_tick);
                    Snake_MusicTick(20);
                }
                start_level = selected;
                if (game_mode != selected_mode) {
                    Snake_PersistMarkDirty();
                }
                game_mode = selected_mode;
                Snake_PersistSave();
                Delay_ms(120);
                LCD_Clear(BLACK);
                Snake_AudioStop();
                Snake_KeyReset();
                return;
            }
            Snake_AnimateHomeSnake(&home_snake_x, &prev_home_snake_x,
                                   &home_snake_valid, &home_anim_tick);
            Snake_MusicTick(20);
        }

        blink = (u8)!blink;
        Snake_DrawHomePrompt(blink);
    }
}

static void Snake_ShowDirection(void)
{
    LED1(1);
    LED2(1);
    LED3(1);
    LED4(1);

    if (dir == DIR_UP) {
        LED1(0);
    } else if (dir == DIR_DOWN) {
        LED2(0);
    } else if (dir == DIR_LEFT) {
        LED3(0);
    } else {
        LED4(0);
    }
}

static char Snake_MapCell(u8 x, u8 y)
{
    if (Snake_IsOpenMode()) {
        if (x >= OPEN_WORLD_COLS || y >= OPEN_WORLD_ROWS) {
            return '#';
        }

        if (x == 0 || y == 0 ||
            x == (OPEN_WORLD_COLS - 1) || y == (OPEN_WORLD_ROWS - 1)) {
            return '#';
        }

        if (((x == 8 || x == 31) && y > 5 && y < 50 && (y % 7) != 0) ||
            ((y == 12 || y == 39) && x > 4 && x < 35 && (x % 8) != 0)) {
            return '#';
        }

        if (((x > 4 && x < 14) && (y == 24 || y == 25) && x != 9) ||
            ((x > 25 && x < 36) && (y == 28 || y == 29) && x != 31)) {
            return '#';
        }

        if (((x > 14 && x < 25) && (y == 6 || y == 49) && (x % 5) != 0) ||
            ((y > 16 && y < 23) && (x == 17 || x == 22) && y != 19)) {
            return '#';
        }

        if (((x + y) % 17) == 0 && x > 4 && x < 36 && y > 5 && y < 51) {
            return '#';
        }

        if ((x == 12 && y == 18) || (x == 27 && y == 43)) {
            return 'A';
        }
        if ((x == 34 && y == 9) || (x == 5 && y == 46)) {
            return 'B';
        }

        return '.';
    }

    return level_map[level_index][y][x];
}

static u8 Snake_OnSnake(u8 x, u8 y, u16 limit)
{
    u16 i;

    for (i = 0; i < limit; i++) {
        if (snake_x[i] == x && snake_y[i] == y) {
            return 1;
        }
    }

    return 0;
}

static u8 Snake2_OnSnake(u8 x, u8 y, u16 limit)
{
    u16 i;

    for (i = 0; i < limit; i++) {
        if (snake2_x[i] == x && snake2_y[i] == y) {
            return 1;
        }
    }

    return 0;
}

static u8 Snake_CellCanHoldFood(u8 x, u8 y)
{
    if (Snake_MapCell(x, y) != '.') {
        return 0;
    }

    if (Snake_OnSnake(x, y, snake_len)) {
        return 0;
    }

    if (Snake_IsDuoMode() && Snake2_OnSnake(x, y, snake2_len)) {
        return 0;
    }

    return 1;
}

static u16 Snake_CountWalkableCells(void)
{
    u8 x;
    u8 y;
    u16 count = 0;
    u8 cols = Snake_WorldCols();
    u8 rows = Snake_WorldRows();

    for (y = 0; y < rows; y++) {
        for (x = 0; x < cols; x++) {
            if (Snake_MapCell(x, y) == '.') {
                count++;
            }
        }
    }

    return count;
}

static void Snake_DrawCell(u8 x, u8 y, u16 color)
{
    u16 px = BOARD_X + (u16)x * CELL_W;
    u16 py = BOARD_Y + (u16)y * CELL_H;

    LCD_Fill(px + 1, py + 1, px + CELL_W - 2, py + CELL_H - 2, color);
}

static void Snake_DrawCellAt(u16 board_x, u16 board_y, u8 x, u8 y, u16 color)
{
    u16 px = board_x + (u16)x * CELL_W;
    u16 py = board_y + (u16)y * CELL_H;

    LCD_Fill(px + 1, py + 1, px + CELL_W - 2, py + CELL_H - 2, color);
}

static void Snake_DrawMapTile(u8 x, u8 y, u16 color)
{
    u16 px = BOARD_X + (u16)x * CELL_W;
    u16 py = BOARD_Y + (u16)y * CELL_H;

    LCD_Fill(px, py, px + CELL_W - 1, py + CELL_H - 1, BLACK);
    if (color != BLACK) {
        LCD_Fill(px + 1, py + 1, px + CELL_W - 2, py + CELL_H - 2, color);
    }
}

static u8 Snake_HandleRankingSerialInput(u8 *selected_mode)
{
    SnakeUartCmd cmd;
    u8 should_return = 0;

    while (SnakeUart_PopCommand(&cmd)) {
        if (cmd.type == SNAKE_UART_CMD_RESET ||
            cmd.type == SNAKE_UART_CMD_START ||
            cmd.type == SNAKE_UART_CMD_PAUSE) {
            should_return = 1;
        } else if (cmd.type == SNAKE_UART_CMD_UP ||
                   cmd.type == SNAKE_UART_CMD_LEFT) {
            *selected_mode = (u8)((*selected_mode == 0) ?
                (GAME_MODE_COUNT - 1) : (*selected_mode - 1));
        } else if (cmd.type == SNAKE_UART_CMD_DOWN ||
                   cmd.type == SNAKE_UART_CMD_RIGHT) {
            *selected_mode = (u8)((*selected_mode + 1) % GAME_MODE_COUNT);
        }
    }

    return should_return;
}

static void Snake_DrawRankingTable(u8 selected_mode)
{
    u8 i;
    u16 y;

    LCD_Fill(18, 140, 221, 292, HOME_PANEL);
    Snake_DrawHomeFrame(18, 140, 221, 292, HOME_TEAL);
    Snake_ShowText(36, 150, 12, "RANK", WHITE, HOME_PANEL, 1);
    Snake_ShowText(132, 150, 12, "SCORE", WHITE, HOME_PANEL, 1);

    for (i = 0; i < RANKING_TOP_COUNT; i++) {
        y = (u16)(176 + i * 22);
        POINT_COLOR = HOME_BLUE;
        LCD_DrawLine(30, (u16)(y - 6), 208, (u16)(y - 6));

        POINT_COLOR = HOME_GOLD;
        BACK_COLOR = HOME_PANEL;
        LCD_ShowNum(44, y, (u32)(i + 1), 1, 16);
        Snake_ShowText(58, y, 16, ".", HOME_GOLD, HOME_PANEL, 1);

        if (ranking_scores[selected_mode][i] == 0) {
            Snake_ShowText(126, y, 16, "---", LGRAY, HOME_PANEL, 1);
        } else {
            POINT_COLOR = WHITE;
            BACK_COLOR = HOME_PANEL;
            LCD_ShowNum(126, y, ranking_scores[selected_mode][i], 3, 16);
        }
    }
}

static void Snake_DrawRankingScreen(u8 selected_mode, short snake_x)
{
    u8 i;

    LCD_Clear(BLACK);
    LCD_Fill(0, 0, LCD_W - 1, 78, HOME_NAVY);
    LCD_Fill(0, 78, LCD_W - 1, 124, HOME_BLUE);
    LCD_Fill(0, 124, LCD_W - 1, LCD_H - 1, BLACK);

    POINT_COLOR = GRAYBLUE;
    for (i = 0; i < 6; i++) {
        LCD_DrawLine((u16)(i * 46), 76, (u16)(i * 46 + 24), 122);
    }

    Snake_ShowTextCenter(20, 16, "RANKING", WHITE, HOME_NAVY, 1);
    Snake_ShowTextCenter(44, 12, Snake_RankingModeText(selected_mode),
                         CYAN, HOME_NAVY, 1);

    Snake_DrawRankingTable(selected_mode);
    LCD_Fill(0, 294, LCD_W - 1, LCD_H - 1, BLACK);
    Snake_ShowTextCenter(300, 12, "KNOB MODE  R/ANY KEY BACK",
                         LGRAY, BLACK, 1);
    Snake_DrawHomeSnake(snake_x, 0, snake_x);
}

static void Snake_ShowRanking(void)
{
    u8 selected_mode = game_mode;
    u8 last_mode = 0xff;
    u8 raw;
    u16 knob_value;
    u16 last_knob_value;
    short rank_snake_x = -142;
    short prev_rank_snake_x = -142;
    u8 rank_snake_valid = 0;
    u8 rank_anim_tick = 0;

    Snake_ClearPendingUartCommands();
    while (Snake_KeyReadRaw() != 0) {
        Delay_ms(20);
    }

    last_knob_value = Snake_KnobRead();

    while (1) {
        if (selected_mode != last_mode) {
            Snake_DrawRankingScreen(selected_mode, rank_snake_x);
            rank_snake_valid = 1;
            last_mode = selected_mode;
        }

        knob_value = Snake_KnobRead();
        if ((u16)(knob_value + KNOB_TURN_DELTA) < last_knob_value ||
            knob_value > (u16)(last_knob_value + KNOB_TURN_DELTA)) {
            selected_mode = (u8)(knob_value / KNOB_MODE_STEP);
            if (selected_mode >= GAME_MODE_COUNT) {
                selected_mode = GAME_MODE_COUNT - 1;
            }
            last_knob_value = knob_value;
        }

        if (Snake_HandleRankingSerialInput(&selected_mode)) {
            LCD_Clear(BLACK);
            Snake_KeyReset();
            Snake_KnobReset();
            return;
        }

        raw = Snake_KeyReadRaw();
        if (raw != 0) {
            while (Snake_KeyReadRaw() != 0) {
                Delay_ms(20);
            }
            LCD_Clear(BLACK);
            Snake_KeyReset();
            Snake_KnobReset();
            return;
        }

        Snake_AnimateHomeSnake(&rank_snake_x, &prev_rank_snake_x,
                               &rank_snake_valid, &rank_anim_tick);
        Delay_ms(40);
    }
}

static void Snake_DrawMapTileAt(u16 board_x, u16 board_y, u8 x, u8 y, u16 color)
{
    u16 px = board_x + (u16)x * CELL_W;
    u16 py = board_y + (u16)y * CELL_H;

    LCD_Fill(px, py, px + CELL_W - 1, py + CELL_H - 1, BLACK);
    if (color != BLACK) {
        LCD_Fill(px + 1, py + 1, px + CELL_W - 2, py + CELL_H - 2, color);
    }
}

static u8 Snake_WorldToViewAt(u8 wx, u8 wy, u8 base_x, u8 base_y,
                              u8 view_rows, u8 *vx, u8 *vy)
{
    if (wx < base_x || wy < base_y) {
        return 0;
    }

    wx = (u8)(wx - base_x);
    wy = (u8)(wy - base_y);
    if (wx >= GRID_COLS || wy >= view_rows) {
        return 0;
    }

    *vx = wx;
    *vy = wy;
    return 1;
}

static u8 Snake_WorldToView(u8 wx, u8 wy, u8 *vx, u8 *vy)
{
    return Snake_WorldToViewAt(wx, wy, viewport_x, viewport_y,
                               GRID_ROWS, vx, vy);
}

static void Snake_DrawWorldCell(u8 x, u8 y, u16 color)
{
    u8 vx;
    u8 vy;

    if (Snake_WorldToView(x, y, &vx, &vy)) {
        Snake_DrawCell(vx, vy, color);
    }
}

static void Snake_DrawWorldCellAt(u16 board_x, u16 board_y, u8 view_x,
                                  u8 view_y, u8 view_rows,
                                  u8 x, u8 y, u16 color)
{
    u8 vx;
    u8 vy;

    if (Snake_WorldToViewAt(x, y, view_x, view_y, view_rows, &vx, &vy)) {
        Snake_DrawCellAt(board_x, board_y, vx, vy, color);
    }
}

static void Snake_DrawMapCell(u8 x, u8 y)
{
    char c = Snake_MapCell(x, y);
    u8 vx;
    u8 vy;

    if (!Snake_WorldToView(x, y, &vx, &vy)) {
        return;
    }

    if (c == '#') {
        Snake_DrawMapTile(vx, vy, GRAY);
    } else if (c == 'A') {
        Snake_DrawMapTile(vx, vy, CYAN);
    } else if (c == 'B') {
        Snake_DrawMapTile(vx, vy, MAGENTA);
    } else {
        Snake_DrawMapTile(vx, vy, BLACK);
    }
}

static void Snake_DrawMapCellAt(u16 board_x, u16 board_y, u8 view_x,
                                u8 view_y, u8 view_rows, u8 x, u8 y)
{
    char c = Snake_MapCell(x, y);
    u16 color = BLACK;

    if (c == '#') {
        color = GRAY;
    } else if (c == 'A') {
        color = CYAN;
    } else if (c == 'B') {
        color = MAGENTA;
    }

    if (Snake_WorldToViewAt(x, y, view_x, view_y, view_rows, &x, &y)) {
        Snake_DrawMapTileAt(board_x, board_y, x, y, color);
    }
}

static void Snake_DrawFood(void)
{
    if (food_type == FOOD_POISON) {
        Snake_DrawWorldCell(food_x, food_y, MAGENTA);
    } else if (food_type == FOOD_BONUS) {
        Snake_DrawWorldCell(food_x, food_y, CYAN);
    } else {
        Snake_DrawWorldCell(food_x, food_y, RED);
    }
}

static void Snake_DrawFoodAt(u16 board_x, u16 board_y, u8 view_x,
                             u8 view_y, u8 view_rows)
{
    u16 color = RED;

    if (food_type == FOOD_POISON) {
        color = MAGENTA;
    } else if (food_type == FOOD_BONUS) {
        color = CYAN;
    }

    Snake_DrawWorldCellAt(board_x, board_y, view_x, view_y, view_rows,
                          food_x, food_y, color);
}

static void Snake_SaveRenderState(void)
{
    prev_snake_len = snake_len;
    if (snake_len != 0) {
        prev_snake_head_x = snake_x[0];
        prev_snake_head_y = snake_y[0];
        prev_snake_tail_x = snake_x[snake_len - 1];
        prev_snake_tail_y = snake_y[snake_len - 1];
    }
    prev_snake2_len = snake2_len;
    if (snake2_len != 0) {
        prev_snake2_head_x = snake2_x[0];
        prev_snake2_head_y = snake2_y[0];
        prev_snake2_tail_x = snake2_x[snake2_len - 1];
        prev_snake2_tail_y = snake2_y[snake2_len - 1];
    }
    prev_food_x = food_x;
    prev_food_y = food_y;
    prev_viewport_x = viewport_x;
    prev_viewport_y = viewport_y;
    prev_viewport2_x = viewport2_x;
    prev_viewport2_y = viewport2_y;
}

static void Snake_UpdateOneViewport(u8 head_x, u8 head_y, u8 view_rows,
                                    u8 *view_x, u8 *view_y)
{
    u8 cols = Snake_WorldCols();
    u8 rows = Snake_WorldRows();
    u8 max_x = (cols > GRID_COLS) ? (u8)(cols - GRID_COLS) : 0;
    u8 max_y = (rows > view_rows) ? (u8)(rows - view_rows) : 0;
    u8 y_margin = (view_rows > 10) ? 5 : 2;

    if (head_x < *view_x + 4) {
        *view_x = (head_x > 4) ? (u8)(head_x - 4) : 0;
    } else if (head_x >= *view_x + GRID_COLS - 4) {
        *view_x = (u8)(head_x - GRID_COLS + 5);
    }

    if (head_y < *view_y + y_margin) {
        *view_y = (head_y > y_margin) ? (u8)(head_y - y_margin) : 0;
    } else if (head_y >= *view_y + view_rows - y_margin) {
        *view_y = (u8)(head_y - view_rows + y_margin + 1);
    }

    if (*view_x > max_x) {
        *view_x = max_x;
    }
    if (*view_y > max_y) {
        *view_y = max_y;
    }
}

static void Snake_UpdateViewport(void)
{
    if (!Snake_IsOpenMode()) {
        viewport_x = 0;
        viewport_y = 0;
        viewport2_x = 0;
        viewport2_y = 0;
        return;
    }

    Snake_UpdateOneViewport(snake_x[0], snake_y[0],
                            Snake_IsOpenDuoMode() ? OPEN_DUO_ROWS : GRID_ROWS,
                            &viewport_x, &viewport_y);
    if (Snake_IsOpenDuoMode()) {
        Snake_UpdateOneViewport(snake2_x[0], snake2_y[0],
                                OPEN_DUO_ROWS, &viewport2_x, &viewport2_y);
    } else {
        viewport2_x = viewport_x;
        viewport2_y = viewport_y;
    }
}

static void Snake_SetStatus(const char *msg)
{
    status_msg = msg;
}

static void Snake_DrawHeader(void)
{
    LCD_Fill(0, 0, LCD_W - 1, BOARD_Y - 8, DARKBLUE);
    POINT_COLOR = WHITE;
    BACK_COLOR = DARKBLUE;

    LCD_ShowString(6, 4, 16, (u8 *)"SnakeLab", 0);
    LCD_ShowString(88, 4, 16, (u8 *)"L", 0);
    LCD_ShowNum(102, 4, (u32)(level_index + 1), 1, 16);
    LCD_ShowString(122, 4, 16, (u8 *)level_name[level_index], 0);

    if (Snake_IsDuoMode()) {
        LCD_ShowString(6, 24, 16, (u8 *)"P1:", 0);
        LCD_ShowNum(38, 24, score, 3, 16);
        LCD_ShowString(88, 24, 16, (u8 *)"P2:", 0);
        LCD_ShowNum(120, 24, score2, 3, 16);
    } else {
        LCD_ShowString(6, 24, 16, (u8 *)"S:", 0);
        LCD_ShowNum(28, 24, score, 3, 16);
        LCD_ShowString(70, 24, 16, (u8 *)"B:", 0);
        LCD_ShowNum(92, 24, high_score, 3, 16);
        LCD_ShowString(134, 24, 16, (u8 *)"HP:", 0);
        LCD_ShowNum(166, 24, lives, 1, 16);
    }
    if (Snake_IsOpenMode()) {
        LCD_ShowString(160, 24, 16, (u8 *)(Snake_IsOpenDuoMode() ? "V1:" : "M:"), 0);
        LCD_ShowNum(186, 24, viewport_x, 2, 16);
        LCD_ShowString(202, 24, 16, (u8 *)",", 0);
        LCD_ShowNum(210, 24, viewport_y, 2, 16);
    } else {
        LCD_ShowString(190, 24, 16, (u8 *)"T:", 0);
        if (game_mode == GAME_MODE_CLASSIC || Snake_IsDuoMode() ||
            level_time_limit[level_index] == 0) {
            LCD_ShowString(212, 24, 16, (u8 *)"--", 0);
        } else {
            LCD_ShowNum(212, 24, time_left, 2, 16);
        }
    }

    LCD_ShowString(6, 48, 16, (u8 *)status_msg, 0);
}

static void Snake_DrawPauseHint(void)
{
    LCD_Fill(12, 148, LCD_W - 13, 186, BLACK);
    Snake_ShowTextCenter(160, 16, "Press KEY1 or R to exit", WHITE, BLACK, 0);
}

static void Snake_RenderViewportAt(u16 board_x, u16 board_y, u8 view_x,
                                   u8 view_y, u8 view_rows)
{
    u8 x;
    u8 y;
    u8 wx;
    u8 wy;

    for (y = 0; y < view_rows; y++) {
        for (x = 0; x < GRID_COLS; x++) {
            wx = (u8)(view_x + x);
            wy = (u8)(view_y + y);
            Snake_DrawMapCellAt(board_x, board_y, view_x, view_y,
                                view_rows, wx, wy);
        }
    }

    POINT_COLOR = GRAY;
    LCD_DrawRectangle(board_x - 1, board_y - 1,
                      board_x + GRID_COLS * CELL_W,
                      board_y + view_rows * CELL_H);
}

static void Snake_RenderBoard(void)
{
    u8 x;
    u8 y;
    u8 wx;
    u8 wy;

    if (!Snake_IsOpenMode() || force_board_clear) {
        LCD_Fill(BOARD_X, BOARD_Y, BOARD_X + GRID_COLS * CELL_W - 1,
                 BOARD_Y + GRID_ROWS * CELL_H - 1, BLACK);
        force_board_clear = 0;
    }

    for (y = 0; y < GRID_ROWS; y++) {
        for (x = 0; x < GRID_COLS; x++) {
            wx = (u8)(viewport_x + x);
            wy = (u8)(viewport_y + y);
            Snake_DrawMapCell(wx, wy);
        }
    }

    POINT_COLOR = GRAY;
    LCD_DrawRectangle(BOARD_X - 1, BOARD_Y - 1,
                      BOARD_X + GRID_COLS * CELL_W,
                      BOARD_Y + GRID_ROWS * CELL_H);
}

static void Snake_DrawOpenDuoLabels(void)
{
    POINT_COLOR = WHITE;
    BACK_COLOR = BLACK;
    LCD_ShowString(4, OPEN_DUO_P1_Y + 38, 16, (u8 *)"P1", 0);
    LCD_ShowString(4, OPEN_DUO_P2_Y + 38, 16, (u8 *)"P2", 0);
}

static void Snake_DrawSnakeOnViewport(u16 board_x, u16 board_y, u8 view_x,
                                      u8 view_y, u8 view_rows,
                                      u8 *sx, u8 *sy, u16 len,
                                      u16 head_color, u16 body_color)
{
    u16 i;

    for (i = 0; i < len; i++) {
        Snake_DrawWorldCellAt(board_x, board_y, view_x, view_y, view_rows,
                              sx[i], sy[i], (i == 0) ? head_color : body_color);
    }
}

static void Snake_RenderOpenDuo(void)
{
    Snake_UpdateViewport();
    Snake_DrawHeader();
    LCD_Fill(0, BOARD_Y - 4, LCD_W - 1, LCD_H - 1, BLACK);

    Snake_RenderViewportAt(BOARD_X, OPEN_DUO_P1_Y, viewport_x, viewport_y,
                           OPEN_DUO_ROWS);
    Snake_DrawFoodAt(BOARD_X, OPEN_DUO_P1_Y, viewport_x, viewport_y,
                     OPEN_DUO_ROWS);
    Snake_DrawSnakeOnViewport(BOARD_X, OPEN_DUO_P1_Y, viewport_x, viewport_y,
                              OPEN_DUO_ROWS, snake_x, snake_y, snake_len,
                              YELLOW, GREEN);
    Snake_DrawSnakeOnViewport(BOARD_X, OPEN_DUO_P1_Y, viewport_x, viewport_y,
                              OPEN_DUO_ROWS, snake2_x, snake2_y, snake2_len,
                              MAGENTA, CYAN);

    Snake_RenderViewportAt(BOARD_X, OPEN_DUO_P2_Y, viewport2_x, viewport2_y,
                           OPEN_DUO_ROWS);
    Snake_DrawFoodAt(BOARD_X, OPEN_DUO_P2_Y, viewport2_x, viewport2_y,
                     OPEN_DUO_ROWS);
    Snake_DrawSnakeOnViewport(BOARD_X, OPEN_DUO_P2_Y, viewport2_x, viewport2_y,
                              OPEN_DUO_ROWS, snake_x, snake_y, snake_len,
                              YELLOW, GREEN);
    Snake_DrawSnakeOnViewport(BOARD_X, OPEN_DUO_P2_Y, viewport2_x, viewport2_y,
                              OPEN_DUO_ROWS, snake2_x, snake2_y, snake2_len,
                              MAGENTA, CYAN);
    Snake_DrawOpenDuoLabels();
    Snake_SaveRenderState();
}

static void Snake_Render(void)
{
    u16 i;

    if (Snake_IsOpenDuoMode()) {
        Snake_RenderOpenDuo();
        return;
    }

    Snake_UpdateViewport();
    Snake_DrawHeader();
    Snake_RenderBoard();
    Snake_DrawFood();

    for (i = 0; i < snake_len; i++) {
        if (i == 0) {
            Snake_DrawWorldCell(snake_x[i], snake_y[i], YELLOW);
        } else {
            Snake_DrawWorldCell(snake_x[i], snake_y[i], GREEN);
        }
    }

    if (Snake_IsDuoMode()) {
        for (i = 0; i < snake2_len; i++) {
            if (i == 0) {
                Snake_DrawWorldCell(snake2_x[i], snake2_y[i], MAGENTA);
            } else {
                Snake_DrawWorldCell(snake2_x[i], snake2_y[i], CYAN);
            }
        }
    }

    Snake_SaveRenderState();
}

static void Snake_RestoreCell(u8 x, u8 y)
{
    Snake_DrawMapCell(x, y);
    if (x == food_x && y == food_y) {
        Snake_DrawFood();
    }
}

static void Snake_RestoreCellAt(u16 board_x, u16 board_y, u8 view_x,
                                u8 view_y, u8 view_rows, u8 x, u8 y)
{
    Snake_DrawMapCellAt(board_x, board_y, view_x, view_y, view_rows, x, y);
    if (x == food_x && y == food_y) {
        Snake_DrawFoodAt(board_x, board_y, view_x, view_y, view_rows);
    }
}

static void Snake_RenderStepOnViewport(u16 board_x, u16 board_y, u8 view_x,
                                       u8 view_y, u8 view_rows,
                                       u8 food_changed)
{
    if (prev_snake_len != 0) {
        if (snake_len <= prev_snake_len) {
            Snake_RestoreCellAt(board_x, board_y, view_x, view_y, view_rows,
                                prev_snake_tail_x, prev_snake_tail_y);
        }
        Snake_RestoreCellAt(board_x, board_y, view_x, view_y, view_rows,
                            prev_snake_head_x, prev_snake_head_y);
        if (Snake_IsDuoMode() && prev_snake2_len != 0) {
            if (snake2_len <= prev_snake2_len) {
                Snake_RestoreCellAt(board_x, board_y, view_x, view_y, view_rows,
                                    prev_snake2_tail_x, prev_snake2_tail_y);
            }
            Snake_RestoreCellAt(board_x, board_y, view_x, view_y, view_rows,
                                prev_snake2_head_x, prev_snake2_head_y);
        }
    }

    if (food_changed) {
        Snake_RestoreCellAt(board_x, board_y, view_x, view_y, view_rows,
                            prev_food_x, prev_food_y);
        Snake_DrawFoodAt(board_x, board_y, view_x, view_y, view_rows);
    }

    if (snake_len > 1) {
        Snake_DrawWorldCellAt(board_x, board_y, view_x, view_y, view_rows,
                              snake_x[1], snake_y[1], GREEN);
    }
    Snake_DrawWorldCellAt(board_x, board_y, view_x, view_y, view_rows,
                          snake_x[0], snake_y[0], YELLOW);
    if (Snake_IsDuoMode()) {
        if (snake2_len > 1) {
            Snake_DrawWorldCellAt(board_x, board_y, view_x, view_y, view_rows,
                                  snake2_x[1], snake2_y[1], CYAN);
        }
        Snake_DrawWorldCellAt(board_x, board_y, view_x, view_y, view_rows,
                              snake2_x[0], snake2_y[0], MAGENTA);
    }
}

static void Snake_RenderStep(u8 food_changed)
{
    Snake_UpdateViewport();
    if (Snake_IsOpenDuoMode()) {
        if (viewport_x != prev_viewport_x || viewport_y != prev_viewport_y ||
            viewport2_x != prev_viewport2_x || viewport2_y != prev_viewport2_y ||
            prev_snake_len == 0) {
            Snake_Render();
            return;
        }

        Snake_RenderStepOnViewport(BOARD_X, OPEN_DUO_P1_Y, viewport_x,
                                   viewport_y, OPEN_DUO_ROWS, food_changed);
        Snake_RenderStepOnViewport(BOARD_X, OPEN_DUO_P2_Y, viewport2_x,
                                   viewport2_y, OPEN_DUO_ROWS, food_changed);
        Snake_DrawOpenDuoLabels();
        Snake_DrawHeader();
        Snake_SaveRenderState();
        return;
    }

    if (viewport_x != prev_viewport_x || viewport_y != prev_viewport_y) {
        Snake_Render();
        return;
    }

    if (prev_snake_len != 0) {
        if (snake_len <= prev_snake_len) {
            Snake_RestoreCell(prev_snake_tail_x, prev_snake_tail_y);
        }
        Snake_RestoreCell(prev_snake_head_x, prev_snake_head_y);
        if (Snake_IsDuoMode() && prev_snake2_len != 0) {
            if (snake2_len <= prev_snake2_len) {
                Snake_RestoreCell(prev_snake2_tail_x, prev_snake2_tail_y);
            }
            Snake_RestoreCell(prev_snake2_head_x, prev_snake2_head_y);
        }
    } else {
        Snake_Render();
        return;
    }

    if (food_changed) {
        Snake_RestoreCell(prev_food_x, prev_food_y);
        Snake_DrawFood();
    }

    if (snake_len > 1) {
        Snake_DrawWorldCell(snake_x[1], snake_y[1], GREEN);
    }
    Snake_DrawWorldCell(snake_x[0], snake_y[0], YELLOW);
    if (Snake_IsDuoMode()) {
        if (snake2_len > 1) {
            Snake_DrawWorldCell(snake2_x[1], snake2_y[1], CYAN);
        }
        Snake_DrawWorldCell(snake2_x[0], snake2_y[0], MAGENTA);
    }
    Snake_DrawHeader();
    Snake_SaveRenderState();
}

static void Snake_ShowCenter(const char *line1, const char *line2)
{
    LCD_Fill(12, 124, LCD_W - 13, 206, BLACK);
    Snake_ShowTextCenter(136, 24, line1, YELLOW, BLACK, 0);
    Snake_ShowTextCenter(176, 16, line2, WHITE, BLACK, 0);
}

static u8 Snake_ResultReturnRequested(void)
{
    SnakeUartCmd cmd;

    if (GPIO_ReadInputDataBit(GPIOD, GPIO_Pin_11) == 0) {
        return 1;
    }

    while (SnakeUart_PopCommand(&cmd)) {
        if (cmd.type == SNAKE_UART_CMD_RESET) {
            return 1;
        }
    }

    return 0;
}

static u16 Snake_FindOnSnakeIndex(u8 x, u8 y, u16 limit)
{
    u16 i;

    for (i = 0; i < limit; i++) {
        if (snake_x[i] == x && snake_y[i] == y) {
            return i;
        }
    }

    return SNAKE_NO_INDEX;
}

static void Snake_ClearPendingUartCommands(void)
{
    SnakeUartCmd cmd;

    while (SnakeUart_PopCommand(&cmd)) {
    }
}

static void Snake_WaitReturnHome(void)
{
    Snake_AudioStop();
    restart_request = 0;
    Snake_ClearPendingUartCommands();

    while (GPIO_ReadInputDataBit(GPIOD, GPIO_Pin_11) == 0) {
        Delay_ms(20);
    }

    while (!Snake_ResultReturnRequested()) {
        Delay_ms(20);
    }

    while (GPIO_ReadInputDataBit(GPIOD, GPIO_Pin_11) == 0) {
        Delay_ms(20);
    }

    Delay_ms(250);
    Snake_KeyReset();
    LCD_Clear(BLACK);
    Snake_ShowHome();
}

static void Snake_PlaceFood(void)
{
    u16 tries;
    u8 x;
    u8 y;
    u8 cols = Snake_WorldCols();
    u8 rows = Snake_WorldRows();

    for (tries = 0; tries < 1000; tries++) {
        x = (u8)Snake_Rand(cols);
        y = (u8)Snake_Rand(rows);
        if (Snake_CellCanHoldFood(x, y)) {
            food_x = x;
            food_y = y;
            break;
        }
    }

    if (tries >= 1000) {
        for (y = 0; y < rows; y++) {
            for (x = 0; x < cols; x++) {
                if (Snake_CellCanHoldFood(x, y)) {
                    food_x = x;
                    food_y = y;
                    y = rows;
                    break;
                }
            }
        }
    }

    if (Snake_IsOpenMode()) {
        u8 r = (u8)Snake_Rand(12);
        if (r == 0) {
            food_type = FOOD_POISON;
        } else if (r == 11 || r == 10) {
            food_type = FOOD_BONUS;
        } else {
            food_type = FOOD_NORMAL;
        }
    } else if (level_index == 3) {
        u8 r = (u8)Snake_Rand(10);
        if (r < 2) {
            food_type = FOOD_POISON;
        } else if (r == 9) {
            food_type = FOOD_BONUS;
        } else {
            food_type = FOOD_NORMAL;
        }
    } else {
        food_type = FOOD_NORMAL;
    }
}

static void Snake_ResetSnake(void)
{
    snake_len = 4;
    snake2_len = 4;
    prev_snake_len = 0;
    prev_snake2_len = 0;
    dir = DIR_RIGHT;
    next_dir = DIR_RIGHT;
    dir2 = DIR_LEFT;
    next_dir2 = DIR_LEFT;
    turn_pending = 0;
    turn_pending2 = 0;
    duo_winner = 0;
    paused = 0;
    restart_request = 0;
    return_home_request = 0;
    force_board_clear = 0;
    time_acc_ms = 0;
    viewport_x = 0;
    viewport_y = 0;
    prev_viewport_x = 0xff;
    prev_viewport_y = 0xff;
    viewport2_x = 0;
    viewport2_y = 0;
    prev_viewport2_x = 0xff;
    prev_viewport2_y = 0xff;

    if (Snake_IsOpenMode()) {
        snake_x[0] = 20;
        snake_y[0] = 28;
        snake_x[1] = 19;
        snake_y[1] = 28;
        snake_x[2] = 18;
        snake_y[2] = 28;
        snake_x[3] = 17;
        snake_y[3] = 28;

        snake2_x[0] = 24;
        snake2_y[0] = 28;
        snake2_x[1] = 25;
        snake2_y[1] = 28;
        snake2_x[2] = 26;
        snake2_y[2] = 28;
        snake2_x[3] = 27;
        snake2_y[3] = 28;
    } else {
        snake_x[0] = 7;
        snake_y[0] = 10;
        snake_x[1] = 6;
        snake_y[1] = 10;
        snake_x[2] = 5;
        snake_y[2] = 10;
        snake_x[3] = 4;
        snake_y[3] = 10;

        snake2_x[0] = 11;
        snake2_y[0] = 10;
        snake2_x[1] = 12;
        snake2_y[1] = 10;
        snake2_x[2] = 13;
        snake2_y[2] = 10;
        snake2_x[3] = 14;
        snake2_y[3] = 10;
    }

    Snake_KeyReset();
    Snake_KnobReset();
    Snake_ShowDirection();
}

static void Snake_StartLevel(u8 lv)
{
    level_index = lv;
    level_score = 0;
    if (Snake_IsBattleMode()) {
        time_left = 0;
        classic_target_len = 0;
        Snake_BattleStart();
        return;
    }
    time_left = (game_mode == GAME_MODE_CLASSIC || Snake_IsOpenMode()) ?
                0 : level_time_limit[level_index];
    classic_target_len = Snake_CountWalkableCells();
    if (Snake_IsDuoMode()) {
        Snake_SetStatus(Snake_IsOpenMode() ? "OPEN DUO" : "P1 WASD P2 1235");
    } else if (Snake_IsOpenMode()) {
        Snake_SetStatus("OPEN MAP");
    } else {
        Snake_SetStatus(game_mode == GAME_MODE_CLASSIC ? "CLASSIC" : "K1+K2 Pause");
    }
    Snake_MusicSetLevel(level_index);
    Snake_ResetSnake();
    rng_state ^= 0x5a5a0000u + score + GPIO_ReadInputData(GPIOA);
    Snake_PlaceFood();
    Snake_Render();
}

static void Snake_StartGame(void)
{
    score = 0;
    score2 = 0;
    lives = 3;
    Snake_StartLevel(start_level);
}

static SnakeDir Snake_KeyToDir(u8 key_bit)
{
    if (key_bit == KEY_UP_MASK) return DIR_UP;
    if (key_bit == KEY_DOWN_MASK) return DIR_DOWN;
    if (key_bit == KEY_LEFT_MASK) return DIR_LEFT;
    return DIR_RIGHT;
}

static SnakeDir Snake_RelativeTurn(SnakeDir base, u8 knob_event)
{
    if (knob_event == KNOB_LEFT_EVENT) {
        if (base == DIR_UP) return DIR_LEFT;
        if (base == DIR_DOWN) return DIR_RIGHT;
        if (base == DIR_LEFT) return DIR_DOWN;
        return DIR_UP;
    }

    if (base == DIR_UP) return DIR_RIGHT;
    if (base == DIR_DOWN) return DIR_LEFT;
    if (base == DIR_LEFT) return DIR_UP;
    return DIR_DOWN;
}

static u8 Snake_PopDirectionKey(void)
{
    u8 key = 0;

    if (key_press_latch & KEY_UP_MASK) {
        key = KEY_UP_MASK;
    } else if (key_press_latch & KEY_DOWN_MASK) {
        key = KEY_DOWN_MASK;
    } else if (key_press_latch & KEY_LEFT_MASK) {
        key = KEY_LEFT_MASK;
    } else if (key_press_latch & KEY_RIGHT_MASK) {
        key = KEY_RIGHT_MASK;
    }

    if (key != 0) {
        key_press_latch &= (u8)(~key);
    }

    return key;
}

static BattleDir Snake_BattleFromSnakeDir(SnakeDir d)
{
    if (d == DIR_UP) return BATTLE_DIR_UP;
    if (d == DIR_DOWN) return BATTLE_DIR_DOWN;
    if (d == DIR_LEFT) return BATTLE_DIR_LEFT;
    return BATTLE_DIR_RIGHT;
}

static void Snake_BattleSetInputDir(u8 player, BattleDir d)
{
    if (player >= BATTLE_PLAYER_COUNT) {
        return;
    }
    battle_input.set_dir[player] = 1;
    battle_input.dir[player] = d;
}

static void Snake_BattleUpdateViewport(void)
{
    const BattleSnake *player = &battle_state.snakes[0];
    short vx;
    short vy;
    const short left_margin = 4;
    const short right_margin = GRID_COLS - 5;
    const short top_margin = 5;
    const short bottom_margin = GRID_ROWS - 6;

    if (!player->alive || player->len == 0) {
        return;
    }

    vx = battle_view_x;
    vy = battle_view_y;

    if ((short)player->x[0] < vx + left_margin) {
        vx = (short)player->x[0] - left_margin;
    } else if ((short)player->x[0] > vx + right_margin) {
        vx = (short)player->x[0] - right_margin;
    }

    if ((short)player->y[0] < vy + top_margin) {
        vy = (short)player->y[0] - top_margin;
    } else if ((short)player->y[0] > vy + bottom_margin) {
        vy = (short)player->y[0] - bottom_margin;
    }

    if (vx < 0) vx = 0;
    if (vy < 0) vy = 0;
    if (vx > BATTLE_WORLD_COLS - GRID_COLS) {
        vx = BATTLE_WORLD_COLS - GRID_COLS;
    }
    if (vy > BATTLE_WORLD_ROWS - GRID_ROWS) {
        vy = BATTLE_WORLD_ROWS - GRID_ROWS;
    }

    battle_view_x = (u8)vx;
    battle_view_y = (u8)vy;
    viewport_x = battle_view_x;
    viewport_y = battle_view_y;
}

static u8 Snake_BattleWorldToView(u8 x, u8 y, u8 *vx, u8 *vy)
{
    if (x < battle_view_x || y < battle_view_y) {
        return 0;
    }
    x = (u8)(x - battle_view_x);
    y = (u8)(y - battle_view_y);
    if (x >= GRID_COLS || y >= GRID_ROWS) {
        return 0;
    }
    *vx = x;
    *vy = y;
    return 1;
}

static u8 Snake_BattleFps(void)
{
    if (battle_frame_ms <= BATTLE_FRAME_MS_30FPS) return 30;
    if (battle_frame_ms <= BATTLE_FRAME_MS_15FPS) return 15;
    return 10;
}

static void Snake_BattleSetFrameMs(u16 frame_ms)
{
    battle_frame_ms = frame_ms;
    battle_header_dirty = 1;
    if (frame_ms == BATTLE_FRAME_MS_30FPS) {
        Snake_SetStatus("FPS 30");
    } else if (frame_ms == BATTLE_FRAME_MS_15FPS) {
        Snake_SetStatus("FPS 15");
    } else {
        Snake_SetStatus("FPS 10");
    }
}

static void Snake_BattleFillBoardRect(short x1, short y1,
                                      short x2, short y2, u16 color)
{
    short bx1 = BOARD_X + 1;
    short by1 = BOARD_Y + 1;
    short bx2 = BOARD_X + GRID_COLS * CELL_W - 2;
    short by2 = BOARD_Y + GRID_ROWS * CELL_H - 2;

    if (x1 > x2 || y1 > y2) {
        return;
    }
    if (x2 < bx1 || y2 < by1 || x1 > bx2 || y1 > by2) {
        return;
    }
    if (x1 < bx1) x1 = bx1;
    if (y1 < by1) y1 = by1;
    if (x2 > bx2) x2 = bx2;
    if (y2 > by2) y2 = by2;

    LCD_Fill((u16)x1, (u16)y1, (u16)x2, (u16)y2, color);
}

static void Snake_BattleDrawSpotAt(short px, short py, u16 color, u8 inset)
{
    u8 r;

    r = (CELL_H / 2 > inset) ? (u8)(CELL_H / 2 - inset) : 1;
    if (r > 5) {
        r = 5;
    }
    if (px - r < BOARD_X + 1 || py - r < BOARD_Y + 1 ||
        px + r > BOARD_X + GRID_COLS * CELL_W - 2 ||
        py + r > BOARD_Y + GRID_ROWS * CELL_H - 2) {
        return;
    }
    gui_circle(px, py, color, r, 1);
}

static void Snake_BattleDrawDotRadius(short px, short py, u16 color, u8 radius)
{
    if (px - radius < BOARD_X + 1 || py - radius < BOARD_Y + 1 ||
        px + radius > BOARD_X + GRID_COLS * CELL_W - 2 ||
        py + radius > BOARD_Y + GRID_ROWS * CELL_H - 2) {
        return;
    }
    gui_circle(px, py, color, radius, 1);
}

static void Snake_BattleDrawSpot(u8 vx, u8 vy, u16 color, u8 inset)
{
    short px = (short)(BOARD_X + (u16)vx * CELL_W + CELL_W / 2);
    short py = (short)(BOARD_Y + (u16)vy * CELL_H + CELL_H / 2);

    Snake_BattleDrawSpotAt(px, py, color, inset);
}

static u16 Snake_BattlePelletColor(const BattlePellet *pellet)
{
    static const u16 colors[6] = {
        RED, YELLOW, GREEN, CYAN, MAGENTA, BRRED
    };

    if (pellet->type == BATTLE_PELLET_CORPSE) {
        return HOME_GOLD;
    }
    return colors[pellet->color % 6];
}

static u16 Snake_BattleSnakeBodyColor(u8 player)
{
    static const u16 colors[BATTLE_PLAYER_COUNT] = {
        GREEN, CYAN, BROWN, MAGENTA, BRRED
    };
    return colors[player % BATTLE_PLAYER_COUNT];
}

static u16 Snake_BattleSnakeShadowColor(u8 player)
{
    static const u16 colors[BATTLE_PLAYER_COUNT] = {
        HOME_TEAL, GRAYBLUE, BROWN, DARKBLUE, RED
    };
    return colors[player % BATTLE_PLAYER_COUNT];
}

static u16 Snake_BattleSnakeHeadColor(u8 player)
{
    static const u16 colors[BATTLE_PLAYER_COUNT] = {
        YELLOW, MAGENTA, HOME_GOLD, LIGHTBLUE, RED
    };
    return colors[player % BATTLE_PLAYER_COUNT];
}

static u8 Snake_BattleNailStage(u16 len)
{
    if (len < 12) return 0;
    if (len < 24) return 1;
    if (len < 45) return 2;
    return 3;
}

static void Snake_BattleDrawNailHeadAt(short px, short py, u16 len)
{
    u8 stage = Snake_BattleNailStage(len);
    u16 body = (stage < 2) ? YELLOW : HOME_GOLD;
    u8 r = (u8)(4 + stage);

    if (px - r < BOARD_X + 1 || py - r < BOARD_Y + 1 ||
        px + r > BOARD_X + GRID_COLS * CELL_W - 2 ||
        py + r > BOARD_Y + GRID_ROWS * CELL_H - 2) {
        return;
    }
    gui_circle(px, py, body, r, 1);
    gui_circle(px - 3, py - 2, GREEN, 1, 1);
    gui_circle(px + 3, py - 2, GREEN, 1, 1);
    Snake_BattleFillBoardRect((short)(px - 5), (short)(py + 2),
                              (short)(px - 3), (short)(py + 4), BRRED);
    Snake_BattleFillBoardRect((short)(px + 3), (short)(py + 2),
                              (short)(px + 5), (short)(py + 4), BRRED);
    if (stage == 0) {
        Snake_BattleFillBoardRect((short)(px - 4), (short)(py - r - 1),
                                  (short)(px - 3), (short)(py - r), WHITE);
        Snake_BattleFillBoardRect((short)(px + 3), (short)(py - r - 1),
                                  (short)(px + 4), (short)(py - r), WHITE);
    } else {
        Snake_BattleFillBoardRect((short)(px - 5), (short)(py - r - 2),
                                  (short)(px - 3), (short)(py - r), WHITE);
        Snake_BattleFillBoardRect((short)(px + 3), (short)(py - r - 2),
                                  (short)(px + 5), (short)(py - r), WHITE);
    }
    if (stage >= 2) {
        Snake_BattleFillBoardRect((short)(px - 1), (short)(py - r - 3),
                                  (short)(px + 1), (short)(py - r - 1),
                                  BRRED);
    }
}

static void Snake_BattleAdvanceCell(BattleDir dir, short *x, short *y)
{
    if (dir == BATTLE_DIR_UP) (*y)--;
    else if (dir == BATTLE_DIR_DOWN) (*y)++;
    else if (dir == BATTLE_DIR_LEFT) (*x)--;
    else (*x)++;
}

static u16 Snake_BattleStepInterval(const BattleSnake *snake)
{
    return snake->boost_on ? BATTLE_BOOST_STEP_MS : BATTLE_NORMAL_STEP_MS;
}

static u8 Snake_BattleInterpAlpha(const BattleSnake *snake)
{
    u16 interval = Snake_BattleStepInterval(snake);
    u32 alpha;

    if (interval == 0) {
        return 0;
    }
    alpha = ((u32)snake->move_acc_ms * BATTLE_INTERP_SCALE) / interval;
    if (alpha > BATTLE_INTERP_SCALE) {
        alpha = BATTLE_INTERP_SCALE;
    }
    return (u8)alpha;
}

static u8 Snake_BattleInterpolatedCenter(const BattleSnake *snake, u16 index,
                                         short *px, short *py)
{
    short sx;
    short sy;
    short tx;
    short ty;
    short dx;
    short dy;
    u8 alpha;

    if (index >= snake->len) {
        return 0;
    }

    sx = snake->x[index];
    sy = snake->y[index];
    tx = sx;
    ty = sy;

    if (index == 0) {
        tx = sx;
        ty = sy;
        Snake_BattleAdvanceCell(snake->next_dir, &tx, &ty);
    } else {
        tx = snake->x[index - 1];
        ty = snake->y[index - 1];
    }

    dx = (short)(tx - sx);
    dy = (short)(ty - sy);
    alpha = Snake_BattleInterpAlpha(snake);

    *px = (short)(BOARD_X +
                  ((sx - (short)battle_view_x) * CELL_W) +
                  CELL_W / 2 +
                  (dx * CELL_W * alpha) / BATTLE_INTERP_SCALE);
    *py = (short)(BOARD_Y +
                  ((sy - (short)battle_view_y) * CELL_H) +
                  CELL_H / 2 +
                  (dy * CELL_H * alpha) / BATTLE_INTERP_SCALE);

    if (*px < BOARD_X - CELL_W || *py < BOARD_Y - CELL_H ||
        *px > BOARD_X + GRID_COLS * CELL_W + CELL_W ||
        *py > BOARD_Y + GRID_ROWS * CELL_H + CELL_H) {
        return 0;
    }
    return 1;
}

static void Snake_BattleDrawLinkAt(short px1, short py1, short px2, short py2,
                                   u16 color, u8 radius)
{
    short dx = (short)(px2 - px1);
    short dy = (short)(py2 - py1);
    short adx = dx < 0 ? (short)-dx : dx;
    short ady = dy < 0 ? (short)-dy : dy;
    short left = px1 < px2 ? px1 : px2;
    short right = px1 > px2 ? px1 : px2;
    short top = py1 < py2 ? py1 : py2;
    short bottom = py1 > py2 ? py1 : py2;
    short steps;
    short i;
    short px;
    short py;

    if (adx > 1 && ady > 1) {
        steps = (short)(((adx > ady ? adx : ady) / 4) + 1);
        for (i = 0; i <= steps; i++) {
            px = (short)(px1 + (dx * i) / steps);
            py = (short)(py1 + (dy * i) / steps);
            Snake_BattleDrawDotRadius(px, py, color, radius);
        }
        return;
    }

    Snake_BattleFillBoardRect((short)(left - radius), (short)(top - radius),
                              (short)(right + radius), (short)(bottom + radius),
                              color);
}

static void Snake_BattleDrawSnakeCell(u8 player, u16 index)
{
    const BattleSnake *snake = &battle_state.snakes[player];
    u8 inset;
    u16 body_color;
    u16 shadow_color;
    short px;
    short py;
    short prev_px;
    short prev_py;

    if (!Snake_BattleInterpolatedCenter(snake, index, &px, &py)) {
        return;
    }

    if (player == 0 && battle_skin != 0) {
        body_color = (Snake_BattleNailStage(snake->len) >= 2) ? HOME_GOLD : YELLOW;
        shadow_color = BROWN;
        if (index > 0 &&
            Snake_BattleInterpolatedCenter(snake, (u16)(index - 1),
                                           &prev_px, &prev_py)) {
            Snake_BattleDrawLinkAt(px, py, prev_px, prev_py, shadow_color, 4);
            Snake_BattleDrawLinkAt(px, py, prev_px, prev_py, body_color, 3);
            Snake_BattleDrawLinkAt(px, py, prev_px, prev_py, WHITE, 1);
        }
        if (index == 0) {
            Snake_BattleDrawNailHeadAt(px, py, snake->len);
        } else {
            inset = (u8)(Snake_BattleNailStage(snake->len) >= 2 ? 2 : 3);
            Snake_BattleDrawSpotAt(px, py, body_color, inset);
            if ((index % 3) == 0) {
                Snake_BattleDrawSpotAt(px, py, WHITE, 5);
            }
        }
        return;
    }

    body_color = Snake_BattleSnakeBodyColor(player);
    shadow_color = Snake_BattleSnakeShadowColor(player);
    if (index > 0 &&
        Snake_BattleInterpolatedCenter(snake, (u16)(index - 1),
                                       &prev_px, &prev_py)) {
        Snake_BattleDrawLinkAt(px, py, prev_px, prev_py, shadow_color, 4);
        Snake_BattleDrawLinkAt(px, py, prev_px, prev_py, body_color, 3);
    }

    Snake_BattleDrawSpotAt(px, py,
                           index == 0 ? Snake_BattleSnakeHeadColor(player) :
                                        body_color,
                           index == 0 ? 2 : 3);
}

static void Snake_BattleBoundsReset(BattleRenderBounds *bounds)
{
    bounds->x1 = 32767;
    bounds->y1 = 32767;
    bounds->x2 = -32768;
    bounds->y2 = -32768;
    bounds->valid = 0;
}

static void Snake_BattleBoundsInclude(BattleRenderBounds *bounds,
                                      short px, short py, u8 radius)
{
    short x1 = (short)(px - radius);
    short y1 = (short)(py - radius);
    short x2 = (short)(px + radius);
    short y2 = (short)(py + radius);

    if (!bounds->valid) {
        bounds->x1 = x1;
        bounds->y1 = y1;
        bounds->x2 = x2;
        bounds->y2 = y2;
        bounds->valid = 1;
        return;
    }

    if (x1 < bounds->x1) bounds->x1 = x1;
    if (y1 < bounds->y1) bounds->y1 = y1;
    if (x2 > bounds->x2) bounds->x2 = x2;
    if (y2 > bounds->y2) bounds->y2 = y2;
}

static void Snake_BattleSnakeBounds(u8 player, BattleRenderBounds *bounds)
{
    const BattleSnake *snake = &battle_state.snakes[player];
    u16 i;
    u16 tail_start;
    short px;
    short py;

    Snake_BattleBoundsReset(bounds);
    if (!snake->alive) {
        return;
    }

    tail_start = (snake->len > 5) ? (u16)(snake->len - 5) : 0;
    for (i = 0; i < snake->len; i++) {
        if (i >= 5 && i < tail_start) {
            continue;
        }
        if (Snake_BattleInterpolatedCenter(snake, i, &px, &py)) {
            Snake_BattleBoundsInclude(bounds, px, py, 14);
        }
    }
}

static void Snake_BattleClearBounds(const BattleRenderBounds *bounds)
{
    if (!bounds->valid) {
        return;
    }
    Snake_BattleFillBoardRect(bounds->x1, bounds->y1,
                              bounds->x2, bounds->y2, BATTLE_BG);
}

static void Snake_BattleDrawPellets(void)
{
    u16 i;
    u8 vx;
    u8 vy;

    for (i = 0; i < BATTLE_MAX_PELLETS; i++) {
        const BattlePellet *pellet = &battle_state.pellets[i];
        if (!pellet->active) {
            continue;
        }
        if (Snake_BattleWorldToView(pellet->x, pellet->y, &vx, &vy)) {
            Snake_BattleDrawSpot(vx, vy, Snake_BattlePelletColor(pellet),
                                 pellet->type == BATTLE_PELLET_CORPSE ? 3 : 4);
        }
    }
}

static void Snake_BattleDrawSnakes(void)
{
    u16 i;
    u8 player;

    for (player = BATTLE_PLAYER_COUNT; player > 0; player--) {
        const BattleSnake *snake = &battle_state.snakes[player - 1];
        if (!snake->alive) {
            continue;
        }
        for (i = snake->len; i > 0; i--) {
            Snake_BattleDrawSnakeCell((u8)(player - 1), (u16)(i - 1));
        }
    }
}

static void Snake_BattleDrawWorldBoundary(void)
{
    short x;
    short y;

    if (battle_view_x == 0) {
        Snake_BattleFillBoardRect(BOARD_X + 1, BOARD_Y + 1,
                                  BOARD_X + 3,
                                  BOARD_Y + GRID_ROWS * CELL_H - 2,
                                  HOME_GOLD);
    }
    if ((u16)battle_view_x + GRID_COLS >= BATTLE_WORLD_COLS) {
        x = (short)(BOARD_X +
                    ((BATTLE_WORLD_COLS - battle_view_x) * CELL_W) - 2);
        Snake_BattleFillBoardRect(x, BOARD_Y + 1,
                                  (short)(x + 2),
                                  BOARD_Y + GRID_ROWS * CELL_H - 2,
                                  HOME_GOLD);
    }
    if (battle_view_y == 0) {
        Snake_BattleFillBoardRect(BOARD_X + 1, BOARD_Y + 1,
                                  BOARD_X + GRID_COLS * CELL_W - 2,
                                  BOARD_Y + 3,
                                  HOME_GOLD);
    }
    if ((u16)battle_view_y + GRID_ROWS >= BATTLE_WORLD_ROWS) {
        y = (short)(BOARD_Y +
                    ((BATTLE_WORLD_ROWS - battle_view_y) * CELL_H) - 2);
        Snake_BattleFillBoardRect(BOARD_X + 1, y,
                                  BOARD_X + GRID_COLS * CELL_W - 2,
                                  (short)(y + 2),
                                  HOME_GOLD);
    }
}

static void Snake_BattleSaveBounds(BattleRenderBounds *bounds)
{
    u8 player;

    for (player = 0; player < BATTLE_PLAYER_COUNT; player++) {
        battle_prev_bounds[player] = bounds[player];
    }
    battle_render_view_x = battle_view_x;
    battle_render_view_y = battle_view_y;
    battle_render_valid = 1;
}

static void Snake_BattleDrawHeader(void)
{
    static u16 last_score = 0xffff;
    static u16 last_len = 0xffff;
    static u16 last_ai_score = 0xffff;
    static u8 last_skin = 0xff;
    static u8 last_fps = 0xff;
    static const char *last_status;
    u16 ai_score = 0;
    u8 fps;
    u8 i;

    for (i = 1; i < BATTLE_PLAYER_COUNT; i++) {
        ai_score = (u16)(ai_score + battle_state.snakes[i].score);
    }
    fps = Snake_BattleFps();

    if (!battle_header_dirty &&
        last_score == battle_state.snakes[0].score &&
        last_len == battle_state.snakes[0].len &&
        last_ai_score == ai_score &&
        last_skin == battle_skin &&
        last_fps == fps &&
        last_status == status_msg) {
        return;
    }

    LCD_Fill(0, 0, LCD_W - 1, BOARD_Y - 8, BATTLE_PANEL);
    POINT_COLOR = BATTLE_TEXT;
    BACK_COLOR = BATTLE_PANEL;
    LCD_ShowString(6, 4, 16, (u8 *)"Battle", 0);
    LCD_ShowString(82, 4, 16, (u8 *)(battle_skin ? "NAIL" : "CLSC"), 0);
    LCD_ShowString(6, 24, 16, (u8 *)"P:", 0);
    LCD_ShowNum(30, 24, battle_state.snakes[0].score, 3, 16);
    LCD_ShowString(74, 24, 16, (u8 *)"L:", 0);
    LCD_ShowNum(98, 24, battle_state.snakes[0].len, 3, 16);
    LCD_ShowString(142, 24, 16, (u8 *)"AI:", 0);
    LCD_ShowNum(176, 24, ai_score, 3, 16);
    LCD_ShowString(206, 24, 16, (u8 *)"F", 0);
    LCD_ShowNum(218, 24, fps, 2, 16);
    LCD_ShowString(6, 48, 16, (u8 *)status_msg, 0);
    last_score = battle_state.snakes[0].score;
    last_len = battle_state.snakes[0].len;
    last_ai_score = ai_score;
    last_skin = battle_skin;
    last_fps = fps;
    last_status = status_msg;
    battle_header_dirty = 0;
}

static void Snake_BattleRender(void)
{
    u8 player;
    BattleRenderBounds current_bounds[BATTLE_PLAYER_COUNT];

    Snake_BattleUpdateViewport();
    Snake_BattleDrawHeader();

    for (player = 0; player < BATTLE_PLAYER_COUNT; player++) {
        Snake_BattleSnakeBounds(player, &current_bounds[player]);
    }

    if (!battle_render_valid || battle_force_full_render ||
        battle_render_view_x != battle_view_x ||
        battle_render_view_y != battle_view_y) {
        LCD_Fill(BOARD_X, BOARD_Y, BOARD_X + GRID_COLS * CELL_W - 1,
                 BOARD_Y + GRID_ROWS * CELL_H - 1, BATTLE_BG);
        Snake_BattleDrawPellets();
        Snake_BattleDrawSnakes();
        Snake_BattleDrawWorldBoundary();
        POINT_COLOR = BATTLE_GRID;
        LCD_DrawRectangle(BOARD_X - 1, BOARD_Y - 1,
                          BOARD_X + GRID_COLS * CELL_W,
                          BOARD_Y + GRID_ROWS * CELL_H);
        Snake_BattleSaveBounds(current_bounds);
        battle_force_full_render = 0;
        return;
    }

    for (player = 0; player < BATTLE_PLAYER_COUNT; player++) {
        Snake_BattleClearBounds(&battle_prev_bounds[player]);
        Snake_BattleClearBounds(&current_bounds[player]);
    }

    Snake_BattleDrawPellets();
    Snake_BattleDrawSnakes();
    Snake_BattleDrawWorldBoundary();

    POINT_COLOR = BATTLE_GRID;
    LCD_DrawRectangle(BOARD_X - 1, BOARD_Y - 1,
                      BOARD_X + GRID_COLS * CELL_W,
                      BOARD_Y + GRID_ROWS * CELL_H);
    Snake_BattleSaveBounds(current_bounds);
}

static void Snake_BattleDrawPauseOverlay(void)
{
    LCD_Fill(32, 132, LCD_W - 33, 196, BATTLE_PANEL);
    POINT_COLOR = CYAN;
    LCD_DrawRectangle(32, 132, LCD_W - 33, 196);
    Snake_ShowTextCenter(144, 16, "PAUSED", BATTLE_TEXT, BATTLE_PANEL, 0);
    Snake_ShowTextCenter(166, 16, "P/SPACE resume", BATTLE_MUTED, BATTLE_PANEL, 0);
    Snake_ShowTextCenter(184, 16, "KEY1 or R home", BATTLE_MUTED, BATTLE_PANEL, 0);
}

static void Snake_BattleTogglePause(void)
{
    paused = (u8)!paused;
    pause_lock = 1;
    battle_pause_cooldown_ms = BATTLE_PAUSE_DEBOUNCE_MS;
    battle_header_dirty = 1;
    Snake_SetStatus(paused ? "PAUSED" : "RUNNING");
    if (paused) {
        Snake_AudioStop();
        Snake_BattleDrawHeader();
        Snake_BattleDrawPauseOverlay();
    }
}

static u8 Snake_BattleCellHitsOther(u8 self, u8 x, u8 y)
{
    u8 player;
    u16 i;

    for (player = 0; player < BATTLE_PLAYER_COUNT; player++) {
        const BattleSnake *snake = &battle_state.snakes[player];
        if (player == self || !snake->alive) {
            continue;
        }
        for (i = 0; i < snake->len; i++) {
            if (snake->x[i] == x && snake->y[i] == y) {
                return 1;
            }
        }
    }
    return 0;
}

static void Snake_BattleAdvance(BattleDir dir, short *x, short *y)
{
    if (dir == BATTLE_DIR_UP) (*y)--;
    else if (dir == BATTLE_DIR_DOWN) (*y)++;
    else if (dir == BATTLE_DIR_LEFT) (*x)--;
    else (*x)++;
}

static int Snake_BattleAiScore(u8 player, BattleDir dir, u8 tx, u8 ty)
{
    const BattleSnake *snake = &battle_state.snakes[player];
    short nx = snake->x[0];
    short ny = snake->y[0];
    int dist;
    int wall;

    if (Battle_IsReverse(snake->dir, dir)) {
        return -30000;
    }
    Snake_BattleAdvance(dir, &nx, &ny);
    if (nx < 0 || ny < 0 || nx >= BATTLE_WORLD_COLS || ny >= BATTLE_WORLD_ROWS) {
        return -30000;
    }
    if (Snake_BattleCellHitsOther(player, (u8)nx, (u8)ny)) {
        return -20000;
    }
    dist = (tx > nx ? tx - nx : nx - tx) + (ty > ny ? ty - ny : ny - ty);
    wall = nx;
    if (ny < wall) wall = ny;
    if ((BATTLE_WORLD_COLS - 1 - nx) < wall) wall = BATTLE_WORLD_COLS - 1 - nx;
    if ((BATTLE_WORLD_ROWS - 1 - ny) < wall) wall = BATTLE_WORLD_ROWS - 1 - ny;
    return wall * 2 - dist * 8;
}

static void Snake_BattleUpdateAi(BattleInput *input)
{
    u8 player;
    u8 i;
    BattleDir dir;

    for (player = 1; player < BATTLE_PLAYER_COUNT; player++) {
        BattleSnake *snake = &battle_state.snakes[player];
        u8 tx = BATTLE_WORLD_COLS / 2;
        u8 ty = BATTLE_WORLD_ROWS / 2;
        int best_target = -30000;
        int best_score = -30000;
        BattleDir best_dir = snake->dir;

        if (!snake->alive || snake->len == 0) {
            continue;
        }
        for (i = 0; i < BATTLE_MAX_PELLETS; i++) {
            const BattlePellet *pellet = &battle_state.pellets[i];
            int dist;
            int score;
            if (!pellet->active) {
                continue;
            }
            dist = (pellet->x > snake->x[0] ? pellet->x - snake->x[0] :
                    snake->x[0] - pellet->x) +
                   (pellet->y > snake->y[0] ? pellet->y - snake->y[0] :
                    snake->y[0] - pellet->y);
            score = pellet->value * 20 - dist;
            if (score > best_target) {
                best_target = score;
                tx = pellet->x;
                ty = pellet->y;
            }
        }
        for (dir = BATTLE_DIR_UP; dir <= BATTLE_DIR_RIGHT; dir++) {
            int score = Snake_BattleAiScore(player, dir, tx, ty);
            if (score > best_score) {
                best_score = score;
                best_dir = dir;
            }
        }
        input->set_dir[player] = 1;
        input->dir[player] = best_dir;
        input->boost[player] = (u8)(snake->len > 16 && best_target < -10);
    }
}

static void Snake_BattleHandleSerial(void)
{
    SnakeUartCmd cmd;
    SnakeDir want;

    while (SnakeUart_PopCommand(&cmd)) {
        if (cmd.type == SNAKE_UART_CMD_RESET) {
            return_home_request = 1;
        } else if (cmd.type == SNAKE_UART_CMD_PAUSE ||
                   cmd.type == SNAKE_UART_CMD_START) {
            if (battle_pause_cooldown_ms == 0) {
                Snake_BattleTogglePause();
            }
        } else if (cmd.type == SNAKE_UART_CMD_LEVEL) {
            if (cmd.value == 0) {
                Snake_BattleSetFrameMs(BATTLE_FRAME_MS_10FPS);
            } else if (cmd.value == 1) {
                Snake_BattleSetFrameMs(BATTLE_FRAME_MS_15FPS);
            } else if (cmd.value == 2) {
                Snake_BattleSetFrameMs(BATTLE_FRAME_MS_30FPS);
            } else if (cmd.value == 3) {
                battle_skin = 0;
                battle_header_dirty = 1;
                battle_force_full_render = 1;
                Snake_SetStatus("CLASSIC SKIN");
            } else if (cmd.value == 4) {
                battle_skin = 1;
                battle_header_dirty = 1;
                battle_force_full_render = 1;
                Snake_SetStatus("NAIL SKIN");
            }
        } else if (Snake_CommandToDir(cmd.type, &want)) {
            Snake_BattleSetInputDir(0, Snake_BattleFromSnakeDir(want));
        }
    }
}

static void Snake_BattleHandleInput(BattleInput *input)
{
    u8 key;
    u8 knob_event;
    SnakeDir want;

    Snake_BattleHandleSerial();
    Snake_KeyScan();
    Snake_KnobScan();
    knob_event = Snake_KnobPopEvent();

    if (paused && (key_press_latch & KEY_UP_MASK)) {
        key_press_latch &= (u8)(~KEY_UP_MASK);
        return_home_request = 1;
        Snake_SetStatus("EXIT HOME");
        battle_header_dirty = 1;
        return;
    }

    if ((key_stable & KEY_PAUSE_MASK) == KEY_PAUSE_MASK) {
        if (!pause_lock) {
            Snake_BattleTogglePause();
        }
        return;
    }
    pause_lock = 0;

    if (paused) {
        return;
    }

    if ((key_stable & (KEY_LEFT_MASK | KEY_RIGHT_MASK)) ==
        (KEY_LEFT_MASK | KEY_RIGHT_MASK)) {
        input->boost[0] = 1;
        key_press_latch &= (u8)(~(KEY_LEFT_MASK | KEY_RIGHT_MASK));
        return;
    }

    if (knob_event != 0) {
        BattleDir base = battle_state.snakes[0].next_dir;
        if (knob_event == KNOB_LEFT_EVENT) {
            if (base == BATTLE_DIR_UP) base = BATTLE_DIR_LEFT;
            else if (base == BATTLE_DIR_DOWN) base = BATTLE_DIR_RIGHT;
            else if (base == BATTLE_DIR_LEFT) base = BATTLE_DIR_DOWN;
            else base = BATTLE_DIR_UP;
        } else {
            if (base == BATTLE_DIR_UP) base = BATTLE_DIR_RIGHT;
            else if (base == BATTLE_DIR_DOWN) base = BATTLE_DIR_LEFT;
            else if (base == BATTLE_DIR_LEFT) base = BATTLE_DIR_UP;
            else base = BATTLE_DIR_DOWN;
        }
        Snake_BattleSetInputDir(0, base);
        return;
    }

    key = Snake_PopDirectionKey();
    if (key != 0) {
        want = Snake_KeyToDir(key);
        Snake_BattleSetInputDir(0, Snake_BattleFromSnakeDir(want));
    }
}

static void Snake_BattleStart(void)
{
    Battle_Init(&battle_state, rng_state ^ GPIO_ReadInputData(GPIOA));
    Battle_ResetInput(&battle_input);
    battle_view_x = 0;
    battle_view_y = 0;
    battle_frame_ms = BATTLE_FRAME_MS_DEFAULT;
    battle_pause_cooldown_ms = 0;
    battle_header_dirty = 1;
    battle_frame_led = 0;
    battle_render_valid = 0;
    battle_force_full_render = 1;
    paused = 0;
    pause_lock = 0;
    restart_request = 0;
    return_home_request = 0;
    Snake_KeyReset();
    Snake_KnobReset();
    Snake_SetStatus("BATTLE AIx4");
    LCD_Clear(BLACK);
    Snake_BattleRender();
}

static void Snake_BattleLoop(void)
{
    while (Snake_IsBattleMode()) {
        Battle_ResetInput(&battle_input);
        Snake_BattleHandleInput(&battle_input);

        if (return_home_request) {
            score = battle_state.snakes[0].score;
            Snake_UpdateBest();
            Snake_RecordModeResults();
            Snake_PersistSave();
            return_home_request = 0;
            LCD_Clear(BLACK);
            Snake_ShowHome();
            return;
        }
        if (restart_request) {
            restart_request = 0;
            Snake_BattleStart();
            continue;
        }
        if (!paused) {
            Snake_BattleUpdateAi(&battle_input);
            Battle_Update(&battle_state, &battle_input, battle_frame_ms);
            if (battle_state.last_events &
                (BATTLE_EVENT_DEATH_P1 | BATTLE_EVENT_DEATH_P2 |
                 BATTLE_EVENT_RESPAWN_P1 | BATTLE_EVENT_RESPAWN_P2 |
                 BATTLE_EVENT_MATCH_END)) {
                battle_force_full_render = 1;
            }
            score = battle_state.snakes[0].score;
            score2 = 0;
            Snake_UpdateBest();
            Snake_BattleRender();
            battle_frame_led = (u8)!battle_frame_led;
            LED5(battle_frame_led);
        }
        Delay_ms(battle_frame_ms);
        if (battle_pause_cooldown_ms > battle_frame_ms) {
            battle_pause_cooldown_ms = (u16)(battle_pause_cooldown_ms -
                                             battle_frame_ms);
        } else {
            battle_pause_cooldown_ms = 0;
        }
    }
}

static void Snake_HandleInput(void)
{
    u8 key;
    u8 knob_event;
    u8 next_level;
    SnakeDir want;

    Snake_HandleSerialInput();
    Snake_KeyScan();
    Snake_KnobScan();
    knob_event = Snake_KnobPopEvent();

    if (paused && (key_press_latch & KEY_UP_MASK)) {
        key_press_latch &= (u8)(~KEY_UP_MASK);
        return_home_request = 1;
        Snake_SetStatus("EXIT HOME");
        Snake_DrawHeader();
        Snake_DrawPauseHint();
        return;
    }

    if ((key_stable & KEY_PAUSE_MASK) == KEY_PAUSE_MASK) {
        if (!pause_lock) {
            Snake_TogglePause();
        }
        return;
    }

    pause_lock = 0;
    if (paused) {
        if (knob_event != 0) {
            if (knob_event == KNOB_LEFT_EVENT) {
                next_level = (level_index == 0) ? (LEVEL_COUNT - 1) : (level_index - 1);
            } else {
                next_level = (u8)((level_index + 1) % LEVEL_COUNT);
            }

            Snake_SelectLevel(next_level, "KNOB LEVEL");
        }
        return;
    }

    if (turn_pending) {
        return;
    }

    if (knob_event != 0) {
        want = Snake_RelativeTurn(next_dir, knob_event);
        next_dir = want;
        turn_pending = 1;
        Snake_SetStatus(knob_event == KNOB_LEFT_EVENT ? "KNOB LEFT" : "KNOB RIGHT");
        Snake_DrawHeader();
        return;
    }

    key = Snake_PopDirectionKey();
    if (key == 0) {
        return;
    }

    want = Snake_KeyToDir(key);
    if (want == next_dir || Snake_IsReverse(dir, want)) {
        return;
    }

    next_dir = want;
    turn_pending = 1;
}

static u16 Snake_StepDelay(void)
{
    u16 delay = 300;
    u16 speed_score = score;

    if (speed_score > 12) {
        speed_score = 12;
    }

    delay = (u16)(300 - speed_score * 14);
    if (level_index >= 3 && delay > 40) {
        delay = (u16)(delay - 30);
    }
    if (delay < 110) {
        delay = 110;
    }

    return delay;
}

static u8 Snake_TimerTick(u16 ms)
{
    if (game_mode == GAME_MODE_CLASSIC || Snake_IsOpenMode()) {
        return 1;
    }

    if (level_time_limit[level_index] == 0) {
        return 1;
    }

    if (Snake_IsDuoMode()) {
        return 1;
    }

    time_acc_ms = (u16)(time_acc_ms + ms);
    while (time_acc_ms >= 1000) {
        time_acc_ms = (u16)(time_acc_ms - 1000);
        if (time_left > 0) {
            time_left--;
            Snake_DrawHeader();
        }
        if (time_left == 0) {
            return 0;
        }
    }

    return 1;
}

static u8 Snake_WaitStep(u16 ms)
{
    u16 elapsed = 0;

    while (elapsed < ms) {
        Snake_HandleInput();

        if (return_home_request) {
            return_home_request = 0;
            Snake_KeyReset();
            LCD_Clear(BLACK);
            Snake_ShowHome();
            Snake_StartGame();
            return 1;
        }

        if (restart_request) {
            return 1;
        }

        if (paused) {
            Delay_ms(20);
            continue;
        }

        Snake_MusicTick(MUSIC_TICK_MS);
        elapsed = (u16)(elapsed + MUSIC_TICK_MS);
        if (!Snake_TimerTick(MUSIC_TICK_MS)) {
            return 0;
        }
    }

    return 1;
}

static u8 Snake_FindOtherPortal(char portal, u8 *x, u8 *y)
{
    u8 ix;
    u8 iy;
    char other = (portal == 'A') ? 'B' : 'A';

    for (iy = 0; iy < GRID_ROWS; iy++) {
        for (ix = 0; ix < GRID_COLS; ix++) {
            if (Snake_MapCell(ix, iy) == other) {
                *x = ix;
                *y = iy;
                return 1;
            }
        }
    }

    return 0;
}

static void Snake_UpdateBest(void)
{
    if (score > high_score) {
        high_score = score;
        Snake_PersistMarkDirty();
    }
}

static u8 Snake_ApplyFood(void)
{
    if (food_type == FOOD_POISON) {
        if (score > 0) {
            score--;
        }
        if (lives > 0) {
            lives--;
        }
        Snake_SetStatus("POISON");
        Snake_PlayTone(NOTE_D4, 120);
        if (lives == 0) {
            return STEP_DEAD;
        }
    } else if (food_type == FOOD_BONUS) {
        score = (u16)(score + 2);
        level_score = (u8)(level_score + 2);
        if (lives < 5) {
            lives++;
        }
        Snake_SetStatus("BONUS");
        Snake_BeepLevel();
    } else {
        score++;
        level_score++;
        Snake_SetStatus("FOOD");
        Snake_PlayTone(NOTE_C6, 30);
    }

    Snake_UpdateBest();
    if (Snake_IsOpenMode()) {
        /* Open map has no fixed clear condition. */
    } else if (game_mode == GAME_MODE_CLASSIC) {
        if (snake_len >= classic_target_len) {
            return STEP_WIN;
        }
    } else if (level_score >= level_target[level_index]) {
        return STEP_LEVEL_DONE;
    }

    Snake_PlaceFood();
    return STEP_ALIVE;
}

static u8 Snake_HandleSelfCollision(u16 hit_index)
{
    u16 removed_len;

    if (hit_index == SNAKE_NO_INDEX || hit_index >= snake_len) {
        return STEP_DEAD;
    }

    removed_len = (u16)(snake_len - hit_index);

    Snake_UpdateBest();
    if (lives > 0) {
        lives--;
    }

    if (score > removed_len) {
        score = (u16)(score - removed_len);
    } else {
        score = 0;
    }

    if (level_score > removed_len) {
        level_score = (u8)(level_score - removed_len);
    } else {
        level_score = 0;
    }

    snake_len = hit_index;
    Snake_SetStatus("SELF HIT");
    Snake_Render();
    Snake_PlayTone(NOTE_D4, 150);
    Snake_PlayTone(NOTE_G3, 120);

    if (lives == 0) {
        return STEP_SELF_GAME_OVER;
    }

    return STEP_SELF_HIT;
}

static u8 Snake_Step(void)
{
    short nx = (short)snake_x[0];
    short ny = (short)snake_y[0];
    u8 eat;
    u8 food_changed = 0;
    u16 check_len;
    u16 self_index;
    int i;
    char cell;
    u8 result;
    u8 cols = Snake_WorldCols();
    u8 rows = Snake_WorldRows();

    dir = next_dir;
    turn_pending = 0;

    if (dir == DIR_UP) ny--;
    else if (dir == DIR_DOWN) ny++;
    else if (dir == DIR_LEFT) nx--;
    else nx++;

    if (nx < 0 || nx >= cols || ny < 0 || ny >= rows) {
        return STEP_DEAD;
    }

    cell = Snake_MapCell((u8)nx, (u8)ny);
    if (cell == '#') {
        return STEP_DEAD;
    }

    if (cell == 'A' || cell == 'B') {
        u8 tx = (u8)nx;
        u8 ty = (u8)ny;
        if (Snake_FindOtherPortal(cell, &tx, &ty)) {
            nx = (short)tx;
            ny = (short)ty;
            Snake_SetStatus("PORTAL");
            Snake_PlayTone(NOTE_D6, 60);
        }
    }

    eat = ((u8)nx == food_x && (u8)ny == food_y);
    check_len = eat ? snake_len : (u16)(snake_len - 1);

    self_index = Snake_FindOnSnakeIndex((u8)nx, (u8)ny, check_len);
    if (self_index != SNAKE_NO_INDEX) {
        return Snake_HandleSelfCollision(self_index);
    }

    if (eat && food_type != FOOD_POISON && snake_len < MAX_SNAKE_LEN) {
        snake_len++;
    }

    for (i = (int)snake_len - 1; i > 0; i--) {
        snake_x[i] = snake_x[i - 1];
        snake_y[i] = snake_y[i - 1];
    }

    snake_x[0] = (u8)nx;
    snake_y[0] = (u8)ny;

    result = STEP_ALIVE;
    if (eat) {
        result = Snake_ApplyFood();
        food_changed = 1;
    } else {
        Snake_SetStatus("RUNNING");
    }

    Snake_ShowDirection();
    Snake_RenderStep(food_changed);
    return result;
}

static void Snake_AdvancePoint(SnakeDir move_dir, short *x, short *y)
{
    if (move_dir == DIR_UP) (*y)--;
    else if (move_dir == DIR_DOWN) (*y)++;
    else if (move_dir == DIR_LEFT) (*x)--;
    else (*x)++;
}

static void Snake_MoveBody(u8 *x, u8 *y, u16 len,
                           short nx, short ny)
{
    int i;

    for (i = (int)len - 1; i > 0; i--) {
        x[i] = x[i - 1];
        y[i] = y[i - 1];
    }

    x[0] = (u8)nx;
    y[0] = (u8)ny;
}

static u8 Snake_DuoHitWallOrMap(short x, short y)
{
    u8 cols = Snake_WorldCols();
    u8 rows = Snake_WorldRows();

    if (x < 0 || x >= cols || y < 0 || y >= rows) {
        return 1;
    }

    return (u8)(Snake_MapCell((u8)x, (u8)y) == '#');
}

static u8 Snake_DuoStep(void)
{
    short nx1 = (short)snake_x[0];
    short ny1 = (short)snake_y[0];
    short nx2 = (short)snake2_x[0];
    short ny2 = (short)snake2_y[0];
    u8 p1_dead = 0;
    u8 p2_dead = 0;
    u8 eat1;
    u8 eat2;
    u16 check_len1;
    u16 check_len2;
    u8 food_changed = 0;

    dir = next_dir;
    dir2 = next_dir2;
    turn_pending = 0;
    turn_pending2 = 0;

    Snake_AdvancePoint(dir, &nx1, &ny1);
    Snake_AdvancePoint(dir2, &nx2, &ny2);

    p1_dead = Snake_DuoHitWallOrMap(nx1, ny1);
    p2_dead = Snake_DuoHitWallOrMap(nx2, ny2);

    eat1 = (u8)(!p1_dead && (u8)nx1 == food_x && (u8)ny1 == food_y);
    eat2 = (u8)(!p2_dead && (u8)nx2 == food_x && (u8)ny2 == food_y);
    check_len1 = eat1 ? snake_len : (u16)(snake_len - 1);
    check_len2 = eat2 ? snake2_len : (u16)(snake2_len - 1);

    if (!p1_dead && Snake_OnSnake((u8)nx1, (u8)ny1, check_len1)) {
        p1_dead = 1;
    }
    if (!p2_dead && Snake2_OnSnake((u8)nx2, (u8)ny2, check_len2)) {
        p2_dead = 1;
    }
    if (!p1_dead && Snake2_OnSnake((u8)nx1, (u8)ny1, check_len2)) {
        p1_dead = 1;
    }
    if (!p2_dead && Snake_OnSnake((u8)nx2, (u8)ny2, check_len1)) {
        p2_dead = 1;
    }
    if (!p1_dead && !p2_dead && nx1 == nx2 && ny1 == ny2) {
        p1_dead = 1;
        p2_dead = 1;
    }

    if (p1_dead || p2_dead) {
        if (p1_dead && p2_dead) duo_winner = 3;
        else duo_winner = p1_dead ? 2 : 1;
        Snake_SetStatus(duo_winner == 1 ? "P1 WINS" :
                        (duo_winner == 2 ? "P2 WINS" : "DRAW"));
        Snake_Render();
        return STEP_DEAD;
    }

    if (eat1 && snake_len < MAX_SNAKE_LEN) {
        snake_len++;
        score++;
        food_changed = 1;
    }
    if (eat2 && snake2_len < MAX_SNAKE_LEN) {
        snake2_len++;
        score2++;
        food_changed = 1;
    }

    Snake_MoveBody(snake_x, snake_y, snake_len, nx1, ny1);
    Snake_MoveBody(snake2_x, snake2_y, snake2_len, nx2, ny2);

    if (food_changed) {
        Snake_SetStatus(eat1 && eat2 ? "BOTH" : (eat1 ? "P1 +" : "P2 +"));
        Snake_PlaceFood();
    } else {
        Snake_SetStatus("DUO");
    }

    Snake_ShowDirection();
    Snake_RenderStep(food_changed);
    return STEP_ALIVE;
}

static u8 Snake_LoseLife(const char *reason)
{
    if (lives > 0) {
        lives--;
    }
    Snake_UpdateBest();

    Snake_SetStatus(reason);
    Snake_Render();
    Snake_PlayTone(NOTE_C4, 150);
    Snake_PlayTone(NOTE_G3, 180);

    if (lives == 0) {
        return 0;
    }

    Snake_ShowCenter("LIFE LOST", "Resume same level");
    Delay_ms(800);
    time_left = level_time_limit[level_index];
    Snake_ResetSnake();
    Snake_PlaceFood();
    force_board_clear = 1;
    Snake_SetStatus("TRY AGAIN");
    Snake_Render();
    return 1;
}

static void Snake_GameOver(void)
{
    Snake_UpdateBest();
    Snake_RecordModeResults();
    Snake_PersistSave();
    Snake_ShowCenter("GAME OVER", "Press KEY1 or R to return");
    Snake_PlayTone(NOTE_C4, 160);
    Snake_PlayTone(NOTE_G3, 230);
    Snake_WaitReturnHome();
}

static void Snake_WinGame(void)
{
    Snake_UpdateBest();
    Snake_RecordModeResults();
    Snake_PersistSave();
    Snake_ShowCenter("YOU WIN", "Press KEY1 or R to return");
    Snake_BeepLevel();
    Snake_WaitReturnHome();
}

static void Snake_NextLevel(void)
{
    if (level_index + 1 >= LEVEL_COUNT) {
        Snake_UpdateBest();
        Snake_RecordModeResults();
        Snake_PersistSave();
        Snake_ShowCenter("YOU WIN", "Press KEY1 or R to return");
        Snake_BeepLevel();
        Snake_WaitReturnHome();
        Snake_StartGame();
    } else {
        Snake_ShowCenter("LEVEL CLEAR", "Next level");
        Snake_BeepLevel();
        Delay_ms(900);
        Snake_StartLevel((u8)(level_index + 1));
    }
}

int main(void)
{
    u8 step_result;

    high_score = 0;
    game_mode = GAME_MODE_STAGE;
    Snake_PersistLoad();

    SystemInit();
    GPIO_Configuration();
    Snake_ADCConfiguration();
    SnakeUart_Init();
    Snake_AudioInit();
    LCD_Init();
    LCD_Clear(BLACK);
    BEEP(0);

    Snake_ShowHome();
    Snake_StartGame();

    while (1) {
        if (restart_request) {
            restart_request = 0;
            Snake_StartGame();
            continue;
        }

        if (Snake_IsBattleMode()) {
            Snake_BattleLoop();
            Snake_StartGame();
            continue;
        }

        if (!Snake_WaitStep(Snake_StepDelay())) {
            if (!Snake_LoseLife("TIME OUT")) {
                Snake_GameOver();
                Snake_StartGame();
            }
            continue;
        }

        if (restart_request) {
            restart_request = 0;
            Snake_StartGame();
            continue;
        }

        step_result = Snake_IsDuoMode() ? Snake_DuoStep() : Snake_Step();
        if (step_result == STEP_DEAD) {
            if (Snake_IsDuoMode()) {
                Snake_RecordModeResults();
                Snake_PersistSave();
                Snake_ShowCenter(duo_winner == 1 ? "P1 WINS" :
                                (duo_winner == 2 ? "P2 WINS" : "DRAW"),
                                "Press KEY1 or R to return");
                Snake_BeepLevel();
                Snake_WaitReturnHome();
                Snake_StartGame();
            } else {
                if (!Snake_LoseLife("CRASH")) {
                    Snake_GameOver();
                    Snake_StartGame();
                }
            }
        } else if (step_result == STEP_LEVEL_DONE) {
            Snake_NextLevel();
        } else if (step_result == STEP_WIN) {
            Snake_WinGame();
            Snake_StartGame();
        } else if (step_result == STEP_SELF_GAME_OVER) {
            Snake_GameOver();
            Snake_StartGame();
        } else if (step_result == STEP_SELF_HIT) {
            continue;
        }
    }
}
