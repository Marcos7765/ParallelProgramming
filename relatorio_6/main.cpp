#include <cstdio>
#include <unordered_map>
#include <string>
#include <cerrno>
#include "utils.h"
#include <cmath>
#include <omp.h>
#include <random>
enum PRINT_MODE {
    REGULAR,
    CSV
};


std::unordered_map<std::string, PRINT_MODE> print_flags = {
    {"-printCSV", CSV},
};

typedef utils::_return_data<double> return_data;

bool unitary_circle_collision(double x, double y){
    return x*x + y*y <= 1.;
}

return_data estimate_pi_serial(unsigned long total_samples, unsigned long num_threads){
    
    auto start = utils::mark_time();
    unsigned long hit_count = 0;
    double res;
    std::uniform_real_distribution<> sampler(-1.,1.);
    std::random_device rand_dev;
    std::mt19937 generator(rand_dev());

    for (unsigned long i = 0; i < total_samples; i++){
        if (unitary_circle_collision(sampler(generator), sampler(generator))){
            hit_count++;
        }
    }

    res = (4*hit_count)/static_cast<double>(total_samples);
    auto end = utils::mark_time();
    return {res, utils::calc_time_interval_ms(start, end)};
}

return_data estimate_pi_naive_parallel(unsigned long total_samples, unsigned long num_threads){
    
    auto start = utils::mark_time();
    unsigned long hit_count = 0;
    double res;
    std::uniform_real_distribution<> sampler(-1.,1.);
    std::random_device rand_dev;
    std::mt19937 generator(rand_dev());

    #pragma omp parallel for num_threads(num_threads)
    for (unsigned long i = 0; i < total_samples; i++){
        if (unitary_circle_collision(sampler(generator), sampler(generator))){
            hit_count++;
        }
    }

    res = (4*hit_count)/static_cast<double>(total_samples);
    auto end = utils::mark_time();
    return {res, utils::calc_time_interval_ms(start, end)};
}

return_data estimate_pi_parallel(unsigned long total_samples, unsigned long num_threads){
    
    auto start = utils::mark_time();
    unsigned long hit_count = 0;
    unsigned long local_hit_count = 0;
    double res;
    std::uniform_real_distribution<> sampler(-1.,1.);
    std::mt19937 generator;
    #pragma omp parallel num_threads(num_threads) default(none) shared(res, sampler, hit_count, total_samples) private(generator) firstprivate(local_hit_count)
    {
        std::random_device::result_type seed;
        #pragma omp critical
        {
            seed = std::random_device()();
        }
        generator.seed(seed);
        #pragma omp for
        for (unsigned long i = 0; i < total_samples; i++){
            if (unitary_circle_collision(sampler(generator), sampler(generator))){
                    local_hit_count++;
            }
        }
        #pragma omp critical
        {
            hit_count += local_hit_count;
        }
    }

    res = (4*hit_count)/static_cast<double>(total_samples);
    auto end = utils::mark_time();
    return {res, utils::calc_time_interval_ms(start, end)};
}


typedef return_data (*target_func_ptr)(size_t, size_t);

std::unordered_map<std::string, target_func_ptr> modes = {
    {"SERIAL", estimate_pi_serial},
    {"NAIVE_PARALLEL", estimate_pi_naive_parallel},
    {"PARALLEL", estimate_pi_parallel},
};

int main(int argc, char* argv[]){
    PRINT_MODE print_mode = REGULAR;
    if (argc < 4){
        printf("Insufficient arguments, use: %s <MODE> <NUM_SAMPLES> <NUM_THREADS> [-printCSV]\n", argv[0]);
        printf("<MODE> being either SERIAL, NAIVE_PARALLEL or PARALLEL\n");
        exit(1);
    } else{
        if (argc > 5) {
            printf("Too many arguments starting at %s. Use: %s <MODE> <NUM_SAMPLES> <NUM_THREADS> [-printCSV]\n",
                argv[5], argv[0]);
            exit(1);
        }
        if (argc == 5){
            auto temp_print = print_flags.find(argv[4]);
            if (temp_print == print_flags.end()){
                printf("Invalid argument: %s. Usage: %s <MODE> <NUM_SAMPLES> <NUM_THREADS> [-printCSV]\n",
                    argv[4], argv[0]);
                exit(1);
            }
            print_mode = temp_print->second;
        }
    }

    auto mode = modes.find(argv[1]);
    if (mode == modes.end()){
        printf("Invalid mode: %s. Choose between SERIAL, NAIVE_PARALLEL or PARALLEL\n", argv[1]);
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
        printf("%s, %lu, %lu, %f, %f\n", argv[1], params[0], params[1], exec_data.res, exec_data.time_ms);
        break;
    default:
        printf("Result for n = %lu: %f; Total elapsed time: %f ms\n", params[0], exec_data.res, exec_data.time_ms);
        break;
    }

    return 0;
}