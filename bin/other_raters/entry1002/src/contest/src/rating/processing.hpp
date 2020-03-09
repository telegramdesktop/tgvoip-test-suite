#pragma once

#include <stdexcept>
#include <string>

#include "measure.hpp"
#include "series.hpp"



namespace tgvoipcontest {

void input_filter(SignalInfo& info);

void apply_filters(Signal data);

void calc_VAD(const SignalInfo& pinfo);

void pieces_locate(RatingContext& info, Signal ftmp);

void dc_block(Signal data, long size);

void apply_filter(Signal data, int, double [][2]);

double pow_of(const Signal, long, long, long);

void apply_VAD(long, const Signal data, Signal VAD, Signal logVAD);

void crude_align(RatingContext& info, long piece_id, Signal ftmp);

void time_align(RatingContext& info, long piece_id, Signal ftmp);

void split_align(RatingContext& info, Signal ftmp,
                 long piece_start, long piece_speech_start, long piece_speech_end, long piece_end,
                 long piece_delay_est, float piece_delay_conf,
                 long* Best_ED1, long* Best_D1, float* Best_DC1, long* Best_ED2,
                 long* Best_D2, float* Best_DC2, long* Best_BP);

void voip_qos_model(RatingContext& info);

}
