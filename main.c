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
static void create_keyboard(int check);

/* Buchstaben */
const char *buchstaben[] = {
    "Q", "W", "E", "R", "T", "Z",
    "U", "I", "O", "P", "<-", "A",
    "S", "D", "F", "G", "H", "J",
    "K", "L", ":", "123", "<>", "Y",
    "X", "C", "V", "!_", "_!", "B",
    "N","M","->","^^","UP"
};
/* Buchstaben */
const char *buchstaben1[] = {
    "q", "w", "e", "r", "t", "z",
    "u", "i", "o", "p", "<-", "a",
    "s", "d", "f", "g", "h", "j",
    "k", "l", ".", "123", "<>", "y",
    "x", "c", "v", "!_", "_!", "b",
    "n","m","->", "^^"
};
const char *zahlen[] = {
    "1", "2", "3", "4", "5", "6",
    "7", "8", "9", "0", "<-", "+",
    "-", "*", "/", "=", "(", ")",
    "{", "}", "[", "]", "ABC", "$",
    "!", "?", "@", "!_", "_!", "<",
    ">","#","->","^^"
};

static char text[100] = "";
static lv_obj_t *label;
static lv_obj_t *keyboard_container;
int raw = 0;
int selectes = 0;

static lv_color_t my_color(uint8_t r, uint8_t g, uint8_t b){return lv_color_make(b, r, g);}

/* Touch für LVGL */
static void touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    uint16_t x, y;
    bool pressed;
    if (touch_read() == ESP_OK) {
        get_coor(&x, &y, &pressed);
        data->point.x = x;
        data->point.y = y;
        if (pressed) {data->state = LV_INDEV_STATE_PRESSED;
        } else { data->state = LV_INDEV_STATE_RELEASED;}

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
        } else if (strcmp(buchstabe, "!_") == 0 ||strcmp(buchstabe, "_!") == 0) {
                    strcat(text, " ");
        } else if (strcmp(buchstabe, "<>") == 0){
            if (selectes == 1){selectes = 0;} else {selectes = 1;}
            create_keyboard(1);
        }else if (strcmp(buchstabe, "123") == 0){
            selectes  = 2;
            create_keyboard(1);
        }else if (strcmp(buchstabe, "ABC") == 0){
            selectes  = 0;
            create_keyboard(1);
        } else if (strcmp(buchstabe, "UP") == 0){selectes  = 0;
            create_keyboard(1);} else if (strcmp(buchstabe, "^^") == 0){
            create_keyboard(0);
        } else{
            strcat(text, buchstabe);
        }
        lv_label_set_text(label, text);
    }
}

static lv_obj_t *keyboard(const char *button_text,int x,int y,int check,lv_event_cb_t callback)
{
    lv_obj_t *button;
    if (check == 1) {button = lv_button_create(keyboard_container);
    } else {button = lv_button_create(lv_screen_active());}
    lv_obj_set_style_bg_color(button,lv_color_make(128, 128, 128),0);
    lv_obj_set_style_bg_color(button,lv_color_hex(0x000000),LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_size(button, 93, 70);
    lv_obj_set_pos(button, x, y);
    lv_obj_set_style_radius(button, 0, 0);
    lv_obj_set_style_shadow_width(button, 0, 0);
    lv_obj_t *label_button = lv_label_create(button);
    lv_label_set_text(label_button, button_text);
    lv_obj_set_style_text_font(label_button, &lv_font_unscii_8, 0);
    lv_obj_set_style_text_color(label_button,lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_opa(label_button,LV_OPA_COVER,0);
    lv_obj_center(label_button);
    lv_obj_add_event_cb(button,callback, LV_EVENT_CLICKED,(void *)button_text);
    return button;
}

static void create_keyboard(int check)
{
    if (keyboard_container != NULL) {
        lv_obj_delete(keyboard_container);
        keyboard_container = NULL;
    }
    if (check == 1) {
        keyboard_container = lv_obj_create(lv_screen_active());
        lv_obj_set_size(keyboard_container, 1024, 280);
        lv_obj_set_pos(keyboard_container, 0, 320);
        lv_obj_set_style_bg_opa(keyboard_container, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(keyboard_container, 0, 0);
        lv_obj_set_style_pad_all(keyboard_container, 0, 0);
        int xh = 0;
        int yh = 0;
        for (int i = 0; i < 34; i++) {

            if (i == 33) {
                xh = 0;
                yh = 0;
            }
            else if (i < 11) {
                xh = i * 93;
                yh = 70;
            }
            else if (i < 22) {
                xh = (i - 11) * 93;
                yh = 140;
            }
            else {
                xh = (i - 22) * 93;
                yh = 210;
            }
            keyboard(selectes == 2 ? zahlen[i] :selectes == 1 ? buchstaben1[i] : buchstaben[i],xh, yh, check, button_event);
        }
    } else {keyboard(buchstaben[34] ,93, 530,check, button_event);}
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
    lv_label_set_text(label,"Druecke einen Button");
    lv_obj_set_style_text_font(label, &lv_font_unscii_8, 0);
    lv_obj_align(label,LV_ALIGN_CENTER,0,-50);
    create_keyboard(1);
    /* LVGL entsperren */
    lvgl_port_unlock();
    /* Main Task */
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
