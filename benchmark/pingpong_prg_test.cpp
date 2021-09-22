#include "bench_fabric.hpp"
#include "comm_exp.hpp"
#include "thread_utils.hpp"
#include "bench_ib.hpp"
#include <atomic>
#include <vector>
#include <thread>

#define _GNU_SOURCE // sched_getcpu(3) is glibc-specific (see the man page)

#include <sched.h>
using namespace fb;

int thread_num = 4;
int min_size = 8;
int max_size = 64 * 1024;
bool touch_data = false;
int rank, size, target_rank;
device_t device;
cq_t *cqs;
cq_t *rx_cqs;
ctx_t *ctxs;
addr_t *addrs;

std::atomic<int> *syncs; // TODO： fix false sharing
std::atomic<bool> thread_stop = {false};
std::atomic<int> thread_started = {0};

int rx_thread_num = 1;

void *send_thread(void *arg) {
    //printf("I am a send thread\n");
    int thread_id = omp::thread_id();
    int thread_count = omp::thread_count();
    int cpu_num = sched_getcpu();
    fprintf(stderr, "Thread %3d is running on CPU %3d\n", thread_id, cpu_num);
    ctx_t &ctx = ctxs[thread_id];
    char *s_buf = (char *) device.heap_ptr + thread_id * 2 * max_size;
    req_t req = {REQ_TYPE_NULL};
     printf("I am %d, sending msg. iter first is %d, iter second is %d\n", rank,
            (rank % (size / 2) * thread_count + thread_id),
            ((size / 2) * thread_count));
    int count = 0;
    RUN_VARY_MSG({min_size, max_size}, (rank == 0 && thread_id == 0), [&](int msg_size, int iter) {
//        if (rank == 0 && ++count % 2 == 0) {
//        }
        isend_tag(ctx, s_buf, msg_size, thread_id, &req);
        printf("rank %d, sent msg. waiting for progress thread to unblock. thread: %d\n", rank, thread_id);
        while (req.type != REQ_TYPE_NULL) continue;
        printf("rank %d, send unblocked! iter: %d -- thread: %d\n", rank, iter, thread_id);
        while (syncs[thread_id] == 0) continue;
        --syncs[thread_id];
        printf("worker thread got msg from progress thread!\n");
        }, {rank % (size / 2) * thread_count + thread_id, (size / 2) * thread_count});

    return nullptr;
}

void *recv_thread(void *arg) {
    //printf("I am a recv thread\n");
    int thread_id = omp::thread_id();
    int thread_count = omp::thread_count();
    ctx_t &ctx = ctxs[thread_id];
    char *s_buf = (char *) device.heap_ptr + thread_id * 2 * max_size;
    req_t req = {REQ_TYPE_NULL};
    int cpu_num = sched_getcpu();
    fprintf(stderr, "Thread %3d is running on CPU %3d\n", thread_id, cpu_num);

     printf("I am %d, recving msg. iter first is %d, iter second is %d\n", rank,
            (rank % (size / 2) * thread_count + thread_id),
            ((size / 2) * thread_count));

    RUN_VARY_MSG({min_size, max_size}, (rank == 0 && thread_id == 0), [&](int msg_size, int iter) {
        printf("rank %d, waiting for progress thread to unblock recv. - thread: %d\n", rank, thread_id);
        while (syncs[thread_id] == 0) continue;
        printf("rank %d, recv unblocked! iter: %d -- thread: %d\n", rank, iter, thread_id);
        --syncs[thread_id];
        isend_tag(ctx, s_buf, msg_size, thread_id, &req);
        printf("rank %d, sent msg. waiting for progress thread to unblock. thread: %d\n", rank, thread_id);
        while (req.type != REQ_TYPE_NULL) continue;
        printf("rank %d, send unblocked! iter: %d -- thread: %d\n", rank, iter, thread_id);
        }, {rank % (size / 2) * thread_count + thread_id, (size / 2) * thread_count});

    return nullptr;
}

void progress_thread(int id) {
    int spin = 64;
    int core = 0;
    // todo: progress thread binding
//    if (getenv("FB_SCORE"))
//        core = atoi(getenv("FB_SCORE"));
    bool bind_prg_thread = true;
    std::vector<int> recv_count(rx_thread_num);
    if (bind_prg_thread) {
        // todo: currently bind to cpu 6; fix this when testing multiple progress thread
        auto err = comm_set_me_to(6);
        if (err) {
            errno = err;
            printf("setting progress threa affinity failed: error %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }
    }

    // Put the queue pairs to the correct states
    for (int i = id; i < thread_num; i += rx_thread_num) {
        connect_ctx(ctxs[i], addrs[i]);
    }
    int cpu_num = sched_getcpu();
    fprintf(stderr, "progress Thread is running on CPU %3d\n",  cpu_num);
    req_t *reqs = (req_t*) calloc(thread_num, sizeof(req_t));
    // Post receive requests
    for (int i = id; i < thread_num; i += rx_thread_num) {
        char *buf = (char*) device.heap_ptr + (2 * i + 1) * max_size;
        irecv_tag_srq(device, buf, max_size, i, &reqs[i]);
    }
    printf("progress thread id: %d\n", id);
    // Mark the thread as started
    thread_started++;
    while (!thread_stop.load()) {
        // progress each send completion
        for (int j = id; j < thread_num; j += rx_thread_num)
            progress(cqs[j], nullptr);
        // Progress the receives
        bool ret = progress(rx_cqs[id], reqs);
        if (ret) {
            for (int i = id; i < thread_num; i += rx_thread_num) {
                // zli89: when the progress thread receives certain message
                if (reqs[i].type == REQ_TYPE_NULL) {
                    printf("(progress thread) completed recv from rank %d - thread: %d \n", pmi_get_rank(), i);
                    // todo: the following shows the cause of deadlock and is observed in testing. Fixed
//                    if (syncs[i] == 1) {
//                        printf("(progress thread) found that previous recv hasn't been completed! %d - thread: %d \n", pmi_get_rank(), i);
//                        exit(5);
//                    }
                    ++syncs[i];
                    // todo: Don't post receive when the thread receives enough; not working; investigate why
                    if (++recv_count[i] < 10000 / rx_thread_num) {
                        char *buf = (char*) device.heap_ptr + (2 * i + 1) * max_size;
                        irecv_tag_srq(device, buf, max_size, i, &reqs[i]);
                    }
                    // todo: why breaking here? would this be the cause for the deadlock? shouldn't break as this isn't p2p
                    break;
                }
            }
        }
        // todo: remove this if every thread is on its own core
        if (spin-- == 0) { sched_yield(); spin = 64; }
    }
    free(reqs);
}

int main(int argc, char *argv[]) {
    if (argc > 1)
        thread_num = atoi(argv[1]);
    if (argc > 2)
        rx_thread_num = atoi(argv[2]);
    if (argc > 3)
        min_size = atoi(argv[3]);
    if (argc > 4)
        max_size = atoi(argv[4]);
    //printf("got all arguments");
    if (thread_num * 2 * max_size > HEAP_SIZE) {
        printf("HEAP_SIZE is too small! (%d < %d required)\n", HEAP_SIZE, thread_num * 2 * max_size);
        exit(1);
    }
    comm_init();
    init_device(&device, thread_num != 1);

    rank = pmi_get_rank();
    size = pmi_get_size();
    target_rank = (rank + size / 2) % size;
    // Send completion queues(per workers)
    cqs = (cq_t *) calloc(thread_num, sizeof(cq_t));
    // Send context (per workers)
    ctxs = (ctx_t *) calloc(thread_num, sizeof(ctx_t));
    addrs = (addr_t *) calloc(thread_num, sizeof(addr_t));
    // Receive completion queues(per progress thread)
    rx_cqs = (cq_t*) calloc(rx_thread_num, sizeof(cq_t));
    // Set up receive completion queue, one per progress thread
    for (int i = 0; i < rx_thread_num; ++i) {
        init_cq(device, &rx_cqs[i]);
    }
    for (int i = 0; i < thread_num; ++i) {
        init_cq(device, &cqs[i]);
        init_ctx(&device, cqs[i], rx_cqs[i % rx_thread_num], &ctxs[i]);
        put_ctx_addr(ctxs[i], i);
    }
    flush_ctx_addr();
    for (int i = 0; i < thread_num; ++i) {
        get_ctx_addr(device, target_rank, i, &addrs[i]);
    }

    // atomic flag(per worker) used by the progress thread to signal that the worker can read from the receive buf
    syncs = (std::atomic<int>*) calloc(thread_num, sizeof(std::atomic<int>));
    for (int i = 0; i < thread_num; ++i) {
        syncs[i] = 0;
    }
    std::vector<std::thread> threads(rx_thread_num);
    for (int id = 0; id < rx_thread_num; id++) {
        threads[id] = std::thread(progress_thread, id);
    }
    while (thread_started.load() != rx_thread_num) continue;
    printf("all progress thread ready, starting worker\n");
    if (rank < size / 2) {
        omp::thread_run(send_thread, thread_num);
    } else {
        omp::thread_run(recv_thread, thread_num);
    }
    thread_stop = true;
    for (int id = 0; id < rx_thread_num; id++) {
        threads[id].join();
    }
    // todo: fix resource cleanup error
    for (int i = 0; i < thread_num; ++i) {
        free_ctx(&ctxs[i]);
        free_cq(&cqs[i]);
    }
    free_device(&device);
    free(addrs);
    free(ctxs);
    free(cqs);
    free(rx_cqs);
    comm_free();
    return 0;
}
