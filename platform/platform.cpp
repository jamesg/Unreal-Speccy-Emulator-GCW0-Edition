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

#include "platform.h"

namespace xPlatform
{

static eHandler* handler = NULL;

eHandler::eHandler()
{
    //UP, DOWN, LEFT, RIGHT, FIRST, SELECT, ONE, TWO, A, B, C,
    //ALT_UP, ALT_DOWN, ALT_LEFT, ALT_RIGHT, ALT_FIRST, ALT_SELECT, ALT_ONE, ALT_TWO, ALT_A, ALT_B, ALT_C,
    for(int i = 0; i < END_PHYSICAL_KEY; ++i)
        m_keymap[i] = 0;
	assert(!handler);
	handler = this;
}
eHandler::~eHandler()
{
	handler = NULL;
}
void eHandler::RemapKey(physicalKey key, char action)
{
	m_keymap[key] = action;
}
eHandler* Handler() { return handler; }

void GetScaleWithAspectRatio43(float* sx, float* sy, int _w, int _h)
{
	*sx = 1.0f;
	*sy = 1.0f;
	float a = (float)_w/_h;
	float a43 = 4.0f/3.0f;
	if(a > a43)
		*sx = a43/a;
	else
		*sy = a/a43;
}

}
//namespace xPlatform
