#include <assert.h>
#include <SDL.h>

typedef uint32_t u32;
typedef uint8_t bool;
typedef uint8_t u8;
typedef int16_t i16;

#define true 1
#define false 0

#define internal static // static functions are "internal"

#define PIXEL_SCALE 5
#define SCREEN_WIDTH 100
#define SCREEN_HEIGHT 100

// Identify empty space
#define EMPTY_SPACE 0x00000000
// Return value for pixels outside the screen area
#define OUT_OF_BOUNDS 0x00000001

// Create projectiles as pixel particles
#define PROJECTILE_COLOR 0xFFFF0000 // opaque red
#define GRAVITY 1
#define PHYSICS_RATE 4 // must be a power of 2 so u32 % rate wraps OK
#define BLAST -12

typedef struct
{
    int x; // row of topleft corner
    int y; // col of topleft corner
    int w; // width
    int h; // height
} rect_t;

typedef struct
{
    i16 dx; // vertical (think rows)
    i16 dy; // horizontal (think cols)
} momentum_t;

/**
 *  \brief Move rectangle topleft to x,y
 *
 *  \param rect Pointer to the rectangle to move
 *  \param x    Destination row
 *  \param y    Destination column
 *
 *  Example: to move player UP one pixel:
 *      MoveRect(&player, player.x++, player.y)
 */
internal void MoveRect(rect_t *rect, int x, int y)
{
    // TODO: clamp to allowed positions
    rect->x = x;
    rect->y = y;
}

internal void FillRect(rect_t rect, u32 pixel_color, u32 *buffer)
{
    assert(buffer);
    for (int row=0; row < rect.h; row++)
    {
        for (int col=0; col < rect.w; col++)
        {
            buffer[ (rect.x + row)*SCREEN_WIDTH + (rect.y + col) ] = pixel_color;
        }
    }
}

/**
 *  \brief Set pixel color in PREV buffer.
 *
 *  \param x    Screen row number (0 is top)
 *  \param y    Screen col number (0 is left)
 *  \param color    Color to set at this pixel
 *  \param screen_pixels    Pointer to the screen buffer to write to
 */
inline internal void ColorSetUnsafe(int x, int y, u32 color, u32 *screen_pixels)
{
    screen_pixels[x*SCREEN_WIDTH+y] = color;
}

/**
 *  \brief Get pixel color
 *
 *  \param x    Screen row number (0 is top)
 *  \param y    Screen col number (0 is left)
 *  \param screen_pixels    Pointer to the screen buffer
 *
 *  \return color   ARGB as unsigned 32-bit, or 1 if (x,y) is outside screen
 */
inline internal u32 ColorAt(int x, int y, u32 *screen_pixels)
{
    if ((x >= 0) && (y >= 0) && (x < SCREEN_HEIGHT) && (y < SCREEN_WIDTH))
    {
        return screen_pixels[x*SCREEN_WIDTH+y];
    }
    else // Pixel is outside screen area
    {
        // Any value that is NOT "EMPTY_SPACE" acts as a boundary
        assert(EMPTY_SPACE != OUT_OF_BOUNDS);
        return OUT_OF_BOUNDS;
    }
}

/**
 *  \brief Get particle momentum
 *
 *  \param x    Screen row number (0 is top)
 *  \param y    Screen col number (0 is left)
 *  \param momentum Pointer to the momentum buffer
 *
 *  \return momentum_t {i16 dx, i16 dy}
 */
inline internal momentum_t MomentumAt(int x, int y, momentum_t *momentum)
{
    if ((x >= 0) && (y >= 0) && (x < SCREEN_HEIGHT) && (y < SCREEN_WIDTH))
    {
        return momentum[x*SCREEN_WIDTH+y];
    }
    else // Pixel is outside screen area
    {
        // Gotta return something. How about 0,0?
        momentum_t out_of_bounds_momentum = {0,0};
        return out_of_bounds_momentum;
    }
}

/**
 *  \brief Set momentum in PREV buffer.
 *
 *  \param x    Screen row number (0 is top)
 *  \param y    Screen col number (0 is left)
 *  \param momentum Momentum to set at this pixel
 *  \param momentum_buffer Pointer to the momentum buffer to write to
 */
inline internal void MomentumSetUnsafe(int x, int y, momentum_t momentum, momentum_t *momentum_buffer)
{
    momentum_buffer[x*SCREEN_WIDTH+y] = momentum;
}

/**
 *  \brief Start a new projectile
 *
 *  \param projectile_buffer   Pointer to the position buffer `projectile_buffer`
 *  \param momentum_buffer Pointer to the projectile momentum buffer `momentum`
 */
internal void InitProjectile(u32 *projectile_buffer, momentum_t *momentum_buffer)
{
    int x = SCREEN_HEIGHT-1;
    int y = SCREEN_WIDTH/2;
    momentum_t momentum = {BLAST,0};

    if (ColorAt(x,y,projectile_buffer) == EMPTY_SPACE)
    {
        ColorSetUnsafe(x, y, PROJECTILE_COLOR, projectile_buffer);
        MomentumSetUnsafe(x, y, momentum, momentum_buffer);
    }
}

/**
 *  \brief Update projectiles
 *
 *  Read the projectile locations from PREV frame and write the new locations to
 *  the NEXT frame.
 *
 *  \param frame        Pointer to the `projectile_buffer`
 *                      Projectile POSITIONS for PREV frame
 *  \param frame_next   Pointer to the `projectile_buffer_next`
 *                      Projectile POSITIONS for NEXT frame
 *  \param momentum_prev Pointer to `momentum`
 *                      Projectile MOMENTUM for PREV frame
 *  \param momentum_next Pointer to `momentum_next`
 *                      Projectile MOMENTUM for NEXT frame
 */
internal void DrawProjectile( // u32 *frame, u32 *frame_next
        u32 *frame, u32 *frame_next,
        momentum_t *momentum_prev, momentum_t *momentum_next
        )
{
    for (int row=0; row < SCREEN_HEIGHT; row++)
        for (int col=0; col < SCREEN_WIDTH; col++)
        {
            momentum_t momentum = MomentumAt(row, col, momentum_prev);
            // Decelerate
            momentum.dx += GRAVITY;
            u32 color = ColorAt(row, col, frame);
            u32 color_predict = ColorAt(row+momentum.dx, col, frame);
            switch (color)
            {
                case PROJECTILE_COLOR:
                        // TEMP: stop at top of screen
                        if (color_predict == OUT_OF_BOUNDS)
                        {
                            // Erase the projectile
                            ColorSetUnsafe(row, col, EMPTY_SPACE, frame_next);
                            momentum_t momentum_new = {0,0};
                            MomentumSetUnsafe(row, col, momentum_new, momentum_next);
                        }
                        // Keep moving: not at top of screen yet
                        else
                        {
                            ColorSetUnsafe(row+momentum.dx, col, PROJECTILE_COLOR, frame_next);
                            MomentumSetUnsafe(row+momentum.dx, col, momentum, momentum_next);
                        }
                    break;
            }
        }
}


int main(int argc, char **argv)
{
    u8 frame_num = 0;
    // ---------
    // | Setup |
    // ---------

    // ---Window and Renderer---

    SDL_Init(SDL_INIT_VIDEO);

    // Window is resizable with mouse. Pixels resize so that
    // SCREEN_WIDTH x SCREEN_HEIGHT spans the window.
    // For precise pixel scaling, pass scaled values for
    // SCREEN_WIDTH and SCREEN_HEIGHT in call to
    // SDL_CreateWindow().
    SDL_Window *window = SDL_CreateWindow(
            "momentum - Space to launch a particle", // const char *title
            /* SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, // int x, int y */
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, // int x, int y
            PIXEL_SCALE*SCREEN_WIDTH, PIXEL_SCALE*SCREEN_HEIGHT, // int w, int h,
            SDL_WINDOW_RESIZABLE // Uint32 flags
            );
    assert(window);

    SDL_Renderer *renderer = SDL_CreateRenderer(
            window, // SDL_Window *
            -1, // int index
            SDL_RENDERER_ACCELERATED // Uint32 flags
            );
    assert(renderer);

    // ---Textures---

    SDL_PixelFormat *format = SDL_AllocFormat(SDL_PIXELFORMAT_ARGB8888);
    assert(format);

    SDL_Texture *player_texture = SDL_CreateTexture(
            renderer, // SDL_Renderer *
            format->format, // Uint32 format,
            SDL_TEXTUREACCESS_STREAMING, // int access,
            SCREEN_WIDTH, SCREEN_HEIGHT // int w, int h
            );
    assert(player_texture);
    SDL_SetTextureBlendMode(player_texture, SDL_BLENDMODE_BLEND);

    SDL_Texture *projectile_texture = SDL_CreateTexture(
            renderer, // SDL_Renderer *
            format->format, // Uint32 format,
            SDL_TEXTUREACCESS_STREAMING, // int access,
            SCREEN_WIDTH, SCREEN_HEIGHT // int w, int h
            );
    assert(projectile_texture);
    SDL_SetTextureBlendMode(projectile_texture, SDL_BLENDMODE_BLEND);

    // ---Pixel Artwork Buffers---

    u32 *player_buffer = (u32*) calloc(SCREEN_WIDTH * SCREEN_HEIGHT, sizeof(u32));
    assert(player_buffer);

    u32 *projectile_buffer = (u32*) calloc(SCREEN_WIDTH * SCREEN_HEIGHT, sizeof(u32));
    assert(projectile_buffer);
    u32 *projectile_buffer_next = (u32*) calloc(SCREEN_WIDTH * SCREEN_HEIGHT, sizeof(u32));
    assert(projectile_buffer_next);
    momentum_t *momentum = (momentum_t*) calloc(SCREEN_WIDTH * SCREEN_HEIGHT, sizeof(momentum_t));
    assert(momentum);
    momentum_t *momentum_next = (momentum_t*) calloc(SCREEN_WIDTH * SCREEN_HEIGHT, sizeof(momentum_t));
    assert(momentum_next);

    // Create player: a 1x1 rectangle
    const u8 player_size = 1;
    rect_t player = {0,0,player_size,player_size}; // row,col,w,h
    const u32 player_color = 0x8000FF00; // transparent green

    // Start player at bottom left
    MoveRect(&player, (SCREEN_HEIGHT-1)-player.h, 0);

    // Create a rect for clearing pixel artwork from the screen
    rect_t entire_screen = {0,0,SCREEN_WIDTH,SCREEN_HEIGHT};

    // Initialize player controls
    bool pressed_space = false;
    bool pressed_down  = false;
    bool pressed_up    = false;
    bool pressed_left  = false;
    bool pressed_right = false;

    // -------------
    // | Game Loop |
    // -------------

    bool done = false;

    while (!done)
    {
        // Erase old artwork before updating position
        FillRect(player, 0x00000000, player_buffer);

        // --------------
        // | Get inputs |
        // --------------

        SDL_Event event;
        while(SDL_PollEvent(&event))
        {
            if (event.type == SDL_QUIT) // Click window close
            {
                done = true;
            }

            SDL_Keycode code = event.key.keysym.sym;

            switch (code)
            {
                case SDLK_ESCAPE: // Esc - Quit
                    done = true;
                    break;

                case SDLK_SPACE: // Space - do something
                    pressed_space = (event.type == SDL_KEYDOWN);
                    break;

                case SDLK_j: // j - move me down
                    pressed_down = (event.type == SDL_KEYDOWN);
                    break;

                case SDLK_k: // k - move me up
                    pressed_up = (event.type == SDL_KEYDOWN);
                    break;

                case SDLK_h: // h - move me left
                    pressed_left = (event.type == SDL_KEYDOWN);
                    break;

                case SDLK_l: // l - move me right
                    pressed_right = (event.type == SDL_KEYDOWN);
                    break;

                default:
                    break;
            }
        }

        // ------------------
        // | Process inputs |
        // ------------------

        if (pressed_space)
        {
            InitProjectile(projectile_buffer, momentum);
            pressed_space = false;
        }
        if (pressed_down)
        {
            if ((player.x + player.h) < (SCREEN_HEIGHT-1) ) // not at bottom yet
            {
                MoveRect(&player, player.x+player.h, player.y);
                pressed_down = false;
            }
        }
        if (pressed_up)
        {
            if (player.x > (0 + player.h)) // not at top yet
            {
                MoveRect(&player, player.x-player.h, player.y);
                pressed_up = false;
            }
        }
        if (pressed_left)
        {
            if (player.y > 0)
            {
                MoveRect(&player, player.x, player.y-player.w);
                pressed_left = false;
            }
        }
        if (pressed_right)
        {
            if (player.y < (SCREEN_WIDTH - player.w))
            {
                MoveRect(&player, player.x, player.y+player.w);
                pressed_right = false;
            }
        }

        // -------------
        // | Rect Draw |
        // -------------

        // Draw player
        FillRect(player, player_color, player_buffer);

        // --------------
        // | Pixel Draw |
        // --------------

        if (frame_num++%PHYSICS_RATE == 0)
        {
            // Erase old artwork
            FillRect(entire_screen, EMPTY_SPACE, projectile_buffer_next);

            // Draw projectiles for next frame
            DrawProjectile(projectile_buffer, projectile_buffer_next, momentum, momentum_next);

            // Load next position frame
            u32 *tmp_pix = projectile_buffer;
            projectile_buffer = projectile_buffer_next;
            projectile_buffer_next = tmp_pix;
            // Load next momentum frame
            momentum_t *tmp_mom = momentum;
            momentum = momentum_next;
            momentum_next = tmp_mom;
        }

        SDL_UpdateTexture(
                player_texture,     // SDL_Texture *
                NULL,               // const SDL_Rect * - NULL updates entire texture
                player_buffer, // const void *pixels
                SCREEN_WIDTH * sizeof(u32) // int pitch - n bytes in a row of pixel data
                );
        SDL_UpdateTexture(
                projectile_texture, // SDL_Texture *
                NULL,               // const SDL_Rect * - NULL updates entire texture
                projectile_buffer,  // const void *pixels
                SCREEN_WIDTH * sizeof(u32) // int pitch - n bytes in a row of pixel data
                );

        SDL_RenderClear(renderer);
        SDL_RenderCopy(
                renderer,       // SDL_Renderer *
                player_texture, // SDL_Texture *
                NULL, // const SDL_Rect * - SRC rect, NULL for entire TEXTURE
                NULL  // const SDL_Rect * - DEST rect, NULL for entire RENDERING TARGET
                );
        SDL_RenderCopy(
                renderer,       // SDL_Renderer *
                projectile_texture, // SDL_Texture *
                NULL, // const SDL_Rect * - SRC rect, NULL for entire TEXTURE
                NULL  // const SDL_Rect * - DEST rect, NULL for entire RENDERING TARGET
                );
        SDL_RenderPresent(renderer);
        SDL_Delay(15); // sets frame rate
    }
    // ---Cleanup---

    SDL_DestroyTexture(player_texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
