#include <cstdlib>
#include <SimpleIni.h>
#include "config.hpp"
#include "gui.hpp"

namespace GUI {

/* Settings */
CSimpleIniA ini(true, false, false);

/* Window settings */
int last_window_size = 1;

/* Controls settings */
SDL_Scancode KEY_A     [] = { SDL_SCANCODE_A,      SDL_SCANCODE_ESCAPE };
SDL_Scancode KEY_B     [] = { SDL_SCANCODE_S,      SDL_SCANCODE_ESCAPE };
SDL_Scancode KEY_SELECT[] = { SDL_SCANCODE_SPACE,  SDL_SCANCODE_ESCAPE };
SDL_Scancode KEY_START [] = { SDL_SCANCODE_RETURN, SDL_SCANCODE_ESCAPE };
SDL_Scancode KEY_UP    [] = { SDL_SCANCODE_UP,     SDL_SCANCODE_ESCAPE };
SDL_Scancode KEY_DOWN  [] = { SDL_SCANCODE_DOWN,   SDL_SCANCODE_ESCAPE };
SDL_Scancode KEY_LEFT  [] = { SDL_SCANCODE_LEFT,   SDL_SCANCODE_ESCAPE };
SDL_Scancode KEY_RIGHT [] = { SDL_SCANCODE_RIGHT,  SDL_SCANCODE_ESCAPE };
int BTN_UP    [] = { -1, -1 };
int BTN_DOWN  [] = { -1, -1 };
int BTN_LEFT  [] = { -1, -1 };
int BTN_RIGHT [] = { -1, -1 };
int BTN_A     [] = { -1, -1 };
int BTN_B     [] = { -1, -1 };
int BTN_SELECT[] = { -1, -1 };
int BTN_START [] = { -1, -1 };




/* Load settings */
void load_settings(int screen_size)
{
    set_size(screen_size);

    /* Control settings */
    for (int p = 0; p < 1; p++)
    {
        const char* section = p==0?"controls p1":"controls p2";
        KEY_UP[p] = (SDL_Scancode)82;
        KEY_DOWN[p] = (SDL_Scancode)81;
        KEY_LEFT[p] = (SDL_Scancode)80;
        KEY_RIGHT[p] = (SDL_Scancode)79;
        KEY_A[p] = (SDL_Scancode)4;
        KEY_B[p] = (SDL_Scancode)22;
        KEY_SELECT[p] = (SDL_Scancode)44;
        KEY_START[p] = (SDL_Scancode)40;
    }
}



}
