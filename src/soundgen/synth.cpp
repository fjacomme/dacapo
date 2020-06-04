#include "soundgen/synth.hpp"

const char* synth::param_name(param p)
{
    switch (p) {
    case note: return "note";
    case note_slide: return "note_slide";
    case note_slide_shape: return "note_slide_shape";
    case note_slide_curve: return "note_slide_curve";
    case amp: return "amp";
    case amp_slide: return "amp_slide";
    case amp_slide_shape: return "amp_slide_shape";
    case amp_slide_curve: return "amp_slide_curve";
    case pan: return "pan";
    case pan_slide: return "pan_slide";
    case pan_slide_shape: return "pan_slide_shape";
    case pan_slide_curve: return "pan_slide_curve";
    case attack: return "attack";
    case sustain: return "sustain";
    case decay: return "decay";
    case release: return "release";
    case attack_level: return "attack_level";
    case decay_level: return "decay_level";
    case sustain_level: return "sustain_level";
    case env_curve: return "env_curve";
    case cutoff: return "cutoff";
    case cutoff_slide: return "cutoff_slide";
    case cutoff_slide_shape: return "cutoff_slide_shape";
    case cutoff_slide_curve: return "cutoff_slide_curve";
    case cutoff_attack: return "cutoff_attack";
    case cutoff_sustain: return "cutoff_sustain";
    case cutoff_decay: return "cutoff_decay";
    case cutoff_release: return "cutoff_release";
    case cutoff_min: return "cutoff_min";
    case cutoff_min_slide: return "cutoff_min_slide";
    case cutoff_min_slide_shape: return "cutoff_min_slide_shape";
    case cutoff_min_slide_curve: return "cutoff_min_slide_curve";
    case cutoff_attack_level: return "cutoff_attack_level";
    case cutoff_decay_level: return "cutoff_decay_level";
    case cutoff_sustain_level: return "cutoff_sustain_level";
    case cutoff_env_curve: return "cutoff_env_curve";
    case res: return "res";
    case res_slide: return "res_slide";
    case res_slide_shape: return "res_slide_shape";
    case res_slide_curve: return "res_slide_curve";
    case wave: return "wave";
    case pulse_width: return "pulse_width";
    case pulse_width_slide: return "pulse_width_slide";
    case pulse_width_slide_shape: return "pulse_width_slide_shape";
    case pulse_width_slide_curve: return "pulse_width_slide_curve";
    case out_bus: return "out_bus";
    case _nb_params:
    default: return nullptr;
    }
}