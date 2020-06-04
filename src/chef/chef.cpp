#include "chef/chef.hpp"

#include <iostream>

chef::chef(soundgen& sg)
    : sg(sg)
{
    last_call = std::chrono::system_clock::now();
}

void chef::update()
{
    auto          now = std::chrono::system_clock::now();
    int64_t const elapsed_us
        = std::chrono::duration_cast<std::chrono::microseconds>(now - last_call).count();
    int64_t const dt_us = int64_t((60.0 / tempo) * 1000000) / sub_beats_per_beat;
    if (elapsed_us < dt_us) {
        return;
    }
    sub_beat++;
    if (sub_beat > sub_beats_per_beat) {
        sub_beat = 1;
        beat++;
        if (beat > beats_per_measure) {
            beat = 1;
            measure++;
        }
        std::cout << measure << ":" << beat << "/" << beats_per_measure
                  << " elapsed: " << elapsed_us / 1000 << "ms" << std::endl;
    }

    for (auto& a : asts) {
        for (auto& st : a.second) {
            std::visit(*this, st);
        }
    }
    last_call = std::chrono::system_clock::now();
}

void chef::operator()(affect& i)
{
    if (i.name == "tempo") {
        tempo = int(i.val);
    }
    else if (i.name == "measure") {
        measure = int(i.val);
    }
}

void chef::operator()(on_beat& i)
{
    int const sub_on = (i.sub_beat - 1) * sub_beats_per_beat / i.nb_sub;
    int const cur_on = sub_beat - 1;
    if (i.beat == beat && sub_on == cur_on) {
        for (auto& st : i.statements) {
            std::visit(*this, st);
        }
    }
}

void chef::operator()(between_measure& i)
{
    if (i.m1 <= measure && (i.m2 < 0 || i.m2 >= measure)) {
        for (auto& st : i.statements) {
            std::visit(*this, st);
        }
    }
}

void chef::operator()(sequence& i)
{
    auto const prev_beat = beat;
    auto const prev_bpm  = beats_per_measure;
    if (i.s_start_m == 0 || measure - i.s_start_m >= i.nb_measure) {
        i.s_start_m = measure;
    }
    beat              = beat + (prev_bpm * (measure - i.s_start_m));
    beats_per_measure = i.nb_measure * beats_per_measure;
    for (auto& st : i.statements) {
        std::visit(*this, st);
    }
    beat              = prev_beat;
    beats_per_measure = prev_bpm;
}

void chef::operator()(play_sound& i)
{
    std::cout << beat << " " << sub_beat << "/" << sub_beats_per_beat << " ";
    std::visit(
        [this](auto& snd) {
            std::cout << snd.name << std::endl;
            sg.play(snd);
        },
        i.sound);
}