#include "bsp_illuminate.h"
#include "lvgl.h"
#include "bsp_i2c.h"
#include "bsp_display.h"
#include "esp_ldo_regulator.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include <stdio.h>
#include <stdbool.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>

char WIFI_SSID[100] = "";
char WIFI_PASS[100] = "";
static lv_obj_t *active_field = NULL;
LV_FONT_DECLARE(lv_font_unscii_8);

#define SERVER_IP   "192.168.178.91"
#define SERVER_PORT 8765

#define CONNECTED BIT0
#define FAILED    BIT1
static int label_y[100];
char ip[32];

static void create_keyboard(int check);

const char *buchstaben[] = {
    "Q", "W", "E", "R", "T", "Z",
    "U", "I", "O", "P", "<-", "A",
    "S", "D", "F", "G", "H", "J",
    "K", "L", ":", "123", "<>", "Y",
    "X", "C", "V", "!_", "_!", "B",
    "N", "M", "->", "^^", "UP"
};

const char *buchstaben1[] = {
    "q", "w", "e", "r", "t", "z",
    "u", "i", "o", "p", "<-", "a",
    "s", "d", "f", "g", "h", "j",
    "k", "l", ".", "123", "<>", "y",
    "x", "c", "v", "!_", "_!", "b",
    "n", "m", "->", "^^"
};

const char *zahlen[] = {
    "1", "2", "3", "4", "5", "6",
    "7", "8", "9", "0", "<-", "+",
    "-", "*", "/", "=", "(", ")",
    "{", "}", "[", "]", "ABC", "$",
    "!", "?", "@", "!_", "_!", "<",
    ">", "#", "->", "^^"
};
lv_obj_t *label_button[100];
static char text[100] = "";
static lv_obj_t *label;
static lv_obj_t *keyboard_container;

int raw = 0;
int selectes = 0;
int wselect = 0;
int line = 0;
static char callback[20] = "";


int worky = 0;
int wline = 0;
static const char *TAG = "ESP32";
static EventGroupHandle_t wifi_events;
static int retries = 0;

/* Verhindert mehrfachen Start der WLAN-Task */
static bool wifi_task_started = false;

static void activ_text(lv_event_t *e){active_field = lv_event_get_user_data(e);
lv_label_set_text(active_field, "");}

static void inputfields(int x, int y,int xs, int ys,  const char *name){
    lv_obj_t *button = lv_button_create(lv_screen_active());
    lv_obj_set_size(button, xs, ys);
    lv_obj_set_pos(button, x, y);
    if (keyboard_container != NULL) {
    lv_obj_move_background(keyboard_container);}
    lv_obj_move_foreground(button);
    lv_obj_t *label_button = lv_label_create(button);
    lv_label_set_text(label_button, name);
    lv_obj_set_style_text_font(label_button, &lv_font_unscii_8, 0);
    lv_obj_set_style_text_color(label_button, lv_color_hex(0xFFFFFF), 0);
    lv_obj_move_foreground(label_button);
    lv_obj_center(label_button);
    lv_obj_add_event_cb(button, activ_text, LV_EVENT_CLICKED, label_button);
}

static void logkonsole(const char *name)
{
    if (line >= 100) return;

    printf("lines:%d\n", line);

    for (int i = 0; i < line; i++) {
        label_y[i] -= 20;
        lv_obj_set_pos(label_button[i], 30, label_y[i]);
    }

    label_button[line] = lv_label_create(label);
    lv_obj_set_layout(label, LV_LAYOUT_NONE);
    lv_label_set_text(label_button[line], name);
    lv_obj_set_style_text_font(label_button[line], &lv_font_unscii_8, 0);
    lv_obj_set_style_text_color(label_button[line], lv_color_black(), 0);
    lv_obj_set_size(label_button[line], 900, 20);
    lv_label_set_long_mode(label_button[line], LV_LABEL_LONG_CLIP);

    label_y[line] = 300;
    lv_obj_set_pos(label_button[line], 30, label_y[line]);

    line++;
}
/* Nachricht senden */
static void send_message(const char *msg)
{
    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (sock < 0) return;

    struct sockaddr_in server = {
        .sin_family = AF_INET,
        .sin_port = htons(SERVER_PORT)
    };

    inet_pton(AF_INET, SERVER_IP, &server.sin_addr);

    if (connect(sock, (struct sockaddr *)&server, sizeof(server)) == 0) {
        send(sock, msg, strlen(msg), 0);
    }

    close(sock);
}


/* Nachricht empfangen */
static int receive_message(char *buffer, size_t size){
    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (sock < 0) return -1;
    struct sockaddr_in server = {
        .sin_family = AF_INET,
        .sin_port = htons(SERVER_PORT)
    };
    inet_pton(AF_INET, SERVER_IP, &server.sin_addr);

    if (connect(sock, (struct sockaddr *)&server, sizeof(server)) != 0) {
        close(sock);
        return -1;
    }

    int len = recv(sock, buffer, size - 1, 0);

    if (len > 0) {
        buffer[len] = '\0';
    }
    close(sock);
    return len;
}


/* WLAN-Ereignisse */
static void wifi_event(void *arg, esp_event_base_t base,int32_t id, void *data){
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    }else if (base == WIFI_EVENT &&id == WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupClearBits(wifi_events, CONNECTED);
        if (retries++ < 5) {esp_wifi_connect();} else {
            xEventGroupSetBits(wifi_events, FAILED);}
    }
    else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = data;
        snprintf(ip, sizeof(ip), "IP: " IPSTR, IP2STR(&event->ip_info.ip));
        logkonsole(ip);
        retries = 0;
        xEventGroupSetBits(wifi_events, CONNECTED);
    }
}

/* WLAN starten */
static bool wifi_start(void){
    wifi_events = xEventGroupCreate();
    if (!wifi_events) return false;
    ESP_ERROR_CHECK(esp_netif_init());
    esp_err_t err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {ESP_ERROR_CHECK(err);}
    if (!esp_netif_create_default_wifi_sta()) return false;
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event, NULL));
    wifi_config_t config = {0};
    strcpy((char *)config.sta.ssid, WIFI_SSID);
    strcpy((char *)config.sta.password, WIFI_PASS);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &config));
    ESP_ERROR_CHECK(esp_wifi_start());
    EventBits_t bits = xEventGroupWaitBits(wifi_events,CONNECTED | FAILED,pdFALSE,pdFALSE,portMAX_DELAY);
    return (bits & CONNECTED) != 0;
}


/* WLAN und Nachrichten in eigener Task */
static void wifi_task(void *arg){
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
        err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    if (wifi_start()) {
        while (true) {
            send_message("1");
            vTaskDelay(pdMS_TO_TICKS(2000));
        }
    }
    wifi_task_started = false;
    vTaskDelete(NULL);
}

/* Touch für LVGL */
static void touch_read_cb(lv_indev_t *indev,lv_indev_data_t *data){
    uint16_t x, y;
    bool pressed;
    if (touch_read() == ESP_OK) {
        get_coor(&x, &y, &pressed);
        data->point.x = x;
        data->point.y = y;
        if (pressed) {data->state = LV_INDEV_STATE_PRESSED;
        } else { data->state = LV_INDEV_STATE_RELEASED;}
    } else {data->state = LV_INDEV_STATE_RELEASED;}
}

/* WLAN-Eingabe über den Button */
static void wificonnection(){
        wselect = wselect + 1;
        if (wselect == 1){
        logkonsole("WlanName:");
        strcpy(callback, "wificonnection");
        } else if (wselect == 2) {
            const char *inhalt = lv_label_get_text(active_field);
            logkonsole(inhalt);
            strncat(WIFI_SSID, inhalt,sizeof(WIFI_SSID) - strlen(WIFI_SSID) - 1);
            logkonsole("Wlan Passwort:");
        } else if (wselect == 3){
            const char *inhalt2 = lv_label_get_text(active_field);
            logkonsole(inhalt2);
            logkonsole("Verbinde mit WLAN...");
            strncat(WIFI_PASS, inhalt2,sizeof(WIFI_PASS) - strlen(WIFI_PASS) - 1);
            if (!wifi_task_started) {
                wifi_task_started = true;
                BaseType_t result = xTaskCreate(wifi_task,"wifi_task",8192,NULL,5,NULL);
                if (result != pdPASS) {wifi_task_started = false;}
            }
            logkonsole("IP des zu verbindenen Gerätes:");
        } else if (wselect == 4){
            const char *inhalt = lv_label_get_text(active_field);
            callback[0] = '\0';


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
        }
        else if (strcmp(buchstabe, "!_") == 0 ||
                 strcmp(buchstabe, "_!") == 0) {
            strcat(text, " ");
        }
        else if (strcmp(buchstabe, "<>") == 0) {
            if (selectes == 1) {
                selectes = 0;
            } else {
                selectes = 1;
            }

            create_keyboard(1);
        }
        else if (strcmp(buchstabe, "123") == 0) {
            selectes = 2;
            create_keyboard(1);
        }else if (strcmp(buchstabe, "->") == 0) {
    if (strcmp(callback, "") != 0) {
        if (strcmp(callback, "wificonnection") == 0) {
            wificonnection();
        }
    }
}
        else if (strcmp(buchstabe, "ABC") == 0) {
            selectes = 0;
            create_keyboard(1);
        }
        else if (strcmp(buchstabe, "UP") == 0) {
            selectes = 0;
            create_keyboard(1);
        }
        else if (strcmp(buchstabe, "^^") == 0) {
            create_keyboard(0);
        }
        else {
            strcat(text, buchstabe);
        }

        lv_label_set_text(active_field, text);
    }
}

static lv_obj_t *keyboard(const char *button_text,int x, int y, int sx, int sy, int check,lv_event_cb_t callback)
{
    lv_obj_t *button;

    if (check == 1) {
        button = lv_button_create(keyboard_container);
    } else {
        button = lv_button_create(lv_screen_active());
    }
    lv_obj_remove_flag(keyboard_container, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_set_style_bg_color(button, lv_color_make(128, 128, 128), 0);

    lv_obj_set_style_bg_color(
        button, lv_color_hex(0x000000),
        LV_PART_MAIN | LV_STATE_PRESSED);

    lv_obj_set_size(button, sx, sy);
    lv_obj_set_pos(button, x, y);
    lv_obj_set_style_radius(button, 0, 0);
    lv_obj_set_style_shadow_width(button, 0, 0);

    lv_obj_t *label_button = lv_label_create(button);

    lv_label_set_text(label_button, button_text);
    lv_obj_set_style_text_font(
        label_button, &lv_font_unscii_8, 0);

    lv_obj_set_style_text_color(
        label_button, lv_color_hex(0xFFFFFF), 0);

    lv_obj_set_style_text_opa(
        label_button, LV_OPA_COVER, 0);

    lv_obj_center(label_button);

    lv_obj_add_event_cb(
        button, callback, LV_EVENT_CLICKED,
        (void *)button_text);

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

        lv_obj_set_style_bg_opa(
            keyboard_container, LV_OPA_TRANSP, 0);

        lv_obj_set_style_border_width(
            keyboard_container, 0, 0);

        lv_obj_set_style_pad_all(
            keyboard_container, 0, 0);

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

            keyboard(
                selectes == 2 ? zahlen[i] :
                selectes == 1 ? buchstaben1[i] :
                                buchstaben[i],
                xh, yh, 93,70, check, button_event);
        }
    }
    else {
        keyboard(buchstaben[34], 0, 550, 50 , 50, check,
                 button_event);
    }
}

void app_main(void){
    if (i2c_init() != ESP_OK) {
        ESP_LOGE(TAG, "I2C initialization failed");
        return;}
    if (touch_init() != ESP_OK) {
        ESP_LOGE(TAG, "Touch initialization failed");
        return;}
    esp_ldo_channel_handle_t ldo3 = NULL;
    esp_ldo_channel_config_t ldo3_config = { .chan_id = 3,.voltage_mv = 2500,};
    esp_ldo_acquire_channel(&ldo3_config, &ldo3);
    esp_ldo_channel_handle_t ldo4 = NULL;
    esp_ldo_channel_config_t ldo4_config = {.chan_id = 4,.voltage_mv = 3300,};
    esp_ldo_acquire_channel(&ldo4_config, &ldo4);
    display_init();
    set_lcd_blight(80);
    lvgl_port_lock(0);
    lv_indev_t *touch = lv_indev_create();
    lv_indev_set_type(touch, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(touch, touch_read_cb);
    label = lv_obj_create(lv_screen_active());
    lv_obj_set_size(label, 1024, 320);
    lv_obj_set_pos(label, 0, 0);
    lv_obj_set_style_border_width(label, 0, 0);
    lv_obj_set_style_pad_all(label, 0, 0);
    inputfields(100, 330, 924, 60, "befehl");
    create_keyboard(1);
    wificonnection();
    lvgl_port_unlock();
    while (1) {vTaskDelay(pdMS_TO_TICKS(100));}
}
