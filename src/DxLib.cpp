#include "DxLib.h"
#include <locale.h>
#ifdef __PROSPERO__
#include <unistd.h>
#include <sys/stat.h>
#endif

SDL_Joystick* joystick;
static SDL_GameController* controller;
static SDL_Window* window;
static SDL_Surface* windowSurface;

bool keysHeld[SDL_NUM_SCANCODES];

// The game names its files relative to its folder: a title's working
// directory is not that folder, so they are looked up under assetDir
static const char *assetDir = "";
struct AssetPath {
    char path[512];
    explicit AssetPath(const char *name) { snprintf(path, sizeof(path), "%s%s", assetDir, name); }
    const char *c_str() const { return path; }
};
static AssetPath assetPath(const char *name)
{
    return AssetPath(name);
}



void Trace(const char *fmt, ...)
{
    static FILE *log;
#ifdef __PROSPERO__
    if (!log)
	log = fopen("/data/syobon/log.txt", "w");
    if (!log)
	log = fopen("/download0/log.txt", "w");
#endif
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fflush(stderr);
    if (log) {
	va_start(args, fmt);
	vfprintf(log, fmt, args);
	va_end(args);
	fflush(log);
    }
}
bool sound = true;
void deinit();
int DxLib_Init()
{
    atexit(deinit);
    setlocale(LC_CTYPE, "ja_JP.UTF-8");

#ifdef __PROSPERO__
    // The game opens res/, BGM/ and SE/ by relative path: they sit beside the
    // eboot in a homebrew title (/app0), or in /data/syobon for the payload.
    struct stat st;
    if (stat("/app0/assets/res", &st) == 0) assetDir = "/app0/assets/";
    else assetDir = "/data/syobon/";
    Trace("assets: %s\n", assetDir);
#endif

    Trace("init: dir ok\n");
    static const struct { Uint32 flag; const char *name; } subsystems[] = {
	{ 0, "core" },
	{ SDL_INIT_EVENTS, "events" },
	{ SDL_INIT_VIDEO, "video" },
	{ SDL_INIT_GAMECONTROLLER, "gamecontroller" },
	{ SDL_INIT_AUDIO, "audio" },
    };
    for (const auto &subsystem : subsystems) {
	Trace("init: SDL %s...\n", subsystem.name);
	int failed = subsystem.flag ? SDL_InitSubSystem(subsystem.flag) : SDL_Init(0);
	if (failed < 0) {
	    Trace("Unable to init SDL %s: %s\n", subsystem.name, SDL_GetError());
	    if (subsystem.flag != SDL_INIT_AUDIO && subsystem.flag != SDL_INIT_GAMECONTROLLER)
		return -1;
	    if (subsystem.flag == SDL_INIT_AUDIO)
		sound = false;
	}
    }

    Trace("init: SDL ok\n");
    // The window's own surface, scaled into by ScreenFlip(): the PS5 video
    // driver gives windows a framebuffer surface and no SDL_Renderer.
    SDL_DisplayMode mode;
    int w = 480 * 2, h = 420 * 2;
#ifdef __PROSPERO__
    if (SDL_GetCurrentDisplayMode(0, &mode) == 0) {
	w = mode.w;
	h = mode.h;
    }
#endif
    window = SDL_CreateWindow("Syobon Action (しょぼんのアクション)",
			      SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
			      w, h, SDL_WINDOW_RESIZABLE);
    Trace("init: window %p (%dx%d): %s\n", (void *)window, w, h, SDL_GetError());
    if (window)
	windowSurface = SDL_GetWindowSurface(window);
    Trace("init: window surface %p: %s\n", (void *)windowSurface, SDL_GetError());
    if (windowSurface)
	screen = SDL_CreateRGBSurfaceWithFormat(0, 480 /*(int)fmax/100 */ ,
						420 /*(int)fymax/100 */ , 32,
						windowSurface->format->format);
    Trace("init: screen %p: %s\n", (void *)screen, SDL_GetError());
    if (screen)
	screenRenderer = SDL_CreateSoftwareRenderer(screen);
    if (!screenRenderer) {
	Trace("Unable to set up the screen: %s\n", SDL_GetError());
	SDL_Quit();
	return -1;
    }
    Trace("video: driver %s, window %dx%d, format %s\n", SDL_GetCurrentVideoDriver(),
	  windowSurface->w, windowSurface->h, SDL_GetPixelFormatName(windowSurface->format->format));
    SDL_ShowCursor(SDL_DISABLE);

    Trace("init: video ok\n");
    if(IMG_Init(IMG_INIT_PNG) != IMG_INIT_PNG)
    {
        Trace("Unable to init SDL_img: %s\n", IMG_GetError());
        return -1;
    }

    Trace("init: image ok\n");
    //Initialize font
    if (TTF_Init() == -1) {
	Trace("Unable to init SDL_ttf: %s\n", TTF_GetError());
	return -1;
    }

    Trace("init: ttf ok\n");
    //Audio Rate, Audio Format, Audio Channels, Audio Buffers
#define AUDIO_CHANNELS 2
    if (sound && Mix_OpenAudio(22050, AUDIO_S16SYS, AUDIO_CHANNELS, 1024)) {
        Trace("Unable to init SDL_mixer: %s\n", Mix_GetError());
        sound = false;
        }
    //Try to get a controller, or else a plain joystick
    for (int i = 0; i < SDL_NumJoysticks() && !controller; i++)
	if (SDL_IsGameController(i))
	    controller = SDL_GameControllerOpen(i);
    if (!controller)
	joystick = SDL_JoystickOpen(0);

    for (int i = 0; i < SDL_NUM_SCANCODES; i++)
	keysHeld[i] = false;
    for (int i = 0; i < FONT_MAX; i++)
	font[i] = NULL;
    srand(time(NULL));
    Trace("init: sound %d, controller %s, joysticks %d\n", sound,
	  controller ? SDL_GameControllerName(controller) : "none", SDL_NumJoysticks());

    return 0;
}

//Main screen
SDL_Surface *screen;
SDL_Renderer *screenRenderer;

// Scale the 480x420 picture into the window, keeping its shape
void ScreenFlip()
{
    static int frames;
    windowSurface = SDL_GetWindowSurface(window);
    if (!windowSurface) {
	Trace("flip: no window surface: %s\n", SDL_GetError());
	return;
    }
    int w = windowSurface->w, h = windowSurface->h;
    SDL_Rect dst;
    if (w * 420 > h * 480) {
	dst.h = h;
	dst.w = h * 480 / 420;
    } else {
	dst.w = w;
	dst.h = w * 420 / 480;
    }
    dst.x = (w - dst.w) / 2;
    dst.y = (h - dst.h) / 2;
    SDL_FillRect(windowSurface, NULL, SDL_MapRGB(windowSurface->format, 0, 0, 0));
    SDL_BlitScaled(screen, NULL, windowSurface, &dst);
    if (frames < 3)
	Trace("flip %d: blitted, updating\n", frames);
    int updated = SDL_UpdateWindowSurface(window);
    if (updated < 0 || frames < 3 || frames % 300 == 0)
	Trace("flip %d: %d\n", frames, updated);
    frames++;
}

//Fonts
byte fontsize = 0;
TTF_Font *font[FONT_MAX];

//Strings
void SetFontSize(byte size)
{
    fontsize = size;
    if (font[size] == NULL) {
	font[size] = TTF_OpenFont(assetPath("res/sazanami-gothic.ttf").c_str(), size);
	if (font[size] == NULL) {
	    Trace("Unable to load font: %s\n", TTF_GetError());
	    exit(1);
	}
    }
}

byte fontType = DX_FONTTYPE_NORMAL;
void ChangeFontType(byte type)
{
    fontType = type;
}

void DrawString(int a, int b, const char *x, Uint32 c)
{
    SDL_Color color = { (Uint8)(c >> 16), (Uint8)(c >> 8), (Uint8)c, 255 };
    SDL_Surface *rendered = TTF_RenderUTF8_Solid(font[fontsize], x, color);
    if (fontType == DX_FONTTYPE_EDGE) {
	SDL_Color blk = { 0, 0, 0 };
	SDL_Surface *shadow = TTF_RenderUTF8_Solid(font[fontsize], x, blk);
	DrawGraphZ(a - 1, b - 1, shadow);
	DrawGraphZ(a, b - 1, shadow);
	DrawGraphZ(a + 1, b - 1, shadow);
	DrawGraphZ(a - 1, b, shadow);
	DrawGraphZ(a + 1, b, shadow);
	DrawGraphZ(a - 1, b + 1, shadow);
	DrawGraphZ(a, b + 1, shadow);
	DrawGraphZ(a + 1, b + 1, shadow);
	SDL_FreeSurface(shadow);
    }
    DrawGraphZ(a, b, rendered);
    SDL_FreeSurface(rendered);
}

void DrawFormatString(int a, int b, Uint32 color, const char *str, ...)
{
    va_list args;
    char *newstr = new char[strlen(str) + 16];
    va_start(args, str);
    vsprintf(newstr, str, args);
    va_end(args);
    DrawString(a, b, newstr, color);
    delete newstr;
}

//void DrawFormatString(int a, int b, int c


bool ex = false;

// A controller's buttons stand for the keys the game reads
static SDL_Scancode controllerKey(Uint8 button)
{
    switch (button) {
    case SDL_CONTROLLER_BUTTON_DPAD_LEFT: return SDL_SCANCODE_LEFT;
    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: return SDL_SCANCODE_RIGHT;
    case SDL_CONTROLLER_BUTTON_DPAD_UP: return SDL_SCANCODE_UP;
    case SDL_CONTROLLER_BUTTON_DPAD_DOWN: return SDL_SCANCODE_DOWN;
    case SDL_CONTROLLER_BUTTON_A: return SDL_SCANCODE_Z;		// Cross: jump
    case SDL_CONTROLLER_BUTTON_B: return SDL_SCANCODE_Z;		// Circle: jump
    case SDL_CONTROLLER_BUTTON_X: return SDL_SCANCODE_SPACE;	// Square: double speed
    case SDL_CONTROLLER_BUTTON_Y: return SDL_SCANCODE_F1;		// Triangle: title screen
    case SDL_CONTROLLER_BUTTON_START: return SDL_SCANCODE_RETURN;	// Options: start, pause
    case SDL_CONTROLLER_BUTTON_BACK: return SDL_SCANCODE_ESCAPE;	// Create: exit
    case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: return SDL_SCANCODE_PAGEUP;	// L1: previous stage
    case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: return SDL_SCANCODE_PAGEDOWN;	// R1: next stage
    default: return SDL_SCANCODE_UNKNOWN;
    }
}

static void stickKeys(Sint16 value, SDL_Scancode less, SDL_Scancode more)
{
    const Sint16 dead = 16000;
    keysHeld[less] = value < -dead;
    keysHeld[more] = value > dead;
}

void UpdateKeys()
{
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
	switch (event.type) {
	case SDL_KEYDOWN:
	    keysHeld[event.key.keysym.scancode] = true;
	    break;
	case SDL_KEYUP:
	    keysHeld[event.key.keysym.scancode] = false;
	    break;
	case SDL_CONTROLLERBUTTONDOWN:
	case SDL_CONTROLLERBUTTONUP:
	    if (SDL_Scancode key = controllerKey(event.cbutton.button))
		keysHeld[key] = event.type == SDL_CONTROLLERBUTTONDOWN;
	    break;
	case SDL_CONTROLLERAXISMOTION:
	    if (event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTX)
		stickKeys(event.caxis.value, SDL_SCANCODE_LEFT, SDL_SCANCODE_RIGHT);
	    else if (event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTY)
		stickKeys(event.caxis.value, SDL_SCANCODE_UP, SDL_SCANCODE_DOWN);
	    break;
	case SDL_CONTROLLERDEVICEADDED:
	    if (!controller)
		controller = SDL_GameControllerOpen(event.cdevice.which);
	    break;
	case SDL_JOYAXISMOTION:
	    if(!controller && event.jaxis.which == 0)
	    {
		if(event.jaxis.axis == JOYSTICK_XAXIS)
		    stickKeys(event.jaxis.value, SDL_SCANCODE_LEFT, SDL_SCANCODE_RIGHT);
		else if(event.jaxis.axis == JOYSTICK_YAXIS)
		    stickKeys(event.jaxis.value, SDL_SCANCODE_UP, SDL_SCANCODE_DOWN);
	    }
	    break;
	case SDL_QUIT:
	    ex = true;
	    break;
	}
    }
}

byte ProcessMessage()
{
    return ex;
}

byte CheckHitKey(int key)
{
    if(key == SDL_SCANCODE_Z && keysHeld[SDL_SCANCODE_SEMICOLON]) return true;
    return keysHeld[key];
}

byte WaitKey()
{
    SDL_Event event;
    while (true) {
	while (SDL_PollEvent(&event)) {
	    if (event.type == SDL_KEYDOWN)
		return event.key.keysym.scancode;
	    if (event.type == SDL_CONTROLLERBUTTONDOWN)
		return controllerKey(event.cbutton.button);
	}
	SDL_Delay(10);
    }
}

/*Uint32 GetColor(byte r, byte g, byte b)
{
    return r << 8 * 3 | g << 8 * 2 | b << 8 | 0xFF;
}*/

void DrawGraphZ(int a, int b, SDL_Surface * mx)
{
    if(mx)
    {
        SDL_Rect offset;
        offset.x = a;
        offset.y = b;
        SDL_BlitSurface(mx, NULL, screen, &offset);
    }
}

void DrawTurnGraphZ(int a, int b, SDL_Surface * mx)
{
    if(mx)
    {
        SDL_Rect offset;
        offset.x = a;
        offset.y = b;

        SDL_Surface *flipped = zoomSurface(mx, -1, 1, 0);
        SDL_SetColorKey(flipped, SDL_TRUE,
                SDL_MapRGB(flipped->format, 9 * 16 + 9, 255, 255));
        SDL_BlitSurface(flipped, NULL, screen, &offset);
        SDL_FreeSurface(flipped);
    }
}

void DrawVertTurnGraph(int a, int b, SDL_Surface * mx)
{
    if(mx)
    {
        SDL_Rect offset;
        offset.x = a - mx->w / 2;
        offset.y = b - mx->h / 2;

        SDL_Surface *flipped = rotozoomSurface(mx, 180, 1, 0);
        SDL_SetColorKey(flipped, SDL_TRUE,
                SDL_MapRGB(flipped->format, 9 * 16 + 9, 255, 255));
        SDL_BlitSurface(flipped, NULL, screen, &offset);
        SDL_FreeSurface(flipped);
    }
}

SDL_Surface *DerivationGraph(int srcx, int srcy, int width, int height,
			     SDL_Surface * src)
{
    // The PNGs are 24-bit: SDL2 has no 32-bit format with their masks, so
    // the piece is cut into the screen's format
    SDL_Surface *img =
	SDL_CreateRGBSurfaceWithFormat(0, width, height, 32,
				       screen->format->format);
    if (!img) {
	Trace("Unable to cut %dx%d from %p: %s\n", width, height, (void *)src, SDL_GetError());
	return NULL;
    }

    SDL_Rect offset;
    offset.x = srcx;
    offset.y = srcy;
    offset.w = width;
    offset.h = height;

    SDL_BlitSurface(src, &offset, img, NULL);
    SDL_SetColorKey(img, SDL_TRUE,
		    SDL_MapRGB(img->format, 9 * 16 + 9, 255, 255));
    return img;
}

//Noticably different than the original
SDL_Surface *LoadGraph(const char *filename)
{
    Trace("load %s\n", filename);
    SDL_Surface *image = IMG_Load(assetPath(filename).c_str());

    if (image) return image;
	Trace("Error: Unable to load %s: %s\n", filename, IMG_GetError());
	exit(1);
}

void PlaySoundMem(Mix_Chunk* s, int l)
{
    if(sound) Mix_PlayChannel(-1, s, l);
}

Mix_Chunk* LoadSoundMem(const char* f)
{
    if(!sound) return NULL;

    Trace("load %s\n", f);
    Mix_Chunk* s = Mix_LoadWAV(assetPath(f).c_str());
    if(s) return s;
    Trace("Error: Unable to load sound %s: %s\n", f, Mix_GetError());
    return NULL;
}

Mix_Music* LoadMusicMem(const char* f)
{
    if(!sound) return NULL;

    Trace("load %s\n", f);
    Mix_Music* m = Mix_LoadMUS(assetPath(f).c_str());
    if(m) return m;
    Trace("Error: Unable to load music %s: %s\n", f, Mix_GetError());
    return NULL;
}

