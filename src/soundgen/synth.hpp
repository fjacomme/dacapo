#pragma once

#include <string>
#include <unordered_map>

struct synth {
    int id;

    std::string name;

    enum param {
        note,
        note_slide,
        note_slide_shape,
        note_slide_curve,
        amp,
        amp_slide,
        amp_slide_shape,
        amp_slide_curve,
        pan,
        pan_slide,
        pan_slide_shape,
        pan_slide_curve,
        attack,
        sustain,
        decay,
        release,
        attack_level,
        decay_level,
        sustain_level,
        env_curve,
        cutoff,
        cutoff_slide,
        cutoff_slide_shape,
        cutoff_slide_curve,
        cutoff_attack,
        cutoff_sustain,
        cutoff_decay,
        cutoff_release,
        cutoff_min,
        cutoff_min_slide,
        cutoff_min_slide_shape,
        cutoff_min_slide_curve,
        cutoff_attack_level,
        cutoff_decay_level,
        cutoff_sustain_level,
        cutoff_env_curve,
        res, // rlpf resonance
        res_slide,
        res_slide_shape,
        res_slide_curve,
        wave,        // 0=saw, 1=pulse, 2=tri
        pulse_width, // only for pulse wave
        pulse_width_slide,
        pulse_width_slide_shape,
        pulse_width_slide_curve,
        out_bus,
        _nb_params
    };

    std::unordered_map<param, float> params;

    static const char* param_name(param p);
};