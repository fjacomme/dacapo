#pragma once
#include "chef/ast.hpp"
#include "soundgen/soundgen.hpp"

#include <chrono>
#include <unordered_map>

class chef {
    std::chrono::system_clock::time_point last_call;

    soundgen& sg;

    public:
    chef(soundgen& sg);

    int tempo = 90;

    int beats_per_measure = 4;

    int beat = 1;

    int sub_beat = 0;

    int sub_beats_per_beat = 48;

    int measure = 1;

    std::unordered_map<std::string, ast> asts;

    void update();

    void operator()(comment&) {}
    void operator()(rest&) {}
    void operator()(affect& i);
    void operator()(on_beat& i);
    void operator()(between_measure& i);
    void operator()(sequence& i);
    void operator()(play_sound& i);

    private:
    chef(chef const&) = delete;
    chef& operator=(chef const&) = delete;
};