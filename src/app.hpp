#pragma once
#include "chef/chef.hpp"
#include "parser/parser.hpp"
#include "soundgen/soundgen.hpp"

#include <filesystem>
#include <fstream>
#include <map>

class app {
    public:
    soundgen sg;
    chef     ch;

    struct mix {
        std::string const name;
        parser            pars;
        bool              saved = true;
        mix(std::string const& n, sound_defs const& d);
        void write_file();
        void read_file();

        private:
        mix(mix const&) = delete;
        mix& operator=(mix const&) = delete;
    };

    std::map<std::string, mix> mixes;

    bool is_running = false;

    bool auto_save = true;

    std::string current_folder;

    app();

    void new_file(std::filesystem::path const& folder, std::string const& filename);

    void set_file(std::filesystem::path const& p);

    void add_file(std::filesystem::path const& p);

    void set_folder(std::filesystem::path const& p);

    void update();

    void zero();

    void parse(std::string const& mn);

    void parse(mix& m);

    void parse_all();

    void read_all();

    void write_all();

    private:
    app(app const&) = delete;
    app& operator=(app const&) = delete;
};