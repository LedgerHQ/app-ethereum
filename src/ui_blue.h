#include "shared_context.h"

#include "os_io_seproxyhal.h"

typedef enum {
  APPROVAL_TRANSACTION,
  APPROVAL_MESSAGE,
} ui_approval_blue_state_t;

#define COLOR_BG_1 0xF9F9F9
#define COLOR_APP 0x0ebdcf
#define COLOR_APP_LIGHT 0x87dee6

extern const bagl_element_t ui_idle_blue[9];
unsigned int ui_idle_blue_button(unsigned int button_mask, unsigned int button_mask_counter);
const bagl_element_t *ui_idle_blue_prepro(const bagl_element_t *element);

extern const bagl_element_t ui_settings_blue[13];
unsigned int ui_settings_blue_button(unsigned int button_mask, unsigned int button_mask_counter);
const bagl_element_t * ui_settings_blue_prepro(const bagl_element_t * e);

extern const bagl_element_t ui_details_blue[16];
unsigned int ui_details_blue_button(unsigned int button_mask, unsigned int button_mask_counter);
const bagl_element_t* ui_details_blue_prepro(const bagl_element_t* element);

extern const bagl_element_t ui_approval_blue[29];
unsigned int ui_approval_blue_button(unsigned int button_mask, unsigned int button_mask_counter);
const bagl_element_t* ui_approval_blue_prepro(const bagl_element_t* element);

extern const bagl_element_t ui_address_blue[8];
unsigned int ui_address_blue_button(unsigned int button_mask, unsigned int button_mask_counter);
unsigned int ui_address_blue_prepro(const bagl_element_t* element);

extern const bagl_element_t ui_data_selector_blue[7];
unsigned int ui_data_selector_blue_button(unsigned int button_mask, unsigned int button_mask_counter);
unsigned int ui_data_selector_blue_prepro(const bagl_element_t* element);

extern const bagl_element_t ui_data_parameter_blue[11];
unsigned int ui_data_parameter_blue_button(unsigned int button_mask, unsigned int button_mask_counter);
unsigned int ui_data_parameter_blue_prepro(const bagl_element_t* element);

extern bagl_element_callback_t ui_approval_blue_ok;
extern bagl_element_callback_t ui_approval_blue_cancel;
extern ui_approval_blue_state_t G_ui_approval_blue_state;
extern const char* ui_approval_blue_values[3];
