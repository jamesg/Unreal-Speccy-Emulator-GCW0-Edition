/*
Portable ZX-Spectrum emulator.
Copyright (C) 2001-2012 SMT, Dexus, Alone Coder, deathsoft, djdron, scor

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

#include <iostream>

#include <map>
#include <stdexcept>
#include <libgen.h>
#include "platform/platform.h"
#include "speccy.h"
#include "devices/memory.h"
#include "devices/ula.h"
#include "devices/input/keyboard.h"
#include "devices/input/kempston_joy.h"
#include "devices/input/kempston_mouse.h"
#include "devices/input/tape.h"
#include "devices/sound/ay.h"
#include "devices/sound/beeper.h"
#include "devices/fdd/wd1793.h"
#include "z80/z80.h"
#include "snapshot/snapshot.h"
#include "platform/io.h"
#include "ui/ui_desktop.h"
#include "platform/custom_ui/ui_main.h"
#include "tools/profiler.h"
#include "tools/options.h"
#include "options_common.h"
#include "file_type.h"
#include "gameconfig.h"
#include "snapshot/rzx.h"

int gcw_fullscreen = 1;

namespace xPlatform
{

class eMacro
{
public:
	eMacro() : frame(-1) {}
	virtual ~eMacro() {}
	virtual bool Do() = 0;
	virtual bool Update()
	{
		++frame;
		return Do();
	}
protected:
	int frame;
};

static struct eSpeccyHandler : public eHandler, public eRZX::eHandler, public xZ80::eZ80::eHandlerIo
{
	eSpeccyHandler() : speccy(NULL), macro(NULL), replay(NULL), video_paused(0), inside_replay_update(false) {}
	virtual ~eSpeccyHandler() { assert(!speccy); }
	virtual void OnInit();
	virtual void OnDone();
	virtual const char* OnLoop();
	virtual void* VideoData() { return speccy->Device<eUla>()->Screen(); }
	virtual void* VideoDataUI()
	{
#ifdef USE_UI
		return ui_desktop->VideoData();
#else//USE_UI
		return NULL;
#endif//USE_UI
	}
	virtual const char* WindowCaption() { return "Unreal Speccy Portable"; }
	virtual void OnKey(char key, dword flags);
	virtual void OnMouse(eMouseAction action, byte a, byte b);
	virtual void Poke(int addr, byte mem);
	virtual void RefreshPokes();
	virtual bool FileTypeSupported(const char* name)
	{
		eFileType* t = eFileType::FindByName(name);
		return t && t->AbleOpen();
	}
	virtual bool OnOpenFile(const char* name, const void* data, size_t data_size);
	bool OpenFile(const char* name, const void* data, size_t data_size);
	virtual bool OnSaveFile(const char* name);
	virtual eActionResult OnAction(eAction action);

	virtual int	AudioSources() { return FullSpeed() ? 0 : SOUND_DEV_COUNT; }
	virtual void* AudioData(int source) { return sound_dev[source]->AudioData(); }
	virtual dword AudioDataReady(int source) { return sound_dev[source]->AudioDataReady(); }
	virtual void AudioDataUse(int source, dword size) { sound_dev[source]->AudioDataUse(size); }
	virtual void VideoPaused(bool paused) {	paused ? ++video_paused : --video_paused; }

	virtual bool FullSpeed() const { return speccy->CPU()->HandlerStep() != NULL; }

	void PlayMacro(eMacro* m) { SAFE_DELETE(macro); macro = m; }
	virtual bool RZX_OnOpenSnapshot(const char* name, const void* data, size_t data_size) { return OpenFile(name, data, data_size); }
	virtual byte Z80_IoRead(word port, int tact)
	{
		byte r = 0xff;
		replay->IoRead(&r);
		return r;
	}
	const char* RZXErrorDesc(eRZX::eError err) const;
	void Replay(eRZX* r)
	{
		speccy->CPU()->HandlerIo(NULL);
		SAFE_DELETE(replay);
		replay = r;
		if(replay)
			speccy->CPU()->HandlerIo(this);
	}

	eSpeccy* speccy;
	std::map<int, byte> m_poke;
#ifdef USE_UI
	xUi::eDesktop* ui_desktop;
#endif//USE_UI
	eMacro* macro;
	eRZX* replay;
	int video_paused;
	bool inside_replay_update;

	enum { SOUND_DEV_COUNT = 3 };
	eDeviceSound* sound_dev[SOUND_DEV_COUNT];
} sh;

void eSpeccyHandler::OnInit()
{
	assert(!speccy);
	speccy = new eSpeccy;
#ifdef USE_UI
	ui_desktop = new xUi::eDesktop;
	ui_desktop->Insert(new xUi::eMainDialog);
#endif//USE_UI
	sound_dev[0] = speccy->Device<eBeeper>();
	sound_dev[1] = speccy->Device<eAY>();
	sound_dev[2] = speccy->Device<eTape>();
	xOptions::Load();
	OnAction(A_RESET);
}
void eSpeccyHandler::OnDone()
{
	xOptions::Store();
	SAFE_DELETE(macro);
	SAFE_DELETE(replay);
	SAFE_DELETE(speccy);
#ifdef USE_UI
	SAFE_DELETE(ui_desktop);
#endif//USE_UI
	PROFILER_DUMP;
}
const char* eSpeccyHandler::OnLoop()
{
	const char* error = NULL;
	if(FullSpeed() || !video_paused)
	{
		if(macro)
		{
			if(!macro->Update())
				SAFE_DELETE(macro);
		}
		if(replay)
		{
			int icount = 0;
			inside_replay_update = true;
			eRZX::eError err = replay->Update(&icount);
			inside_replay_update = false;
			if(err == eRZX::E_OK)
			{
				speccy->Update(&icount);
				err = replay->CheckSync();
			}
			if(err != eRZX::E_OK)
			{
				Replay(NULL);
				error = RZXErrorDesc(err);
			}
		}
		else
			speccy->Update(NULL);
	}
#ifdef USE_UI
	ui_desktop->Update();
#endif//USE_UI
	return error;
}
const char* eSpeccyHandler::RZXErrorDesc(eRZX::eError err) const
{
	switch(err)
	{
	case eRZX::E_OK:		return "rzx_ok";
	case eRZX::E_FINISHED:		return "rzx_finished";
	case eRZX::E_SYNC_LOST:		return "rzx_sync_lost";
	case eRZX::E_INVALID:		return "rzx_invalid";
	case eRZX::E_UNSUPPORTED:	return "rzx_unsupported";
	}
	return NULL;
}
void eSpeccyHandler::OnKey(char key, dword flags)
{
	bool down = (flags&KF_DOWN) != 0;
	bool shift = (flags&KF_SHIFT) != 0;
	bool ctrl = (flags&KF_CTRL) != 0;
	bool alt = (flags&KF_ALT) != 0;

#ifdef USE_UI
	if(!(flags&KF_UI_SENDER))
	{
		ui_desktop->OnKey(key, flags);
		if(ui_desktop->Focused())
			return;
	}
#endif//USE_UI

	if(flags&KF_KEMPSTON)
		speccy->Device<eKempstonJoy>()->OnKey(key, down);

	if(flags&KF_CURSOR)
	{
		int i = -1;
		switch(key)
		{
			case 'l' : i = PHYK_LEFT; break;
			case 'r' : i = PHYK_RIGHT; break;
			case 'u' : i = PHYK_UP; break;
			case 'd' : i = PHYK_DOWN; break;
			// V, X, F, and S letter keys are used to represent 1, 2, F and S
			// gamepad buttons.
			case 'V' : i = PHYK_ONE; break;
			case 'X' : i = PHYK_TWO; break;
			case 'F': i = PHYK_FIRST; break;
			case 'S': i = PHYK_SELECT; break;
		}
		if(i > -1 && m_keymap[i] != 0)
		{
			key = m_keymap[i];
			if(key == 'c')
				shift = down;
			if(key == 's')
				alt = down;
		}
		switch(key)
		{
			case 'l' : key = '5'; shift = down; break;
			case 'r' : key = '8'; shift = down; break;
			case 'u' : key = '7'; shift = down; break;
			case 'd' : key = '6'; shift = down; break;
			case 'f' : key = '0'; shift = false; break;
		}
	}
	else if(flags&KF_QAOP)
	{
		switch(key)
		{
			case 'l' : key = 'O'; break;
			case 'r' : key = 'P'; break;
			case 'u' : key = 'Q'; break;
			case 'd' : key = 'A'; break;
			case 'f' : key = ' '; break;
		}
	}
	else if(flags&KF_SINCLAIR2)
	{
		switch(key)
		{
			case 'l' : key = '6'; break;
			case 'r' : key = '7'; break;
			case 'u' : key = '9'; break;
			case 'd' : key = '8'; break;
			case 'f' : key = '0'; break;
		}
	}

	speccy->Device<eKeyboard>()->OnKey(key, down, shift, ctrl, alt);
}
void eSpeccyHandler::OnMouse(eMouseAction action, byte a, byte b)
{
	switch(action)
	{
	case MA_MOVE: 	speccy->Device<eKempstonMouse>()->OnMouseMove(a, b); 	break;
	case MA_BUTTON:	speccy->Device<eKempstonMouse>()->OnMouseButton(a, b != 0);	break;
	default: break;
	}
}
void eSpeccyHandler::Poke(int addr, byte mem)
{
	m_poke[addr] = mem;
	speccy->Memory()->Write(addr, mem);
}
void eSpeccyHandler::RefreshPokes()
{
	for(std::map<int, byte>::const_iterator i = m_poke.begin(); i != m_poke.end(); ++i)
		speccy->Memory()->Write(i->first, i->second);
}
bool eSpeccyHandler::OnOpenFile(const char* name, const void* data, size_t data_size)
{
	OpLastFile(name);
	return OpenFile(name, data, data_size);
}
bool eSpeccyHandler::OpenFile(const char* name, const void* data, size_t data_size)
{
	eFileType* t = eFileType::FindByName(name);
	if(!t)
		return false;

	if(data && data_size)
		return t->Open(name, data, data_size);

	FILE* f = fopen(name, "rb");
	if(!f)
		return false;
	fseek(f, 0, SEEK_END);
	size_t size = ftell(f);
	fseek(f, 0, SEEK_SET);
	byte* buf = new byte[size];
	size_t r = fread(buf, 1, size, f);
	fclose(f);
	if(r != size)
	{
		delete[] buf;
		return false;
	}
	bool ok = t->Open(name, buf, size);
	delete[] buf;
	return ok;
}
bool eSpeccyHandler::OnSaveFile(const char* name)
{
	OpLastFile(name);
	eFileType* t = eFileType::FindByName(name);
	if(!t)
		return false;
	return t->Store(name);
}

static struct eOptionTapeFast : public xOptions::eOptionBool
{
	eOptionTapeFast() { Set(true); }
	virtual const char* Name() const { return "fast tape"; }
	virtual int Order() const { return 50; }
} op_tape_fast;

static struct eOptionAutoPlayImage : public xOptions::eOptionBool
{
	eOptionAutoPlayImage() { Set(true); }
	virtual const char* Name() const { return "auto play image"; }
	virtual int Order() const { return 55; }
} op_auto_play_image;

static struct eOption48K : public xOptions::eOptionBool
{
	virtual const char* Name() const { return "mode 48k"; }
	virtual void Change(bool next = true)
	{
		eOptionBool::Change();
		Apply();
	}
	virtual void Apply()
	{
		sh.OnAction(A_RESET);
	}
	virtual int Order() const { return 65; }
} op_48k;

static struct eOptionResetToServiceRom : public xOptions::eOptionBool
{
#ifdef GCWZERO
	virtual const char* Name() const { return "reset to s-rom"; }
#else
	virtual const char* Name() const { return "reset to service rom"; }
#endif
	virtual int Order() const { return 79; }
} op_reset_to_service_rom;

#ifdef GCWZERO
static struct eOptionFullscreen : public xOptions::eOptionBool
{
	virtual const char* Name() const { return "fullscreen"; }
	virtual void Change(bool next = true)
	{
		eOptionBool::Change();
		Apply();
	}
	virtual void Apply()
	{
                gcw_fullscreen = !gcw_fullscreen;
	}
	virtual int Order() const { return 75; }
} op_fullscreen;
#endif

eActionResult eSpeccyHandler::OnAction(eAction action)
{
	switch(action)
	{
	case A_RESET:
		if(!inside_replay_update) // can be called from replay->Update()
			SAFE_DELETE(replay);
		SAFE_DELETE(macro);
		speccy->Mode48k(op_48k);
		speccy->Reset();
		if(!speccy->Mode48k())
			speccy->Device<eRom>()->SelectPage(op_reset_to_service_rom ? eRom::ROM_SYS : eRom::ROM_128_1);
		if(inside_replay_update)
			speccy->CPU()->HandlerIo(this);
		return AR_OK;
	case A_TAPE_TOGGLE:
		{
			eTape* tape = speccy->Device<eTape>();
			if(!tape->Inserted())
				return AR_TAPE_NOT_INSERTED;
			if(!tape->Started())
			{
				if(op_tape_fast)
					speccy->CPU()->HandlerStep(fast_tape_emul);
				else
					speccy->CPU()->HandlerStep(NULL);
				tape->Start();
			}
			else
				tape->Stop();
			return tape->Started() ? AR_TAPE_STARTED : AR_TAPE_STOPPED;
		}
	case A_TAPE_QUERY:
		{
			eTape* tape = speccy->Device<eTape>();
			if(!tape->Inserted())
				return AR_TAPE_NOT_INSERTED;
			return tape->Started() ? AR_TAPE_STARTED : AR_TAPE_STOPPED;
		}
	}
	return AR_ERROR;
}

static void SetupSoundChip();
static struct eOptionSoundChip : public xOptions::eOptionInt
{
	eOptionSoundChip() { Set(SC_AY); }
	enum eType { SC_FIRST, SC_AY = SC_FIRST, SC_YM, SC_LAST };
	virtual const char* Name() const { return "sound chip"; }
	virtual const char** Values() const
	{
		static const char* values[] = { "ay", "ym", NULL };
		return values;
	}
	virtual void Change(bool next = true)
	{
		eOptionInt::Change(SC_FIRST, SC_LAST, next);
		Apply();
	}
	virtual void Apply()
	{
		SetupSoundChip();
	}
	virtual int Order() const { return 24; }
}op_sound_chip;

static struct eOptionAYStereo : public xOptions::eOptionInt
{
	eOptionAYStereo() { Set(AS_ABC); }
	enum eMode { AS_FIRST, AS_ABC = AS_FIRST, AS_ACB, AS_BAC, AS_BCA, AS_CAB, AS_CBA, AS_MONO, AS_LAST };
	virtual const char* Name() const { return "ay stereo"; }
	virtual const char** Values() const
	{
		static const char* values[] = { "abc", "acb", "bac", "bca", "cab", "cba", "mono", NULL };
		return values;
	}
	virtual void Change(bool next = true)
	{
		eOptionInt::Change(AS_FIRST, AS_LAST, next);
		Apply();
	}
	virtual void Apply()
	{
		SetupSoundChip();
	}
	virtual int Order() const { return 25; }
}op_ay_stereo;

void SetupSoundChip()
{
	eOptionSoundChip::eType chip = (eOptionSoundChip::eType)(int)op_sound_chip;
	eOptionAYStereo::eMode stereo = (eOptionAYStereo::eMode)(int)op_ay_stereo;
	eAY* ay = sh.speccy->Device<eAY>();
	const SNDCHIP_PANTAB* sndr_pan = SNDR_PAN_MONO;
	switch(stereo)
	{
	case eOptionAYStereo::AS_ABC: sndr_pan = SNDR_PAN_ABC; break;
	case eOptionAYStereo::AS_ACB: sndr_pan = SNDR_PAN_ACB; break;
	case eOptionAYStereo::AS_BAC: sndr_pan = SNDR_PAN_BAC; break;
	case eOptionAYStereo::AS_BCA: sndr_pan = SNDR_PAN_BCA; break;
	case eOptionAYStereo::AS_CAB: sndr_pan = SNDR_PAN_CAB; break;
	case eOptionAYStereo::AS_CBA: sndr_pan = SNDR_PAN_CBA; break;
	case eOptionAYStereo::AS_MONO: sndr_pan = SNDR_PAN_MONO; break;
	case eOptionAYStereo::AS_LAST: break;
	}
	ay->SetChip(chip == eOptionSoundChip::SC_AY ? eAY::CHIP_AY : eAY::CHIP_YM);
	ay->SetVolumes(0x7FFF, chip == eOptionSoundChip::SC_AY ? SNDR_VOL_AY : SNDR_VOL_YM, sndr_pan);
}

static struct eFileTypeRZX : public eFileType
{
	virtual bool Open(const char *name, const void* data, size_t data_size)
	{
		eRZX* rzx = new eRZX;
		if(rzx->Open(data, data_size, &sh) == eRZX::E_OK)
		{
			sh.Replay(rzx);
			return true;
		}
		else
		{
			sh.Replay(NULL);
			SAFE_DELETE(rzx);
		}
		return false;
	}
	virtual const char* Type() { return "rzx"; }
} ft_rzx;

static struct eFileTypeZ80 : public eFileType
{
	virtual bool Open(const char *name, const void* data, size_t data_size)
	{
		sh.OnAction(A_RESET);
		return xSnapshot::Load(sh.speccy, Type(), data, data_size);
	}
	virtual const char* Type() { return "z80"; }
} ft_z80;
static struct eFileTypeSZX : public eFileTypeZ80
{
	virtual const char* Type() { return "szx"; }
} ft_szx;
static struct eFileTypeSNA : public eFileTypeZ80
{
	virtual bool Store(const char* name)
	{
		return xSnapshot::Store(sh.speccy, name);
	}
	virtual const char* Type() { return "sna"; }
} ft_sna;

class eMacroDiskRun : public eMacro
{
	virtual bool Do()
	{
		switch(frame)
		{
		case 100:
			sh.OnKey('e', KF_DOWN|KF_UI_SENDER);
			break;
		case 102:
			sh.OnKey('e', KF_UI_SENDER);
			break;
		case 200:
			sh.OnKey('e', KF_DOWN|KF_UI_SENDER);
			break;
		case 202:
			sh.OnKey('e', KF_UI_SENDER);
			return false;
		}
		return true;
	}
};

static struct eFileTypeTRD : public eFileType
{
	virtual bool Open(const char *name, const void* data, size_t data_size)
	{
		eWD1793* wd = sh.speccy->Device<eWD1793>();
		bool ok = wd->Open(Type(), OpDrive(), data, data_size);
		if(ok && op_auto_play_image)
		{
			sh.OnAction(A_RESET);
			if(wd->BootExist(OpDrive()))
				sh.speccy->Device<eRom>()->SelectPage(eRom::ROM_DOS);
			else if(!sh.speccy->Mode48k())
			{
				sh.speccy->Device<eRom>()->SelectPage(eRom::ROM_SYS);
				sh.PlayMacro(new eMacroDiskRun);
			}
		}
		return ok;
	}
	virtual const char* Type() { return "trd"; }
} ft_trd;
static struct eFileTypeSCL : public eFileTypeTRD
{
	virtual const char* Type() { return "scl"; }
} ft_scl;
static struct eFileTypeFDI : public eFileTypeTRD
{
	virtual const char* Type() { return "fdi"; }
} ft_fdi;

class eMacroTapeLoad : public eMacro
{
	virtual bool Do()
	{
		switch(frame)
		{
		case 100:
			sh.OnKey('J', KF_DOWN|KF_UI_SENDER);
			break;
		case 102:
			sh.OnKey('J', KF_UI_SENDER);
			sh.OnKey('P', KF_DOWN|KF_ALT|KF_UI_SENDER);
			break;
		case 104:
			sh.OnKey('P', KF_UI_SENDER);
			break;
		case 110:
			sh.OnKey('P', KF_DOWN|KF_ALT|KF_UI_SENDER);
			break;
		case 112:
			sh.OnKey('P', KF_UI_SENDER);
			break;
		case 120:
			sh.OnKey('e', KF_DOWN|KF_UI_SENDER);
			break;
		case 122:
			sh.OnKey('e', KF_UI_SENDER);
			sh.OnAction(A_TAPE_TOGGLE);
			return false;
		}
		return true;
	}
};

static struct eFileTypeTAP : public eFileType
{
	virtual bool Open(const char *name, const void* data, size_t data_size)
	{
		bool ok = sh.speccy->Device<eTape>()->Open(Type(), data, data_size);
		if(ok && op_auto_play_image)
		{
			sh.OnAction(A_RESET);
			sh.speccy->Devices().Get<eRom>()->SelectPage(sh.speccy->Devices().Get<eRom>()->ROM_SOS());
			sh.PlayMacro(new eMacroTapeLoad);
		}
		return ok;
	}
	virtual const char* Type() { return "tap"; }
} ft_tap;
static struct eFileTypeCSW : public eFileTypeTAP
{
	virtual const char* Type() { return "csw"; }
} ft_csw;
static struct eFileTypeTZX : public eFileTypeTAP
{
	virtual const char* Type() { return "tzx"; }
} ft_tzx;
static struct eFileTypeZXK : public eFileType
{
	virtual bool Open(const char *name, const void* data, size_t data_size)
	{
		gameConfig gc(data, data_size);
		if(gc.filename() != "")
		{
			// The filename in the .zxk file is relative to the directory of
			// the .zxk file.
			char *directory = strdup(name);
			directory = dirname(directory);
			std::ostringstream oss;
			oss << directory << '/' << gc.filename();
			free(directory);

			//op_48k.Set(gc.mode_48k());
			// Key mappings in the .zxk file are only applied in cursor
			// joystick mode.  Switch to cursor joystick mode now.
			OpJoystick(J_CURSOR);
			Handler()->OnOpenFile(oss.str().c_str());
		}
		return true;
	}
	virtual const char* Type() { return "zxk"; }
} ft_zxk;
static struct eFileTypeAY : public eFileType
{
    class ayAccessor : public xZ80::eZ80
    {
        public:
        void Set(
                short unsigned int pc_,
                short unsigned int sp_,
                short unsigned int hireg,
                short unsigned int loreg,
                short unsigned int i_
                )
        {
            im = 0;
            pc = pc_;
            sp = sp_;
            b = d = h = a /*= xh = yh */= hireg;
            alt.b = alt.d = alt.h = alt.a = hireg;
            c = e = l = f /*= xl = yl */= r_low = loreg;
            alt.c = alt.e = alt.l = alt.f = r_low = loreg;
            i = i_;

            devices->IoWrite(0x7ffd, 0, t);
            devices->Get<eAY>()->Reset();
        }
        void SetPC(short unsigned int pc_)
        {
            pc = pc_;
        }
    };

    unsigned char read_char(const unsigned char *data, size_t offset, size_t data_size)
    {
        // Requested char is outside of data.
        if(offset + 1 >= data_size)
            throw std::runtime_error("reading char");
        return (unsigned char)data[offset];
    }

    unsigned short int read_word(const unsigned char *data, size_t offset, size_t data_size)
    {
        // Requested word is outside of data.
        if(offset + 1 >= data_size)
            throw std::runtime_error("reading word");
        return (((unsigned char)(data[offset]) << 8) & 0xff00) | (unsigned char)data[offset + 1];
    }

    size_t read_ptr(const unsigned char *data, size_t offset, size_t data_size)
    {
        //std::cerr << "read ptr " << offset << " " << data_size << "   " << (int)(data[offset]) << " " << (int)(data[offset+1]) << std::endl;
        // Requested word is outside of data.
        if(offset + 1 >= data_size)
            throw std::runtime_error("reading ptr");
        short int relative_ptr = (((unsigned char)(data[offset]) << 8) & 0xff00) | (unsigned char)data[offset + 1];
        //std::cerr << "rel " << relative_ptr << std::endl;
        // Check that the target of the pointer is within data.
        if((int)offset + relative_ptr > (int)(data_size - 2) || (int)offset + relative_ptr < 0)
            throw std::runtime_error("ptr outside of file");
        return (size_t)(offset + relative_ptr);
    }

	virtual bool Open(const char *name, const void* data_, size_t data_size)
    {
        const unsigned char *data = (const unsigned char *)data_;

        sh.OnAction(A_RESET);
        sh.speccy->Devices().Get<eRom>()->SelectPage(eMemory::P_ROM0);

        // Check that the file is a ZXAYEMUL file.
        if(memcmp(data, "ZXAYEMUL", 8) != 0)
            return false;

        //const unsigned char fileversion = read_char(data, 8, data_size);
        const unsigned char playerversion = read_char(data, 9, data_size);
        const size_t author_ptr = read_ptr(data, 12, data_size);
        std::cerr << "Author: " << (char*)(data + author_ptr) << std::endl;
        const size_t misc_ptr = read_ptr(data, 14, data_size);
        std::cerr << "Misc: " << (char*)(data + misc_ptr) << std::endl;
        const unsigned char num_songs = read_char(data, 16, data_size) + 1;
        std::cerr << "num songs " << (int)num_songs << std::endl;
        const unsigned char first_song = read_char(data, 17, data_size);
        std::cerr << "first song " << (int)first_song << std::endl;
        const size_t song_ptr = read_ptr(data, 18, data_size);

        unsigned char memory[0x10000];
        memset(memory, 0xc9, 0xff);
        memset(memory + 0x0100, 0xff, 0x3f00);
        memset(memory + 0x4000, 0, 0xc000);
        memory[0x38] = 0xfb;

        const unsigned char load_song = first_song;

        const size_t this_song_ptr = song_ptr + (load_song * 4);
        const size_t songname = read_ptr(data, this_song_ptr, data_size);
        const size_t song_data_ptr = read_ptr(data, this_song_ptr + 2, data_size);
        const unsigned short int song_length = read_word(data, song_data_ptr + 4, data_size);
        const unsigned short int fade_length = read_word(data, song_data_ptr + 6, data_size);
        const unsigned char hireg = read_char(data, song_data_ptr + 8, data_size);
        const unsigned char loreg = read_char(data, song_data_ptr + 9, data_size);
        const size_t song_points_ptr = read_ptr(data, song_data_ptr + 10, data_size);
        const size_t song_addresses_ptr = read_ptr(data, song_data_ptr + 12, data_size);

        std::cerr << "Song title: " << (char*)(data + songname) << std::endl;

        const unsigned short int stack = read_word(data, song_points_ptr, data_size);
        unsigned short int init = read_word(data, song_points_ptr + 2, data_size);
        const unsigned short int inter = read_word(data, song_points_ptr + 4, data_size);

        std::cerr << "version " << (int)playerversion << "len " << song_length << " fade " << fade_length << " hireg " << (int)hireg << " loreg " << (int)loreg << " stack " << stack << " init " << init << " inter " << inter << std::endl;

        // Player program used if 'inter' is zero.
        static unsigned char intz[] = { 0xf3, 0xcd, 0, 0, 0xed, 0x5e, 0xfb, 0x76, 0x18, 0xfa };
        // Player program used if 'inter' is not zero.
        static unsigned char intnz[] = { 0xf3, 0xcd, 0, 0, 0xed, 0x56, 0xfb, 0x76, 0xcd, 0, 0, 0x18, 0xf7 };

        for(int i_addr = 0; read_word(data, song_addresses_ptr + (i_addr * 6), data_size) != 0; ++i_addr) {
            const size_t address_block_ptr = song_addresses_ptr + (i_addr * 6);
            const unsigned short addr_z80 = read_word(data, address_block_ptr, data_size);
            if(init == 0)
                init = addr_z80;
        }

        if(inter == 0)
            memcpy(memory, intz, sizeof(intz));
        else
        {
            memcpy(memory, intnz, sizeof(intnz));
            memory[9] = (unsigned char)(inter >> 8);
            memory[10] = (unsigned char)(inter & 0xff);
        }

        memory[2] = (unsigned char)(init >> 8);
        memory[3] = (unsigned char)(init & 0xff);

        for(int i_addr = 0; read_word(data, song_addresses_ptr + (i_addr * 6), data_size) != 0; ++i_addr) {
            const size_t address_block_ptr = song_addresses_ptr + (i_addr * 6);
            const unsigned short addr_z80 = read_word(data, address_block_ptr, data_size);
            const unsigned short length = read_word(data, address_block_ptr + 2, data_size);
            const unsigned short offset = read_ptr(data, address_block_ptr + 4, data_size);
            const unsigned short addr_z80_end = addr_z80 + length;

            std::cerr << "copy " << (int)offset << " to " << (int)addr_z80 << " " << (int)addr_z80_end << " (" << (int)length << ")" << std::endl;

            memcpy(memory + addr_z80, data + offset, length);
        }

        // Overwrite the ROM from address 0.
        memcpy(sh.speccy->Memory()->Get(eMemory::P_ROM0), memory, 0x4000);
        // Overwrite the rest of the addressable memory.
        memcpy(sh.speccy->Memory()->Get(eMemory::P_RAM5), memory + 0x4000, 0x4000);
        memcpy(sh.speccy->Memory()->Get(eMemory::P_RAM2), memory + 0x8000, 0x4000);
        memcpy(sh.speccy->Memory()->Get(eMemory::P_RAM0), memory + 0xc000, 0x4000);

        memset(sh.speccy->Memory()->Get(eMemory::P_RAM1), 0x00, 0x4000);
        memset(sh.speccy->Memory()->Get(eMemory::P_RAM3), 0x00, 0x4000);
        memset(sh.speccy->Memory()->Get(eMemory::P_RAM4), 0x00, 0x4000);
        memset(sh.speccy->Memory()->Get(eMemory::P_RAM6), 0x00, 0x4000);
        memset(sh.speccy->Memory()->Get(eMemory::P_RAM7), 0x00, 0x4000);

        ((ayAccessor*)sh.speccy->CPU())->Set(0, stack, hireg, loreg, 0);

        return true;
    }
	virtual const char* Type() { return "ay"; }
} ft_ay;

}
//namespace xPlatform

// see platform-specific files for main() function
