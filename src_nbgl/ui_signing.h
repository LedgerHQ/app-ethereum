#ifndef UI_SIGNING_H_
#define UI_SIGNING_H_

#define SIGN_BUTTON           "Hold to sign"
#define REJECT_BUTTON         "Reject"
#define SIGN(msg)             "Sign " msg "?"
#define BLIND_SIGN(msg)       "Accept risk and sign " msg "?"
#define REVIEW(msg)           "Review " msg
#define REJECT(msg)           "Reject " msg
#define REJECT_QUESTION(msg)  REJECT(msg) "?"
#define REJECT_CONFIRM_BUTTON "Yes, reject"
#define RESUME(msg)           "Go back to " msg

typedef enum {
    UI_SIGNING_POSITION_START = 0,
    UI_SIGNING_POSITION_REVIEW,
    UI_SIGNING_POSITION_SIGN
} e_ui_signing_position;

extern e_ui_signing_position g_position;

#endif  // UI_SIGNING_H_
