#ifndef _PLUGIN_H_
#define _PLUGIN_H_

void plugin_ui_get_id();
void plugin_ui_get_item();
void plugin_ui_get_item_internal(uint8_t *title_buffer,
                                 size_t title_buffer_size,
                                 uint8_t *msg_buffer,
                                 size_t msg_buffer_size);

#endif  // _PLUGIN_H_
