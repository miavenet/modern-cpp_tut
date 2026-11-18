// calib.cpp - apply a calibration offset to sensor readings, stopping at the first fault or converting all.
// Compile from the tutorial root; output goes to the terminal or your temp directory:
//   bash modules/05-memory-perf/lab/practice/calib_report.sh clang++
// Apply a calibration offset to sensor readings. A reading equal to SENTINEL
// means "sensor fault": the caller wants the index of the first fault.
#include <cstddef>
#include <cstdint>

constexpr std::int32_t SENTINEL = INT32_MIN;

// A: stop at the first fault. Readings after it are left untouched.
std::size_t calib_stop(const std::int32_t* in, std::int32_t* out, std::size_t n, std::int32_t off) {
    for (std::size_t i = 0; i < n; ++i) {
        if (in[i] == SENTINEL) return i;
        out[i] = in[i] + off;
    }
    return n;
}

// B: convert everything, remember whether any fault was seen, locate it afterwards.
std::size_t calib_all(const std::int32_t* in, std::int32_t* out, std::size_t n, std::int32_t off) {
    std::uint32_t fault = 0;
    for (std::size_t i = 0; i < n; ++i) {
        fault |= static_cast<std::uint32_t>(in[i] == SENTINEL);
        out[i] = static_cast<std::int32_t>(static_cast<std::uint32_t>(in[i]) + static_cast<std::uint32_t>(off));
    }
    if (fault == 0) return n;
    std::size_t i = 0;
    while (in[i] != SENTINEL) ++i;
    return i;
}

// C: search only, no store.
std::size_t first_fault(const std::int32_t* in, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i)
        if (in[i] == SENTINEL) return i;
    return n;
}
