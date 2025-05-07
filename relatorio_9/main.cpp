#include <cstdio>
#include <unordered_map>
#include <string>
#include <cerrno>
#include "utils.h"
#include <dirent.h>
#include <omp.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <type_traits>
#include <queue>
#include <set>

enum PRINT_MODE {
    REGULAR,
    CSV
};

PRINT_MODE print_mode = REGULAR; //had to move it here as i wanted to print pretty trees

std::unordered_map<std::string, PRINT_MODE> print_flags = {
    {"-printCSV", CSV},
};

typedef utils::_return_data<unsigned long> return_data;


typedef return_data (*target_func_ptr)(unsigned long, unsigned long);

return_data simple_prime_bins_unnamed(unsigned long num_bins, unsigned long num_threads){
    
    auto bin_2 = std::priority_queue<unsigned long>();
    auto bin_3 = std::priority_queue<unsigned long>();
    
    auto start = utils::mark_time();
    #pragma omp parallel for num_threads(num_threads)
    for (auto i = 0; i < num_bins<<2; i++){
        if (i%2 == 0){
            #pragma omp critical
                bin_2.push(i);
        }
        if (i%3 == 0){
            #pragma omp critical
                bin_3.push(i);
        }
    }

    auto end = utils::mark_time();
    unsigned long res = bin_2.size() + bin_3.size();
    if (print_mode==REGULAR) {
        std::priority_queue<unsigned long> bins[2] = {bin_2, bin_3};
        unsigned long bin_nums[2] = {2, 3};
        for (auto i = 0; i < 2; i++){   
                printf("%lu bin contents: ", bin_nums[i]);
                while (!bins[i].empty()){
                    printf("%lu ", bins[i].top());
                    bins[i].pop();
                }
                printf("\n\n");
            }
    }
    return {res, utils::calc_time_interval_ms(start, end)};
}

return_data simple_prime_bins_named(unsigned long num_bins, unsigned long num_threads){
    
    auto bin_2 = std::priority_queue<unsigned long>();
    auto bin_3 = std::priority_queue<unsigned long>();
    
    auto start = utils::mark_time();
    #pragma omp parallel for num_threads(num_threads)
    for (auto i = 0; i < num_bins<<2; i++){
        if (i%2 == 0){
            #pragma omp critical(bin_2)
                bin_2.push(i);
        }
        if (i%3 == 0){
            #pragma omp critical(bin_3)
                bin_3.push(i);
        }
    }

    auto end = utils::mark_time();
    unsigned long res = bin_2.size() + bin_3.size();
    if (print_mode==REGULAR) {
        std::priority_queue<unsigned long> bins[2] = {bin_2, bin_3};
        unsigned long bin_nums[2] = {2, 3};
        for (auto i = 0; i < 2; i++){   
                printf("%lu bin contents: ", bin_nums[i]);
                while (!bins[i].empty()){
                    printf("%lu ", bins[i].top());
                    bins[i].pop();
                }
                printf("\n\n");
            }
    }

    return {res, utils::calc_time_interval_ms(start, end)};
}

template <typename T>
unsigned long insert_ordered(T* array, T element, unsigned long arr_size, unsigned long max_size){
    if (arr_size==0){array[0] = element; return 0;}
    unsigned long start = 0;
    unsigned long end = arr_size == 0 ? 0 : arr_size -1; //needed to avoid underflow
    unsigned long middle = (end+start)/2;
    while (start < end){
        if (array[middle] < element){
            start = middle+1;
        } else{
            end = middle;
        }
        middle = (end+start)/2;
    }

    if (array[middle] < element){
        middle++;
    }
    
    if (middle <= end){
        std::memmove(array + middle+1 , array+middle, (arr_size-middle-(max_size>arr_size? 0:1) )*sizeof(T));
        array[middle] = element;
    } else{
        if (max_size>arr_size){
            array[middle] = element;
        }
    }

    return middle;
}

//most of dynamic_prime_bins dev time was on this
unsigned long* first_n_primes(unsigned long n, unsigned int num_threads){
    
    unsigned long* res = static_cast<unsigned long*>(std::malloc(sizeof(unsigned long)*n));
    unsigned long count = 0;
    unsigned long index = 2;
    #pragma omp parallel num_threads(num_threads)
    {
        unsigned long* local_finds = static_cast<unsigned long*>(std::malloc(sizeof(unsigned long)*n));
        while (count < n){
            unsigned long local_count = 0;
            unsigned long bound = n-count;
            
            #pragma omp for
            for (unsigned long i = index; i < index*index; i++){
                bool is_prime = local_count < bound;
                if (is_prime){
                    for (auto j = 0; j < count; j++){
                        if (i%res[j] == 0){
                            is_prime=false;
                            break;
                        }
                    }    
                }
                if (is_prime){
                    local_finds[local_count] = i;
                    local_count++;
                }
            }

            #pragma omp barrier

            for (auto i = 0; i < local_count; i++){
                unsigned long add_index;
                #pragma omp critical
                {
                    add_index = insert_ordered(res, local_finds[i], count, n);
                    if (add_index < n){
                        count = std::min(count+1, n);
                    }
                }
                if (add_index >= n){break;}
            }
            #pragma omp barrier
            #pragma omp single
            {
                index = index*index;
            }
        }
        free(local_finds);
    }
    return res;
}

return_data dynamic_prime_bins(unsigned long num_bins, unsigned long num_threads){
    
    auto bins = new std::priority_queue<unsigned long>[num_bins];
    unsigned long* bin_nums = first_n_primes(num_bins, num_threads);
    omp_lock_t* bin_locks = (omp_lock_t*)(std::malloc(num_bins*sizeof(omp_lock_t)));
    for (auto i = 0; i < num_bins; i++){
        omp_init_lock(&bin_locks[i]);
    }

    auto start = utils::mark_time();
    #pragma omp parallel for num_threads(num_threads) schedule(guided)
    for (auto i = 2; i < num_bins<<2; i++){
        for (auto j = 0; (j < num_bins); j++){
            if (i < bin_nums[j]){break;}
            if (i%bin_nums[j] == 0){
                omp_set_lock(&bin_locks[j]);
                bins[j].push(i);
                omp_unset_lock(&bin_locks[j]);
            }
        }
    }

    auto end = utils::mark_time();
    
    unsigned long res = 0;
    for (auto i = 0; i < num_bins; i++){   
        omp_destroy_lock(&bin_locks[i]);
        res += bins[i].size();
        if (print_mode==REGULAR){
            printf("%lu bin contents: ", bin_nums[i]);
            while (!bins[i].empty()){
                printf("%lu ", bins[i].top());
                bins[i].pop();
            }
            printf("\n\n");
        }
    }

    free(bin_locks);
    free((void*) bin_nums);
    delete[] bins;
    return {res, utils::calc_time_interval_ms(start, end)};
}

std::unordered_map<std::string, target_func_ptr> modes = {
    {"SIMPLE_UNNAMED", simple_prime_bins_unnamed},
    {"SIMPLE_NAMED", simple_prime_bins_named},
    {"DYNAMIC", dynamic_prime_bins},
};

int main(int argc, char* argv[]){
    if (argc < 4){
        printf("Insufficient arguments, use: %s <MODE> <NUM_BINS> <NUM_THREADS> [-printCSV]\n", argv[0]);
        printf("<MODE> being either SIMPLE_UNNAMED, SIMPLE_NAMED, DYNAMIC\n");
        exit(1);
    } else{
        if (argc > 5) {
            printf("Too many arguments starting at %s. Use: %s <MODE> <DIR_PATH> <NUM_THREADS> [-printCSV]\n",
                argv[5], argv[0]);
            exit(1);
        }
        if (argc == 5){
            auto temp_print = print_flags.find(argv[4]);
            if (temp_print == print_flags.end()){
                printf("Invalid argument: %s. Usage: %s <MODE> <DIR_PATH> <NUM_THREADS> [-printCSV]\n",
                    argv[4], argv[0]);
                exit(1);
            }
            print_mode = temp_print->second;
        }
    }

    auto mode = modes.find(argv[1]);
    if (mode == modes.end()){
        printf("Invalid mode: %s. Choose between SERIAL, TASK_PARALLEL or TASK_PARALLEL_OPTIMIZED\n", argv[1]);
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

    switch (print_mode){
    case CSV:
        printf("%s, %lu, %lu, %lu, %f\n", argv[1], params[0], params[1], exec_data.res, exec_data.time_ms);
        break;
    default:
        printf("Result for n = %lu: %lu; Total elapsed time: %f ms\n", params[0], exec_data.res, exec_data.time_ms);
        break;
    }

    return 0;
}