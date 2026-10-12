#pragma once

#include "dynamips_common.h"

typedef m_uint64_t timer_id;
typedef struct timer_entry timer_entry_t;
typedef int (*timer_proc)(void *, timer_entry_t *);

#ifdef __cplusplus
extern "C" {
#endif

void timer_flush_queues(void);
int timer_init(void);

/* Intervals and offsets are in milliseconds; zero IDs indicate failure. */
timer_id timer_create_entry(m_tmcnt_t interval, int boundary, int level,
                            timer_proc callback, void *user_arg);
timer_id timer_create_with_offset(m_tmcnt_t interval, m_tmcnt_t offset,
                                  int level, timer_proc callback,
                                  void *user_arg);
int timer_set_interval(timer_id id, long interval);

/* Wait for an active callback to finish before returning. */
int timer_remove(timer_id id);

#ifdef __cplusplus
}
#endif
