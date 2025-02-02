#pragma once
#include <atomic>
#include <chrono>

enum SearchType{
    DEFAULT,
    PERFT,
};

struct SearchLimits{
    //defaults to "go infinite"
    SearchType search_type=SearchType::DEFAULT;
    int search_depth=100;
    int search_time_ms=INT_MAX;
};

class TimePoint{
    public:
    /**
     * @brief creates a time point for the current time
     */
    TimePoint();
    /**
     * @brief creates a time point for in the future
     * @param ms the milliseconds after now that the time point should be for
     */
    TimePoint(uint64_t ms): my_ms_since_epoch(TimePoint().my_ms_since_epoch + ms) {}

    /**
     * @brief detects whether we are past this current time point
     * @returns true if this time point is in the past
     */
    bool NowIsPastTimePoint();

    /**
     * @brief calculates how long ago this time point was
     * @warning NowIsPastTimePoint() must be true, to prevent unsigned overflow
     */
    uint64_t HowLongAgo();

    private:
    uint64_t my_ms_since_epoch;//how many ms since the unix epoch is this time point
};
