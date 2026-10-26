#pragma once

#include <cstdint>
#include <immintrin.h>

namespace qfx {

inline void score_4_venues_avx2(
    const int64_t* asks,
    const int64_t* bids,
    const int64_t* latencies,
    const int64_t* slippages,
    const int64_t* fees,
    int64_t* out_scores
) noexcept {
    __m256i v_asks = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(asks));
    __m256i v_bids = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(bids));
    __m256i v_lats = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(latencies));
    __m256i v_slips = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(slippages));
    __m256i v_fees = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(fees));

    __m256i spread = _mm256_sub_epi64(v_asks, v_bids);
    __m256i lat_mult = _mm256_slli_epi64(v_lats, 6);
    __m256i lat_term = _mm256_add_epi64(lat_mult, _mm256_slli_epi64(v_lats, 5));
    lat_term = _mm256_add_epi64(lat_term, _mm256_slli_epi64(v_lats, 2));

    __m256i score = _mm256_add_epi64(spread, lat_term);
    score = _mm256_add_epi64(score, v_slips);
    score = _mm256_add_epi64(score, v_fees);

    _mm256_storeu_si256(reinterpret_cast<__m256i*>(out_scores), score);
}

}
