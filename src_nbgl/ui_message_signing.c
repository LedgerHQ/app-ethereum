#include "ui_nbgl.h"
#include "ui_signing.h"
#include "common_712.h"
#include "ui_message_signing.h"
#include "glyphs.h"

static void (*g_resume_func)(void) = NULL;
static void (*g_approved_func)(void) = NULL;
static void (*g_rejected_func)(void) = NULL;

static void ui_message_rejection_handler(bool confirm) {
    if (confirm) {
        nbgl_useCaseStatus("Message signing\ncancelled", false, g_rejected_func);
    } else {
        (*g_resume_func)();
    }
}

static void ui_message_confirm_rejection(void) {
    nbgl_useCaseChoice(&C_warning64px,
                       REJECT_QUESTION(TEXT_MESSAGE),
                       NULL,
                       REJECT_CONFIRM_BUTTON,
                       RESUME(TEXT_MESSAGE),
                       ui_message_rejection_handler);
}

void ui_message_review_choice(bool confirm) {
    if (confirm) {
        nbgl_useCaseStatus("MESSAGE\nSIGNED", true, g_approved_func);
    } else {
        ui_message_confirm_rejection();
    }
}

void ui_message_start(const char *title,
                      void (*start_func)(void),
                      void (*resume_func)(void),
                      void (*approved_func)(void),
                      void (*rejected_func)(void)) {
    g_resume_func = resume_func;
    g_approved_func = approved_func;
    g_rejected_func = rejected_func;
    nbgl_useCaseReviewStart(&C_Message_64px,
                            title,
                            NULL,
                            REJECT_BUTTON,
                            start_func,
                            ui_message_confirm_rejection);
}

void ui_message_712_approved(void) {
    ui_712_approve_cb();
}

void ui_message_712_rejected(void) {
    ui_712_reject_cb();
}