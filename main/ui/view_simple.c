#include "view_simple.h"

#include <stdlib.h>

typedef struct {
    ui_device_view_t base;
    const ui_simple_view_config_t *config;
    lv_obj_t **values;
} ui_simple_view_t;

static ui_simple_view_t *simple_view_from_base(ui_device_view_t *base)
{
    return (ui_simple_view_t *)base;
}

static lv_obj_t *create_value_row(ui_simple_view_t *view,
                                  const ui_simple_label_descriptor_t *desc)
{
    if (view == NULL || desc == NULL || view->base.ui == NULL) {
        return NULL;
    }

    lv_obj_t *row = lv_obj_create(view->base.root);
    lv_obj_set_size(row, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_outline_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 8, 0);
    lv_obj_set_style_pad_column(row, 12, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row,
                          LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    lv_obj_t *title = lv_label_create(row);
    lv_label_set_text(title, desc->title ? desc->title : "");
    lv_obj_add_style(title, &view->base.ui->styles.medium, 0);

    lv_obj_t *value = lv_label_create(row);
    lv_label_set_text(value, "--");
    lv_obj_add_style(value, &view->base.ui->styles.value, 0);
    lv_label_set_long_mode(value, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(value, lv_pct(55));
    lv_obj_set_style_text_align(value, LV_TEXT_ALIGN_RIGHT, 0);

    return value;
}

static void simple_view_update(ui_device_view_t *base, const victron_data_t *data)
{
    ui_simple_view_t *view = simple_view_from_base(base);
    if (view == NULL || data == NULL || view->config == NULL) {
        return;
    }

    if (data->type != view->config->type) {
        return;
    }

    if (base->ui && base->ui->lbl_error) {
        lv_label_set_text(base->ui->lbl_error, "");
    }

    for (size_t i = 0; i < view->config->label_count; ++i) {
        if (view->values[i] == NULL) {
            continue;
        }
        const ui_simple_label_descriptor_t *desc = &view->config->labels[i];
        if (desc->formatter) {
            desc->formatter(view->values[i], data);
        } else {
            lv_label_set_text(view->values[i], "");
        }
    }
}

static void simple_view_show(ui_device_view_t *base)
{
    if (base && base->root) {
        lv_obj_clear_flag(base->root, LV_OBJ_FLAG_HIDDEN);
    }
}

static void simple_view_hide(ui_device_view_t *base)
{
    if (base && base->root) {
        lv_obj_add_flag(base->root, LV_OBJ_FLAG_HIDDEN);
    }
}

static void simple_view_destroy(ui_device_view_t *base)
{
    ui_simple_view_t *view = simple_view_from_base(base);
    if (view == NULL) {
        return;
    }

    if (view->base.root) {
        lv_obj_del(view->base.root);
        view->base.root = NULL;
    }

    free(view->values);
    free(view);
}

ui_device_view_t *ui_simple_view_create(ui_state_t *ui,
                                        lv_obj_t *parent,
                                        const ui_simple_view_config_t *config)
{
    if (ui == NULL || parent == NULL || config == NULL || config->labels == NULL ||
        config->label_count == 0) {
        return NULL;
    }

    ui_simple_view_t *view = calloc(1, sizeof(*view));
    if (view == NULL) {
        return NULL;
    }

    view->values = calloc(config->label_count, sizeof(lv_obj_t *));
    if (view->values == NULL) {
        free(view);
        return NULL;
    }

    view->config = config;
    view->base.ui = ui;
    view->base.root = lv_obj_create(parent);
    if (view->base.root == NULL) {
        free(view->values);
        free(view);
        return NULL;
    }

    lv_obj_set_size(view->base.root, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_opa(view->base.root, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(view->base.root, 0, 0);
    lv_obj_set_style_outline_width(view->base.root, 0, 0);
    lv_obj_set_style_pad_all(view->base.root, 12, 0);
    lv_obj_clear_flag(view->base.root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(view->base.root, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(view->base.root,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_SPACE_EVENLY,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(view->base.root, 4, 0);
    lv_obj_add_flag(view->base.root, LV_OBJ_FLAG_HIDDEN);

    for (size_t i = 0; i < config->label_count; ++i) {
        view->values[i] = create_value_row(view, &config->labels[i]);
    }

    view->base.update = simple_view_update;
    view->base.show = simple_view_show;
    view->base.hide = simple_view_hide;
    view->base.destroy = simple_view_destroy;

    return &view->base;
}

