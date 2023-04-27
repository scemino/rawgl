#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define GAME_WIDTH                      (320)
#define GAME_HEIGHT                     (200)

#define GAME_ENTRIES_COUNT_20TH         (178)
#define GAME_MEM_BLOCK_SIZE             (1 * 1024 * 1024)

#define GAME_RES_STATUS_NULL            (0)
#define GAME_RES_STATUS_LOADED          (1)
#define GAME_RES_STATUS_TOLOAD          (2)

#define GAME_VAR_RANDOM_SEED            (0x3C)
#define GAME_VAR_SCREEN_NUM             (0x67)
#define GAME_VAR_LAST_KEYCHAR           (0xDA)
#define GAME_VAR_HERO_POS_UP_DOWN       (0xE5)
#define GAME_VAR_MUSIC_SYNC             (0xF4)
#define GAME_VAR_SCROLL_Y               (0xF9)
#define GAME_VAR_HERO_ACTION            (0xFA)
#define GAME_VAR_HERO_POS_JUMP_DOWN     (0xFB)
#define GAME_VAR_HERO_POS_LEFT_RIGHT    (0xFC)
#define GAME_VAR_HERO_POS_MASK          (0xFD)
#define GAME_VAR_HERO_ACTION_POS_MASK   (0xFE)
#define GAME_VAR_PAUSE_SLICES           (0xFF)

#define GAME_MIX_FREQ                   (44100)
#define GAME_MIX_BUF_SIZE               (4096*8)
#define GAME_MIX_CHANNELS               (4)
#define GAME_SFX_NUM_CHANNELS           (4)
#define GAME_MAX_AUDIO_SAMPLES          (2048*16)    // max number of audio samples in internal sample buffer

#define GAME_DBG_SCRIPT                 (1 << 0)
#define GAME_DBG_BANK                   (1 << 1)
#define GAME_DBG_VIDEO                  (1 << 2)
#define GAME_DBG_SND                    (1 << 3)
#define GAME_DBG_INFO                   (1 << 5)
#define GAME_DBG_PAK                    (1 << 6)
#define GAME_DBG_RESOURCE               (1 << 7)

#define GAME_PART_COPY_PROTECTION       (16000)
#define GAME_PART_INTRO                 (16001)
#define GAME_PART_WATER                 (16002)
#define GAME_PART_PRISON                (16003)
#define GAME_PART_CITE                  (16004)
#define GAME_PART_ARENE                 (16005)
#define GAME_PART_LUXE                  (16006)
#define GAME_PART_FINAL                 (16007)
#define GAME_PART_PASSWORD              (16008)

#define GAME_QUAD_STRIP_MAX_VERTICES    (70)

// bump when nes_t memory layout changes
#define GAME_SNAPSHOT_VERSION           (0x0001)

typedef enum  {
	GAME_LANG_FR,
	GAME_LANG_US
} game_lang_t;

typedef enum  {
	GAME_INPUT_LEFT,
	GAME_INPUT_RIGHT,
	GAME_INPUT_UP,
	GAME_INPUT_DOWN,
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

typedef enum {
    DIR_LEFT  = 1 << 0,
    DIR_RIGHT = 1 << 1,
    DIR_UP    = 1 << 2,
    DIR_DOWN  = 1 << 3
} game_input_dir_t;

typedef struct {
	uint8_t *data;
	uint16_t volume;
} game_audio_sfx_instrument_t;

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

typedef struct {
	uint32_t inc;
	uint64_t offset;
} game_frac_t;

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
	game_audio_sfx_channel_t    channels[GAME_SFX_NUM_CHANNELS];
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

typedef struct {
    gfx_range_t  mem_list;
    gfx_range_t  banks[0xd];
} game_data_t;

// configuration parameters for game_init()
typedef struct {
    int                 part_num;               // indicates the part number where the fame starts
    bool                use_ega;                // true to use EGA palette, false to use VGA palette
    game_lang_t         lang;                   // language to use
    uint8_t*            demo3_joy;              // contains content of demo3.joy file if present
    size_t              demo3_joy_size;
    game_audio_desc_t   audio;
    game_debug_t        debug;
    game_data_t         data;
} game_desc_t;

typedef struct {
    uint8_t buffer[GAME_WIDTH*GAME_HEIGHT];
} game_framebuffer_t;

typedef struct {
	uint8_t     status;        // 0x0
	uint8_t     type;          // 0x1, Resource::ResType
	uint8_t*    buf_ptr;        // 0x2
	uint8_t     rank_num;       // 0x6
	uint8_t     bank_num;       // 0x7
	uint32_t    bank_pos;       // 0x8
	uint32_t    packed_size;    // 0xC
	uint32_t    unpacked_size;  // 0x12
} game_mem_entry_t;

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
    game_data_t         data;
	game_lang_t         lang;
} game_res_t;

typedef struct {
    bool                    valid;
    game_debug_t            debug;
    game_res_t              res;
    const game_str_entry_t* strings_table;
    int                     part_num;
    uint32_t                elapsed;
    uint32_t                sleep;

    struct {
        uint8_t             fb[GAME_WIDTH*GAME_HEIGHT];    // frame buffer: this where is stored the image with indexed color
        game_framebuffer_t  fbs[4];
        uint32_t            palette[16];    // palette containing 16 RGBA colors
        uint8_t*            draw_page_ptr;
        bool                fix_up_palette; // redraw all primitives on setPal script call
    } gfx;

    struct {
        float                   sample_buffer[GAME_MAX_AUDIO_SAMPLES];
        int16_t                 samples[GAME_MIX_BUF_SIZE];
        game_audio_channel_t    channels[GAME_MIX_CHANNELS];
        game_audio_sfx_player_t sfx_player;
        game_audio_callback_t   callback;
    } audio;

    struct {
        uint8_t                 next_pal, current_pal;
        uint8_t                 buffers[3];
        game_pc_t               p_data;
        uint8_t*                data_buf;
        bool                    use_ega;
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
        int         current_task;
    } vm;

    struct {
        uint8_t dir_mask;
        bool    action; // run,shoot
        bool    code;
        bool    pause;
        bool    quit;
        bool    back;
        char    last_char;

        struct {
            uint8_t         keymask;
            uint8_t         counter;

            const uint8_t*  buf_ptr;
            size_t          buf_pos, buf_size;
        } demo_joy;
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
void game_start(game_t* game, game_data_t data);

void game_audio_callback_snapshot_onsave(game_audio_callback_t* snapshot);
void game_audio_callback_snapshot_onload(game_audio_callback_t* snapshot, game_audio_callback_t* sys);
void game_debug_snapshot_onsave(game_debug_t* snapshot);
void game_debug_snapshot_onload(game_debug_t* snapshot, game_debug_t* sys);
bool game_load_snapshot(game_t* game, uint32_t version, game_t* src);
uint32_t game_save_snapshot(game_t* game, game_t* dst);

#ifdef __cplusplus
} /* extern "C" */
#endif
