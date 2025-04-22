#include <cstdio>
#include <unordered_map>
#include <string>
#include <cerrno>
#include "utils.h"
#include <cmath>
#include <omp.h>

//today is a nice day to throw off size_t and go back to use unsigned long. who knows, maybe ill go to uint64_t again

/*
    the idea is to make a Sieve of Erastosthenes operating in a odd number array to save memory. as such, we could map
    the array indexes to the natural numbers with a function N:N i -> 2*i +1, but in such the index 0 would map to 1,
    which we usually already skip by starting at 2 (that we also skip as we are searching only on odd numbers),
    so we would either need to waste a byte with the first position of the array or we would need to shift it by -1 on
    every access. To prevent it, we just shift the whole function by 1, making it N:N i - > 2*(i+1) +1 = 2*i +3.
    With it we get rid of the subtraction in every memory access and only change a constant value that would be summed
    up on the loop condition.
*/

/*
    Given a odd number j, it's index in the odd_integer_field is (j-1)/2 -1; so it's square, which is also odd, is
    (j^2 -1)/2 -1. subtracting j^2 index by j gives us it's offset in the odd_integer_field, which equals j(j-1)/2.
    substituting j by it's index based formula, j = 2*(i+1) + 1, we have that the offset between a odd number at index i
    and it's square is 2(i^2) + 5i +3. In a similar fashion, given a odd number X and its next odd multiple 3X, the
    index offset is X which, in function of X's index Y, is 2*Y+3
*/

//alternatively, you could try parallelizing the following (sketch on regular index but could be done on odd-only just
//fine):
/*
    //you'll prolly need to make separate arrays for counter and targets to vectorization
    struct {unsigned long counter, unsigned long prime_target} array[N/log(N)];
    array[0] = {1, 2};
    unsigned long prime_count = 1;
    for (unsigned long i = 2; i <= N; i++){
        bool is_prime = true;
        for (unsigned long j = 0; j < prime_count; j++){
            array[j].count++;
            if (array[j].count == array[j].target){
                is_prime = false;
                array[j].count == 1;
                break;
            }
        }
        if (is_prime){
            array[prime_count] = {1, i};
            prime_count++;
        }
    }
*/
//the same task vs parallel for comparison would happen here, but you would have trouble sharing the counters.
//the early return and vectorization together on the loop over counters also don't match well (though i guess they would
//be better than each of them individually for big enough numbers)

//making my own queue 'cause std::queue's memory management was giving me a hard time
//template <typename T>
template <typename T>
struct minimal_queue {
    T* front = nullptr;
    T* end = nullptr;
    T* push(T value);
    T pop();
    bool empty() const {return size == 0;};
    unsigned long size = 0;
    unsigned long start_offset = 0;
    const unsigned long max_elements;
    T* const buffer;
    minimal_queue(unsigned long max_elements) : max_elements(max_elements), buffer(new T[max_elements]){}
    ~minimal_queue(){delete[] buffer;}
};

//leaving push() to a full queue as UB, check size first
template <typename T>
T* minimal_queue<T>::push(T value){
    T* const ptr = this->buffer + ((this->start_offset + this->size)%this->max_elements);
    *ptr = value;
    if (this->empty()){
        this->front = ptr;
    }
    this->end = ptr;
    this->size++;
    return ptr;
}

//leaving a pop() to an empty queue as UB, check empty() first
template <typename T>
T minimal_queue<T>::pop(){
    T value = *(this->front);
    if (size == 1){
        this->front = nullptr;
        this->end = nullptr;
    } else{
        this->front = this->buffer + ((this->start_offset+1)%this->max_elements);
    }
    this->size--;
    this->start_offset = (this->start_offset+1)%this->max_elements;
    return value;
}

enum PRINT_MODE {
    REGULAR,
    CSV
};


std::unordered_map<std::string, PRINT_MODE> print_flags = {
    {"-printCSV", CSV},
};

typedef utils::_return_data<unsigned long> return_data;

inline unsigned long i_val_square_index(unsigned long i){
    return 2*i*i + 6*i + 3;
}
inline unsigned long i_odd_multiples_incr(unsigned long i){
    return 2*i +3;
}

typedef struct task_info_struct {
    unsigned long value;
    bool done;
} task_info;

typedef struct minimal_queue<task_info> task_queue;

inline void mark_multiples(unsigned long base_index, unsigned long N, bool* odd_integer_field){

    unsigned long start = i_val_square_index(base_index); //base_index + square offset
    unsigned long incr = i_odd_multiples_incr(base_index);
    unsigned long limit = (N-3)/2;
    for (unsigned long i = start; i <= limit; i+= incr){
        odd_integer_field[i] = true;
    }
}

//num_threads kept here just to keep a uniform interface with the parallel functions
return_data count_primes_serial(unsigned long N, unsigned long num_threads){

    bool* odd_integer_field = (bool*)calloc(N/2 +1, sizeof(bool));
    unsigned long prime_count = (N >= 2) ? 1 : 0;
    unsigned long N_in_odd_index = (N-3)/2;
    auto start = utils::mark_time();

    for (unsigned long i = 0; i <= N_in_odd_index; i++){
        if (!odd_integer_field[i]){
            mark_multiples(i, N, odd_integer_field);
            prime_count++;
        }
    }

    auto end = utils::mark_time();
    free(odd_integer_field);
    return {prime_count, utils::calc_time_interval_ms(start, end)};
}

return_data count_primes_naive_parallel(unsigned long N, unsigned long num_threads){

    bool* odd_integer_field = (bool*)calloc(N/2 +1, sizeof(bool));
    unsigned long prime_count = (N >= 2) ? 1 : 0;
    unsigned long N_in_odd_index = (N-3)/2;
    auto start = utils::mark_time();

    #pragma omp parallel for num_threads(num_threads)
    for (unsigned long i = 0; i <= N_in_odd_index; i++){
        if (!odd_integer_field[i]){
            mark_multiples(i, N, odd_integer_field);
            prime_count++;
        }
    }

    auto end = utils::mark_time();
    free(odd_integer_field);
    return {prime_count, utils::calc_time_interval_ms(start, end)};
}

return_data count_primes_task(unsigned long N, unsigned long num_threads){

    bool* odd_integer_field = (bool*)calloc(N/2 +1, sizeof(bool));
    unsigned long prime_count = (N >= 2) ? 1 : 0;
    task_queue pending_values = {(unsigned long) (N/std::log(N))};
    unsigned long N_in_odd_index = (N-3)/2;
    //omp seems to have no fifo priority for tasks so
    unsigned long prio = omp_get_max_task_priority();
    auto start = utils::mark_time();

    #pragma omp parallel num_threads(num_threads)
    {
        #pragma omp single
        {
            unsigned long i = 0;
            unsigned long last_index = 1; //just putting a different value
            unsigned long progress_threshold = i_val_square_index(i)-1;
            while (i <= N_in_odd_index){
                if (i != last_index){
                    if (!odd_integer_field[i]){
                        task_info* pending_task = pending_values.push({i, false});
                        #pragma omp task firstprivate(pending_task) priority(prio)
                        {
                            mark_multiples(pending_task->value, N, odd_integer_field);
                            pending_task->done = true;
                        }
                        prio = (prio-1)==0 ? 1 : prio-1;
                        prime_count++;
                    }
                }
                last_index = i;
                
                if (!pending_values.empty()){
                    if (pending_values.front->done){
                        progress_threshold = i_val_square_index(pending_values.pop().value)-1;
                    }
                }
                i = i+1 < progress_threshold ? i+1 : progress_threshold;
            }
        }
    }
    auto end = utils::mark_time();
    free(odd_integer_field);
    return {prime_count, utils::calc_time_interval_ms(start, end)};
}

return_data count_primes_for_dynamic(unsigned long N, unsigned long num_threads){

    bool* odd_integer_field = (bool*)calloc(N/2 +1, sizeof(bool));
    unsigned long prime_count = (N >= 2) ? 1 : 0;
    unsigned long N_in_odd_index = (N-3)/2;
    auto start = utils::mark_time();

    #pragma omp parallel num_threads(num_threads) reduction(+:prime_count)
    {
        prime_count = 0;
        for (unsigned long i = 0; i <= N_in_odd_index;){
            unsigned long progress_threshold = i_val_square_index(i)-1;
            progress_threshold = progress_threshold > N_in_odd_index ? N_in_odd_index : progress_threshold;
            unsigned long partition_size = (progress_threshold-i)/(12*num_threads);
            partition_size = partition_size == 0 ? 1 : partition_size;
            #pragma omp for schedule(dynamic,partition_size)
            for (unsigned long j = i; j <= progress_threshold; j++){
                if (!odd_integer_field[j]){
                    mark_multiples(j, N, odd_integer_field);
                    prime_count++;
                }
            }
            i = progress_threshold + 1;
        }
    }

    auto end = utils::mark_time();
    free(odd_integer_field);
    return {prime_count, utils::calc_time_interval_ms(start, end)};
}

return_data count_primes_for_static(unsigned long N, unsigned long num_threads){

    bool* odd_integer_field = (bool*)calloc(N/2 +1, sizeof(bool));
    unsigned long prime_count = (N >= 2) ? 1 : 0;
    unsigned long N_in_odd_index = (N-3)/2;
    auto start = utils::mark_time();

    #pragma omp parallel num_threads(num_threads) reduction(+:prime_count)
    {
        prime_count = 0;
        for (unsigned long i = 0; i <= N_in_odd_index;){
            unsigned long progress_threshold = i_val_square_index(i)-1;
            progress_threshold = progress_threshold > N_in_odd_index ? N_in_odd_index : progress_threshold;
            unsigned long partition_size = (progress_threshold-i)/(12*num_threads);
            partition_size = partition_size == 0 ? 1 : partition_size;
            #pragma omp for schedule(static,partition_size)
            for (unsigned long j = i; j <= progress_threshold; j++){
                if (!odd_integer_field[j]){
                    mark_multiples(j, N, odd_integer_field);
                    prime_count++;
                }
            }
            i = progress_threshold + 1;
        }
    }

    auto end = utils::mark_time();
    free(odd_integer_field);
    return {prime_count, utils::calc_time_interval_ms(start, end)};
}

return_data count_primes_for_guided(unsigned long N, unsigned long num_threads){

    bool* odd_integer_field = (bool*)calloc(N/2 +1, sizeof(bool));
    unsigned long prime_count = (N >= 2) ? 1 : 0;
    unsigned long N_in_odd_index = (N-3)/2;
    auto start = utils::mark_time();

    #pragma omp parallel num_threads(num_threads) reduction(+:prime_count)
    {
        prime_count = 0;
        for (unsigned long i = 0; i <= N_in_odd_index;){
            unsigned long progress_threshold = i_val_square_index(i)-1;
            progress_threshold = progress_threshold > N_in_odd_index ? N_in_odd_index : progress_threshold;
            unsigned long partition_size = (progress_threshold-i)/(12*num_threads);
            partition_size = partition_size == 0 ? 1 : partition_size;
            #pragma omp for schedule(guided)
            for (unsigned long j = i; j <= progress_threshold; j++){
                if (!odd_integer_field[j]){
                    mark_multiples(j, N, odd_integer_field);
                    prime_count++;
                }
            }
            i = progress_threshold + 1;
        }
    }

    auto end = utils::mark_time();
    free(odd_integer_field);
    return {prime_count, utils::calc_time_interval_ms(start, end)};
}

return_data count_primes_for_auto(unsigned long N, unsigned long num_threads){

    bool* odd_integer_field = (bool*)calloc(N/2 +1, sizeof(bool));
    unsigned long prime_count = (N >= 2) ? 1 : 0;
    unsigned long N_in_odd_index = (N-3)/2;
    auto start = utils::mark_time();

    #pragma omp parallel num_threads(num_threads) reduction(+:prime_count)
    {
        prime_count = 0;
        for (unsigned long i = 0; i <= N_in_odd_index;){
            unsigned long progress_threshold = i_val_square_index(i)-1;
            progress_threshold = progress_threshold > N_in_odd_index ? N_in_odd_index : progress_threshold;
            unsigned long partition_size = (progress_threshold-i)/(12*num_threads);
            partition_size = partition_size == 0 ? 1 : partition_size;
            #pragma omp for
            //#pragma omp for
            for (unsigned long j = i; j <= progress_threshold; j++){
                if (!odd_integer_field[j]){
                    mark_multiples(j, N, odd_integer_field);
                    prime_count++;
                }
            }
            i = progress_threshold + 1;
        }
    }

    auto end = utils::mark_time();
    free(odd_integer_field);
    return {prime_count, utils::calc_time_interval_ms(start, end)};
}
typedef return_data (*target_func_ptr)(unsigned long, unsigned long);

//those extra modes were used very briefly to decide which schedule to use, might be present on the post if i've got
//time remaining
std::unordered_map<std::string, target_func_ptr> modes = {
    {"SERIAL", count_primes_serial},
    {"NAIVE_PARALLEL", count_primes_naive_parallel},
    {"TASK_PARALLEL", count_primes_task},
    {"LOOP_PARALLEL_STATIC", count_primes_for_static},
    {"LOOP_PARALLEL_DYNAMIC", count_primes_for_dynamic},
    {"LOOP_PARALLEL_GUIDED", count_primes_for_guided},
    {"LOOP_PARALLEL_AUTO", count_primes_for_auto},
};

int main(int argc, char* argv[]){
    PRINT_MODE print_mode = REGULAR;
    if (argc < 4){
        printf("Insufficient arguments, use: %s <MODE> <MAX_NUMBER> <NUM_THREADS> [-printCSV]\n", argv[0]);
        printf("<MODE> being SERIAL, NAIVE_PARALLEL, TASK_PARALLEL, LOOP_PARALLEL_STATIC, LOOP_PARALLEL_DYNAMIC, LOOP_PARALLEL_GUIDED, LOOP_PARALLEL_AUTO\n");
        exit(1);
    } else{
        if (argc > 5) {
            printf("Too many arguments starting at %s. Use: %s <MODE> <MAX_NUMBER> <NUM_THREADS> [-printCSV]\n",
                argv[5], argv[0]);
            exit(1);
        }
        if (argc == 5){
            auto temp_print = print_flags.find(argv[4]);
            if (temp_print == print_flags.end()){
                printf("Invalid argument: %s. Usage: %s <MODE> <MAX_NUMBER> <NUM_THREADS> [-printCSV]\n",
                    argv[4], argv[0]);
                exit(1);
            }
            print_mode = temp_print->second;
        }
    }

    auto mode = modes.find(argv[1]);
    if (mode == modes.end()){
        printf("Invalid mode: %s. Choose between SERIAL, NAIVE_PARALLEL, TASK_PARALLEL, LOOP_PARALLEL_STATIC, LOOP_PARALLEL_DYNAMIC, LOOP_PARALLEL_GUIDED, LOOP_PARALLEL_AUTO\n", argv[1]);
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
        printf("%s,%lu,%lu,%lu,%f\n", argv[1], params[0], params[1], exec_data.res, exec_data.time_ms);
        break;
    default:
        printf("Result for n = %lu: %lu; Total elapsed time: %f ms\n", params[0], exec_data.res, exec_data.time_ms);
        break;
    }

    return 0;
}