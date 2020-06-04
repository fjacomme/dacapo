#pragma once

#include <string>
#include <unordered_map>

struct sample {
    int id;

    std::string name;

    enum param {
        amp,
        amp_slide,
        amp_slide_shape,
        amp_slide_curve,
        pan,
        pan_slide,
        pan_slide_shape,
        pan_slide_curve,
        attack,
        decay,
        sustain,
        release,
        attack_level,
        decay_level,
        sustain_level,
        env_curve,
        rate,
        start,
        finish,
        lpf,
        lpf_slide,
        lpf_slide_shape,
        lpf_slide_curve,
        lpf_attack,
        lpf_sustain,
        lpf_decay,
        lpf_release,
        lpf_min,
        lpf_min_slide,
        lpf_min_slide_shape,
        lpf_min_slide_curve,
        lpf_init_level,
        lpf_attack_level,
        lpf_decay_level,
        lpf_sustain_level,
        lpf_release_level,
        lpf_env_curve,
        hpf,
        hpf_slide,
        hpf_slide_shape,
        hpf_slide_curve,
        hpf_max,
        hpf_max_slide,
        hpf_max_slide_shape,
        hpf_max_slide_curve,
        hpf_attack,
        hpf_sustain,
        hpf_decay,
        hpf_release,
        hpf_init_level,
        hpf_attack_level,
        hpf_decay_level,
        hpf_sustain_level,
        hpf_release_level,
        hpf_env_curve,
        norm,
        pitch,
        pitch_slide,
        pitch_slide_shape,
        pitch_slide_curve,
        window_size,
        window_size_slide,
        window_size_slide_shape,
        window_size_slide_curve,
        pitch_dis,
        pitch_dis_slide,
        pitch_dis_slide_shape,
        pitch_dis_slide_curve,
        time_dis,
        time_dis_slide,
        time_dis_slide_shape,
        time_dis_slide_curve,
        compress,
        pre_amp,
        pre_amp_slide,
        pre_amp_slide_shape,
        pre_amp_slide_curve,
        threshold,
        threshold_slide,
        threshold_slide_shape,
        threshold_slide_curve,
        clamp_time,
        clamp_time_slide,
        clamp_time_slide_shape,
        clamp_time_slide_curve,
        slope_above,
        slope_above_slide,
        slope_above_slide_shape,
        slope_above_slide_curve,
        slope_below,
        slope_below_slide,
        slope_below_slide_shape,
        slope_below_slide_curve,
        relax_time,
        relax_time_slide,
        relax_time_slide_shape,
        relax_time_slide_curve,
        out_bus,
        _nb_params
    };

    std::unordered_map<param, float> params;

    static const char* param_name(param p);
};