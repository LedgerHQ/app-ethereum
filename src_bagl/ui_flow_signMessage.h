#ifndef UI_FLOW_SIGNMESSAGE_H_
#define UI_FLOW_SIGNMESSAGE_H_

typedef enum { UI_191_POS_REVIEW, UI_191_POS_QUESTION, UI_191_POS_END } e_ui_191_position;

void ui_191_start(void);
void ui_191_switch_to_message(void);
void ui_191_switch_to_message_end(void);
void ui_191_switch_to_sign(void);
void ui_191_switch_to_question(void);

#endif  // UI_FLOW_SIGNMESSAGE_H_
