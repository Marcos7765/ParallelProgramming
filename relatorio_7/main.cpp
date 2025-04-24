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

//config used for timing test:
const unsigned long DEFAULT_FILE_OPERATION_OVERHEAD = 1;
const unsigned long DEFAULT_STARTING_QUEUE_SIZE = 10;
const unsigned long DEFAULT_FILE_BLOCK_SIZE = 5;

const bool alternative_cost_mode = false;
const unsigned long DEFAULT_FILE_COST = 1;
const unsigned long DEFAULT_FOLDER_COST = 0;

//config used for cost measurement
//const unsigned long DEFAULT_FILE_OPERATION_OVERHEAD = 0;
//const unsigned long DEFAULT_STARTING_QUEUE_SIZE = 10;
//const unsigned long DEFAULT_FILE_BLOCK_SIZE = 5;
//
//const bool alternative_cost_mode = false;
//const unsigned long DEFAULT_FILE_COST = 1;
//const unsigned long DEFAULT_FOLDER_COST = 0;

enum PRINT_MODE {
    REGULAR,
    CSV
};

PRINT_MODE print_mode = REGULAR; //had to move it here as i wanted to print pretty trees

std::unordered_map<std::string, PRINT_MODE> print_flags = {
    {"-printCSV", CSV},
};

unsigned long file_count = 0;
#pragma omp threadprivate(file_count)

unsigned long alt_cost = 0;
#pragma omp threadprivate(alt_cost)

typedef utils::_return_data<unsigned long> return_data;

//oh dear, look! it's the minimal_queue from 2 tasks ago!
template <typename T>
struct minimal_queue {
    T* front = nullptr;
    T* end = nullptr;
    T* push(T value);
    T* push_front(T value);
    T pop();
    long popN(unsigned long N, T* out_ptr);
    bool empty() const {return size == 0;};
    unsigned long size = 0;
    unsigned long start_offset = 0;
    unsigned long max_elements;
    T* buffer;
    void realloc_buffer(unsigned long new_max);
    minimal_queue(unsigned long max_elements) : max_elements(max_elements), buffer(new T[max_elements]){}
    ~minimal_queue(){delete[] buffer;}
};

template <typename T>
void minimal_queue<T>::realloc_buffer(unsigned long new_max){
    T* new_buf = new T[new_max];
    long before_wrap = max_elements - start_offset;
    before_wrap = before_wrap > size ? size : before_wrap; //tmp as min(size, tmp)
    long after_wrap = size > before_wrap ? size - before_wrap : 0; //complement as how many elements wrapped around the buffer
    
    if constexpr (std::is_trivially_copyable<T>::value){
        std::memcpy(new_buf, this->buffer + (this->start_offset), before_wrap*sizeof(T));
        std::memcpy(new_buf + before_wrap, this->buffer, after_wrap*sizeof(T));
        printf("memcpy de novo??\n");
    } else {
        for (auto i = 0; i < before_wrap; i++){
            new_buf[i] = std::move(buffer[start_offset + i]);
        }
        for (auto i = 0; i < after_wrap; i++){
            new_buf[before_wrap+i] = std::move(buffer[i]);
        }
    }
    delete[] this->buffer;
    this->buffer = new_buf;
    this->max_elements=new_max;
}

//leaving push() to a full queue as UB, check size first
template <typename T>
T* minimal_queue<T>::push(T value){
    if (this->size == this->max_elements){
        this->realloc_buffer(this->max_elements*2);
    }
    T* const ptr = this->buffer + ((this->start_offset + this->size)%this->max_elements);
    *ptr = value;
    if (this->empty()){
        this->front = ptr;
    }
    this->end = ptr;
    this->size++;
    return ptr;
}

//leaving push() to a full queue as UB, check size first
template <typename T>
T* minimal_queue<T>::push_front(T value){
    if (this->size == this->max_elements){
        this->realloc_buffer(this->max_elements*2);
    }
    this->start_offset = (this->start_offset-1)%this->max_elements;
    T* const ptr = this->buffer + this->start_offset;
    *ptr = value;
    if (this->empty()){
        this->end = ptr;
    }
    this->front = ptr;
    this->size++;
    return ptr;
}

template <typename T>
long minimal_queue<T>::popN(unsigned long N, T* dest_ptr){
    const long res = this->size > N ? N : this->size;
    const long before_wrap = (max_elements - start_offset) > res ? res : (max_elements - start_offset);
    const long after_wrap = res > before_wrap ? res - before_wrap : 0;

    if constexpr (std::is_trivially_copyable<T>::value){
        std::memcpy(dest_ptr, this->buffer + (this->start_offset), before_wrap*sizeof(T));
        std::memcpy(dest_ptr + before_wrap, this->buffer, after_wrap*sizeof(T));
    } else {
        for (long i = 0; i < before_wrap; i++){
            dest_ptr[i] = std::move(this->buffer[start_offset + i]);
        }
        for (long i = 0; i < after_wrap; i++){
            dest_ptr[before_wrap+i] = std::move(this->buffer[i]);
        }
    }
    
    this->size -= res;
    this->start_offset = (this->start_offset+res)%this->max_elements;
    
    if (this->size == 0){
        this->front = nullptr;
        this->end = nullptr;
    } else {
        this->front = this->buffer + this->start_offset;
    }
    return res;
}

//leaving a pop() to an empty queue as UB, check empty() first
template <typename T>
T minimal_queue<T>::pop(){
    T value = *(this->front);
    this->start_offset = (this->start_offset+1)%this->max_elements;
    if (size == 1){
        this->front = nullptr;
        this->end = nullptr;
    } else{
        this->front = this->buffer + this->start_offset;
    }
    this->size--;
    return value;
}

void dummy_file_operation_overhead(unsigned int seconds){
    sleep(seconds);
}

//as POSIX.1 struct dirent has no file type identifier, i'll be using its opendir errno to work around.
//by definition errno should be thread safe. let's hope it is
DIR* handled_opendir(const char* dirname){
    DIR* res = opendir(dirname);
    if (res == NULL){ //using NULL instead of nullptr because dirent is from libc? no good reason actually but w/e
        switch (errno)
        {
        case EACCES:
            printf("Permission denied for %s\n", dirname);
            break;
        case EMFILE:
        case ENFILE:
            printf("Limit exceeded :( failed at %s\n", dirname);
            break;
        case ENAMETOOLONG:
            printf("%s exceeds the path's name limit!?\n", dirname);
            break;
        case ENOENT:
            printf("There is no existing %s file or similar\n", dirname);
            break;
        case ENOTDIR:    
            //resetting it here so i won't need to in the readdir() loop
            errno = 0;
            return res;
            break;
        default:
            printf("%s seems to really mess around\n", dirname);
            break;
        }
        exit(1);
    }
    return res;
}

unsigned long tree_recursive_serial(DIR* dir_ptr, std::string prefix="", unsigned int level = 0,
    unsigned int file_delay=DEFAULT_FILE_OPERATION_OVERHEAD);

unsigned long tree_recursive_task(DIR* dir_ptr, std::string prefix="", unsigned int level = 0,
    unsigned int file_delay=DEFAULT_FILE_OPERATION_OVERHEAD, unsigned int file_task_block=DEFAULT_FILE_BLOCK_SIZE);

unsigned long tree_recursive_task_optimized(DIR* dir_ptr, std::string prefix="", unsigned int level = 0,
    unsigned int file_delay=DEFAULT_FILE_OPERATION_OVERHEAD, unsigned int file_task_block=DEFAULT_FILE_BLOCK_SIZE);

unsigned int file_operation(std::string name, unsigned long level, unsigned long file_delay){
    print_mode==REGULAR && printf("%s [file] [T%d]\n",(std::string(level, '\t')+name).c_str(), omp_get_thread_num());
    dummy_file_operation_overhead(file_delay);
    alt_cost += DEFAULT_FILE_COST;
    return 1;
}

unsigned int folder_operation_serial(DIR* folder_entry, std::string path, std::string name, unsigned long level, unsigned long file_delay){
    print_mode==REGULAR && printf("%s [folder] [T%d]\n",(std::string(level, '\t')+name).c_str(), omp_get_thread_num());
    alt_cost += DEFAULT_FOLDER_COST;
    return 1 + tree_recursive_serial(folder_entry, path+"/", level+1);
}

unsigned int folder_operation_task(DIR* folder_entry, std::string path, std::string name, unsigned long level, unsigned long file_delay){
    print_mode==REGULAR && printf("%s [folder] [T%d]\n",(std::string(level, '\t')+name).c_str(), omp_get_thread_num());
    #pragma omp task
    {           
                file_count += tree_recursive_task(folder_entry, path+"/", level+1);
    }
    alt_cost += DEFAULT_FOLDER_COST;
    return 1;
}

unsigned int folder_operation_task_optmized(DIR* folder_entry, std::string path, std::string name, unsigned long level, unsigned long file_delay){
    print_mode==REGULAR && printf("%s [folder] [T%d]\n",(std::string(level, '\t')+name).c_str(), omp_get_thread_num());
    #pragma omp task
    {
                file_count += tree_recursive_task_optimized(folder_entry, path+"/", level+1);
    }
    alt_cost += DEFAULT_FOLDER_COST;
    return 1;
}

unsigned long tree_recursive_serial(DIR* dir_ptr, std::string prefix, unsigned int level, unsigned int file_delay){
    
    unsigned long res = 0;
    for (dirent64* entry = readdir64(dir_ptr); entry != NULL; entry = readdir64(dir_ptr)){
        
        const std::string name = entry->d_name;
        if (name == "." || name == ".."){continue;}
        const auto path = prefix + name;

        DIR* sub_entry = handled_opendir(path.c_str());
        if (sub_entry == NULL){
            res += file_operation(name, level, file_delay);
        } else {
            res += folder_operation_serial(sub_entry, path, name, level, file_delay);
        }
    }
    closedir(dir_ptr);
    return res;
}

unsigned long tree_recursive_task(DIR* dir_ptr, std::string prefix, unsigned int level,
    unsigned int file_delay, unsigned int file_task_block){

    unsigned long res = 0;    
    for (dirent64* entry = readdir64(dir_ptr); entry != NULL; entry = readdir64(dir_ptr)){
        
        const std::string name = entry->d_name;
        if (name == "." || name == ".."){continue;}
        const auto path = prefix + name;
        
        DIR* sub_entry = handled_opendir(path.c_str());
        if (sub_entry == NULL){
            res += file_operation(name, level, file_delay);
        } else {
            res += folder_operation_task(sub_entry, path, name, level, file_delay);
        }
    }
    closedir(dir_ptr);
    return res;
}

unsigned long tree_recursive_task_optimized(DIR* dir_ptr, std::string prefix,
    unsigned int level, unsigned int file_delay, unsigned int file_task_block){
    
    minimal_queue<std::string> file_queue(DEFAULT_STARTING_QUEUE_SIZE);
    unsigned long res = 0;
    for (dirent64* entry = readdir64(dir_ptr); entry != NULL; entry = readdir64(dir_ptr)){
        
        const std::string name = entry->d_name;
        if (name == "." || name == ".."){continue;}
        auto path = prefix + name;
        DIR* sub_entry = handled_opendir(path.c_str());
        
        if (sub_entry == NULL){
            file_queue.push(name);
        } else {
            res += folder_operation_task_optmized(sub_entry, path, name, level, file_delay);
        }
    }
    closedir(dir_ptr);

    while (file_queue.size > 0){
        std::string* names = new std::string[file_task_block];
        unsigned long block_size = file_queue.popN(file_task_block, names);
        #pragma omp task firstprivate(names, block_size, level)
        {
            for (int i = 0; i < block_size; i++){
                file_count += file_operation(names[i], level, file_delay);
            }
            delete[] names;
        }
    }

    return res;
}

return_data tree_serial(const char* root_path, unsigned long num_threads){    
    auto start = utils::mark_time();
    DIR *base = handled_opendir(root_path);
    unsigned long total_files = tree_recursive_serial(base, root_path);
    auto end = utils::mark_time();
    if constexpr (alternative_cost_mode){
        return {alt_cost, utils::calc_time_interval_ms(start, end)};
    } else {
        return {total_files, utils::calc_time_interval_ms(start, end)};
    }
}

template <auto recursive_func>
return_data tree_parallel(const char* root_path, unsigned long num_threads){
    auto start = utils::mark_time();
    DIR *base = handled_opendir(root_path);
    unsigned long total_files = 0;
    unsigned long total_alt_cost = 0;
    #pragma omp parallel num_threads(num_threads) reduction(+:total_files)
    {
        #pragma omp single
        {
            file_count += recursive_func(base, root_path, 0, DEFAULT_FILE_OPERATION_OVERHEAD, DEFAULT_FILE_BLOCK_SIZE);
        }
        #pragma omp barrier
        total_files = file_count;
        #pragma omp critical
        {
            total_alt_cost = total_alt_cost > alt_cost ? total_alt_cost : alt_cost;
        }
        #pragma omp barrier
        alt_cost = total_alt_cost;
    }
    
    auto end = utils::mark_time();
    if constexpr (alternative_cost_mode){
        return {total_alt_cost, utils::calc_time_interval_ms(start, end)};
    } else {
        return {total_files, utils::calc_time_interval_ms(start, end)};
    }
}

typedef return_data (*target_func_ptr)(const char*, unsigned long);

std::unordered_map<std::string, target_func_ptr> modes = {
    {"SERIAL", tree_serial},
    {"TASK_PARALLEL", tree_parallel<tree_recursive_task>},
    {"TASK_PARALLEL_OPTIMIZED", tree_parallel<tree_recursive_task_optimized>},
};

int main(int argc, char* argv[]){
    if (argc < 4){
        printf("Insufficient arguments, use: %s <MODE> <DIR_PATH> <NUM_THREADS> [-printCSV]\n", argv[0]);
        printf("<MODE> being either SERIAL, TASK_PARALLEL or TASK_PARALLEL_OPTIMIZED\n");
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

    //std::string dir_param(argv[2]);
    unsigned long num_threads = std::strtoul(argv[3], NULL, 10);
        if (num_threads == 0 || errno == ERANGE){
            printf("Invalid parameter: %s.\n", argv[3]);
            exit(1);
        }

    return_data exec_data = mode->second(argv[2], num_threads);

    switch (print_mode){
    case CSV:
        printf("%s, %lu, %lu, %lu, %f\n", argv[1], num_threads, alt_cost, exec_data.res, exec_data.time_ms);
        break;
    default:
        printf("Result for n = %lu: %lu; Total elapsed time: %f ms\n", num_threads, exec_data.res, exec_data.time_ms);
        break;
    }

    return 0;
}