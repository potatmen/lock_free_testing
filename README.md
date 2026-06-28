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

A `std::mutex`-guarded linked list. Simple, correct, no ABA risk, immediate `delete` of popped nodes.  
Used as the performance reference point.

---

### `lf_leaking_seq_cst.hpp` — lock-free, seq_cst, leaking

First lock-free attempt. `head` is `std::atomic<Node*>`; push and pop use `compare_exchange_weak` with default `memory_order_seq_cst`.

**Issues:**
- `seq_cst` emits full memory fences on every CAS — unnecessary for a single shared pointer.
- Popped nodes are **never deleted** (memory leak).

---

### `lf_leaking_acq_rel.hpp` — lock-free, acq/rel, leaking

Relaxed the memory orders:
- Push CAS: `release` on success, `relaxed` on failure — publishes the new node.
- Pop CAS: `acquire` on success (synchronises with the push that wrote the node's `next`), `acquire` on failure (needed to correctly read `next` of any newly-pushed node encountered during retries).

**Why acquire on failure?** If a new node D is pushed while the pop loop is spinning, the failure path updates `to_delete = D` with no ordering. A subsequent relaxed read of `D->next` would be unsynchronised with D's push on weakly-ordered architectures (ARM, POWER). Keeping `acquire` on failure covers this.

Still leaks memory.

---

### `lf_reclaim_seq_cst.hpp` — reclamation, seq_cst

Adds reference-counted safe reclamation:
- `counter` tracks threads currently inside `pop()`.
- Popped nodes go to `delete_list` instead of being freed immediately.
- When `counter == 1` (only the current thread is in `pop()`), it is safe to drain `delete_list` — no other thread holds a pointer into it.

Memory orders still `seq_cst`.

**Known liveness issue:** there is a window between `delete_list.exchange(nullptr)` and `counter.fetch_sub` where a new thread can enter, add a node to the freshly-emptied `delete_list`, observe `counter > 1`, and exit without freeing. That node stays in `delete_list` until the next time a thread is alone (`counter == 1`) or until the destructor runs. Under sustained saturation the list can grow unboundedly. Production systems use hazard pointers or epoch-based reclamation to avoid this.

---

### `lf_reclaim_acq_rel.hpp` — reclamation, acq/rel

Switched reclamation variant to acq/rel orders. `seq_cst` and `acq/rel` produce nearly identical numbers on this benchmark (M1's load-acquire / store-release map directly to `ldar`/`stlr`; `seq_cst` adds `dmb` barriers that the tight loop doesn't expose).

---

### `lf_reclaim_ar_padded.hpp` — reclamation, acq/rel, cache-line padded

`head` is declared `alignas(128)` — matching the M1's actual cache line size — to prevent it from sharing a cache line with unrelated data in surrounding objects.

**Remaining gap:** `counter` and `delete_list` immediately follow `head` in memory (offsets +8 and +16 within the same 128-byte line). All three hot atomics are still on one cache line. A complete fix gives each its own line:

```cpp
alignas(128) std::atomic<Node*> head{nullptr};
alignas(128) std::atomic<int>   counter{0};
alignas(128) std::atomic<Node*> delete_list{nullptr};
```

The improvement visible at 8–16 threads reflects that `head` no longer shares a line with surrounding object data, but contention between `head`, `counter`, and `delete_list` themselves is unresolved.

---

## Benchmark results

Each benchmark calls `push` then `pop` in a tight loop per thread. Time shown is **wall-clock ns per push+pop pair per thread**.

```
CPU: Apple M1, clang++ -O2, 2026-06-28 (averaged over 3 runs)
-------------------------------------------------------------------------
Benchmark                           1T       2T       4T       8T      16T
-------------------------------------------------------------------------
BM_Leaking_SC                     22.9       93      280     2244     4286
BM_Leaking_AR                     21.4       91      352     2145     3753
BM_Reclaim_SC                     42.0      209      610     3248     6400
BM_Reclaim_AR                     40.8      209      588     3267     6071
BM_Reclaim_AR_Padded              40.2      209      556     2854     6276
BM_Blocking                       30.7      124      309      945     2338
-------------------------------------------------------------------------
(all times in nanoseconds)
```

**Observations:**

- **Single thread:** leaking variants are fastest (~21 ns) — no reclamation overhead. Blocking pays mutex lock/unlock (~30 ns). Reclaim variants pay the `counter` fetch-add/sub and `delete_list` CAS (~41 ns).

- **seq_cst vs acq/rel:** the averaged results show a visible gap at 16 threads between leaking_sc (4286 ns) and leaking_ar (3753 ns, ~12% faster). On M1, `seq_cst` CAS emits a `dmb ish` barrier not present in `acq/rel`; under high thread counts that barrier cost accumulates. On x86 the difference would be larger because `seq_cst` stores require `MFENCE` or `LOCK XCHG`.

- **Reclamation cost:** roughly 2× the leaking variant at single thread; overhead grows with thread count due to contention on `counter` and `delete_list`.

- **Padding gain at 8T:** `alignas(128)` reduces per-op latency by ~13% at 8 threads (2854 ns vs 3267 ns). The gain is consistent across runs because `head` is no longer invalidated by writes to neighbouring fields in surrounding objects.

- **Padding at 16T:** the benefit disappears (6276 ns padded vs 6071 ns unpadded, within noise). At 16 threads all cores are hammering `counter` and `delete_list`, which still share the same 128-byte line as `head`. Separating all three atomics onto individual lines is the next required step.

- **Blocking beats lock-free at scale:** the mutex outperforms every lock-free variant above 2 threads in this synthetic benchmark. The tight push/pop loop creates maximum contention; under such conditions the OS sleep/wake in the mutex eliminates the CAS retry storm that the lock-free variants suffer. This is a known pathology of purely contention-bound microbenchmarks — real workloads with non-trivial critical sections or SPSC access patterns favour the lock-free approach.

---

## What is missing / next improvements

**Correctness:**
- **Reclamation liveness bug:** nodes added to `delete_list` during the window between `delete_list.exchange(nullptr)` and `counter.fetch_sub` are orphaned until the destructor. Under sustained concurrent load, `delete_list` can grow without bound. Fix requires hazard pointers or epoch-based reclamation (e.g. `std::hazard_pointer` from C++26, or a manual EBR implementation).
- **No sanitizer runs:** the correctness test passes under `assert`, but the code has never been run under ThreadSanitizer (`-fsanitize=thread`) or AddressSanitizer (`-fsanitize=address`). TSan would catch memory-order races on non-x86 memory models; ASan would catch use-after-free in the reclamation path.

**Performance:**
- **Full cache-line isolation:** `counter` and `delete_list` share the same 128-byte line as `head`. Each should be `alignas(128)` to eliminate remaining false-sharing between them under high thread counts.
- **Shared `delete_list` is a bottleneck:** every thread contends on the same `delete_list` CAS. Per-thread pending lists (merged on drain) would reduce this to a single-writer path per thread.
- **Heap allocator contention:** `new`/`delete` under concurrent load serialises on the allocator's internal lock. A thread-local free-list or a lock-free slab allocator (e.g. jemalloc, mimalloc) would remove this hidden bottleneck, which currently dominates at high thread counts.
- **Benchmark models only worst-case contention:** all threads push and pop on the same stack simultaneously. A separate-producer/consumer benchmark (SPSC or bounded MPMC) would better reflect real workload access patterns where the lock-free advantage over mutex is more pronounced.
