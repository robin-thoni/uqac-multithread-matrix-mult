#define _POSIX_C_SOURCE 199309L
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <pthread.h>

struct matrix
{
  unsigned m;
  unsigned n;
  int** scalars;
};
typedef struct matrix s_matrix;

struct matrix_mult_thread_data
{
  s_matrix mat1;
  s_matrix mat2;
  s_matrix mat;
  unsigned scalar_start;
  unsigned scalars_count;
  unsigned thread_number;
};
typedef struct matrix_mult_thread_data s_matrix_mult_thread_data;

unsigned get_cpu_count(void)
{
  return (unsigned)sysconf(_SC_NPROCESSORS_ONLN);
}

s_matrix matrix_generate(unsigned m, unsigned n, unsigned rmax)
{
  s_matrix mat;
  mat.m = m;
  mat.n = n;

  mat.scalars = malloc(mat.m * sizeof(int*));
  for (unsigned i = 0; i < mat.m; ++i) {
      mat.scalars[i] = malloc(mat.n * sizeof(int));
      for (unsigned j = 0; j < mat.n; ++j) {
        mat.scalars[i][j] = (rmax == 0 ? 0 : (rand() % rmax));
      }
  }

  return mat;
}

void matrix_free(s_matrix mat)
{
  for (unsigned i = 0; i < mat.m; ++i) {
    free(mat.scalars[i]);
  }
  free(mat.scalars);
}

void matrix_print(s_matrix mat)
{
  for (unsigned i = 0; i < mat.m; ++i) {
    printf("|");
    for (unsigned j = 0; j < mat.n; ++j) {
      printf("%5d|", mat.scalars[i][j]);
    }
    printf("\n");
  }
}

int matrix_equals(s_matrix mat1, s_matrix mat2)
{
  if (mat1.n != mat2.n || mat1.m != mat2.n) {
    return 0;
  }
  for (unsigned i = 0; i < mat1.n; ++i) {
    for (unsigned j = 0; j < mat1.m; ++j) {
      if (mat1.scalars[i][j] != mat2.scalars[i][j]) {
        return 0;
      }
    }
  }
  return 1;
}

unsigned matrix_mult_scalar(s_matrix mat1, s_matrix mat2, unsigned i, unsigned j)
{
  unsigned a = 0;
  for (unsigned k = 0; k < mat1.n; ++k) {
    a += mat1.scalars[i][k] * mat2.scalars[k][j];
  }
  return a;
}

s_matrix matrix_mult_sequential(s_matrix mat1, s_matrix mat2)
{
  s_matrix mat;
  if (mat1.n != mat2.m) {
    mat.n = 0;
    mat.m = 0;
    mat.scalars = 0;
  }
  else {
    mat = matrix_generate(mat1.m, mat2.n, 0);
    for (unsigned i = 0; i < mat.m; ++i) {
      for (unsigned j = 0; j < mat.n; ++j) {
        mat.scalars[i][j] = matrix_mult_scalar(mat1, mat2, i, j);
      }
    }
  }
  return mat;
}

void matrix_get_thread_scalars_distribution(unsigned scalars_count, unsigned thread_count, unsigned* distribution)
{
    unsigned scalars_per_thread = scalars_count / thread_count;
    unsigned scalars_not_distributed = scalars_count;
    if (scalars_per_thread == 0) {
      scalars_per_thread = 1;
    }
    unsigned thread_number = 0;
    for (; thread_number < thread_count && scalars_not_distributed > 0; ++thread_number) {
      distribution[thread_number] = (thread_number == thread_count - 1 ? scalars_not_distributed : scalars_per_thread);
      scalars_not_distributed -= distribution[thread_number];
    }
    for (; thread_number < thread_count; ++thread_number) {
      distribution[thread_number] = 0;
    }
}

void* matrix_mult_parallel_thread(void* arg)
{
  s_matrix_mult_thread_data* data = (s_matrix_mult_thread_data*)arg;

  unsigned j = data->scalar_start % data->mat.m;

  for (unsigned i = data->scalar_start / data->mat.m; i < data->mat.m && data->scalars_count > 0; ++i) {
    for (; j < data->mat.n && data->scalars_count > 0; ++j) {
      data->mat.scalars[i][j] = matrix_mult_scalar(data->mat1, data->mat2, i, j);
      --data->scalars_count;
    }
    j = 0;
  }

  free(data);
  return 0;
}

pthread_t matrix_mult_parallel_launch_thread(s_matrix mat1, s_matrix mat2, s_matrix mat, unsigned scalar_start,
                          unsigned scalars_count, unsigned thread_number, int launch)
{
  s_matrix_mult_thread_data* data = (s_matrix_mult_thread_data*)malloc(sizeof(s_matrix_mult_thread_data));
  data->mat1 = mat1;
  data->mat2 = mat2;
  data->mat = mat;
  data->scalar_start = scalar_start;
  data->scalars_count = scalars_count;
  data->thread_number = thread_number;
  pthread_t thread = 0;
  (void)launch;
  if (launch) {
    pthread_create(&thread, 0, matrix_mult_parallel_thread, data);
  }
  else {
    matrix_mult_parallel_thread(data);
  }
  return thread;
}

s_matrix matrix_mult_parallel(s_matrix mat1, s_matrix mat2, unsigned thread_count)
{
  s_matrix mat;
  if (mat1.n != mat2.m) {
    mat.n = 0;
    mat.m = 0;
    mat.scalars = 0;
  }
  else {
    mat = matrix_generate(mat1.m, mat2.n, 0);
    unsigned scalars_count = mat1.m * mat2.n;
    unsigned distribution[thread_count];
    matrix_get_thread_scalars_distribution(scalars_count, thread_count, distribution);
    //unsigned scalar_start = 0;
    unsigned scalar_start = distribution[0];
    pthread_t threads[thread_count];
    threads[0] = 0;

    for (unsigned thread_number = 1; thread_number < thread_count; ++thread_number) {
      unsigned scalars_count = distribution[thread_number];
      if (scalars_count > 0) {
        threads[thread_number] = matrix_mult_parallel_launch_thread(mat1, mat2, mat, scalar_start, scalars_count, thread_number, 1);
        scalar_start += scalars_count;
      }
      else {
        threads[thread_number] = 0;
      }
    }

    matrix_mult_parallel_launch_thread(mat1, mat2, mat, 0, distribution[0], 0, 0);

    for (unsigned thread_number = 0; thread_number < thread_count; ++thread_number) {
      pthread_t thread = threads[thread_number];
      if (thread != 0) {
        pthread_join(thread, 0);
      }
    }
  }
  return mat;
}

struct timespec get_time()
{
    struct timespec start_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time);
    return start_time;
}

struct timespec time_diff(struct timespec* ts1, struct timespec* ts2)
{
  static struct timespec ts;
  ts.tv_sec = ts1->tv_sec - ts2->tv_sec;
  ts.tv_nsec = ts1->tv_nsec - ts2->tv_nsec;
  if (ts.tv_nsec < 0) {
    ts.tv_sec--;
    ts.tv_nsec += 1000000000;
  }
  return ts;
}

struct timespec get_duration(struct timespec* ts)
{
  struct timespec time = get_time();
  return time_diff(&time, ts);
}

void print_time(struct timespec* ts)
{
  long ns = ts->tv_nsec % 1000;
  long us = (ts->tv_nsec / 1000) % 1000;
  long ms = (ts->tv_nsec / 1000000) % 1000;
  long s =  (ts->tv_nsec / 1000000000) % 1000 + ts->tv_sec;
  long t = (s * 1000000000) + (ms * 1000000) + (us * 1000) + ns;
  printf("%3lds %3ldms %3ldus %3ldns %12ld", s, ms, us, ns, t);
}

void test(unsigned size, unsigned thread_count)
{
  s_matrix mat1 = matrix_generate(size, size, 100);

  s_matrix mat;
  struct timespec start = get_time();
  if (thread_count == 0) {
    mat =  matrix_mult_sequential(mat1, mat1);
  }
  else {
    mat = matrix_mult_parallel(mat1, mat1, thread_count);
  }
  struct timespec time = get_duration(&start);
  printf("%3dcpu %3dthreads %4d*%-4d ", get_cpu_count(), thread_count, size, size);
  print_time(&time);
  printf("\n");
  fflush(stdout);
  matrix_free(mat);

}

void check()
{
  s_matrix mat = matrix_generate(3, 3, 0);
  mat.scalars[0][0] = 25;
  mat.scalars[0][1] = 26;
  mat.scalars[0][2] = 90;
  mat.scalars[1][0] = 14;
  mat.scalars[1][1] = 36;
  mat.scalars[1][2] = 1;
  mat.scalars[2][0] = 3;
  mat.scalars[2][1] = 9;
  mat.scalars[2][2] = 6;

  s_matrix mat1 = matrix_mult_sequential(mat, mat);
  s_matrix mat2 = matrix_mult_parallel(mat, mat, 1);
  s_matrix mat3 = matrix_mult_parallel(mat, mat, get_cpu_count());
  if (!matrix_equals(mat1, mat2) || !matrix_equals(mat1, mat3)) {
    matrix_print(mat1);
    printf("\n");
    matrix_print(mat2);
    printf("\n");
    matrix_print(mat3);
    exit(1);
  }
}

int main(void)
{
  srand(time(0));

  check();

  unsigned sizes[] = {10, 100, 1000, 2000, 5000};
  unsigned threads_count = get_cpu_count();
  if (threads_count < 64) {
    threads_count = 64;
  }

  for (unsigned s = 0; s < sizeof(sizes) / sizeof(*sizes); ++s) {
    unsigned size = sizes[s];
    for (unsigned t = threads_count; t > 1; t /= 2) {
      test(size, t);
    }
    test(size, 0);
  }

  return 0;
}
