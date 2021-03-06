#include "bench_fabric.hpp"
#include "thread_utils.hpp"
#include "comm_exp.hpp"
#include <atomic>
#include <thread>
#include <vector>

using namespace fb;

int tx_thread_num = 3;
int rx_thread_num = 1;
int min_size = 8;
int max_size = 8192;
bool touch_data = false;
bool bind_prg_thread = true;
int rank, size, target_rank;
device_t device;
cq_t *tx_cqs;
cq_t *rx_cqs;
ctx_t *tx_ctxs;
ctx_t *rx_ctxs;
addr_t *addrs;
std::atomic<int> *syncs; // TODO： fix false sharing
std::atomic<bool> thread_stop = {false};
std::atomic<int> thread_started = {0};

void* send_thread(void* arg) {
    int thread_id = omp::thread_id();
    int thread_count = omp::thread_count();
    addr_t& addr = addrs[thread_id % rx_thread_num];
    ctx_t& tx_ctx = tx_ctxs[thread_id];
    char *s_buf = (char*) device.heap_ptr + thread_id * 2 * max_size;
    char *r_buf = (char*) device.heap_ptr + (thread_id * 2 + 1) * max_size;
    char s_data = rank * thread_count + thread_id;
    char r_data = target_rank * thread_count + thread_id;
    req_t req = {REQ_TYPE_NULL};

    RUN_VARY_MSG({min_size, max_size}, (rank == 0 && thread_id == 0), [&](int msg_size, int iter) {
        if (touch_data) write_buffer(s_buf, msg_size, s_data);
        isend_tag(tx_ctx, s_buf, msg_size, addr, thread_id, &req);
        while (req.type != REQ_TYPE_NULL) continue;
        while (syncs[thread_id] == 0) continue;
        syncs[thread_id] = 0;
        if (touch_data) check_buffer(r_buf, msg_size, r_data);
    }, {rank % (size / 2) * thread_count + thread_id, (size / 2) * thread_count});

    return nullptr;
}

void* recv_thread(void* arg) {
    int thread_id = omp::thread_id();
    int thread_count = omp::thread_count();
    addr_t& addr = addrs[thread_id % rx_thread_num];
    ctx_t& tx_ctx = tx_ctxs[thread_id];
    char *s_buf = (char*) device.heap_ptr + thread_id * 2 * max_size;
    char *r_buf = (char*) device.heap_ptr + (thread_id * 2 + 1) * max_size;
    char s_data = rank * thread_count + thread_id;
    char r_data = target_rank * thread_count + thread_id;
    req_t req = {REQ_TYPE_NULL};

    RUN_VARY_MSG({min_size, max_size}, (rank == 0 && thread_id == 0), [&](int msg_size, int iter) {
        while (syncs[thread_id] == 0) continue;
        syncs[thread_id] = 0;
        if (touch_data) check_buffer(r_buf, msg_size, r_data);
        if (touch_data) write_buffer(s_buf, msg_size, s_data);
        isend_tag(tx_ctx, s_buf, msg_size, addr, thread_id, &req);
        while (req.type != REQ_TYPE_NULL) continue;
    }, {rank % (size / 2) * thread_count + thread_id, (size / 2) * thread_count});

    return nullptr;
}

void progress_thread(int id) {
    int spin = 64;
    int core = 0;
    if (getenv("FB_SCORE"))
        core = atoi(getenv("FB_SCORE"));
    if (bind_prg_thread)
        comm_set_me_to(core + 2*id); // only for hyper-threaded. FIXME.

    ctx_t& rx_ctx = rx_ctxs[id];
    req_t *reqs = (req_t*) calloc(tx_thread_num, sizeof(req_t));
    for (int i = id; i < tx_thread_num; i += rx_thread_num) {
        char *buf = (char*) device.heap_ptr + (2 * i + 1) * max_size;
        irecv_tag(rx_ctx, buf, max_size, ADDR_ANY, i, &reqs[i]);
    }
    thread_started++;
    while (!thread_stop.load()) {
        for (int j = id; j < tx_thread_num; j += rx_thread_num)
            progress(tx_cqs[j]);
        bool ret = progress(rx_cqs[id]);
        if (ret) {
            for (int i = id; i < tx_thread_num; i += rx_thread_num) {
                if (reqs[i].type == REQ_TYPE_NULL) {
                    syncs[i] = 1;
                    char *buf = (char*) device.heap_ptr + (2 * i + 1) * max_size;
                    irecv_tag(rx_ctx, buf, max_size, ADDR_ANY, i, &reqs[i]);
                    break;
                }
            }
        }
        if (spin-- == 0) { sched_yield(); spin = 64; }
    }
    free(reqs);
}

int main(int argc, char *argv[]) {
    if (argc > 1)
        tx_thread_num = atoi(argv[1]);
    if (argc > 2)
        rx_thread_num = atoi(argv[2]);
    if (argc > 3)
        min_size = atoi(argv[3]);
    if (argc > 4)
        max_size = atoi(argv[4]);
    if (argc > 5)
        touch_data = atoi(argv[5]);
    if (argc > 6)
        bind_prg_thread = atoi(argv[6]);

    if (tx_thread_num * 2 * max_size > HEAP_SIZE){
        printf("HEAP_SIZE is too small! (%d < %d required)\n", HEAP_SIZE, tx_thread_num * 2 * max_size);
        exit(1);
    }
    comm_init();
    init_device(&device, true);
    rank = pmi_get_rank();
    size = pmi_get_size();
    target_rank = (rank + size / 2) % size;

    tx_cqs = (cq_t*) calloc(tx_thread_num, sizeof(cq_t));
    rx_cqs = (cq_t*) calloc(rx_thread_num, sizeof(cq_t));
    tx_ctxs = (ctx_t*) calloc(tx_thread_num, sizeof(ctx_t));
    rx_ctxs = (ctx_t*) calloc(rx_thread_num, sizeof(ctx_t));
    addrs = (addr_t*) calloc(rx_thread_num, sizeof(addr_t));
    for (int i = 0; i < rx_thread_num; ++i) {
        init_cq(device, &rx_cqs[i]);
        init_ctx(&device, rx_cqs[i], &rx_ctxs[i], CTX_RX);
        put_ctx_addr(rx_ctxs[i], i);
    }
    for (int i = 0; i < tx_thread_num; ++i) {
        init_cq(device, &tx_cqs[i]);
        init_ctx(&device, tx_cqs[i], &tx_ctxs[i], CTX_TX);
    }
    flush_ctx_addr();
    for (int i = 0; i < rx_thread_num; ++i) {
        get_ctx_addr(device, target_rank, i, &addrs[i]);
    }

    syncs = (std::atomic<int>*) calloc(tx_thread_num, sizeof(std::atomic<int>));
    for (int i = 0; i < tx_thread_num; ++i) {
        syncs[i] = 0;
    }

    std::vector<std::thread> threads(rx_thread_num);
    for (int id = 0; id < rx_thread_num; id++) {
        threads[id] = std::thread(progress_thread, id);
    }
    while (thread_started.load() != rx_thread_num) continue;

    if (rank < size / 2) {
        omp::thread_run(recv_thread, tx_thread_num);
    } else {
        omp::thread_run(send_thread, tx_thread_num);
    }
    thread_stop = true;
    for (int id = 0; id < rx_thread_num; id++) {
        threads[id].join();
    }

    for (int i = 0; i < tx_thread_num; ++i) {
        free_ctx(&tx_ctxs[i]);
        free_cq(&tx_cqs[i]);
    }
    for (int i = 0; i < rx_thread_num; ++i) {
        free_ctx(&rx_ctxs[i]);
        free_cq(&rx_cqs[i]);
    }
    free_device(&device);
    free(syncs);
    free(tx_cqs);
    free(rx_cqs);
    free(tx_ctxs);
    free(rx_ctxs);
    free(addrs);
    comm_free();
    return 0;
}


