#ifndef UI_712_COMMON_H_
#define UI_712_COMMON_H_

#include <stdbool.h>

void nbgl_712_approve(void);
void nbgl_712_reject(void);
void nbgl_712_confirm_rejection_cb(bool confirm);
void nbgl_712_confirm_rejection(void);
void nbgl_712_review_choice(bool confirm);
void nbgl_712_start(void (*resume_func)(void), const char *title);

#endif  // UI_712_COMMON_H_
