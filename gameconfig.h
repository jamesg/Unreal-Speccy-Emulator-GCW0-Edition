#ifndef GAMECONFIG_H
#define GAMECONFIG_H

#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

#include "platform/platform.h"

class gameConfig
{
	public:
        gameConfig();
        gameConfig(const void *data, const size_t data_size);
        gameConfig(const std::string filename);

        const std::string& filename() const;
        const std::string& title() const;
        const char keymap(enum physicalKey k) const;
        const std::string& keymap_description(enum physicalKey k) const;
        bool mode_48k() const;

	private:
        void init(std::istream& infile);

        // Key map order:
        // Start sequence, up, down, left, right, first, select, 1, 2, A, B, C,
        // (Second set) up, down, left, right, first, select, 1, 2, A, B, C.
        char m_keymap[END_PHYSICAL_KEY];
        // Keymap description.
        std::string m_keymap_description[END_PHYSICAL_KEY];
        // Startup sequence.
        std::vector<char> m_startup_sequence;
        // TZX or TAP file to load.
        std::string m_filename;
        // Game title.
        std::string m_title;
        // Should the game run in 48k mode?
        bool m_48k;
};

#endif

