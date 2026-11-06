#include "input_processing.h"
#include "input_reading.h"

enum ModeState { MODE_1_NORMAL = 1, MODE_2_MODIFY_RED, MODE_3_MODIFY_AMBER, MODE_4_MODIFY_GREEN };
static enum ModeState current_mode = MODE_1_NORMAL;

enum TrafficState { R1_RED_R2_GREEN, R1_RED_R2_AMBER, R1_GREEN_R2_RED, R1_AMBER_R2_RED };
static enum TrafficState traffic_state = R1_RED_R2_GREEN;

static int timer_auto_inc = 20;
static int red_duration = 5;
static int amber_duration = 2;
static int green_duration = 3;

static int temp_duration = 0;

static int countdown_road1 = 0;
static int countdown_road2 = 0;

// Software Timers
static int timer_traffic = 100; // 100 * 10ms = 1s
static int timer_blink = 25;    // 25 * 10ms = 250ms -> 2 pulse state toggle
static int timer_scan = 1;
static uint8_t blink_state = 0;

static int display_buffer[4] = {0, 0, 0, 0};
static int scan_index = 0;

static const uint8_t seg_code[10] = {
    0x3F, // 0
    0x06, // 1
    0x5B, // 2
    0x4F, // 3
    0x66, // 4
    0x6D, // 5
    0x7D, // 6
    0x07, // 7
    0x7F, // 8
    0x6F  // 9
};

static void display7SEG(int num) {
    if (num < 0 || num > 9) return;
    uint8_t val = ~seg_code[num];
    for (int i = 0; i < 7; i++) {
        HAL_GPIO_WritePin(GPIOB, (GPIO_PIN_8 << i), (val & (1 << i)) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    }
}

static void update7SEG_Scanning(void) {
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6 | GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_9, GPIO_PIN_SET);

    display7SEG(display_buffer[scan_index]);

    if (scan_index == 0) HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_RESET);
    else if (scan_index == 1) HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, GPIO_PIN_RESET);
    else if (scan_index == 2) HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_RESET);
    else if (scan_index == 3) HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_RESET);

    scan_index = (scan_index + 1) % 4;
}

static void set_traffic_lights(int r1, int y1, int g1, int r2, int y2, int g2) {
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, r1 ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, y1 ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2, g1 ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, r2 ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, y2 ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, g2 ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void fsm_init(void) {
    current_mode = MODE_1_NORMAL;
    traffic_state = R1_RED_R2_GREEN;
    countdown_road1 = red_duration;
    countdown_road2 = green_duration;
    timer_traffic = 100;
    set_traffic_lights(1, 0, 0, 0, 0, 1);
}
void software_timer_run(void) {
    if (timer_traffic > 0) timer_traffic--;
    if (timer_blink > 0)   timer_blink--;
    if (timer_scan > 0)    timer_scan--;
    if (timer_auto_inc > 0) timer_auto_inc--;
}
void fsm_for_input_processing(void) {
    if (timer_scan <= 0) {
        timer_scan = 1;
        update7SEG_Scanning();
    }

    static uint8_t btn1_prev = 1;
    uint8_t btn1_cur = is_button_pressed(0);
    if (btn1_cur && !btn1_prev) {
        if (current_mode == MODE_1_NORMAL) {
            current_mode = MODE_2_MODIFY_RED;
            temp_duration = red_duration;
        } else if (current_mode == MODE_2_MODIFY_RED) {
            current_mode = MODE_3_MODIFY_AMBER;
            temp_duration = amber_duration;
        } else if (current_mode == MODE_3_MODIFY_AMBER) {
            current_mode = MODE_4_MODIFY_GREEN;
            temp_duration = green_duration;
        } else {
            fsm_init();
        }
    }
    btn1_prev = btn1_cur;

    if (current_mode == MODE_1_NORMAL) {
        display_buffer[0] = countdown_road1 / 10;
        display_buffer[1] = countdown_road1 % 10;
        display_buffer[2] = countdown_road2 / 10;
        display_buffer[3] = countdown_road2 % 10;

        if (timer_traffic <= 0) {
            timer_traffic = 100;
            countdown_road1--;
            countdown_road2--;

            switch (traffic_state) {
                case R1_RED_R2_GREEN:
                    if (countdown_road2 <= 0) {
                        traffic_state = R1_RED_R2_AMBER;
                        countdown_road2 = amber_duration;
                    }
                    break;
                case R1_RED_R2_AMBER:
                    if (countdown_road2 <= 0) {
                        traffic_state = R1_GREEN_R2_RED;
                        countdown_road1 = green_duration;
                        countdown_road2 = red_duration;
                    }
                    break;
                case R1_GREEN_R2_RED:
                    if (countdown_road1 <= 0) {
                        traffic_state = R1_AMBER_R2_RED;
                        countdown_road1 = amber_duration;
                    }
                    break;
                case R1_AMBER_R2_RED:
                    if (countdown_road1 <= 0) {
                        traffic_state = R1_RED_R2_GREEN;
                        countdown_road1 = red_duration;
                        countdown_road2 = green_duration;
                    }
                    break;
            }

            switch (traffic_state) {
                case R1_RED_R2_GREEN: set_traffic_lights(1, 0, 0, 0, 0, 1); break;
                case R1_RED_R2_AMBER: set_traffic_lights(1, 0, 0, 0, 1, 0); break;
                case R1_GREEN_R2_RED: set_traffic_lights(0, 0, 1, 1, 0, 0); break;
                case R1_AMBER_R2_RED: set_traffic_lights(0, 1, 0, 1, 0, 0); break;
            }
        }
    } else {
        display_buffer[0] = temp_duration / 10;
        display_buffer[1] = temp_duration % 10;
        display_buffer[2] = 0;
        display_buffer[3] = current_mode;

        if (timer_blink <= 0) {
            timer_blink = 25;
            blink_state = !blink_state;
            if (current_mode == MODE_2_MODIFY_RED)        set_traffic_lights(blink_state, 0, 0, blink_state, 0, 0);
            else if (current_mode == MODE_3_MODIFY_AMBER) set_traffic_lights(0, blink_state, 0, 0, blink_state, 0);
            else if (current_mode == MODE_4_MODIFY_GREEN) set_traffic_lights(0, 0, blink_state, 0, 0, blink_state);
        }

        static uint8_t btn2_prev = 1;
        uint8_t btn2_cur = is_button_pressed(1);
        if (btn2_cur && !btn2_prev) {
            temp_duration++;
            if (temp_duration > 99) temp_duration = 1;
            timer_auto_inc = 50;
        } else if (is_button_pressed_1s(1)) {
            if (timer_auto_inc <= 0) {
                timer_auto_inc = 20;
                temp_duration++;
                if (temp_duration > 99) temp_duration = 1;
            }
        }
        btn2_prev = btn2_cur;

        if (is_button_pressed(2)) {
            if (current_mode == MODE_2_MODIFY_RED)        red_duration = temp_duration;
            else if (current_mode == MODE_3_MODIFY_AMBER) amber_duration = temp_duration;
            else if (current_mode == MODE_4_MODIFY_GREEN) green_duration = temp_duration;
        }
    }
}
