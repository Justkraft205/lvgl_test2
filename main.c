#include "bsp_illuminate.h"
#include "lvgl.h"
#include "bsp_i2c.h"
#include "bsp_display.h"
#include "esp_ldo_regulator.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

#define TAG "BUTTON"
LV_FONT_DECLARE(lv_font_unscii_8); 

static char text[100] = "";
static lv_obj_t *label;
int raw = 0;

static lv_color_t my_color(uint8_t r, uint8_t g, uint8_t b)
{
    return lv_color_make(b, r, g);
}

/* Touch für LVGL */
static void touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    uint16_t x, y;
    bool pressed;

    if (touch_read() == ESP_OK) {

        get_coor(&x, &y, &pressed);
        data->point.x = x;
        data->point.y = y;

        if (pressed) {
            data->state = LV_INDEV_STATE_PRESSED;
        } else {
            data->state = LV_INDEV_STATE_RELEASED;
        }

    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}


/* Gemeinsame Button-Funktion */
static void button_event(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {

        const char *buchstabe = lv_event_get_user_data(e);

        if (strcmp(buchstabe, "<-") == 0) {

            if (strlen(text) > 0) {
                text[strlen(text) - 1] = '\0';
            }

        /* !_ und _! machen nichts */
        } else if (strcmp(buchstabe, "!_") == 0 ||
                   strcmp(buchstabe, "_!") == 0) {
                    strcat(text, " ");
        /* Normaler Buchstabe */
        } else {
            strcat(text, buchstabe);
        }

        lv_label_set_text(label, text);
    }
}

/* Button erstellen */
static lv_obj_t *create_button(const char *button_text,int x,int y,lv_event_cb_t callback)
{
    lv_obj_t *button = lv_button_create(lv_screen_active());
    lv_obj_set_style_bg_color(button, lv_color_make(128, 128, 128), 0);
    lv_obj_set_style_bg_color(
        button,
    lv_color_hex(0x000000),
    LV_PART_MAIN | LV_STATE_PRESSED
);

    /* Position / Raw */
    if (x >= 2000) {
        x -= 2000;
        raw = 2;
    }
    else if (x >= 1000) {
        x -= 1000;
        raw = 1;
    }

    if (raw == 1) {
        y = 460;
    }

    if (raw == 2) {
        y = 390;
    }

    /* Button Größe */
    lv_obj_set_size(button, 100, 70);

    /* Button Position */
    lv_obj_set_pos(button, x, y);

    /* Keine abgerundeten Ecken */
    lv_obj_set_style_radius(button, 0, 0);

    /* Kein Schatten */
    lv_obj_set_style_shadow_width(button, 0, 0);

    /* Text */
    lv_obj_t *label_button = lv_label_create(button);

    lv_label_set_text(label_button, button_text);
    // Falls du die 8px-Variante in der lv_conf.h aktiviert hast
    lv_obj_set_style_text_font(label_button, &lv_font_unscii_8, 0);


    /* Reines Weiß */
    lv_obj_set_style_text_color(
        label_button,
        lv_color_hex(0xFFFFFF),
        0
    );
    lv_obj_set_style_text_opa(label_button, LV_OPA_COVER, 0);

    /* Kein Text-Schatten */
    lv_obj_set_style_text_opa(
        label_button,
        LV_OPA_COVER,
        0
    );

    /* Text zentrieren */
    lv_obj_center(label_button);

    /* Event */
    lv_obj_add_event_cb(
        button,
        callback,
        LV_EVENT_CLICKED,
        (void *)button_text
    );

    return button;
}


void app_main(void)
{
    /* I2C */
    if (i2c_init() != ESP_OK) {
        ESP_LOGE(TAG, "I2C initialization failed");
        return;
    }


    /* Touch */
    if (touch_init() != ESP_OK) {
        ESP_LOGE(TAG, "Touch initialization failed");
        return;
    }


    /* LDO3: 2.5 V */
    esp_ldo_channel_handle_t ldo3 = NULL;

    esp_ldo_channel_config_t ldo3_config = {
        .chan_id = 3,
        .voltage_mv = 2500,
    };

    esp_ldo_acquire_channel(&ldo3_config, &ldo3);


    /* LDO4: 3.3 V */
    esp_ldo_channel_handle_t ldo4 = NULL;

    esp_ldo_channel_config_t ldo4_config = {
        .chan_id = 4,
        .voltage_mv = 3300,
    };

    esp_ldo_acquire_channel(&ldo4_config, &ldo4);


    /* Display */
    display_init();


    /* Hintergrundbeleuchtung */
    set_lcd_blight(80);


    /* LVGL */
    lvgl_port_lock(0);


    /* Touch als LVGL Eingabegerät */
    lv_indev_t *touch = lv_indev_create();

    lv_indev_set_type(
        touch,
        LV_INDEV_TYPE_POINTER
    );

    lv_indev_set_read_cb(
        touch,
        touch_read_cb
    );

/* ROT */

lv_obj_t *rot = lv_obj_create(lv_screen_active());
lv_obj_set_size(rot, 200, 100);
lv_obj_set_pos(rot, 50, 100);
lv_obj_set_style_bg_color(rot, my_color(255, 0, 0), 0);


/* GRÜN */

lv_obj_t *gruen = lv_obj_create(lv_screen_active());
lv_obj_set_size(gruen, 200, 100);
lv_obj_set_pos(gruen, 300, 100);
lv_obj_set_style_bg_color(gruen, my_color(0, 255, 0), 0);


/* BLAU */

lv_obj_t *blau = lv_obj_create(lv_screen_active());
lv_obj_set_size(blau, 200, 100);
lv_obj_set_pos(blau, 550, 100);
lv_obj_set_style_bg_color(blau, my_color(0, 0, 255), 0);

    /* Haupt-Label */
    label = lv_label_create(lv_screen_active());

    lv_label_set_text(
        label,
        "Druecke einen Button"
    );
    lv_obj_set_style_text_font(label, &lv_font_unscii_8, 0);

    lv_obj_align(
        label,
        LV_ALIGN_CENTER,
        0,
        -50
    );


    /* Buchstaben */
    const char *buchstaben[] = {
        "Y", "X", "C", "V", "!_", "_!",
        "B", "N", "M", "->", "A", "S",
        "D", "F", "G", "H", "J", "K",
        "L", "P", "Q", "W", "E", "R",
        "T", "Z", "U", "I", "O", "<-"
    };


    /* 5 Buttons erstellen */
    int yh = 0;

    for (int i = 0; i < 30; i++) {

        yh = i * 100;
        create_button(buchstaben[i], yh, 530,button_event
        );
    }

    /* LVGL entsperren */
    lvgl_port_unlock();


    /* Main Task */
    while (1) {

        vTaskDelay(
            pdMS_TO_TICKS(100)
        );
    }
}
