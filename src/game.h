#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define GAME_WIDTH                     (320)
#define GAME_HEIGHT                    (200)
#define GAME_DEFAULT_AUDIO_SAMPLE_RATE (44100)
#define GAME_DEFAULT_AUDIO_SAMPLES  (128)        // default number of samples in internal sample buffer
#define GAME_MAX_AUDIO_SAMPLES      (2048*16)        // max number of audio samples in internal sample buffer
#define GAME_ENTRIES_COUNT_20TH     (178)
#define GAME_MEM_BLOCK_SIZE         (1 * 1024 * 1024)
#define GAME_RES_STATUS_NULL        (0)
#define GAME_RES_STATUS_LOADED      (1)
#define GAME_RES_STATUS_TOLOAD      (2)

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

#define  MIX_FREQ               (44100)
#define  MIX_BUF_SIZE           (4096*8)
#define  MIX_CHANNELS           (4)
#define  SFX_NUM_CHANNELS       (4)

#define  FIXUP_PALETTE_REDRAW   (1)

#define  FRAC_BITS              (16)
#define  FRAC_MASK              ((1 << FRAC_BITS) - 1)

typedef struct {

	uint32_t inc;
	uint64_t offset;
} game_frac_t;

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

typedef enum {
    RT_SOUND  = 0,
    RT_MUSIC  = 1,
    RT_BITMAP = 2, // full screen 4bpp video buffer, size=200*320/2
    RT_PALETTE = 3, // palette (1024=vga + 1024=ega), size=2048
    RT_BYTECODE = 4,
    RT_SHAPE = 5,
    RT_BANK = 6, // common part shapes (bank2.mat)
} game_res_type_t;

typedef enum {
    DT_DOS,
    DT_AMIGA,
    DT_ATARI,
    DT_ATARI_DEMO, // ST Action Issue44 Disk28
} game_data_type_t;

typedef struct {
	uint8_t *data;
	uint16_t volume;
} game_audio_sfx_instrument_t;

typedef enum {
    DIR_LEFT  = 1 << 0,
    DIR_RIGHT = 1 << 1,
    DIR_UP    = 1 << 2,
    DIR_DOWN  = 1 << 3
} game_input_dir_t;

typedef struct {
	uint16_t    note_1;
	uint16_t    note_2;
	uint16_t    sample_start;
	uint8_t*    sample_buffer;
	uint16_t    sample_len;
	uint16_t    loop_pos;
	uint16_t    loop_len;
	uint16_t    sample_volume;
} game_audio_sfx_pattern_t;

typedef struct {
	const uint8_t*              data;
	uint16_t                    cur_pos;
	uint8_t                     cur_order;
	uint8_t                     num_order;
	uint8_t*                    order_table;
	game_audio_sfx_instrument_t samples[15];
} game_audio_sfx_module_t;

typedef struct  {
	uint8_t*    sample_data;
	uint16_t    sample_len;
	uint16_t    sample_loop_pos;
	uint16_t    sample_loop_len;
	uint16_t    volume;
	game_frac_t pos;
} game_audio_sfx_channel_t;

typedef struct {
    uint16_t                    delay;
	uint16_t                    res_num;
	game_audio_sfx_module_t     sfx_mod;
	bool                        playing;
	int                         rate;
	int                         samples_left;
	game_audio_sfx_channel_t    channels[SFX_NUM_CHANNELS];
} game_audio_sfx_player_t;

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
	uint8_t     status;        // 0x0
	uint8_t     type;          // 0x1, Resource::ResType
	uint8_t*    bufPtr;        // 0x2
	uint8_t     rankNum;       // 0x6
	uint8_t     bankNum;       // 0x7
	uint32_t    bankPos;       // 0x8
	uint32_t    packedSize;    // 0xC
	uint32_t    unpackedSize;  // 0x12
} game_mem_entry_t;

// TODO: rename this
typedef struct {
	uint8_t *pc;
} game_pc_t;

typedef struct {
    const uint8_t *data;
    game_frac_t   pos;
    uint32_t      len;
    uint32_t      loop_len, loop_pos;
    int           volume;
} game_audio_channel_t;

typedef struct {
	uint16_t    id;
	const char* str;
} game_str_entry_t;

typedef struct {
    game_mem_entry_t    mem_list[GAME_ENTRIES_COUNT_20TH];
    uint16_t            num_mem_list;
	uint8_t             mem[GAME_MEM_BLOCK_SIZE];
	uint16_t            current_part, next_part;
	uint8_t*            script_bak_ptr, *script_cur_ptr, *vid_cur_ptr;
	bool                use_seg_video2;
	uint8_t*            seg_video_pal;
	uint8_t*            seg_code;
	uint8_t*            seg_video1;
	uint8_t*            seg_video2;
	bool                has_password_screen;
	game_data_type_t    data_type;
	game_lang_t         lang;
} game_res_t;

typedef struct {
    bool            valid;
    game_debug_t    debug;
    game_res_t      res;
    int             part_num;
    uint32_t        elapsed;
    uint32_t        sleep;

    struct {
        uint8_t             fb[GAME_WIDTH*GAME_HEIGHT];    // frame buffer: this where is stored the image with indexed color
        game_framebuffer_t  fbs[4];         // frame buffer: this where is stored the image with indexed color
        uint32_t            palette[16];    // palette containing 16 RGBA colors
        uint8_t*            draw_page_ptr;
        int                 u, v;
        int                 fix_up_palette;
        uint32_t            pal[16];
        uint8_t             color_buffer[GAME_WIDTH * GAME_HEIGHT];
    } gfx;

    struct {
        float                   sample_buffer[GAME_MAX_AUDIO_SAMPLES];
        int16_t                 samples[MIX_BUF_SIZE];
        game_audio_channel_t    channels[MIX_CHANNELS];
        game_audio_sfx_player_t sfx_player;
        game_audio_callback_t   callback;
    } audio;

    struct {
        uint8_t                 next_pal, current_pal;
        uint8_t                 buffers[3];
        game_pc_t               p_data;
        uint8_t*                data_buf;
        const game_str_entry_t* strings_table;
        bool                    use_ega;
        uint8_t                 temp_bitmap[GAME_WIDTH * GAME_HEIGHT];
    } video;

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

    struct {
        uint8_t dirMask;
        bool    action; // run,shoot
        bool    jump;
        bool    code;
        bool    pause;
        bool    quit;
        bool    back;
        char    lastChar;
        bool    fastMode;
        bool    screenshot;
    } input;

    const char* title;      // title of the game
} game_t;

gfx_display_info_t game_display_info(game_t* game);
void game_init(game_t* game, const game_desc_t* desc);
void game_exec(game_t* game, uint32_t micro_seconds);
void game_cleanup(game_t* game);
void game_key_down(game_t* game, game_input_t input);
void game_key_up(game_t* game, game_input_t input);
void game_char_pressed(game_t* game, int c);
bool game_get_res_buf(game_t* game, int id, uint8_t* dst);

#ifdef __cplusplus
} /* extern "C" */
#endif
