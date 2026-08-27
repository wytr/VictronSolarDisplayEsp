#ifndef UI_VIEW_REGISTRY_H
#define UI_VIEW_REGISTRY_H

#include "ui/device_view.h"
#include "ui/ui_state.h"

typedef struct {
    victron_record_type_t type;
    const char *name;
    ui_device_view_create_fn create;
} ui_view_descriptor_t;

const ui_view_descriptor_t *ui_view_registry_find(victron_record_type_t type);
ui_device_view_t *ui_view_registry_ensure(ui_state_t *ui,
                                          victron_record_type_t type,
                                          lv_obj_t *parent);
const char *ui_view_registry_name(victron_record_type_t type);

#endif /* UI_VIEW_REGISTRY_H */
