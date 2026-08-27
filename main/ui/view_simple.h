#ifndef UI_VIEW_SIMPLE_H
#define UI_VIEW_SIMPLE_H

#include "ui/device_view.h"
#include "ui/ui_state.h"

typedef void (*ui_simple_formatter_t)(lv_obj_t *label, const victron_data_t *data);

typedef struct {
    const char *title;
    ui_simple_formatter_t formatter;
} ui_simple_label_descriptor_t;

typedef struct {
    victron_record_type_t type;
    const ui_simple_label_descriptor_t *labels;
    size_t label_count;
} ui_simple_view_config_t;

ui_device_view_t *ui_simple_view_create(ui_state_t *ui,
                                        lv_obj_t *parent,
                                        const ui_simple_view_config_t *config);

#endif /* UI_VIEW_SIMPLE_H */

