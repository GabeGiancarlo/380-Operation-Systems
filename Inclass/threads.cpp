#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

// Global variables
double average;
int minimum;
int maximum;

int *numbers;
int count;

// Worker: compute average
void* compute_average(void* arg) {
    int sum = 0;
    for (int i = 0; i < count; i++) {
        sum += numbers[i];
    }
    average = (double) sum / count;
    pthread_exit(0);
}

// Worker: compute minimum
void* compute_minimum(void* arg) {
    int min = numbers[0];
    for (int i = 1; i < count; i++) {
        if (numbers[i] < min) {
            min = numbers[i];
        }
    }
    minimum = min;
    pthread_exit(0);
}

// Worker: compute maximum
void* compute_maximum(void* arg) {
    int max = numbers[0];
    for (int i = 1; i < count; i++) {
        if (numbers[i] > max) {
            max = numbers[i];
        }
    }
    maximum = max;
    pthread_exit(0);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <list of integers>\n", argv[0]);
        return 1;
    }

    count = argc - 1;
    numbers = (int*) malloc(count * sizeof(int));

    for (int i = 0; i < count; i++) {
        numbers[i] = atoi(argv[i + 1]);
    }

    pthread_t tid1, tid2, tid3;

    // Create threads
    pthread_create(&tid1, NULL, compute_average, NULL);
    pthread_create(&tid2, NULL, compute_minimum, NULL);
    pthread_create(&tid3, NULL, compute_maximum, NULL);

    // Wait for threads
    pthread_join(tid1, NULL);
    pthread_join(tid2, NULL);
    pthread_join(tid3, NULL);

    // Print results
    printf("The average value is %.0f\n", average);
    printf("The minimum value is %d\n", minimum);
    printf("The maximum value is %d\n", maximum);

    free(numbers);
    return 0;
}
