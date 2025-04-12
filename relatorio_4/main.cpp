#include <cstdio>
#include <unordered_map>
#include <string>
#include <cerrno>
#include "utils.h"
#include <cmath>
#include <omp.h>

#ifndef HELLSIZE
    //cant go beyond 900 without extending template-depth
    //(you can either extend it or try to rewrite it with a fold expression)
    #define HELLSIZE 800
#endif

enum PRINT_MODE {
    REGULAR,
    CSV
};


std::unordered_map<std::string, PRINT_MODE> print_flags = {
    {"-printCSV", CSV},
};

typedef utils::_return_data<long double> return_data;

return_data memory_bound_serial(double* vec1, double* vec2, size_t n, size_t num_threads){
    long double res = 0;
    auto start = utils::mark_time();
    for (size_t i = 0; i < n; i++){
        res += vec1[i]+vec2[n-i-1];
    }
    auto end = utils::mark_time();
    return {res, utils::calc_time_interval_ms(start, end)};
}

return_data memory_bound_threaded(double* vec1, double* vec2, size_t n, size_t num_threads){
    long double res = 0;
    auto start = utils::mark_time();
    #pragma omp parallel for reduction(+: res) num_threads(num_threads)
    for (size_t i = 0; i < n; i++){
        res += vec1[i]+vec2[n-i-1];
    }
    auto end = utils::mark_time();
    return {res, utils::calc_time_interval_ms(start, end)};
}

template <auto func_ptr>
return_data wrap_memory_bound(size_t n, size_t num_threads){
    double* vec1 = (double*) utils::checking_malloc(sizeof(double)*n);
    double* vec2 = (double*) utils::checking_malloc(sizeof(double)*n);
    for (size_t i = 0; i < n; i++){
        vec1[i] = i;
        vec2[i] = (n-i-1)*2;
    }
    return func_ptr(vec1, vec2, n, num_threads);
}

template <size_t N>
static constexpr long double tanhell(long double x){
    return tanhell<N-1>(std::tanh(x));
}

template <>
inline constexpr long double tanhell<0>(long double x){
    return std::tanh(x);
}

return_data compute_bound_serial(size_t n, size_t){
    long double res = 0;
    auto start = utils::mark_time();
    for (size_t i = 0; i < n; i++){
        res += tanhell<HELLSIZE>(static_cast<long double>(i));
    }
    auto end = utils::mark_time();
    return {res, utils::calc_time_interval_ms(start, end)};
}

return_data compute_bound_threaded(size_t n, size_t num_threads){
    long double res = 0;
    auto start = utils::mark_time();
    #pragma omp parallel for reduction(+: res) num_threads(num_threads)
        for (size_t i = 0; i < n; i++){
            res += tanhell<HELLSIZE>(static_cast<long double>(i));
        }
    auto end = utils::mark_time();
    return {res, utils::calc_time_interval_ms(start, end)};
}

typedef return_data (*target_func_ptr)(size_t, size_t);

std::unordered_map<std::string, target_func_ptr> modes = {
    {"COMP_BOUND_SERIAL", compute_bound_serial},
    {"COMP_BOUND_THREAD", compute_bound_threaded},
    {"MEMO_BOUND_SERIAL", wrap_memory_bound<memory_bound_serial>},
    {"MEMO_BOUND_THREAD", wrap_memory_bound<memory_bound_threaded>},
};

int main(int argc, char* argv[]){
    PRINT_MODE print_mode = REGULAR;
    if (argc < 4){
        printf("Insufficient arguments, use: %s <MODE> <PROBLEM_SIZE> <NUM_THREADS> [-printCSV]\n", argv[0]);
        printf("<MODE> being either COMP_BOUND_SERIAL, COMP_BOUND_THREAD, MEMO_BOUND_SERIAL or MEMO_BOUND_THREAD\n");
        exit(1);
    } else{
        if (argc > 5) {
            printf("Too many arguments starting at %s. Use: %s <MODE> <PROBLEM_SIZE> <NUM_THREADS> [-printCSV]\n",
                argv[5], argv[0]);
            exit(1);
        }
        if (argc == 5){
            auto temp_print = print_flags.find(argv[4]);
            if (temp_print == print_flags.end()){
                printf("Invalid argument: %s. Usage: %s <MODE> <PROBLEM_SIZE> <NUM_THREADS> [-printCSV]\n",
                    argv[4], argv[0]);
                exit(1);
            }
            print_mode = temp_print->second;
        }
    }

    auto mode = modes.find(argv[1]);
    if (mode == modes.end()){
        printf("Invalid mode: %s. Choose between COMP_BOUND_SERIAL, COMP_BOUND_THREAD, MEMO_BOUND_SERIAL or MEMO_BOUND_THREAD\n", argv[1]);
        exit(1);
    }

    target_func_ptr func = mode->second;

    size_t params[2];
    for (auto i = 0; i < 2; i++){
        params[i] = std::strtoul(argv[2+i], NULL, 10);
        if (params[i] == 0 || errno == ERANGE){
            printf("Invalid parameter: %s.\n", argv[2+i]);
            exit(1);
        }
    }

    return_data exec_data = mode->second(params[0], params[1]);

    switch (print_mode)
    {
    case CSV:
        printf("%s,%lu, %lu, %f\n", argv[1], params[0], params[1], exec_data.time_ms);
        break;
    default:
        printf("Result for n = %lu: %Lf; Total elapsed time: %f ms\n", params[0], exec_data.res, exec_data.time_ms);
        break;
    }

    return 0;
}