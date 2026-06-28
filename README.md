# Lock-Free Stack: Implementation Progression

A step-by-step exploration of lock-free stack design in C++17, from a mutex baseline through memory-order optimisation and safe memory reclamation.

## Machine

| Property | Value |
|---|---|
| CPU | Apple M1 |
| Physical / logical cores | 8 / 8 |
| Cache line | **128 bytes** |
| L1-D | 64 KiB per core |
| L2 | 4 MiB per core |
| Compiler | clang++ -std=c++17 -O2 |

---

## Implementations

### `blocking_stack.hpp` — mutex baseline

A `std::mutex`-guarded linked list. Simple, correct, immediate `delete` of popped nodes.  
Used as the performance reference point.

---

### `lf_leaking_seq_cst.hpp` — lock-free, seq_cst, leaking

First lock-free attempt. `head` is `std::atomic<Node*>`; push and pop use `compare_exchange_weak` with default `memory_order_seq_cst`.

**Issues:**
- `seq_cst` emits full memory fences on every CAS — stronger than needed for this data structure.
- Popped nodes are **never deleted** (memory leak).

---

### `lf_leaking_acq_rel.hpp` — lock-free, acq/rel, leaking

Relaxed the memory orders to the minimum required for correctness:

```cpp
// push
Node *newNode = new Node{std::move(value), head.load(std::memory_order_acquire)};
while (!head.compare_exchange_weak(newNode->next, newNode,
                                   std::memory_order_release,   // success: publishes the new node
                                   std::memory_order_relaxed))  // failure: only needs the pointer value
  ;

// pop
Node *to_delete = head.load(std::memory_order_acquire);
while (!head.compare_exchange_weak(
    to_delete, to_delete ? to_delete->next : nullptr,
    std::memory_order_acquire,   // success: synchronises with the push that wrote next
    std::memory_order_acquire))  // failure: needed to correctly read next of a newly-pushed node
  ;
```

**Why acquire on pop failure?** If a new node D is pushed while the pop loop is spinning, the failure path updates `to_delete = D`. To read `D->next` correctly on the next iteration, we need to synchronise with D's push. A relaxed failure load gives no such guarantee.

Still leaks memory.

---

### `lf_reclaim_seq_cst.hpp` — reclamation, seq_cst

Adds reference-counted safe reclamation:
- `counter` tracks threads currently inside `pop()`.
- Popped nodes go to `delete_list` instead of being freed immediately — we cannot `delete` straight away because another thread may have loaded the same node from `head` just before our CAS succeeded and is still holding a pointer to it.
- When `counter == 1` (only the current thread is in `pop()`), no other thread can hold a stale pointer, so draining `delete_list` is safe.

**Known liveness issue:** there is a window between `delete_list.exchange(nullptr)` and `counter.fetch_sub` where a new thread can enter, add a node to the freshly-emptied `delete_list`, observe `counter > 1`, and exit without freeing — orphaning that node until the destructor.

Memory orders still `seq_cst`.

---

### `lf_reclaim_acq_rel.hpp` — reclamation, acq/rel

Same reclamation logic with `seq_cst` replaced by `acq/rel` throughout. `seq_cst` and `acq/rel` produce nearly identical numbers on this benchmark.

---

### `lf_reclaim_ar_padded.hpp` — reclamation, acq/rel, cache-line padded

`head` is declared `alignas(128)` to place it on its own cache line.

**Why only `head`?** `push()` only touches `head`. Without alignment, a push on one thread can invalidate the cache line containing `counter` and `delete_list` on another thread mid-pop, even though push never writes to those variables. Aligning `head` to its own line prevents push from causing spurious invalidations of the pop-side state.

**Why not align all three?** We tried. With `counter` and `delete_list` each on their own 128-byte line the numbers are consistently worse (see benchmark). `pop()` accesses all three atomics in tight sequence; keeping `counter` and `delete_list` co-located preserves spatial locality for that access pattern. The exact mechanism is still under investigation.

---

## Benchmark

Each benchmark calls `push` then `pop` in a tight loop per thread on a shared static stack instance. Time shown is **wall-clock ns per push+pop pair per thread**.

```cpp
static void BM_Reclaim_AR(benchmark::State &state) {
  static reclaim_ar::LockFreeStack<int> s;
  for (auto _ : state) {
    s.push(state.thread_index());
    benchmark::DoNotOptimize(s.pop());
  }
}
BENCHMARK(BM_Reclaim_AR)->Threads(1)->Threads(2)->Threads(4)->Threads(8)->Threads(16);
```

```
CPU: Apple M1, clang++ -O2, 2026-06-28 (averaged over 5 runs)
-------------------------------------------------------------------------
Benchmark                           1T       2T       4T       8T      16T
-------------------------------------------------------------------------
BM_Leaking_SC                     22.4      100      292     1897     3799
BM_Leaking_AR                     21.3       93      345     2061     3801
BM_Reclaim_SC                     42.2      210      615     3333     6485
BM_Reclaim_AR                     40.1      202      585     3227     6063
BM_Reclaim_AR_Padded              40.0      210      575     3053     6013
BM_Blocking                       30.6      123      313      956     2238
-------------------------------------------------------------------------
(all times in nanoseconds)

All-3-padded experiment (alignas(128) on head + counter + delete_list, 5 runs):
BM_Reclaim_AR_Padded              40.2      201      625     3732     7002
```

**Observations:**

- **Single thread:** leaking variants are fastest (~21 ns) — no reclamation overhead. Blocking pays mutex acquire/release (~30 ns). Reclaim variants pay the `counter` fetch-add/sub and `delete_list` CAS (~40 ns).

- **seq_cst vs acq/rel:** a visible gap appears at 16 threads — leaking_sc averages 3701 ns, leaking_ar averages 3525 ns (~5% faster). `seq_cst` emits a fence not present in `acq/rel`; under high thread counts the cost accumulates.

- **Reclamation cost:** roughly 2× the leaking variant at single thread, growing with thread count due to contention on `counter` and `delete_list`.

- **Padding gain at 4–8T:** head-only `alignas(128)` gives ~5% at 4T and ~1% at 8T over the unpadded reclaim variant. Aligning all three atomics separately is consistently worse — spatial locality for pop()'s sequential access to all three outweighs the false-sharing reduction.

- **Blocking beats lock-free at scale:** the mutex outperforms every lock-free variant above 2 threads in this benchmark. Under maximum contention the OS sleep/wake in the mutex eliminates the CAS retry storm. This is a known characteristic of tight push/pop microbenchmarks. WIP — investigating whether this holds under more realistic producer/consumer access patterns.

---

## What is missing

**Correctness:**
- **Reclamation liveness bug:** nodes can be orphaned in `delete_list` between `exchange(nullptr)` and `counter.fetch_sub` as described above. A proper fix requires a different reclamation scheme (hazard pointers, epoch-based reclamation).
- **Sanitizer runs:** the correctness test passes under `assert` but has not been run under ThreadSanitizer (`-fsanitize=thread`) or AddressSanitizer (`-fsanitize=address`). To be added.
