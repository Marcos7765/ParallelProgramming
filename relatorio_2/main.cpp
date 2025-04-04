#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <unordered_map>
#include <string>
#include <cerrno>
#include <concepts>
#include <type_traits>
#include "utils.h"

void* _checking_malloc(size_t bytesize, int line){
    void* res = std::malloc(bytesize);
    if (res == NULL){printf("Allocation error around ln %d\n", line); exit(1);}
    return res;
}

#define checking_malloc(bytesize) _checking_malloc(bytesize, __LINE__)

template <typename T>
void print_vec(T* vec, size_t length){
    for (auto i = 0; i < length-1; i++){
        std::cout << vec[i] << ", ";
    }
    std::cout << vec[length-1] << "\n";
}

enum PRINT_MODE {
    REGULAR,
    VECTOR,
    CSV
};

typedef utils::_return_data<float> return_data;

template <typename T>
utils::_return_data<T> init_vec(T* vec_in_out, size_t vec_size){
    
    auto start = utils::mark_time();
    for (size_t i = 0; i < vec_size; i++){
        vec_in_out[i] = i+i;
        }
    auto end = utils::mark_time();
    return {static_cast<T>(0), utils::calc_time_interval_ms(start, end)};
}

template <int N, typename T> requires (N > 0)
static constexpr void unrolling_sum(T* accumulator_vec, T* vec_in, size_t offset){
    if constexpr (N > 1) {unrolling_sum<N-1, T>(accumulator_vec, vec_in, offset);}
    accumulator_vec[N-1] += vec_in[offset + N-1];
}

template <int N, typename T> requires (N > 0)
static constexpr void reduce_accs(T* accumulator_vec){
    if constexpr (N > 1) {
        accumulator_vec[N-2] += accumulator_vec[N-1];
        reduce_accs<N-1, T>(accumulator_vec);
    }
}

template <typename T, int N> requires std::is_arithmetic_v<T> && (N>=1)
utils::_return_data<T> reduce_block(T* vec_in, size_t vec_size){    
    T accumulators[N];
    for (size_t i = 0; i<N; i++){
        accumulators[i] = static_cast<T>(0);
    }
    auto start = utils::mark_time();
    size_t i = 0;
    for (;i+N-1 < vec_size; i+=N){
        unrolling_sum<N, T>(accumulators, vec_in, i);
    }
    for (;i < vec_size; i++){
        accumulators[0] += vec_in[i];
    }
    reduce_accs<N, T>(accumulators);
    auto end = utils::mark_time();
    return {accumulators[0], utils::calc_time_interval_ms(start, end)};
}

typedef return_data (*target_func_ptr)(float*, size_t);

std::unordered_map<std::string, PRINT_MODE> print_flags = {
    {"-printCSV", CSV},
    {"-printVec", VECTOR}
};

std::unordered_map<std::string, target_func_ptr> modes = {
    {"SINGLE", reduce_block<float,1>},
    {"BLOCK2",  reduce_block<float, 2>},
    {"BLOCK4",  reduce_block<float, 4>},
    {"BLOCK8",  reduce_block<float, 8>},
    {"BLOCK16", reduce_block<float, 16>},
};

int main(int argc, char* argv[]){
    PRINT_MODE print_mode = REGULAR;
    if (argc < 3){
        printf("Insufficient arguments, use: %s <MODE> <VEC_SIZE> [-printCSV | -printVec]\n", argv[0]);
        printf("<MODE> being either SINGLE or BLOCK<2 | 4 | 8 | 16>\n");
        exit(1);
    } else{
        if (argc > 4) {
            printf("Too many arguments starting at %s. Use: %s <MODE> <VEC_SIZE> [-printCSV | -printVec]\n",
                argv[4], argv[0]);
            exit(1);
        }
        if (argc == 4){
            auto temp_print = print_flags.find(argv[3]);
            if (temp_print == print_flags.end()){
                printf("Invalid argument: %s. Usage: %s <MODE> <VEC_SIZE> [-printCSV | -printVec]\n",
                    argv[3], argv[0]);
                exit(1);
            }
            print_mode = temp_print->second;
        }
    }

    auto mode = modes.find(argv[1]);
    if (mode == modes.end()){
        printf("Invalid mode: %s. Choose between SINGLE or BLOCK<2 | 4 | 8 | 16>\n", argv[1]);
        exit(1);
    }

    target_func_ptr func = mode->second;

    size_t params[1];
    for (auto i = 0; i < 1; i++){
        params[i] = std::strtoul(argv[2+i], NULL, 10);
        if (params[i] == 0 || errno == ERANGE){
            printf("Invalid parameter: %s.\n", argv[2+i]);
            exit(1);
        }
    }

    float* vec_in = (float*) checking_malloc(params[0]*sizeof(float));

    return_data init_data = init_vec(vec_in, params[0]);
    return_data exec_data = func(vec_in, params[0]);

    switch (print_mode)
    {
    case VECTOR:
        print_vec(vec_in, params[0]);
        break;
    case CSV:
        printf("%s,%lu,%f\n", "VEC_INIT", params[0], init_data.time_ms);
        printf("%s,%lu,%f\n", argv[1], params[0], exec_data.time_ms);
        break;
    default:
        printf("Result value: %f; in %f ms\n", exec_data.res, exec_data.time_ms);
        break;
    }

    free(vec_in);
    return 0;
}