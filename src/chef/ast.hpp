#pragma once
#include "soundgen/soundgen.hpp"

#include <variant>
#include <vector>

struct source {
    int begin;
    int end;
};

struct comment;
struct rest;
struct affect;
struct on_beat;
struct between_measure;
struct play_sound;
struct sequence;

using statement = std::variant<comment,         //
                               rest,            //
                               affect,          //
                               on_beat,         //
                               play_sound,      //
                               between_measure, //
                               sequence         //
                               >;

struct rest {
    source _src;
};

struct comment {
    std::string text;

    source _src;
};

struct affect {
    std::string name;
    float       val;

    source _src;
};

struct on_beat {
    int                    beat;
    int                    sub_beat = 1;
    int                    nb_sub   = 1;
    std::vector<statement> statements;

    source _src;
};

struct between_measure {
    int                    m1, m2;
    std::vector<statement> statements;

    source _src;
};

struct play_sound {
    std::variant<synth, sample> sound;

    source _src;
};

struct sequence {
    int                    nb_measure;
    std::vector<statement> statements;

    source _src;

    int s_start_m = 0;
};

using ast = std::vector<statement>;

void print(ast const&, std::string&);

ast test_1();