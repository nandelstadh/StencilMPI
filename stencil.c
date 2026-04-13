#include "stencil.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mpi.h"

int main(int argc, char** argv) {
    if (4 != argc) {
        printf("Usage: stencil input_file output_file number_of_applications\n");
        return 1;
    }
    char* input_name = argv[1];
    char* output_name = argv[2];
    int num_steps = atoi(argv[3]);

    int myid, p;
    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &p);
    MPI_Comm_rank(MPI_COMM_WORLD, &myid);
    MPI_Status stat;

    int num_values;
    double* input;
    double* global_input;
    int global_num_values;
    // Read input file
    if (myid == 0) {
        if (0 > (global_num_values = read_input(input_name, &global_input))) {
            return 2;
        }
    }

    MPI_Bcast(&global_num_values, 1, MPI_INT, 0, MPI_COMM_WORLD);
    num_values = global_num_values / p;

    printf("Number of values locally is: %d\n", num_values);
    input = malloc(sizeof(double) * num_values);

    MPI_Scatter(global_input, num_values, MPI_DOUBLE, input, num_values, MPI_DOUBLE, 0, MPI_COMM_WORLD);

    // Stencil values
    double h = 2.0 * PI / global_num_values;
    const int STENCIL_WIDTH = 5;
    const int EXTENT = STENCIL_WIDTH / 2;
    const double STENCIL[] = {1.0 / (12 * h), -8.0 / (12 * h), 0.0, 8.0 / (12 * h), -1.0 / (12 * h)};
    // Start timer
    double start = MPI_Wtime();

    // Allocate data for result
    double* output;
    if (NULL == (output = malloc(num_values * sizeof(double)))) {
        perror("Couldn't allocate memory for output");
        return 2;
    }

    MPI_Request recv_higher;
    MPI_Request recv_lower;
    MPI_Request send_higher;
    MPI_Request send_lower;

    double high_r_buf[EXTENT];
    double low_r_buf[EXTENT];
    double high_s_buf[EXTENT];
    double low_s_buf[EXTENT];

    printf("Higher processor: %d\n", (myid + 1 + p) % p);
    printf("Lower processor: %d\n", (myid - 1 + p) % p);
    printf("%f\n", high_s_buf[0]);
    MPI_Recv_init(high_r_buf, EXTENT, MPI_DOUBLE, (myid + 1 + p) % p, 1, MPI_COMM_WORLD, &recv_higher);
    MPI_Recv_init(low_r_buf, EXTENT, MPI_DOUBLE, (myid - 1 + p) % p, 0, MPI_COMM_WORLD, &recv_lower);
    MPI_Send_init(high_s_buf, EXTENT, MPI_DOUBLE, (myid + 1 + p) % p, 0, MPI_COMM_WORLD, &send_higher);
    MPI_Send_init(low_s_buf, EXTENT, MPI_DOUBLE, (myid - 1 + p) % p, 1, MPI_COMM_WORLD, &send_lower);

    // Repeatedly apply stencil
    for (int s = 0; s < num_steps; s++) {
        memcpy(high_s_buf, input + num_values - EXTENT, EXTENT * sizeof(double));
        memcpy(low_s_buf, input, EXTENT * sizeof(double));
        MPI_Start(&recv_higher);
        MPI_Start(&recv_lower);
        MPI_Start(&send_higher);
        MPI_Start(&send_lower);

        MPI_Wait(&send_higher, &stat);
        MPI_Wait(&send_lower, &stat);
        MPI_Wait(&recv_higher, &stat);
        MPI_Wait(&recv_lower, &stat);

        printf("EXTENT: %d\n", EXTENT);

        for (int i = 0; i < num_values; i++) {
            double result = 0;
            for (int j = 0; j < STENCIL_WIDTH; j++) {
                int id = i - EXTENT + j;
                double temp;
                if (id < 0) {
                    temp = low_r_buf[id + EXTENT];
                } else if (id >= num_values) {
                    temp = high_r_buf[id - num_values];
                } else {
                    temp = input[id];
                }
                result += STENCIL[j] * temp;
            }
            output[i] = result;
        }
        /* for (int i = 0; i < EXTENT; i++) { */
        /*     double result = 0; */
        /*     for (int j = i; j < EXTENT; j++) { */
        /*         result += STENCIL[j] * low_r_buf[j + i]; */
        /*     } */
        /*     for (int j = EXTENT - i; j < STENCIL_WIDTH; j++) { */
        /*         int index = (i - EXTENT + j + num_values) % num_values; */
        /*         result += STENCIL[j] * input[index]; */
        /*     } */
        /*     output[i] = result; */
        /* } */
        /* // The logic here should remain unchanged */
        /* for (int i = EXTENT; i < num_values - EXTENT; i++) { */
        /*     double result = 0; */
        /*     for (int j = 0; j < STENCIL_WIDTH; j++) { */
        /*         int index = i - EXTENT + j; */
        /*         result += STENCIL[j] * input[index]; */
        /*     } */
        /*     output[i] = result; */
        /* } */
        // The logic here should be changed so that it exchanges data with node (my_id + 1)
        /* for (int i = num_values - EXTENT; i < num_values; i++) { */
        /*     double result = 0; */
        /*     for (int j = 0; j < STENCIL_WIDTH; j++) { */
        /*         int index = (i - EXTENT + j) % num_values; */
        /*         result += STENCIL[j] * input[index]; */
        /*     } */
        /*     for (int j = STENCIL_WIDTH - EXTENT; j < STENCIL_WIDTH; j++) { */
        /*         result += STENCIL[j] * high_r_buf[j - STENCIL_WIDTH + EXTENT + i]; */
        /*     } */
        /*     output[i] = result; */
        /* } */

        // Swap input and output
        if (s < num_steps - 1) {
            double* tmp = input;
            input = output;
            output = tmp;
        }
    }
    free(input);
    // Stop timer
    double my_execution_time = MPI_Wtime() - start;
    double global_execution_time;

    double* global_output;
    if (myid == 0) {
        if (NULL == (global_output = malloc(global_num_values * sizeof(double)))) {
            perror("Couldn't allocate memory for global output");
            return 2;
        }
    }

    MPI_Reduce(&my_execution_time, &global_execution_time, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Gather(output, num_values, MPI_DOUBLE, global_output, num_values, MPI_DOUBLE, 0, MPI_COMM_WORLD);

    // Write result
    if (myid == 0) {
        printf("%f\n", global_execution_time);
#ifdef PRODUCE_OUTPUT_FILE
        if (0 != write_output(output_name, global_output, global_num_values)) {
            return 2;
        }
#endif
        free(global_output);
    }

    MPI_Request_free(&recv_higher);
    MPI_Request_free(&recv_lower);
    MPI_Request_free(&send_higher);
    MPI_Request_free(&send_lower);

    MPI_Finalize();
    // Clean up
    free(output);

    return 0;
}

int read_input(const char* file_name, double** values) {
    FILE* file;
    if (NULL == (file = fopen(file_name, "r"))) {
        perror("Couldn't open input file");
        return -1;
    }
    int num_values;
    if (EOF == fscanf(file, "%d", &num_values)) {
        perror("Couldn't read element count from input file");
        return -1;
    }
    if (NULL == (*values = malloc(num_values * sizeof(double)))) {
        perror("Couldn't allocate memory for input");
        return -1;
    }
    for (int i = 0; i < num_values; i++) {
        if (EOF == fscanf(file, "%lf", &((*values)[i]))) {
            perror("Couldn't read elements from input file");
            return -1;
        }
    }
    if (0 != fclose(file)) {
        perror("Warning: couldn't close input file");
    }
    return num_values;
}

int write_output(char* file_name, const double* output, int num_values) {
    FILE* file;
    if (NULL == (file = fopen(file_name, "w"))) {
        perror("Couldn't open output file");
        return -1;
    }
    for (int i = 0; i < num_values; i++) {
        if (0 > fprintf(file, "%.4f ", output[i])) {
            perror("Couldn't write to output file");
        }
    }
    if (0 > fprintf(file, "\n")) {
        perror("Couldn't write to output file");
    }
    if (0 != fclose(file)) {
        perror("Warning: couldn't close output file");
    }
    return 0;
}
