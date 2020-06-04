#pragma once
#include "soundgen/sample.hpp"
#include "soundgen/synth.hpp"

#include <memory>
#include <string>

struct sound_defs {
    std::vector<std::string> synths;
    std::vector<std::string> samples;
};

class soundgen {
    public:
    sound_defs defs;

    soundgen();

    ~soundgen();

    void play(synth const& s);

    void play(sample const& s);

    private:
    struct pimpl;
    std::unique_ptr<pimpl> _p;
    soundgen(soundgen const&) = delete;
    soundgen& operator=(soundgen const&) = delete;
};