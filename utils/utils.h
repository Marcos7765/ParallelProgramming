#include <chrono>

namespace utils{
    std::chrono::steady_clock global_clock;

    template <typename T>
    struct _return_data {
        T res;
        double time_ms;
    };

    auto mark_time(){
        return global_clock.now();
    }

    double calc_time_interval(std::chrono::time_point<std::chrono::steady_clock> t_start,
        std::chrono::time_point<std::chrono::steady_clock> t_end){
        return std::chrono::duration<double, std::chrono::milliseconds::period>(t_end - t_start).count();
    }
}