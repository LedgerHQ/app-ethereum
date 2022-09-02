#ifndef SIGN_MESSAGE_H_
#define SIGN_MESSAGE_H_

typedef enum { STATE_191_HASH_DISPLAY = 0, STATE_191_HASH_ONLY } sign_message_state;
typedef enum { UI_191_REVIEW = 0, UI_191_QUESTION, UI_191_END } ui_191_position;


#define UI_191_BUFFER strings.tmp.tmp

void question_switcher(void);
void skip_rest_of_message(void);
void continue_displaying_message(void);
void dummy_pre_cb(void);
void theres_more_click_cb(void);
void dummy_post_cb(void);

#endif  // SIGN_MESSAGE_H_
