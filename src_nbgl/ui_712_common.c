#include "ui_nbgl.h"
#include "ui_712_common.h"
#include "common_712.h"

static void (*g_resume_func)(void) = NULL;

void nbgl_712_review_approve(void) {
    ui_712_approve_cb(NULL);
}

void nbgl_712_review_reject(void) {
    ui_712_reject_cb(NULL);
}

void nbgl_712_confirm_rejection_cb(bool confirm) {
    if (confirm) {
        nbgl_useCaseStatus("Message signing\ncancelled", false, nbgl_712_review_reject);
    } else {
        (*g_resume_func)();
    }
}

void nbgl_712_confirm_rejection(void) {
    nbgl_useCaseChoice(&C_warning64px,
                       "Reject message?",
                       NULL,
                       "Yes, reject",
                       "Go back to message",
                       nbgl_712_confirm_rejection_cb);
}

void nbgl_712_review_choice(bool confirm) {
    if (confirm) {
        nbgl_useCaseStatus("MESSAGE\nSIGNED", true, nbgl_712_review_approve);
    } else {
        nbgl_712_confirm_rejection();
    }
}

void nbgl_712_start(void (*resume_func)(void), const char *title) {
    g_resume_func = resume_func;
    nbgl_useCaseReviewStart(&C_Message_64px,
                            title,
                            NULL,
                            "Reject",
                            resume_func,
                            nbgl_712_confirm_rejection);
}
