#include "input_reading.h"

#define DURATION_FOR_AUTO_INCREASING 100 // 100 * 10ms = 1s
#define BUTTON_IS_PRESSED  GPIO_PIN_RESET
#define BUTTON_IS_RELEASED GPIO_PIN_SET

static GPIO_PinState buttonBuffer[NO_OF_BUTTONS] = {BUTTON_IS_RELEASED, BUTTON_IS_RELEASED, BUTTON_IS_RELEASED};
static GPIO_PinState debounceButtonBuffer1[NO_OF_BUTTONS] = {BUTTON_IS_RELEASED, BUTTON_IS_RELEASED, BUTTON_IS_RELEASED};
static GPIO_PinState debounceButtonBuffer2[NO_OF_BUTTONS] = {BUTTON_IS_RELEASED, BUTTON_IS_RELEASED, BUTTON_IS_RELEASED};

static uint8_t flagForButtonPress1s[NO_OF_BUTTONS] = {0};
static uint16_t counterForButtonPress1s[NO_OF_BUTTONS] = {0};

void button_reading(void) {
    for (char i = 0; i < NO_OF_BUTTONS; i++) {
        debounceButtonBuffer2[i] = debounceButtonBuffer1[i];

        if (i == 0) debounceButtonBuffer1[i] = HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_0);
        else if (i == 1) debounceButtonBuffer1[i] = HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_1);
        else if (i == 2) debounceButtonBuffer1[i] = HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_2);

        if (debounceButtonBuffer1[i] == debounceButtonBuffer2[i]) {
            buttonBuffer[i] = debounceButtonBuffer1[i];

            if (buttonBuffer[i] == BUTTON_IS_PRESSED) {
                if (counterForButtonPress1s[i] < DURATION_FOR_AUTO_INCREASING) {
                    counterForButtonPress1s[i]++;
                } else {
                    flagForButtonPress1s[i] = 1;
                    counterForButtonPress1s[i] = DURATION_FOR_AUTO_INCREASING - 20;
                }
            } else {
                counterForButtonPress1s[i] = 0;
                flagForButtonPress1s[i] = 0;
            }
        }
    }
}

unsigned char is_button_pressed(unsigned char index) {
    if (index >= NO_OF_BUTTONS) return 0;
    return (buttonBuffer[index] == BUTTON_IS_PRESSED);
}

unsigned char is_button_pressed_1s(unsigned char index) {
    if (index >= NO_OF_BUTTONS) return 0;
    if (flagForButtonPress1s[index] == 1) {
            flagForButtonPress1s[index] = 0;
            return 1;
        }
        return 0;
}
