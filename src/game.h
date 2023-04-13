#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define GAME_DEFAULT_AUDIO_SAMPLE_RATE (44100)
#define GAME_DEFAULT_AUDIO_SAMPLES (128)        // default number of samples in internal sample buffer
#define GAME_MAX_AUDIO_SAMPLES (2048*16)        // max number of audio samples in internal sample buffer
#define GAME_ENTRIES_COUNT_20TH (178)

#define VAR_RANDOM_SEED          (0x3C)
#define VAR_SCREEN_NUM           (0x67)
#define VAR_LAST_KEYCHAR         (0xDA)
#define VAR_HERO_POS_UP_DOWN     (0xE5)
#define VAR_MUSIC_SYNC           (0xF4)
#define VAR_SCROLL_Y             (0xF9)
#define VAR_HERO_ACTION          (0xFA)
#define VAR_HERO_POS_JUMP_DOWN   (0xFB)
#define VAR_HERO_POS_LEFT_RIGHT  (0xFC)
#define VAR_HERO_POS_MASK        (0xFD)
#define VAR_HERO_ACTION_POS_MASK (0xFE)
#define VAR_PAUSE_SLICES         (0xFF)

typedef enum  {
	GAME_LANG_FR,
	GAME_LANG_US,
	GAME_LANG_DE,
	GAME_LANG_ES,
	GAME_LANG_IT
} game_lang_t;

typedef enum  {
	GAME_INPUT_LEFT,
	GAME_INPUT_RIGHT,
	GAME_INPUT_UP,
	GAME_INPUT_DOWN,
	GAME_INPUT_JUMP,
	GAME_INPUT_ACTION,
    GAME_INPUT_BACK,
    GAME_INPUT_CODE,
    GAME_INPUT_PAUSE,
} game_input_t;

typedef struct {
    void (*func)(const float* samples, int num_samples, void* user_data);
    void* user_data;
} game_audio_callback_t;

typedef struct {
    game_audio_callback_t callback;
    int num_samples;
    int sample_rate;
    float volume;
} game_audio_desc_t;

// configuration parameters for game_init()
typedef struct {
    const char *data_dir;       // the directory where the game files are located
    int part_num;               // indicates the part number where the fame starts
    bool use_ega;               // true to use EGA palette, false to use VGA palette
    game_lang_t lang;           // language to use
    bool demo3_joy_inputs;      // read the demo3.joy file if present
    game_audio_desc_t audio;
    game_debug_t debug;
} game_desc_t;

typedef struct {
    uint8_t buffer[320*200];
} game_framebuffer_t;

typedef struct {
	uint8_t status;        // 0x0
	uint8_t type;          // 0x1, Resource::ResType
	uint8_t *bufPtr;       // 0x2
	uint8_t rankNum;       // 0x6
	uint8_t bankNum;       // 0x7
	uint32_t bankPos;      // 0x8
	uint32_t packedSize;   // 0xC
	uint32_t unpackedSize; // 0x12
} game_mem_entry_t;

// TODO: rename this
typedef struct {
	uint8_t *pc;
} game_pc_t;

typedef struct {
    bool valid;
    game_debug_t debug;

    struct {
        uint8_t             fb[320*200];    // frame buffer: this where is stored the image with indexed color
        game_framebuffer_t  fbs[4];         // frame buffer: this where is stored the image with indexed color
        uint32_t            palette[16];    // palette containing 16 RGBA colors
    } gfx;

    struct {
        int16_t     vars[256];
        uint16_t    stack_calls[64];
        uint16_t    tasks[2][64];
        uint8_t     states[2][64];
        game_pc_t   ptr;
        uint8_t     stack_ptr;
        bool        paused;
        bool        fast_mode;
        int         screen_num;
        uint32_t    start_time, time_stamp;
    } vm;

    const char* title;      // title of the game
} game_t;

gfx_display_info_t game_display_info(game_t* game);
void game_init(game_t* game, const game_desc_t* desc);
void game_exec(game_t* game, uint32_t micro_seconds);
void game_cleanup(game_t* game);
void game_key_down(game_t* game, game_input_t input);
void game_key_up(game_t* game, game_input_t input);
void game_char_pressed(game_t* game, int c);
void game_get_resources(game_t* game, game_mem_entry_t** res);
uint8_t* game_get_pc(game_t* game);
bool game_get_res_buf(int id, uint8_t* dst);

#ifdef __cplusplus
} /* extern "C" */
#endif
