#include "Timer.h"
#include <atomic>
#include <chrono>
#include <thread>
#include <cassert>

TimePoint::TimePoint(): 
my_ms_since_epoch(
    std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch())
    .count()
) {}

bool TimePoint::NowIsPastTimePoint()
{
    return my_ms_since_epoch < TimePoint().my_ms_since_epoch;
}

uint64_t TimePoint::HowLongAgo()
{
    assert(NowIsPastTimePoint());//prevent unsigned overflow

    return TimePoint().my_ms_since_epoch - my_ms_since_epoch;
}
