#include <lvgl.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <ArduinoJson.h>

// Touchscreen pins
#define XPT2046_IRQ 36   // T_IRQ
#define XPT2046_MOSI 32  // T_DIN
#define XPT2046_MISO 39  // T_OUT
#define XPT2046_CLK 25   // T_CLK
#define XPT2046_CS 33    // T_CS

// --- RGB LED Pins ---
#define LED_RED 4
#define LED_GREEN 16
#define LED_BLUE 17
#define TFT_BL 21

// --- Image Declarations ---
// Once you convert your images, you will declare them like this:
LV_IMG_DECLARE(intel); 
LV_IMG_DECLARE(nvidia);
LV_IMG_DECLARE(chip);

static const uint32_t screenWidth  = 320;
static const uint32_t screenHeight = 240;
static uint8_t draw_buf[screenWidth * screenHeight / 10];

TFT_eSPI tft = TFT_eSPI();
SPIClass touchscreenSPI = SPIClass(VSPI);
XPT2046_Touchscreen touchscreen(XPT2046_CS, XPT2046_IRQ);

#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 320

// Touchscreen coordinates: (x, y) and pressure (z)
int x, y, z;

lv_obj_t * bar_cpu, * bar_cput, * bar_mem, * bar_gpu, * bar_gput, * bar_gpumem;
lv_obj_t * bar_net_up, * bar_net_down, * bar_disk;

void my_disp_flush(lv_display_t * disp, const lv_area_t * area, uint8_t * px_map) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);
    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushColors((uint16_t *)px_map, w * h, true);
    tft.endWrite();
    lv_display_flush_ready(disp);
}

void set_led_color(bool r, bool g, bool b) {
    digitalWrite(LED_RED, r ? LOW : HIGH);
    digitalWrite(LED_GREEN, g ? LOW : HIGH);
    digitalWrite(LED_BLUE, b ? LOW : HIGH);
}

// Updated function to accept an image source instead of text
lv_obj_t* create_icon_box(lv_obj_t* parent, int y, const lv_image_dsc_t* img_src) {
    lv_obj_t* obj = lv_obj_create(parent);
    lv_obj_set_size(obj, 60, 60);
    lv_obj_set_pos(obj, 245, y);
    lv_obj_set_style_bg_opa(obj, 0, 0);       // Make box background transparent
    lv_obj_set_style_border_width(obj, 0, 0); // Remove border
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);

    // Create the image object inside the box
    lv_obj_t* img = lv_image_create(obj);
    lv_image_set_src(img, img_src);
    lv_obj_center(img);
    
    return obj;
}

// Get the Touchscreen data
void touchscreen_read(lv_indev_t * indev, lv_indev_data_t * data) {
  // Checks if Touchscreen was touched, and prints X, Y and Pressure (Z)
  if(touchscreen.tirqTouched() && touchscreen.touched()) {
    // Get Touchscreen points
    TS_Point p = touchscreen.getPoint();
    // Calibrate Touchscreen points with map function to the correct width and height
    x = map(p.x, 200, 3700, 1, SCREEN_WIDTH);
    y = map(p.y, 240, 3800, 1, SCREEN_HEIGHT);
    z = p.z;

    data->state = LV_INDEV_STATE_PRESSED;

    // Set the coordinates
    data->point.x = x;
    data->point.y = y;

    // Print Touchscreen info about X, Y and Pressure (Z) on the Serial Monitor
    /*Serial.print("X = ");
    Serial.print(x);
    Serial.print(" | Y = ");
    Serial.print(y);
    Serial.print(" | Pressure = ");
    Serial.print(z);
    Serial.println();*/
  }
  else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

void setup_ui() {
    lv_obj_t * screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x0a0a0c), LV_PART_MAIN);

    // 1. HEADER BAR
    lv_obj_t * header = lv_obj_create(screen);
    lv_obj_set_size(header, 320, 35);
    lv_obj_set_pos(header, 0, 0);
    lv_obj_set_style_bg_color(header, lv_color_hex(0x1f2126), 0);
    lv_obj_set_style_radius(header, 0, 0);
    lv_obj_set_style_border_width(header, 0, 0);

    lv_obj_t * title = lv_label_create(header);
    lv_label_set_text(title, "HARDWARE MONITOR SYSTEM");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 10, 0);

    // 2. STAT ROWS
    auto add_row = [&](const char* txt, int y, lv_color_t color) {
        lv_obj_t* lbl = lv_label_create(screen);
        lv_label_set_text(lbl, txt);
        lv_obj_set_style_text_color(lbl, lv_color_hex(0x9CA3AF), 0);
        lv_obj_set_pos(lbl, 10, y);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);

        lv_obj_t* bar = lv_bar_create(screen);
        lv_obj_set_size(bar, 140, 12);
        lv_obj_set_pos(bar, 85, y + 2);
        lv_obj_set_style_bg_color(bar, lv_color_hex(0x1f2126), LV_PART_MAIN);
        lv_obj_set_style_bg_color(bar, color, LV_PART_INDICATOR);
        return bar;
    };

    bar_cpu = add_row("CPU %", 45, lv_color_hex(0x0071C5));
    bar_cput = add_row("CPU Temp", 65, lv_color_hex(0x0071C5));
    bar_mem = add_row("Mem %", 85, lv_color_hex(0x0071C5));

    bar_gpu = add_row("GPU %", 115, lv_color_hex(0x0071C5));
    bar_gput = add_row("GPU Temp", 135, lv_color_hex(0x0071C5));
    bar_gpumem = add_row("GPU Mem", 155, lv_color_hex(0x0071C5));

    bar_net_up = add_row("Net Up", 185, lv_color_hex(0x0071C5));
    bar_net_down = add_row("Net Down", 205, lv_color_hex(0x0071C5));
    bar_disk = add_row("Disk I/O", 225, lv_color_hex(0x0071C5));

    // 3. LOGO BOXES (Exactly like your image)
    create_icon_box(screen, 45,  &intel);
    create_icon_box(screen, 115, &nvidia);
    create_icon_box(screen, 185, &chip);
}

void setup() {
    Serial.begin(115200);
    pinMode(TFT_BL, OUTPUT); digitalWrite(TFT_BL, HIGH);
    pinMode(LED_RED, OUTPUT); pinMode(LED_GREEN, OUTPUT); pinMode(LED_BLUE, OUTPUT);
    


    tft.begin(); tft.setRotation(1);
    lv_init();
    lv_tick_set_cb((uint32_t (*)(void))millis);
    lv_display_t * disp = lv_display_create(screenWidth, screenHeight);
    lv_display_set_flush_cb(disp, my_disp_flush);
    lv_display_set_buffers(disp, draw_buf, NULL, sizeof(draw_buf), LV_DISPLAY_RENDER_MODE_PARTIAL);
    
    // Start the SPI for the touchscreen and init the touchscreen
    touchscreenSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
    touchscreen.begin(touchscreenSPI);
    // Set the Touchscreen rotation in landscape mode
    // Note: in some displays, the touchscreen might be upside down, so you might need to set the rotation to 0: touchscreen.setRotation(0);
    touchscreen.setRotation(1);

    // Initialize an LVGL input device object (Touchscreen)
    lv_indev_t * indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    // Set the callback function to read Touchscreen input
    lv_indev_set_read_cb(indev, touchscreen_read);

    setup_ui();
}

void loop() {
    lv_timer_handler();
    if (Serial.available()) {
        String data = Serial.readStringUntil('\n');
        JsonDocument doc;
        if (deserializeJson(doc, data) == DeserializationError::Ok) {
            if (doc.containsKey("cpu")) {
                int val = doc["cpu"];
                lv_bar_set_value(bar_cpu, val, LV_ANIM_ON);
                if (val < 33) set_led_color(0,1,0);
                else if (val < 66) set_led_color(0,0,1);
                else set_led_color(1,0,0);
            }
            if (doc.containsKey("cput")) lv_bar_set_value(bar_cput, doc["cput"], LV_ANIM_ON);
            if (doc.containsKey("mem")) lv_bar_set_value(bar_mem, doc["mem"], LV_ANIM_ON);
            if (doc.containsKey("gpu")) lv_bar_set_value(bar_gpu, doc["gpu"], LV_ANIM_ON);
            if (doc.containsKey("gput")) lv_bar_set_value(bar_gput, doc["gput"], LV_ANIM_ON);
            if (doc.containsKey("gpum")) lv_bar_set_value(bar_gpumem, doc["gpum"], LV_ANIM_ON);
            if (doc.containsKey("netu")) lv_bar_set_value(bar_net_up, doc["netu"], LV_ANIM_ON);
            if (doc.containsKey("netd")) lv_bar_set_value(bar_net_down, doc["netd"], LV_ANIM_ON);
            if (doc.containsKey("disk")) lv_bar_set_value(bar_disk, doc["disk"], LV_ANIM_ON);
        }
    }
    delay(5);
}