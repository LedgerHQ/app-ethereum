#ifndef UI_MESSAGE_SIGNING_H_
#define UI_MESSAGE_SIGNING_H_

#include <stdbool.h>

void ui_message_review_choice(bool confirm);
void ui_message_start(const char *title,
                      void (*start_func)(void),
                      void (*resume_func)(void),
                      void (*approved_func)(void),
                      void (*rejected_func)(void));

void ui_message_712_approved(void);
void ui_message_712_rejected(void);

#endif  // UI_MESSAGE_SIGNING_H_
