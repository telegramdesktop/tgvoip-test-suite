#pragma once

#include <stdexcept>
#include <string>

#include "magic.hpp"
#include "series.hpp"

namespace tgvoipcontest {

struct SignalInfo {
    long n_samples = 0;

    Signal data;
    Signal VAD;
    Signal logVAD;
};

struct RatingContext {
    SignalInfo src;
    SignalInfo rec;

    long n_pieces;

    long crude_delay;
    long piece_search_start[magic::MAX_PIECES];
    long piece_search_end[magic::MAX_PIECES];
    long piece_delay_est[magic::MAX_PIECES];
    long piece_delay[magic::MAX_PIECES];
    float piece_delay_confidence[magic::MAX_PIECES];
    long piece_start[magic::MAX_PIECES];
    long piece_end[magic::MAX_PIECES];

    float rate;
};


class RatingModelException : public std::runtime_error {
public:
    explicit RatingModelException(const std::string& arg);
};

void measure_rate(RatingContext& ctx);

}