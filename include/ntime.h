#pragma once
#include <chrono>
#include <ctime> /* time_t */

namespace ntime {
    using namespace std;
    using namespace chrono;
    using clock = chrono::system_clock;
    using ms = chrono::milliseconds;
    using sec = chrono::seconds;
    using time_point = chrono::time_point<clock>;
    using chrono::duration_cast;
    using std::chrono::duration;
    
    time_point now() {
        return clock::now();
    }
    
    template<class T = ms>
    T cast(auto du) {
        return duration_cast<T>(du);
    }
    
    time_point from_time(std::time_t time) {
        return clock::from_time_t(time);
    }
}