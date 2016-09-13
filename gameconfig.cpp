#include "gameconfig.h"

namespace
{
    char str_to_key(const std::string& phys_key)
    {
        char phys_c = 0;
        // The .zxk format permits special keycodes, specified with two
        // characters.
        if(phys_key.length() == 1)
            phys_c = phys_key.at(0);
        else if(phys_key == "EN")
            // Enter key.
            phys_c = 'e';
        else if(phys_key == "SS")
            // Symbol shift.
            phys_c = 's';
        else if(phys_key == "CS")
            // Caps shift.
            phys_c = 'c';
        else if(phys_key == "SP")
            // Space.
            phys_c = ' ';

        return phys_c;
    }
}

gameConfig::gameConfig() :
    m_48k(false)
{
}

gameConfig::gameConfig(const void *data, const size_t data_size) :
    m_48k(false)
{
    std::string s((const char*)data, data_size);
    std::stringstream infile(s);
    init(infile);
}

gameConfig::gameConfig(const std::string filename) :
    m_48k(false)
{
    std::ifstream infile(filename.c_str());
    init(infile);
}

const std::string& gameConfig::filename() const
{
    return m_filename;
}

const std::string& gameConfig::title() const
{
    return m_title;
}

const char gameConfig::keymap(enum physicalKey k) const
{
    return m_keymap[k];
}

const std::string& gameConfig::keymap_description(enum physicalKey k) const
{
    return m_keymap_description[k];
}

bool gameConfig::mode_48k() const
{
    return m_48k;
}

void gameConfig::init(std::istream& infile)
{
    std::cerr << "Parsing ZXK" << std::endl;
    std::string line;
    while(std::getline(infile, line))
    {
        // Some files have Windows line endings.
        if(line.at(line.length() - 1) == '\r')
            line.resize(line.length() - 1);

        // .zxk files allow comments starting with a #.
        if(line.at(0) == '#')
            continue;

        const std::string::size_type colon = line.find(':');
        if(colon == std::string::npos)
            continue;

        const std::string key = line.substr(0, colon);
        const std::string value = line.substr(colon + 1);

        if(key == "F")
            m_filename = value;

        if(key == "T")
            m_title = value;

        if(key == "M")
        {
            if(value == "48")
                m_48k = true;
        }

        if(key == "D")
        {
            std::string::size_type pos = 0;
            pos = value.find(';');
            std::string start_seq = value.substr(0, pos);

            for(int i = 0; i < END_PHYSICAL_KEY; ++i)
            {
                const int next = value.find(';', pos + 1);
                m_keymap_description[i] = value.substr(pos + 1, next - (pos + 1));
                pos = next;
            }
        }

        if(key == "K")
        {
            // The first item in the list is a startup sequence.
            std::string::size_type pos = 0;
            pos = value.find(';');
            std::string start_seq = value.substr(0, pos);

            for(std::string::size_type start_pos = 0; start_pos != std::string::npos;)
            {
                const std::string::size_type next =
                    start_seq.find(' ', start_pos);
                const std::string phys_key =
                    (next == std::string::npos) ? start_seq.substr(start_pos) : start_seq.substr(start_pos, next - start_pos);
                m_startup_sequence.push_back(str_to_key(phys_key));
                start_pos = (next == std::string::npos) ? next : next + 1;
            }

            for(int i = 0; i < END_PHYSICAL_KEY; ++i)
            {
                if(pos == std::string::npos)
                {
                    m_keymap[i] = 0;
                    continue;
                }

                const int next = value.find(';', pos + 1);
                std::string phys_key = value.substr(pos + 1, next - (pos + 1));
                pos = next;
                char phys_c = str_to_key(phys_key);
                m_keymap[i] = phys_c;
                xPlatform::Handler()->RemapKey((physicalKey)i, phys_c);
            }
        }
    }

    std::cerr << "Filename '" << m_filename << "'" << std::endl;
    std::cerr << "Title '" << m_title << "'" << std::endl;

    for(int i = 0; i < END_PHYSICAL_KEY; ++i)
        std::cerr << "Map " << i << " '" << m_keymap[i] << "' " << m_keymap_description[i] << std::endl;

    std::cerr << "Startup sequence:";
    for(std::vector<char>::const_iterator ic = m_startup_sequence.begin(); ic != m_startup_sequence.end(); ++ic)
        std::cerr << " '" << *ic << "'";
    std::cerr << std::endl;
}

