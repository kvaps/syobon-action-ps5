//This file is a reverse engineered "DxLib.h" to the extent that
//Syobon Action uses it. Minor functions are just replaced with SDL
//counterparts.
#include <SDL2/SDL.h>
#include <SDL2/SDL2_rotozoom.h>
#include <SDL2/SDL2_gfxPrimitives.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_mixer.h>
#include <SDL2/SDL_ttf.h>
#include <time.h>
#include <stdio.h>
#include <math.h>
#include <string>

#include "joyconfig.h"

#define TRUE 1
#define FALSE 0
#define byte unsigned char

void ScreenFlip();
#define GetNowCount() SDL_GetTicks()

//UNIMPLEMENTED - macro substitution
#define SetFontThickness(f);
#define StopSoundMem(s);
#define ChangeVolumeSoundMem(s, v);

//Sound
#define DX_PLAYTYPE_LOOP -1
#define DX_PLAYTYPE_BACK 0
void PlaySoundMem(Mix_Chunk* s, int l);
Mix_Chunk* LoadSoundMem(const char* f);
Mix_Music* LoadMusicMem(const char* f);
#define CheckSoundMem(s) !s

int DxLib_Init();

// Debug trace to stderr and /data/syobon/log.txt (PS5)
void Trace(const char *fmt, ...);

//Main screen: the game draws into a 480x420 surface, ScreenFlip() scales it
//to the window; screenRenderer draws SDL2_gfx primitives into that surface
extern SDL_Surface *screen;
extern SDL_Renderer *screenRenderer;

//Fonts
#define FONT_MAX 64
extern TTF_Font *font[FONT_MAX];

//Strings & fonts
#define DX_FONTTYPE_NORMAL 0
#define DX_FONTTYPE_EDGE 1
void SetFontSize(byte size);
void ChangeFontType(byte type);
void DrawString(int a, int b, const char *x, Uint32 c);
void DrawFormatString(int a, int b, Uint32 color, const char *str, ...);

//Key Aliases
#define KEY_INPUT_ESCAPE SDL_SCANCODE_ESCAPE
#define KEY_INPUT_LEFT SDL_SCANCODE_LEFT
#define KEY_INPUT_RIGHT SDL_SCANCODE_RIGHT
#define KEY_INPUT_DOWN SDL_SCANCODE_DOWN
#define KEY_INPUT_UP SDL_SCANCODE_UP
#define KEY_INPUT_F1 SDL_SCANCODE_F1
#define KEY_INPUT_O SDL_SCANCODE_O
#define KEY_INPUT_Z SDL_SCANCODE_Z
#define KEY_INPUT_RETURN SDL_SCANCODE_RETURN
#define KEY_INPUT_SPACE SDL_SCANCODE_SPACE
#define KEY_INPUT_1 SDL_SCANCODE_1
#define KEY_INPUT_2 SDL_SCANCODE_2
#define KEY_INPUT_3 SDL_SCANCODE_3
#define KEY_INPUT_4 SDL_SCANCODE_4
#define KEY_INPUT_5 SDL_SCANCODE_5
#define KEY_INPUT_6 SDL_SCANCODE_6
#define KEY_INPUT_7 SDL_SCANCODE_7
#define KEY_INPUT_8 SDL_SCANCODE_8
#define KEY_INPUT_9 SDL_SCANCODE_9
#define KEY_INPUT_0 SDL_SCANCODE_0
#define KEY_INPUT_PGUP SDL_SCANCODE_PAGEUP
#define KEY_INPUT_PGDN SDL_SCANCODE_PAGEDOWN

extern SDL_Joystick* joystick;

void UpdateKeys();
byte ProcessMessage();
byte CheckHitKey(int key);
byte WaitKey();

#define GetColor(r, g, b) SDL_MapRGB(screen->format, r, g, b)

//Uint32 GetColor(byte r, byte g, byte b);

#define DrawGraph(a, b, mx, z) DrawGraphZ(a, b, mx)
void DrawGraphZ(int a, int b, SDL_Surface * mx);

#define DrawTurnGraph(a, b, mx, z) DrawTurnGraphZ(a, b, mx)
void DrawTurnGraphZ(int a, int b, SDL_Surface * mx);

//#define DrawVertTurnGraph(x, y, e, a, mx, z) DrawRotaGraphZ(x, y, a, mx)
void DrawVertTurnGraph(int a, int b, SDL_Surface * mx);

SDL_Surface *DerivationGraph(int srcx, int srcy, int width, int height,
			     SDL_Surface * src);

//Noticably different than the original
SDL_Surface *LoadGraph(const char *filename);
