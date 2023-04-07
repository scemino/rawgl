#ifdef __cplusplus
extern "C" {
#endif

// configuration parameters for game_init()
typedef struct {
    const char *data_dir;
    int part_num;
} game_desc_t;

typedef struct {
    uint8_t fb[320*200];    // frame buffer: this where is stored the image with indexed color
    uint32_t palette[16];   // palette containing 16 RGBA colors
    const char* title;      // title of the game
} game_t;

gfx_display_info_t game_display_info(game_t* game);
void game_init(game_t* game, const game_desc_t* desc);
void game_exec(game_t* game);
void game_cleanup(game_t* game);

#ifdef __cplusplus
} /* extern "C" */
#endif
