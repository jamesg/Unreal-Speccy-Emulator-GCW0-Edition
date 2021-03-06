/*
Portable ZX-Spectrum emulator.
Copyright (C) 2001-2010 SMT, Dexus, Alone Coder, deathsoft, djdron, scor

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "../platform.h"
#include "../../speccy.h"
#ifdef USE_SDL
#ifndef SDL_UNUSE_VIDEO

#include <SDL.h>
#include "../../ui/ui.h"

#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 240
#define BPP            16
#define BORDER_WIDTH   32
#define BORDER_HEIGHT  24

namespace xPlatform
{

static SDL_Surface* screen = NULL;
static SDL_Surface* offscreen = NULL;

static struct eCachedColors
{
	inline dword RGBX(byte r, byte g, byte b) const { return (b << 16)|(g << 8)|r; }
	void Init(SDL_PixelFormat* pf)
	{
		const byte brightness = 200;
		const byte bright_intensity = 55;
		for(int c = 0; c < 16; ++c)
		{
			byte i = c&8 ? brightness + bright_intensity : brightness;
			byte b = c&1 ? i : 0;
			byte r = c&2 ? i : 0;
			byte g = c&4 ? i : 0;
			items[c] = SDL_MapRGB(pf, r, g, b);
			items_rgbx[c] = RGBX(r, g, b);
		}
	}
	word items[16];
	dword items_rgbx[16];
}
color_cache;

bool InitVideo()
{
#ifdef GCWZERO
    screen = SDL_SetVideoMode(SCREEN_WIDTH, SCREEN_HEIGHT, BPP, SDL_HWSURFACE|SDL_TRIPLEBUF);
#else
    screen = SDL_SetVideoMode(SCREEN_WIDTH, SCREEN_HEIGHT, BPP, SDL_HWSURFACE);
#endif
    if(!screen)
        return false;
	offscreen = SDL_CreateRGBSurface(SDL_SWSURFACE, SCREEN_WIDTH, SCREEN_HEIGHT, BPP,
						screen->format->Rmask,
						screen->format->Gmask,
						screen->format->Bmask,
						screen->format->Amask);
	if(!offscreen)
		return false;
	color_cache.Init(screen->format);
	return true;
}
void DoneVideo()
{
	if(offscreen)
		SDL_FreeSurface(offscreen);
	if(screen)
		SDL_FreeSurface(screen);
}

void UpdateScreen()
{

#ifdef GCWZERO
	static int gcw_fullscreen_current = 0;
	if (gcw_fullscreen_current != gcw_fullscreen)
	{
		gcw_fullscreen_current = gcw_fullscreen;
		if(gcw_fullscreen)
		{
			screen = SDL_SetVideoMode(SCREEN_WIDTH - 2 * BORDER_WIDTH, SCREEN_HEIGHT - 2 * BORDER_HEIGHT, BPP, SDL_HWSURFACE|SDL_TRIPLEBUF);
		}
		else
		{
			screen = SDL_SetVideoMode(SCREEN_WIDTH, SCREEN_HEIGHT, BPP, SDL_HWSURFACE|SDL_TRIPLEBUF);
		}
	}
#endif

	SDL_LockSurface(offscreen);
	byte* data = (byte*)Handler()->VideoData();
	word* scr = (word*)offscreen->pixels;
#ifdef USE_UI
	byte* data_ui = (byte*)Handler()->VideoDataUI();
	if(data_ui)
	{
		for(int y = 0; y < SCREEN_HEIGHT; ++y)
		{
			for(int x = 0; x < SCREEN_WIDTH; ++x)
			{
				xUi::eRGBAColor c_ui = xUi::palette[*data_ui++];
				xUi::eRGBAColor c = color_cache.items_rgbx[*data++];
				*scr++ = SDL_MapRGB(screen->format, (c.r >> c_ui.a) + c_ui.r, (c.g >> c_ui.a) + c_ui.g, (c.b >> c_ui.a) + c_ui.b);
			}
			scr += offscreen->pitch - SCREEN_WIDTH * 2;
		}
	}
	else
#endif//USE_UI
	{
		for(int y = 0; y < SCREEN_HEIGHT; ++y)
		{
			for(int x = 0; x < (SCREEN_WIDTH / 4); ++x)
			{
				*scr++ = color_cache.items[*data++];
				*scr++ = color_cache.items[*data++];
				*scr++ = color_cache.items[*data++];
				*scr++ = color_cache.items[*data++];
			}
			scr += offscreen->pitch - SCREEN_WIDTH * 2;
		}
	}


	SDL_UnlockSurface(offscreen);
#ifdef GCWZERO
	if (gcw_fullscreen)
	{
		SDL_Rect dst;
		dst.x = BORDER_WIDTH;
		dst.y = BORDER_HEIGHT;
		dst.w = SCREEN_WIDTH - 2 * dst.x;
		dst.h = SCREEN_HEIGHT - 2 * dst.y;
		SDL_BlitSurface(offscreen, &dst, screen, NULL);
	}
	else
	{
		SDL_BlitSurface(offscreen, NULL, screen, NULL);
	}
#else
	SDL_BlitSurface(offscreen, NULL, screen, NULL);
#endif

	SDL_Flip(screen);
}

}
//namespace xPlatform

#endif//SDL_UNUSE_VIDEO
#endif//USE_SDL
