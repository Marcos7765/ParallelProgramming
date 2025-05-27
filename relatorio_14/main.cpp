#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include "utils.h"
#include <mpi.h>

/*
void* _checking_malloc(size_t bytesize, int line){
    void* res = std::malloc(bytesize);
    if (res == NULL){printf("Allocation error around ln %d\n", line); exit(1);}
    return res;
}*/

//#define checking_malloc(bytesize) _checking_malloc(bytesize, __LINE__)

int main(int argc, char* argv[]){
    MPI_Init(&argc, &argv);
    if (argc < 2){
        printf("Insufficient arguments, use: %s <BYTESIZE>\n", argv[0]);
        printf("<BYTESZE> being the size of the message to pass\n");
        exit(1);
    }

    size_t bytesize;
    bytesize = std::strtoul(argv[1], NULL, 10);
    if (bytesize == 0 || errno == ERANGE){
        printf("Invalid parameter: %s.\n", argv[1]);
        exit(1);
    }

    int rank;
    int size;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (size != 2){
        rank==0 && printf("Please use only 2 processes\n");
        MPI_Finalize();
        return 1;    
    }
    
    unsigned char* buffer = (unsigned char*) utils::checking_malloc(bytesize);
    double start, end;
    if (rank==0){
        start = MPI_Wtime();
        MPI_Send(buffer, bytesize, MPI_UNSIGNED_CHAR, 1, 0, MPI_COMM_WORLD);
        MPI_Recv(buffer, bytesize, MPI_UNSIGNED_CHAR, 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        end = MPI_Wtime();
    }else{
        MPI_Recv(buffer, bytesize, MPI_UNSIGNED_CHAR, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        MPI_Send(buffer, bytesize, MPI_UNSIGNED_CHAR, 0, 0, MPI_COMM_WORLD);
    }
    
    free(buffer);
    MPI_Finalize();
    
    rank==0 && printf("%lu,%f\n", bytesize, end-start);

    return 0;
}