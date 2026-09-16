/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6100 LVGL GUI
 *
 *  Copyright (c) 2022-2023 Belousov Oleg aka R1CBU
 */

#include <stdio.h>
#include <stdlib.h>
#include "pannel.h"
#include "styles.h"
#include "util.h"
#include "events.h"
#include "radio.h"
#include "params/params.h"
#include "rtty.h"
#include "cw_encoder.h"
#include "keyboard.h"

static lv_obj_t     *obj;
static lv_obj_t     *tx_box = NULL;
static char         buf[1024];
static char         tmp_buf[1024];
static char         *last_line;

static void check_lines() {
    char        *second_line = NULL;
    char        *ptr = (char *) &buf;
    uint16_t    count = 0;

    while (*ptr) {
        if (*ptr == '\n') {
            count++;

            if (count == 1) {
                second_line = ptr + 1;
            }
        }
        ptr++;
    }

    if (count > 4) {
        strcpy(tmp_buf, second_line);
        strcpy(buf, tmp_buf);
    }

    ptr = (char *) &buf;

    while (*ptr) {
        if (*ptr == '\n') {
            last_line = ptr + 1;
        }
        ptr++;
    }

    *last_line = '\0';
}

static void pannel_update_cb(lv_event_t * e) {
    lv_point_t line_size;
    lv_point_t text_size;

    char *text = lv_event_get_param(e);

    if (strcmp(text, "\n") == 0) {
        if (last_line[strlen(last_line) - 1] != '\n') {
            strcat(last_line, text);
            check_lines();
        }
    } else {
        lv_txt_get_size(&line_size, last_line, &sony_38, 0, 0, LV_COORD_MAX, 0);
        lv_txt_get_size(&text_size, text, &sony_38, 0, 0, LV_COORD_MAX, 0);

        if (line_size.x + text_size.x > (lv_obj_get_width(obj) - 40)) {
            strcat(last_line, "\n");
            check_lines();
        }

        strcat(last_line, text);
    }

    lv_label_set_text_static(obj, buf);
}


static void tx_box_ready_cb(lv_event_t * e) {
    const char *text = lv_textarea_get_text(tx_box);

    if (text && strlen(text) > 0) {
        cw_encoder_send(text, false);
    } else {
        cw_encoder_stop();
    }
    lv_textarea_set_text(tx_box, "");
}

lv_obj_t * pannel_init(lv_obj_t *parent) {
    obj = lv_label_create(parent);

    lv_obj_add_style(obj, &pannel_style, 0);
    lv_obj_add_event_cb(obj, pannel_update_cb, EVENT_PANNEL_UPDATE, NULL);
    lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);

    tx_box = lv_obj_create(parent);
    lv_obj_add_style(tx_box, &cw_tx_style, 0);
    lv_textarea_set_one_line(ta, true);
    lv_textarea_set_placeholder_text(ta, "Type to TX - Enter to send - Escape to clear - Enter after send to cancel");
    lv_obj_set_style_pad_all(tx_box, 10, 0);
    lv_obj_set_style_text_font(tx_box, &sony_30, 0);

    lv_group_add_obj(keyboard_group, ta);
    lv_group_focus_obj(ta);

    return obj;
}

void pannel_add_text(const char * text) {
    event_send(obj, EVENT_PANNEL_UPDATE, strdup(text));
}

void pannel_hide() {
    lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(tx_box, LV_OBJ_FLAG_HIDDEN);
    lv_group_remove_obj(tx_box);
}

void pannel_visible() {
    x6100_mode_t    mode = radio_current_mode();
    bool            on = false;

    switch (mode) {
        case x6100_mode_cw:
        case x6100_mode_cwr:
            on = params.cw_decoder;
            break;

        case x6100_mode_usb:
        case x6100_mode_lsb:
        case x6100_mode_usb_dig:
        case x6100_mode_lsb_dig:
            on = rtty_get_state() != RTTY_OFF;
            break;
    }

    if (on) {
        strcpy(buf, "");
        last_line = (char *) &buf;
        lv_label_set_text_static(obj, buf);
        lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);

        lv_obj_clear_flag(tx_box, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(tx_box);     /* raise above waterfall & later-built widgets */

        lv_group_remove_obj(tx_box);        /* avoid duplicate adds on repeated calls */
        lv_group_add_obj(keyboard_group, tx_box);
        lv_group_focus_obj(tx_box);         /* steal focus from spectrum */
        lv_group_set_editing(keyboard_group, true);
    } else {
        lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);

        lv_obj_add_flag(tx_box, LV_OBJ_FLAG_HIDDEN);
        lv_group_remove_obj(tx_box);        /* focus falls back to spectrum */
    }
}
