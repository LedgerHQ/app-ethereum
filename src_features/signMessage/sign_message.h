#ifndef SIGN_MESSAGE_H_
#define SIGN_MESSAGE_H_

#define UI_191_BUFFER strings.tmp.tmp

typedef enum { STATE_191_HASH_DISPLAY = 0, STATE_191_HASH_ONLY } sign_message_state;

void question_switcher(void);
void skip_rest_of_message(void);
void continue_displaying_message(void);

#endif  // SIGN_MESSAGE_H_
