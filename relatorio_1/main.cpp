#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <unordered_map>
#include <string>
#include <cerrno>
#include "utils.h"

void* _checking_malloc(size_t bytesize, int line){
    void* res = std::malloc(bytesize);
    if (res == NULL){printf("Allocation error around ln %d\n", line); exit(1);}
    return res;
}

#define checking_malloc(bytesize) _checking_malloc(bytesize, __LINE__)

template <typename T>
void print_vec(T* vec, size_t length){
    for (auto i = 0; i < length; i++){
        std::cout << vec[i] << ", ";
    }
    std::cout << "\n";
}

enum MAT_ORDER{
    ROW_MAJOR,
    COL_MAJOR
};

enum PRINT_MODE {
    REGULAR,
    VECTOR,
    CSV
};

template <typename T>
T min(T a, T b){
    return a > b ? b : a;
}

template <typename T>
T generator(size_t row, size_t col, size_t seed){
    return (T)(seed + row*col + 5*col)/(T)(1 << min(min(row, col)+1, sizeof(T) -1));
}

//_mat_order being ROW_MAJOR or COL_MAJOR; generator being a function to be called with (row, column, seed)
template <typename T>
void populate_mat(T* mat_in_out, size_t mat_ln, size_t mat_col, enum MAT_ORDER _mat_order,
    T (*generator)(size_t, size_t, size_t), size_t seed = 0){

    if (_mat_order == ROW_MAJOR){
        for (size_t i = 0; i < mat_ln; i++){
            for (size_t j = 0; j < mat_col; j++){
                *(mat_in_out+ j + i*mat_col) = generator(i, j, seed);
            }
        }
    } else {
        for (size_t j = 0; j < mat_col; j++){
            for (size_t i = 0; i < mat_ln; i++){
                *(mat_in_out + i + j*mat_ln) = generator(i, j, seed);
            }
        }   
    }
}

//i can't measure time outside of each function due to the function call overhead, but there surely was a way for ln 17
//to not repeat in every different strategy
template <typename T>
double mat_x_vec(T* mat_in, T* vec_in, size_t mat_ln, size_t mat_col, size_t vec_ln, T* vec_out){
    if (mat_col != vec_ln) {printf("Tamanho incompatível: (%lu, %lu) x (%lu, 1)\n", mat_col, mat_ln, vec_ln); exit(1);}
    
    auto start = utils::mark_time();
    for (size_t i = 0; i < mat_ln; i++){
        for (size_t j = 0; j < mat_col; j++){
            vec_out[i] += mat_in[j + i*mat_col]*vec_in[j];
        }
    }
    auto end = utils::mark_time();
    return utils::calc_time_interval_ms(start, end);
}

template <typename T>
double mat_x_vec_done_wrong(T* mat_in, T* vec_in, size_t mat_ln, size_t mat_col, size_t vec_ln, T* vec_out){
    if (mat_col != vec_ln) {printf("Tamanho incompatível: (%lu, %lu) x (%lu, 1)\n", mat_col, mat_ln, vec_ln); exit(1);}
    
    auto start = utils::mark_time();
    for (size_t j = 0; j < mat_col; j++){
        for (size_t i = 0; i < mat_ln; i++){
            vec_out[i] += mat_in[j + i*mat_col]*vec_in[j];
        }
    }
    auto end = utils::mark_time();
    return utils::calc_time_interval_ms(start, end);
}

//keeping it on float to favour vectorized-fma like optimizations
double alternative_mat_x_vec(float* mat_in, float* vec_in, size_t mat_ln, size_t mat_col, size_t vec_ln, float* vec_out){
    if (mat_col != vec_ln) {printf("Tamanho incompatível: (%lu, %lu) x (%lu, 1)\n", mat_col, mat_ln, vec_ln); exit(1);}
    
    auto start = utils::mark_time();
    for (size_t j = 0; j < mat_col; j++){
        for (size_t i = 0; i < mat_ln; i++){
            vec_out[i] += mat_in[i + j*mat_ln]*vec_in[j];
        }
    }
    auto end = utils::mark_time();
    return utils::calc_time_interval_ms(start, end);
}

typedef double (*mat_x_vec_func_ptr)(float*, float*, size_t, size_t, size_t, float*);

typedef struct {
    mat_x_vec_func_ptr func;
   enum MAT_ORDER _mat_order; 
} exec_data;

std::unordered_map<std::string, PRINT_MODE> print_flags = {
    {"-printVec", VECTOR},
    {"-printCSV", CSV},
};

std::unordered_map<std::string, exec_data> modes = {
    {"ROW_ORDER", {mat_x_vec<float>, ROW_MAJOR}},
    {"COL_ORDER", {mat_x_vec_done_wrong<float>, ROW_MAJOR}},
    {"COL_ORDER_ALT", {alternative_mat_x_vec, COL_MAJOR}}
};

int main(int argc, char* argv[]){
    PRINT_MODE print_mode = REGULAR;
    if (argc < 4){
        printf("Insufficient arguments, use: %s <MODE> <MAT_ROWS> <MAT_COLS> [-printVec | -printCSV]\n", argv[0]);
        printf("<MODE> being either ROW_ORDER, COL_ORDER or COL_ORDER_ALT\n");
        exit(1);
    } else{
        if (argc > 5) {
            printf("Too many arguments starting at %s. Use: %s <MODE> <MAT_ROWS> <MAT_COLS> [-printVec | -printCSV]\n",
                argv[5], argv[0]);
            exit(1);
        }
        if (argc == 5){
            auto temp_print = print_flags.find(argv[4]);
            if (temp_print == print_flags.end()){
                printf("Invalid argument: %s. Usage: %s <MODE> <MAT_ROWS> <MAT_COLS> [-printVec | -printCSV]\n",
                    argv[4], argv[0]);
                exit(1);
            }
            print_mode = temp_print->second;
        }
    }

    auto mode = modes.find(argv[1]);
    if (mode == modes.end()){
        printf("Invalid mode: %s. Choose between ROW_ORDER, COL_ORDER or COL_ORDER_ALT\n", argv[1]);
        exit(1);
    }

    mat_x_vec_func_ptr func = mode->second.func;

    size_t params[2];
    for (auto i = 0; i < 2; i++){
        params[i] = std::strtoul(argv[2+i], NULL, 10);
        if (params[i] == 0 || errno == ERANGE){
            printf("Invalid parameter: %s.\n", argv[2+i]);
            exit(1);
        }
    }

    float* mat_in = (float*) checking_malloc(params[0]*params[1]*sizeof(float));
    float* vec_in = (float*) checking_malloc(params[1]*sizeof(float));
    float* vec_out = (float*) checking_malloc(params[1]*sizeof(float));

    populate_mat(mat_in, params[0], params[1], mode->second._mat_order, generator, 0);
    populate_mat(vec_in, params[1], 1, mode->second._mat_order, generator, 10);

    double exec_time = mode->second.func(mat_in, vec_in, params[0], params[1], params[1], vec_out);

    switch (print_mode)
    {
    case VECTOR:
        print_vec(vec_out, params[1]);
        break;
    case CSV:
        printf("%s,%lu,%lu,%f\n", argv[1], params[0], params[1], exec_time);
        break;
    default:
        printf("Total elapsed time: %f ms\n", exec_time);
        break;
    }

    free(mat_in);
    free(vec_in);
    free(vec_out);
    return 0;
}