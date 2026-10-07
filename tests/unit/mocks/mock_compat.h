#pragma once
#include "unity.h"
#include <stdint.h>
#include <string.h>

/* CMocka-compatible shim for Unity.
 *
 * CMocka maintains per-function return queues: will_return(f, v) pushes
 * to f's private queue and mock() inside f pops from it. That means push
 * order does NOT need to match call interleaving across different functions.
 *
 * This shim reproduces the same semantics using a small table of per-name
 * FIFOs keyed on the stringified function name.  will_return(f, v) pushes
 * to the "#f" bucket; mock() / mock_type(T) use __func__ to find the right
 * bucket at pop time.
 */

#define MOCK_QUEUE_SIZE 64
#define MOCK_FUNC_COUNT 16

/* ---- Per-function mock return queues ----------------------------------- */

typedef struct {
    const char *name; /* compile-time string literal from #func  */
    uintptr_t q[MOCK_QUEUE_SIZE];
    int head, tail;
} _mock_fq_t;

static _mock_fq_t _fq_table[MOCK_FUNC_COUNT];
static int _fq_count = 0;

static inline _mock_fq_t *_fq_get(const char *name) {
    for (int i = 0; i < _fq_count; i++) {
        if (strcmp(_fq_table[i].name, name) == 0) return &_fq_table[i];
    }
    TEST_ASSERT_MESSAGE(_fq_count < MOCK_FUNC_COUNT,
                        "mock_compat: too many distinct mocked functions "
                        "(increase MOCK_FUNC_COUNT)");
    _fq_table[_fq_count].name = name;
    _fq_table[_fq_count].head = 0;
    _fq_table[_fq_count].tail = 0;
    return &_fq_table[_fq_count++];
}

static inline void _mock_push_for(const char *name, uintptr_t val) {
    _mock_fq_t *fq = _fq_get(name);
    TEST_ASSERT_MESSAGE(fq->tail - fq->head < MOCK_QUEUE_SIZE, "mock queue full");
    fq->q[fq->tail++ % MOCK_QUEUE_SIZE] = val;
}

static inline uintptr_t _mock_pop_for(const char *name) {
    _mock_fq_t *fq = _fq_get(name);
    TEST_ASSERT_MESSAGE(fq->head < fq->tail, "mock queue empty — unexpected mock() call");
    return fq->q[fq->head++ % MOCK_QUEUE_SIZE];
}

/* CMocka-compatible macros — func arg is stringified to key the bucket   */
#define will_return(func, val) _mock_push_for(#func, (uintptr_t) (val))
#define mock()                 ((uintptr_t) _mock_pop_for(__func__))
#define mock_type(type)        ((type) _mock_pop_for(__func__))

/* ---- Parameter expectation queue --------------------------------------- */

typedef enum {
    _CHECK_INT, /* integer / pointer equality */
    _CHECK_STR, /* strcmp equality             */
    _CHECK_MEM, /* memcmp equality (ptr+len)  */
    _CHECK_ANY  /* unconditional pass          */
} _check_kind_t;

typedef struct {
    uintptr_t ival;
    const char *sval;
    const void *mptr;
    size_t mlen;
    _check_kind_t kind;
} _check_entry_t;

static _check_entry_t _check_queue[MOCK_QUEUE_SIZE];
static int _check_q_head = 0;
static int _check_q_tail = 0;

static inline void _check_push_int(uintptr_t val) {
    TEST_ASSERT_MESSAGE(_check_q_tail - _check_q_head < MOCK_QUEUE_SIZE, "check queue full");
    _check_queue[_check_q_tail % MOCK_QUEUE_SIZE].ival = val;
    _check_queue[_check_q_tail % MOCK_QUEUE_SIZE].sval = NULL;
    _check_queue[_check_q_tail % MOCK_QUEUE_SIZE].kind = _CHECK_INT;
    _check_q_tail++;
}

static inline void _check_push_str(const char *str) {
    TEST_ASSERT_MESSAGE(_check_q_tail - _check_q_head < MOCK_QUEUE_SIZE, "check queue full");
    _check_queue[_check_q_tail % MOCK_QUEUE_SIZE].ival = 0;
    _check_queue[_check_q_tail % MOCK_QUEUE_SIZE].sval = str;
    _check_queue[_check_q_tail % MOCK_QUEUE_SIZE].kind = _CHECK_STR;
    _check_q_tail++;
}

static inline void _check_push_mem(const void *ptr, size_t len) {
    TEST_ASSERT_MESSAGE(_check_q_tail - _check_q_head < MOCK_QUEUE_SIZE, "check queue full");
    _check_queue[_check_q_tail % MOCK_QUEUE_SIZE].mptr = ptr;
    _check_queue[_check_q_tail % MOCK_QUEUE_SIZE].mlen = len;
    _check_queue[_check_q_tail % MOCK_QUEUE_SIZE].kind = _CHECK_MEM;
    _check_q_tail++;
}

static inline void _check_push_any(void) {
    TEST_ASSERT_MESSAGE(_check_q_tail - _check_q_head < MOCK_QUEUE_SIZE, "check queue full");
    _check_queue[_check_q_tail % MOCK_QUEUE_SIZE].kind = _CHECK_ANY;
    _check_q_tail++;
}

static inline _check_entry_t _check_pop(void) {
    TEST_ASSERT_MESSAGE(_check_q_head < _check_q_tail,
                        "check queue empty — unexpected check_expected() call");
    return _check_queue[_check_q_head++ % MOCK_QUEUE_SIZE];
}

/* CMocka-compatible macros */
#define expect_value(func, param, val)       _check_push_int((uintptr_t) (val))
#define expect_string(func, param, str)      _check_push_str((const char *) (str))
#define expect_memory(func, param, mem, len) _check_push_mem((const void *) (mem), (size_t) (len))
#define expect_any(func, param)              _check_push_any()

#define check_expected(actual)                                             \
    do {                                                                   \
        _check_entry_t _e = _check_pop();                                  \
        if (_e.kind == _CHECK_INT) {                                       \
            TEST_ASSERT_EQUAL_UINT64(_e.ival, (uintptr_t) (actual));       \
        } else if (_e.kind == _CHECK_STR) {                                \
            TEST_ASSERT_EQUAL_STRING(_e.sval, (const char *) (actual));    \
        }                                                                  \
        /* _CHECK_MEM and _CHECK_ANY: use check_expected_ptr for memory */ \
    } while (0)

#define check_expected_ptr(actual)                                      \
    do {                                                                \
        _check_entry_t _e = _check_pop();                               \
        if (_e.kind == _CHECK_INT) {                                    \
            TEST_ASSERT_EQUAL_PTR((void *) _e.ival, (void *) (actual)); \
        } else if (_e.kind == _CHECK_STR) {                             \
            TEST_ASSERT_EQUAL_STRING(_e.sval, (const char *) (actual)); \
        } else if (_e.kind == _CHECK_MEM) {                             \
            TEST_ASSERT_EQUAL_MEMORY(_e.mptr, (actual), _e.mlen);       \
        }                                                               \
        /* _CHECK_ANY: no comparison */                                 \
    } while (0)

/* ---- Combined reset (call from setUp when using mock_compat.h) --------- */

static inline void _mock_all_reset(void) {
    memset(_fq_table, 0, sizeof(_fq_table));
    _fq_count = 0;
    _check_q_head = 0;
    _check_q_tail = 0;
}
