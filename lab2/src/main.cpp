#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <iostream>
#include <random>

struct Thread_Data{
    double radius;
    int points_per_thread;
};

int total_points_in_circle = 0;
pthread_mutex_t mutex;



void* monte_carlo(void* arg) {
    struct Thread_Data* data = (struct Thread_Data*)arg;
    int inside_circle = 0;

    std::random_device rd;  // Получаем случайное начальное значение
    std::mt19937 gen(rd());
    std::uniform_real_distribution<double> dis(-data->radius, data->radius);

    double radius_square = data->radius * data->radius;

    for (int i = 0; i < data->points_per_thread; ++i) {
        double x = dis(gen);
        double y = dis(gen);
        if (x * x + y * y <= (radius_square)) {
            inside_circle++;
        }
    }

    pthread_mutex_lock(&mutex);
    total_points_in_circle += inside_circle;
    pthread_mutex_unlock(&mutex);
    pthread_exit(NULL);
}


int main(int argc, char* argv[]) {
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <radius> <threads_number>" << std::endl;
        return EXIT_FAILURE;
    }

    double radius = atof(argv[1]);
    int threads_number = atoi(argv[2]);
    if (radius <= 0 || threads_number <= 0) {
        std::cerr << "Radius and threads_number must be positive numbers." << std::endl;
        return EXIT_FAILURE;
    }

    srand(time(NULL));
    int total_points = 100000000;

    pthread_t threads[threads_number];
    int points_per_thread = 100000000 / threads_number;
    struct Thread_Data data;
    data.radius = radius;
    data.points_per_thread = points_per_thread;

    pthread_mutex_init(&mutex, nullptr);

    for (int i = 0; i < threads_number; ++i) {
        pthread_create(&threads[i], NULL, monte_carlo, &data);
    }

    for (int i = 0; i < threads_number; ++i) {
        pthread_join(threads[i], NULL);
    }

    pthread_mutex_destroy(&mutex);

    double area = ((double)total_points_in_circle / total_points) * (radius * radius * 4);
    std::cout << "Estimated area of the circle with radius " << radius << ": " << area << std::endl;

    // Display the number of threads used
    std::cout << "Number of threads used: " << threads_number << std::endl;

    clock_gettime(CLOCK_MONOTONIC, &end);
    double elapsed_time = (end.tv_sec - start.tv_sec) +
                          (end.tv_nsec - start.tv_nsec) / 1e9;
    std::cout << "Elapsed time: " << elapsed_time << " seconds" << std::endl;

    return EXIT_SUCCESS;

}
