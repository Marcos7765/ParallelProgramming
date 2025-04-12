#include <chrono>
#include <cstdlib>

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

    double calc_time_interval_ms(std::chrono::time_point<std::chrono::steady_clock> t_start,
        std::chrono::time_point<std::chrono::steady_clock> t_end){
        return std::chrono::duration<double, std::chrono::milliseconds::period>(t_end - t_start).count();
    }

    void* _checking_malloc(size_t bytesize, int line){
        void* res = std::malloc(bytesize);
        if (res == NULL){printf("Allocation error around ln %d\n", line); exit(1);}
        return res;
    }
    
    #define checking_malloc(bytesize) _checking_malloc(bytesize, __LINE__)
}