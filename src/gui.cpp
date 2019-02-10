#include <csignal>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>

#include <unistd.h> 
#include <string.h> 

#include "apu.hpp"
#include "cartridge.hpp"
#include "cpu.hpp"
#include "gui.hpp"
#include "config.hpp"
#include "remote_client.hpp"
#include "ppu.hpp"

namespace GUI {

// Screen size:
const unsigned WIDTH  = 256;
const unsigned HEIGHT = 240;



// SDL structures:
SDL_Window* window;
SDL_Renderer* renderer;
SDL_Texture* gameTexture;
SDL_Texture* background;
TTF_Font* font;
u8 const* keys;
SDL_PixelFormat pixel_format;

bool pause = true;

/* Set the window size multiplier */
void set_size(int mul)
{
    last_window_size = mul;
    SDL_SetWindowSize(window, WIDTH * mul, HEIGHT * mul);
    SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
}

/* Initialize GUI */
void init(char *rom_path)
{
    // Initialize graphics system:
    SDL_Init(SDL_INIT_VIDEO);
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
    TTF_Init();

    APU::init();

    // Initialize graphics structures:
    window      = SDL_CreateWindow  ("LaiNES",
                                     SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                     WIDTH * last_window_size, HEIGHT * last_window_size, 0);

    renderer    = SDL_CreateRenderer(window, -1,
                                     SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    SDL_RenderSetLogicalSize(renderer, WIDTH, HEIGHT);

    gameTexture = SDL_CreateTexture (renderer,
                                     SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
                                     WIDTH, HEIGHT);

    int w, h;
    Uint32 pix_format;
    SDL_QueryTexture(gameTexture, &pix_format, nullptr, &w, &h);                                     
    pixel_format.format = pix_format;

    font = TTF_OpenFont("res/font.ttf", FONT_SZ);
    keys = SDL_GetKeyboardState(0);

    // Initial background:
    SDL_Surface* backSurface  = IMG_Load("res/init.png");
    background = SDL_CreateTextureFromSurface(renderer, backSurface);
    SDL_SetTextureColorMod(background, 60, 60, 60);
    SDL_FreeSurface(backSurface);

    /* henrik */
    Cartridge::load(rom_path);
    toggle_pause();
    return;
}

/* Render a texture on screen */
void render_texture(SDL_Texture* texture, int x, int y)
{
    int w, h;
    SDL_Rect dest;

    SDL_QueryTexture(texture, NULL, NULL, &dest.w, &dest.h);
    if (x == TEXT_CENTER)
        dest.x = WIDTH/2 - dest.w/2;
    else if (x == TEXT_RIGHT)
        dest.x = WIDTH - dest.w - 10;
    else
        dest.x = x + 10;
    dest.y = y + 5;

    SDL_RenderCopy(renderer, texture, NULL, &dest);
}

/* Generate a texture from text */
SDL_Texture* gen_text(std::string text, SDL_Color color)
{
    SDL_Surface* surface = TTF_RenderText_Blended(font, text.c_str(), color);
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);

    SDL_FreeSurface(surface);
    return texture;
}


//bool display_screen_to_client = false;
bool display_screen_to_client = true;
static u8 remote_joypad_state[2] = {0, 0};  // initial state of the remote joypad


u8 get_joypad_state_from_tcp(int n)
{
    return remote_joypad_state[n];

}


void send_message_to_remote(char *msg)
{
    char *start_msg = (char *)"message:";
    char *end_msg = (char *)"\n";
    write(controller_fd, start_msg, strlen(start_msg));
    write(controller_fd, msg, strlen(msg));
    write(controller_fd, end_msg, strlen(end_msg));
}


/* Send the contents of the screen to the remote. 
   Sends: 256x240 pixels from screen[], encoded as a list of 
   r,g,b,r2,g2,b2 ... */
void send_screen_to_remote()
{
    /* Dump the 'pixels' over TCP to the client */

    char *header = (char *)"display:\n";
    write(controller_fd, header, strlen(header));

    char bigbuf[WIDTH * HEIGHT * 3 * 5] = ""; /* 3 rgb values, with max 4 chars per colour */
    char *bigbuf_ptr = bigbuf;


    /**** USE BIGBUFPTR TO AVOID CALLING WRITE TOO OFTEN ****/

    printf("SENDING SCREEN\n");
    for (int r = 0; r < HEIGHT; r++) {
        int offset = r * WIDTH;

        char linebuf[10000] = "";
        linebuf[0] = '\0';
        char *lineptr = linebuf;

//        printf(" line number %d:\n", r);

        for (int c = 0; c < WIDTH; c++) {
            Uint8 r, g, b;

            Uint32 pixel = PPU::pixels[offset + c];

            r = (0x00ff0000 & pixel) >> 16;
            g = (0x0000ff00 & pixel) >> 8;
            b = (0x000000ff & pixel);

            char minibuf[100];
            snprintf(minibuf, sizeof(minibuf) - 1, "%hhu,%hhu,%hhu,", r, g, b);
            //strcat(linebuf, minibuf);

            char *p = minibuf;
            while (*p) {
                *lineptr = *p;
                lineptr++;
                p++;
            }
        }
        //strcat(linebuf, "\n");
        *(lineptr++) = '\n';
        *(lineptr++) = '\0';
        write(controller_fd, linebuf, strlen(linebuf));

    }
    send_message_to_remote((char *)"Done");

    printf("bigbuf = >%s<", bigbuf);
}


/* Send the contents of the screen to the remote in binary. 
   Sends: 256x240 pixels from screen[], in binary format */
void send_binary_screen_to_remote()
{
    /* Dump the 'pixels' over TCP to the client */

    Uint8 buf[WIDTH * HEIGHT * 3];
    Uint8 *ptr = buf;

    for (int c = 0; c < HEIGHT * WIDTH; c++) {
        Uint8 r, g, b;
        Uint32 pixel = PPU::pixels[c];

        r = (0x00ff0000 & pixel) >> 16;
        g = (0x0000ff00 & pixel) >> 8;
        b = (0x000000ff & pixel);

        *(ptr++) = r;
        *(ptr++) = g;
        *(ptr++) = b;
    }
    write(controller_fd, buf, sizeof(buf));
}

/* Parse an actual command string that's come in */
void handle_remote_command(char *cmd)
{
    if (strstarts("j ", cmd)) {
        /* joypad value */
        char *p = cmd;
        p += 2;
        unsigned short pad_value, pad;

        int num_read = sscanf(p, "%hu %hu", &pad_value, &pad);

        if (num_read == 0) {
            printf("Malformed input!");
            send_message_to_remote((char *)"malformed input. Expected 'p <value> [pad (0 / 1]'");
            return;
        }

        if (num_read == 1) {
            // user didn't specify pad, assume 0 
            pad = 0;
        }
        printf("cmd = >>%s<<, Setting remote joypad state to %d\n", cmd, pad_value);
        remote_joypad_state[pad] = pad_value;
    }
    else if (strstarts("reset", cmd)) {
        /* reset */
        send_message_to_remote((char *)"Resetting console!");
        printf("Resetting console!\n");

        CPU::power();
        PPU::reset();
        APU::reset();
    }
    else if (strstarts("screen", cmd)) {
        /* dump contents of the screen */
        send_screen_to_remote();
    }
    else if (strstarts("binscreen", cmd)) {
        /* dump contents of the screen in binary */
        send_binary_screen_to_remote();
    }
    else if (strstarts("poweroff", cmd)) {
        /* kill ourselves */
        printf("Powering off console\n");
        exit(0);
    }
}


char remote_input_buffer[1000] = "";
char *remote_buf_input_ptr = remote_input_buffer;
/* handle the remote player */
void handle_remote_input()
{
    /* Read commands from tcp/ip */

    /* try to read from the tcp socket (non-blocking), to see if
    there's more user input */
    char tmp_buf[1000];
    int read_bytes = 0;
    bzero(tmp_buf, 1000);
    read_bytes = read(controller_fd, tmp_buf, sizeof(tmp_buf));

    if (read_bytes > 0) {
        /* append tmp_buf to remote_input buffer, and see if there is a 
           complete command to process */
        strncat(remote_input_buffer, tmp_buf, sizeof(remote_input_buffer) - 1);

        char *p = (char *)(tmp_buf + strlen(tmp_buf) - 1);
        bool found_command;
        do {
            /* see if there's a command */
            char *p = remote_input_buffer;
            found_command = false;
            while (*p && found_command == false) {
                if (*p == '\n') {
                    found_command = true;
                } else {
                    p++;
                }
            }

            if (found_command) {
                /* command exists between beginning of remote_input_buffer and p */
                char cmd[1000];
                strncpy(cmd, remote_input_buffer, p - remote_input_buffer);
                cmd[p - remote_input_buffer] = '\0';

                //printf("REMOTE COMMAND:  >>>%s<<<\n", cmd);
                handle_remote_command(cmd);

                /* copy contents from p until '\0' is encounted to a buffer, then 
                   copy the buffer contents to remote_input_buffer */
                if (*p == '\n')
                    p++; // we need to get rid of the \n in *p if it's there
                memcpy(remote_input_buffer, p, sizeof(remote_input_buffer) - 1);
            }
        } while (found_command);
    }
}


/* Get the joypad state from SDL */
u8 get_joypad_state(int n)
{
    static int been_here_before = 0;

    u8 j = 0;
    j |= (keys[KEY_A[n]])      << 0;
    j |= (keys[KEY_B[n]])      << 1;
    j |= (keys[KEY_SELECT[n]]) << 2;
    j |= (keys[KEY_START[n]])  << 3;
    j |= (keys[KEY_UP[n]])     << 4;
    j |= (keys[KEY_DOWN[n]])   << 5;
    j |= (keys[KEY_LEFT[n]])   << 6;
    j |= (keys[KEY_RIGHT[n]])  << 7;

    u8 keyboard_state = j;

    if (controller_fd > 0) {
        /* tcp connection has been set up */

        /* note, we OR this here, so that we can still use the
           keyboard to play with when testing... */
        j |= get_joypad_state_from_tcp(n);
//        printf("Remote state after tcp: %d\n", j);
    }

//    printf("keyboard_state: %d, get_joypad_state:  %d\n", keyboard_state, j);
    return j;
}

/* Send the rendered frame to the GUI */
void new_frame(u32* pixels)
{
    SDL_UpdateTexture(gameTexture, NULL, pixels, WIDTH * sizeof(u32));
}

void new_samples(const blip_sample_t* samples, size_t count)
{
//    soundQueue->write(samples, count);
}

/* Render the screen */
void render()
{
    SDL_RenderClear(renderer);

    // Draw the NES screen:
    if (Cartridge::loaded())
        SDL_RenderCopy(renderer, gameTexture, NULL, NULL);
    else
        SDL_RenderCopy(renderer, background, NULL, NULL);

    SDL_RenderPresent(renderer);
}

/* Play/stop the game */
void toggle_pause()
{
    pause = not pause;

    if (pause)
        SDL_SetTextureColorMod(gameTexture,  60,  60,  60);
    else
        SDL_SetTextureColorMod(gameTexture, 255, 255, 255);
}

/* Prompt for a key, return the scancode */
SDL_Scancode query_key()
{
    SDL_Texture* prompt = gen_text("Press a key...", { 255, 255, 255 });
    render_texture(prompt, TEXT_CENTER, HEIGHT - FONT_SZ*4);
    SDL_RenderPresent(renderer);

    SDL_Event e;
    while (true)
    {
        SDL_PollEvent(&e);
        if (e.type == SDL_KEYDOWN)
            return e.key.keysym.scancode;
    }
}

int query_button()
{
    SDL_Texture* prompt = gen_text("Press a button...", { 255, 255, 255 });
    render_texture(prompt, TEXT_CENTER, HEIGHT - FONT_SZ*4);
    SDL_RenderPresent(renderer);

    SDL_Event e;
    while (true)
    {
        SDL_PollEvent(&e);
        if (e.type == SDL_JOYBUTTONDOWN)
            return e.jbutton.button;
    }
}

/* Run the emulator */
void run()
{
    SDL_Event e;

    // Framerate control:
    u32 frameStart, frameTime;
    const int FPS   = 60;
    const int DELAY = 1000.0f / FPS;

    while (true)
    {
        frameStart = SDL_GetTicks();

        if (controller_fd > 0) {
            /* we have tcp conn */
            handle_remote_input();

            /* send screen updates */

            //send_screen_to_remote();
        }

        // Handle events:
        while (SDL_PollEvent(&e))
            switch (e.type)
            {
                case SDL_QUIT: return;
                case SDL_KEYDOWN:

                    /* Could maybe inject key presses here? */
                    if (keys[SDL_SCANCODE_ESCAPE] and Cartridge::loaded())
                        toggle_pause();
            }

        if (not pause) CPU::run_frame();
        render();

        // Wait to mantain framerate:
        frameTime = SDL_GetTicks() - frameStart;
        if (frameTime < DELAY)
            SDL_Delay((int)(DELAY - frameTime));
    }
}


}
