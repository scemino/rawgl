#ifdef __cplusplus
extern "C" {
#endif

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

// configuration parameters for game_init()
typedef struct {
    const char *data_dir;       // the directory where the game files are located
    int part_num;               // indicates the part number where the fame starts
    bool use_ega;               // true to use EGA palette, false to use VGA palette
    game_lang_t lang;           // language to use
    bool demo3_joy_inputs;      // read the demo3.joy file if present
} game_desc_t;

typedef struct {
    bool valid;
    uint8_t fb[320*200];    // frame buffer: this where is stored the image with indexed color
    uint32_t palette[16];   // palette containing 16 RGBA colors
    const char* title;      // title of the game
} game_t;

gfx_display_info_t game_display_info(game_t* game);
void game_init(game_t* game, const game_desc_t* desc);
void game_exec(game_t* game, uint32_t micro_seconds);
void game_cleanup(game_t* game);
void game_key_down(game_t* game, game_input_t input);
void game_key_up(game_t* game, game_input_t input);

#ifdef __cplusplus
} /* extern "C" */
#endif
