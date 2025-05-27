#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <unordered_map>
#include <cstring>
#include <cerrno>
#include "utils.h"
#include <mpi.h>


void* _checking_malloc(size_t bytesize, int line){
    void* res = std::malloc(bytesize);
    if (res == NULL){printf("Allocation error around ln %d\n", line); exit(1);}
    return res;
}

#define checking_malloc(bytesize) _checking_malloc(bytesize, __LINE__)

enum PRINT_MODE {
    REGULAR,
    VECTOR,
    CSV
};


template <typename T>
T print_vec(T* vec, size_t length){
    T total = 0;
    for (auto i = 0; i < length; i++){
        std::cout << vec[i] << ", ";
        total += vec[i];
    }
    std::cout << "\n";
    return total;
}

template <typename T>
void ordered_print_vec(T* local_vec, size_t length, int rank, int size, MPI_Comm comm=MPI_COMM_WORLD){
    double total = 0;
    for (auto i = 0; i < size; i++){
        if (rank == i){
            printf("[R%d] ", rank);
            total = print_vec(local_vec, length);
            total -= ((rank != 0 ? local_vec[0] : 0) + (rank != size-1 ? local_vec[length-1]: 0));
        }
        MPI_Barrier(comm);
    }
    MPI_Allreduce(&total, &total, 1, MPI_DOUBLE, MPI_SUM, comm);
    rank==0 && printf("[R%d] Total: %f\n", rank, total);
}

int update_ghostpoints(int rank, int size, unsigned long local_size, double* local_bar, MPI_Comm comm=MPI_COMM_WORLD){
    if (rank%2 == 0){
        if (rank != size -1){
            int buffer_offset = (local_size-2 > 0) ? local_size-2 : 0; //beware underflows 
            MPI_Send(local_bar+buffer_offset, 1, MPI_DOUBLE, rank+1, 0, comm);
            MPI_Recv(local_bar+buffer_offset+1, 1, MPI_DOUBLE, rank+1, 0, comm, MPI_STATUS_IGNORE);
        }
        if (rank != 0){
            MPI_Recv(local_bar, 1, MPI_DOUBLE, rank-1, 0, comm, MPI_STATUS_IGNORE);
            MPI_Send(local_bar+1, 1, MPI_DOUBLE, rank-1, 0, comm);
        }
    } else{
        MPI_Recv(local_bar, 1, MPI_DOUBLE, rank-1, 0, comm, MPI_STATUS_IGNORE);
        MPI_Send(local_bar+1, 1, MPI_DOUBLE, rank-1, 0, comm);
        if (rank != size-1){
            int buffer_offset = (local_size-2 > 0) ? local_size-2 : 0; //beware underflows
            MPI_Send(local_bar+buffer_offset, 1, MPI_DOUBLE, rank+1, 0, comm);
            MPI_Recv(local_bar+buffer_offset+1, 1, MPI_DOUBLE, rank+1, 0, comm, MPI_STATUS_IGNORE);
        }
    }
    return 0;
}

int update_ghostpoints_instant(int rank, int size, unsigned long local_size, double* local_bar, int req_num,
    MPI_Request* requests, MPI_Comm comm=MPI_COMM_WORLD){
    int progress = 0;
    
    int buffer_offset = (local_size-2 > 0) ? local_size-2 : 0; //beware underflows 
    if (rank != size -1){
        MPI_Isend(local_bar+buffer_offset, 1, MPI_DOUBLE, rank+1, 0, comm, requests);
        MPI_Irecv(local_bar+buffer_offset+1, 1, MPI_DOUBLE, rank+1, 0, comm, requests+1); //index 1 for everyone but size-1
        progress += 2;
    }
    if (rank != 0){
        MPI_Isend(local_bar+1, 1, MPI_DOUBLE, rank-1, 0, comm, requests+progress);
        MPI_Irecv(local_bar, 1, MPI_DOUBLE, rank-1, 0, comm, requests+progress+1); //index 3 for inner nodes, 1 for size -1, none for 0
    }

    return 0;
}

void initial_condition(int rank, int size, unsigned long local_size, double* local_bar, double max_heat){
    std::srand(utils::mark_time().time_since_epoch().count());
    for (auto i = 0; i < local_size; i++){
        if (rank==0 && i == 0){
            local_bar[i] = ((double)std::rand()/RAND_MAX)*max_heat;
            continue;
        }
        local_bar[i] = i;
    }
    update_ghostpoints(rank, size, local_size, local_bar);
}

void workload_blocking(int rank, int size, unsigned long local_size, double* local_bar_in, double* local_bar_out,
    unsigned int max_iter){
    double* vec_A = local_bar_in;
    double* vec_B = local_bar_out;
    //applying T(x) = (T(x-h)+T(x+h))/2 for the core, T(x) = (T(x)+T(x+-h))/2 for the borders.
    for (auto i = 0; i < max_iter; i++){
        update_ghostpoints(rank, size, local_size, vec_A);
        if (rank == 0){
            vec_B[0] = (vec_A[0]+vec_A[1])/2.; //using vec_A[0]/2 to keep energy conservation
        }
        if (rank == size-1){
            vec_B[local_size-1] = (vec_A[local_size-1]+vec_A[local_size-2])/2.;
        }
        for (auto j = 1; j < local_size-1; j++){
            vec_B[j] = (vec_A[j-1]+vec_A[j+1])/2.;
        }
        double* temp_ptr = vec_A;
        vec_A = vec_B;
        vec_B = temp_ptr;
    }
    std::memmove(local_bar_out, vec_B, sizeof(double)*local_size);
}

void workload_instant_wait(int rank, int size, unsigned long local_size, double* local_bar_in, double* local_bar_out,
    unsigned int max_iter){
    double* vec_A = local_bar_in;
    double* vec_B = local_bar_out;
    for (auto i = 0; i < max_iter; i++){
        int req_num = (rank == 0 || rank == size-1) ? 2 : 4;
        MPI_Request requests[req_num];
        update_ghostpoints_instant(rank, size, local_size, vec_A, req_num, requests);
        for (auto i = 0; i < req_num; i++){
            MPI_Wait(requests+i, MPI_STATUS_IGNORE);
        }
        if (rank == 0){
            //vec_B[0] = (vec_A[0]+vec_A[1])/2.; //using vec_A[0]/2 to keep energy conservation
            vec_B[0] = (vec_A[0]+vec_A[1]) - vec_A[0]; //using vec_A[0]/2 to keep energy conservation
        }
        if (rank == size-1){
            //vec_B[local_size-1] = (vec_A[local_size-1]+vec_A[local_size-2])/2.;
            vec_B[local_size-1] = (vec_A[local_size-1]+vec_A[local_size-2]) - vec_A[local_size-1];
        }
        for (auto j = 1; j < local_size-1; j++){
            //vec_B[j] = (vec_A[j-1]+vec_A[j+1])/2.;
            vec_B[j] = (vec_A[j-1]+vec_A[j+1]) - vec_A[j];
        }
        double* temp_ptr = vec_A;
        vec_A = vec_B;
        vec_B = temp_ptr;
    }
    std::memmove(local_bar_out, vec_B, sizeof(double)*local_size);
}

int test_and_exec(int rank, int size, unsigned long local_size, double* local_bar_in, double* local_bar_out,
    int req_num, MPI_Request* requests, int* ready_vec, int index, int it){
    
    MPI_Test(requests+index, ready_vec+index, MPI_STATUS_IGNORE);
    if (ready_vec[index]){
        switch (index){
        case 1:
            if (rank!=size-1){
                local_bar_out[local_size-2] = (local_bar_in[local_size-3]+local_bar_in[local_size-1])/2.;
            } else{
                local_bar_out[1] = (local_bar_in[0]+local_bar_in[2])/2.;
            }
            break;
        case 3:
            local_bar_out[1] = (local_bar_in[0]+local_bar_in[2])/2.; //3 is only reachable if rank!=size-1 || rank != 0
            break;
        }
        return index+1;
    }
    return index;
}

void workload_instant_test(int rank, int size, unsigned long local_size, double* local_bar_in, double* local_bar_out,
    unsigned int max_iter){
    double* vec_A = local_bar_in;
    double* vec_B = local_bar_out;
    for (auto i = 0; i < max_iter; i++){
        MPI_Barrier(MPI_COMM_WORLD);
        int req_num = (rank == 0 || rank == size-1) ? 2 : 4;
        MPI_Request requests[req_num];
        int ready_vec[req_num];

        int ready_index = 0;
        update_ghostpoints_instant(rank, size, local_size, vec_A, req_num, requests);
        
        if (rank == 0){
            vec_B[0] = (vec_A[0]+vec_A[1])/2.; //using vec_A[0]/2 to keep energy conservation
            vec_B[1] = (vec_A[0]+vec_A[2])/2.;
            //vec_B[0] = (vec_A[0]+vec_A[1]) - vec_A[0]; //using vec_A[0]/2 to keep energy conservation
            //vec_B[1] = (vec_A[0]+vec_A[2]) - vec_A[1];
        }
        if (rank == size-1){
            vec_B[local_size-1] = (vec_A[local_size-1]+vec_A[local_size-2])/2.;
            vec_B[local_size-2] = (vec_A[local_size-3]+vec_A[local_size-1])/2.;
            //vec_B[local_size-1] = (vec_A[local_size-1]+vec_A[local_size-2]) - vec_A[local_size-1];
            //vec_B[local_size-2] = (vec_A[local_size-3]+vec_A[local_size-1]) - vec_A[local_size-2];
        }
        ready_index = test_and_exec(rank, size, local_size, vec_A, vec_B, req_num, requests, ready_vec, ready_index, i);
        for (auto j = 2; j < local_size-2; j++){
            vec_B[j] = (vec_A[j-1]+vec_A[j+1])/2.;
            //vec_B[j] = (vec_A[j-1]+vec_A[j+1]) - vec_A[j];
            if (ready_index < req_num){
                ready_index = test_and_exec(rank, size, local_size, vec_A, vec_B, req_num, requests, ready_vec,
                    ready_index, i);
            }
        }
        while (ready_index < req_num){
            ready_index = test_and_exec(rank, size, local_size, vec_A, vec_B, req_num, requests, ready_vec,ready_index,i);
        }
        MPI_Barrier(MPI_COMM_WORLD);
        double* temp_ptr = vec_A;
        vec_A = vec_B;
        vec_B = temp_ptr;
    }
    std::memmove(local_bar_out, vec_B, sizeof(double)*local_size);
}

typedef void (*target_func_ptr)(int, int, unsigned long, double*, double*, unsigned int);

std::unordered_map<std::string, target_func_ptr> modes = {
    {"BLOCKING", workload_blocking},
    {"INSTANT_WAIT", workload_instant_wait},
    {"INSTANT_TEST", workload_instant_test},
};

int main(int argc, char* argv[]){
    MPI_Init(&argc, &argv);
    if (argc < 5){
        printf("Insufficient arguments, use: %s <MODE> <BARSIZE> <MAX_HEAT> <NUM_ITER>\n", argv[0]);
        printf("<MODE> being either BLOCKING, INSTANT_WAIT or INSTANT_TEST\n");
        printf("<BARSIZE> being the length of the simulated bar to pass\n");
        printf("<MAX_HEAT> being the maximum randomly generated heat of the initial condition (integer, though it will be used as a double)\n");
        printf("<NUM_ITER> being the total number of iterations\n");
        exit(1);
    }

    auto mode = modes.find(argv[1]);
    if (mode == modes.end()){
        printf("Invalid mode: %s. Choose between BLOCKING, INSTANT_WAIT or INSTANT_TEST\n", argv[1]);
        exit(1);
    }

    target_func_ptr func = mode->second;

    size_t params[3];
    for (auto i = 0; i < 3; i++){
        params[i] = std::strtoul(argv[2+i], NULL, 10);
        if (params[i] == 0 || errno == ERANGE){
            printf("Invalid parameter: %s.\n", argv[1+i]);
            exit(1);
        }
    }

    int rank;
    int size;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (( size < 2 || params[0]/size <= 1)){
        rank==0 && printf("Please use at least 2 processes and be sure that each one of them holds at least one bar element (BARSIZE/n >= 1)\n");
        MPI_Finalize();
        return 1;    
    }
    
    unsigned long barsize = params[0]; //readability
    unsigned long it_num = params[2];
    unsigned long local_size = barsize/size + (barsize%size > rank ? 1 : 0) + (size > 1 ? (rank == 0 || rank == size-1 ? 1 : 2) : 0);
    double* local_bar = new double[local_size];
    double* local_bar_aux = new double[local_size];
    double start, end;
    
    initial_condition(rank, size, local_size, local_bar, (double) params[1]);
    
    #ifdef DEBUG
    ordered_print_vec(local_bar, local_size, rank, size);
    #endif
    start = MPI_Wtime();
    func(rank, size, local_size, local_bar, local_bar_aux, it_num);
    end = MPI_Wtime();
    #ifdef DEBUG
    ordered_print_vec(local_bar, local_size, rank, size);
    #endif

    delete[] local_bar;
    delete[] local_bar_aux;
    MPI_Finalize();
    rank==0 && printf("%s,%d,%lu,%lu,%f\n", argv[1], size, barsize, it_num, end-start);

    return 0;
}