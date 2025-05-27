#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include "utils.h"
#include <mpi.h>


void create_and_commit_column_type(int row_count, int col_spacing, unsigned long element_size, MPI_Datatype* dt_out){

    MPI_Datatype temp;
    MPI_Type_vector(row_count, 1, col_spacing, MPI_CHAR, &temp);
    MPI_Type_create_resized(temp, 0, element_size, dt_out);
    MPI_Type_commit(dt_out);
}

unsigned long calc_local_columns(unsigned long total_columns, int rank, int size){
    return total_columns/size + (total_columns%size > rank ? 1 : 0);
}

//just to not use whatever garbage's in memory
template <typename T>
T print_vec_int(T* vec, size_t length){
    T total = 0;
    for (auto i = 0; i < length; i++){
        std::cout << (int) vec[i] << ", ";
        total += vec[i];
    }
    std::cout << "\n";
    return total;
}

template <typename T>
T* alloc_and_init(unsigned long element_count){
    T* arr = (T*) utils::checking_malloc(element_count*sizeof(T)); //this won't work well with the __LINE__ print
    for (auto i = 0; i < element_count; i++){
        arr[i] = i;
    }
    return arr;
}

template <typename T>
void mat_x_vec(T* mat_in, T* vec_in, T* vec_out, unsigned long row_count, unsigned long col_count){
    for (auto i = 0; i < row_count; i++){
        for (auto j = 0; j < col_count; j++){
            vec_out[i] += mat_in[i*col_count + j]*vec_in[j];
        }
    }
}

int main(int argc, char* argv[]){
    MPI_Init(&argc, &argv);
    if (argc < 3){
        printf("Insufficient arguments, use: %s <ROWS> <COLUMNS>\n", argv[0]);
        exit(1);
    }

    size_t M;
    M = std::strtoul(argv[1], NULL, 10);
    if (M == 0 || errno == ERANGE){
        printf("Invalid parameter: %s.\n", argv[1]);
        exit(1);
    }
    size_t N;
    N = std::strtoul(argv[1], NULL, 10);
    if (N == 0 || errno == ERANGE){
        printf("Invalid parameter: %s.\n", argv[1]);
        exit(1);
    }

    int rank;
    int size;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (size < 1){
        rank==0 && printf("Please at least 2 processes\n");
        MPI_Finalize();
        return 1;    
    }

    unsigned long local_column_total = calc_local_columns(N, rank, size);
    unsigned char* A_local = (unsigned char*) utils::checking_malloc(M*local_column_total*sizeof(unsigned char));
    unsigned char* y_local = (unsigned char*) utils::checking_malloc(M*sizeof(unsigned char));

    MPI_Datatype col_vec_sender_resized;
    MPI_Datatype col_vec_receiver_resized;
    if (rank==0){
        create_and_commit_column_type(M, N, sizeof(unsigned char), &col_vec_sender_resized);
    }
    create_and_commit_column_type(M, local_column_total, sizeof(unsigned char), &col_vec_receiver_resized);

    for (auto i = 0; i < N; i++){
        y_local[i] = 0;
    }
    unsigned char* x_vector;
    
    double start, end;
    if (rank==0){
        unsigned char* A_global = alloc_and_init<unsigned char>(M*N);
        unsigned char* y_global = (unsigned char*) utils::checking_malloc(M*sizeof(unsigned char));
        unsigned char* x_global = alloc_and_init<unsigned char>(N);
        x_vector = (unsigned char*) utils::checking_malloc(local_column_total*sizeof(unsigned char));
        //x_vector = alloc_and_init<unsigned char>(N);
        int* column_counts = (int*) utils::checking_malloc(size*sizeof(int));
        int* element_counts = (int*) utils::checking_malloc(size*sizeof(int));
        int* offsets = (int*) utils::checking_malloc(size*sizeof(int));
        int* column_offsets = (int*) utils::checking_malloc(size*sizeof(int));
        int total = 0;
        for (int i = 0; i < size; i++){
            column_counts[i] = calc_local_columns(N, i, size);
            element_counts[i] = column_counts[i]*M;
            column_offsets[i] = total;
            offsets[i] = total*M;
            total += column_counts[i];
        }
        
        start = MPI_Wtime();

        MPI_Scatterv(A_global, column_counts, column_offsets, col_vec_sender_resized, A_local, local_column_total,
            col_vec_receiver_resized, 0, MPI_COMM_WORLD);
        MPI_Scatterv(x_global, column_counts, column_offsets, MPI_CHAR, x_vector, local_column_total, MPI_CHAR, 0,
            MPI_COMM_WORLD);
        mat_x_vec(A_local, x_vector, y_local, M, local_column_total);
        MPI_Reduce(y_local, y_global, N, MPI_CHAR, MPI_SUM, 0, MPI_COMM_WORLD);
        
        end = MPI_Wtime();

        #ifdef DEBUG
        std::cout << local_column_total;
        printf("\n\n");
        print_vec_int(A_global, M*N);
        printf("\n\n");
        print_vec_int(x_vector, N);
        printf("\n\n");
        print_vec_int(y_local, M);
        printf("\n\n");
        print_vec_int(y_global, M);
        printf("\n\n");
        #endif

        free(column_counts);
        free(element_counts);
        free(offsets);
        free(column_offsets);
    }else{
        x_vector = (unsigned char*) utils::checking_malloc(local_column_total*sizeof(unsigned char));
        MPI_Scatterv(NULL, NULL, NULL, MPI_DATATYPE_NULL, A_local, local_column_total, col_vec_receiver_resized, 0,
            MPI_COMM_WORLD);
        MPI_Scatterv(NULL, NULL, NULL, MPI_DATATYPE_NULL, x_vector, local_column_total, MPI_CHAR, 0,
            MPI_COMM_WORLD);
        mat_x_vec(A_local, x_vector, y_local, M, local_column_total);
        MPI_Reduce(y_local, NULL, N, MPI_CHAR, MPI_SUM, 0, MPI_COMM_WORLD);
    }

    free(A_local);
    free(y_local);
    free(x_vector);
    MPI_Finalize();

    rank==0 && printf("%d,%lu,%lu,%f\n", size, M, N, end-start);

    return 0;
}