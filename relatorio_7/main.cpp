#include <cstdio>
#include <unordered_map>
#include <string>
#include <cerrno>
#include "utils.h"
#include <dirent.h>
#include <omp.h>
#include <unistd.h>

enum PRINT_MODE {
    REGULAR,
    CSV
};

std::unordered_map<std::string, PRINT_MODE> print_flags = {
    {"-printCSV", CSV},
};

typedef utils::_return_data<unsigned long> return_data;

//oh dear, look! it's the minimal_queue from 2 tasks ago!
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

//needed for my choice of being unreasonably compatible with POSIX.1
//typedef struct {
//    DIR* dir_ptr;
//    bool is_file;
//} w_dir_ptr;

void dummy_file_operation_overhead(unsigned int seconds){
    sleep(seconds);
}

//as POSIX.1 struct dirent has no file type identifier, i'll be using its opendir errno to work around.
//by definition errno should be thread safe. let's hope it is
//w_dir_ptr handled_opendir(const char* dirname){
DIR* handled_opendir(const char* dirname){
    //w_dir_ptr res = {opendir(dirname), false};
    DIR* res = opendir(dirname);
    //if (res.dir_ptr == NULL){ //using NULL instead of nullptr because dirent is from libc? no good reason actually but w/e
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
        //printf("%s is not a directory!\n", dirname);
            //res.is_file = true;
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

unsigned long tree_recursive_serial(DIR* dir_ptr, std::string prefix="", unsigned int level = 0, unsigned int file_delay=1){
    unsigned long res = 0;
    for (dirent64* entry = readdir64(dir_ptr); entry != NULL; entry = readdir64(dir_ptr)){
        std::string name = entry->d_name;
        if (name == "." || name == ".."){continue;}
        res++;
        auto path = prefix + name;
        printf("%s",(std::string(level, '\t')+name).c_str());
        DIR* sub_entry = handled_opendir(path.c_str());
        if (sub_entry == NULL){
            printf(" [file] [T%d]\n", omp_get_thread_num());
            dummy_file_operation_overhead(file_delay);
        } else {
            printf(" [folder] [T%d]\n", omp_get_thread_num());
            res += tree_recursive_serial(sub_entry, path+"/", level+1);
        }

    }
    closedir(dir_ptr);
    return res;
}

return_data tree_serial(const char* root_path, unsigned long num_threads){    
    auto start = utils::mark_time();
    DIR *base = handled_opendir(root_path);
    unsigned long total_files = tree_recursive_serial(base, root_path);
    auto end = utils::mark_time();
    return {total_files, utils::calc_time_interval_ms(start, end)};
}

unsigned long file_count = 0;
#pragma omp threadprivate(file_count)

unsigned long tree_recursive_task(DIR* dir_ptr, std::string prefix="", unsigned int level = 0, unsigned int file_delay=1){
    unsigned long res = 0;
    for (dirent64* entry = readdir64(dir_ptr); entry != NULL; entry = readdir64(dir_ptr)){
        std::string name = entry->d_name;
        if (name == "." || name == ".."){continue;}
        res++;
        auto path = prefix + name;
        printf("%s",(std::string(level, '\t')+name).c_str());
        DIR* sub_entry = handled_opendir(path.c_str());
        
        if (sub_entry == NULL){
            printf(" [file] [T%d]\n", omp_get_thread_num());
            dummy_file_operation_overhead(file_delay);
        } else {
            printf(" [folder] [T%d]\n", omp_get_thread_num());
            #pragma omp task
            {
                file_count += tree_recursive_task(sub_entry, path+"/", level+1);
            }
        }

    }
    closedir(dir_ptr);
    return res;
}

return_data tree_parallel(const char* root_path, unsigned long num_threads){    
    auto start = utils::mark_time();
    DIR *base = handled_opendir(root_path);
    unsigned long total_files = 0;
    #pragma omp parallel num_threads(num_threads) reduction(+:total_files)
    {
        #pragma omp single
        {
            #pragma omp task
            {
                file_count += tree_recursive_task(base, root_path);
            }
        }
        #pragma omp barrier
        total_files = file_count;
    }
    
    auto end = utils::mark_time();
    return {total_files, utils::calc_time_interval_ms(start, end)};
}

typedef return_data (*target_func_ptr)(const char*, unsigned long);

std::unordered_map<std::string, target_func_ptr> modes = {
    {"SERIAL", tree_serial},
    {"TASK_PARALLEL", tree_parallel},
};

int main(int argc, char* argv[]){
    PRINT_MODE print_mode = REGULAR;
    if (argc < 4){
        printf("Insufficient arguments, use: %s <MODE> <DIR_PATH> <NUM_THREADS> [-printCSV]\n", argv[0]);
        printf("<MODE> being either SERIAL or TASK_PARALLEL\n");
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
        printf("Invalid mode: %s. Choose between SERIAL or TASK_PARALLEL\n", argv[1]);
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
        printf("%s, %lu, %lu, %f\n", argv[1], num_threads, exec_data.res, exec_data.time_ms);
        break;
    default:
        printf("Result for n = %lu: %lu; Total elapsed time: %f ms\n", num_threads, exec_data.res, exec_data.time_ms);
        break;
    }

    return 0;
}