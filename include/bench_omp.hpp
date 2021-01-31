#ifndef FABRICBENCH_BENCH_OMP_HPP
#define FABRICBENCH_BENCH_OMP_HPP

#include <omp.h>

namespace omp {

typedef void* (*func_t)(void*);

static inline void thread_run(func_t f, int n)
{
  #pragma omp parallel num_threads(n)
  {
      f((void*)0);
  }
}

static inline int thread_id()
{
  return omp_get_thread_num();
}

static inline int thread_count()
{
  return omp_get_num_threads();
}

static inline void thread_barrier()
{
  #pragma omp barrier
}

}
#endif