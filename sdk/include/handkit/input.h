#ifndef HK_INPUT_H
#define HK_INPUT_H

/* Buttons as printed on the handheld: A is the right face button, B the bottom one.
 * HkInput reads them through SDL's gamepad mapping, so firmwares with a different
 * controller (muOS) work too. The values are the stock raw joystick indices, used when
 * a controller has no mapping. L2/R2 are axes: see left_trigger / right_trigger.
 * NextUI also calls the front F1/F2 buttons L4/R4.
 */
typedef enum HkButton {
    HK_BUTTON_B = 0,
    HK_BUTTON_A = 1,
    HK_BUTTON_Y = 2,
    HK_BUTTON_X = 3,
    HK_BUTTON_L1 = 4,
    HK_BUTTON_R1 = 5,
    HK_BUTTON_SELECT = 6,
    HK_BUTTON_START = 7,
    HK_BUTTON_MENU = 8,
    HK_BUTTON_L3 = 9,
    HK_BUTTON_R3 = 10,
    HK_BUTTON_F1 = 11,
    HK_BUTTON_F2 = 12,
    HK_BUTTON_VOLUME_DOWN = 13,
    HK_BUTTON_VOLUME_UP = 14,
    HK_BUTTON_HOME = 15,
    HK_BUTTON_COUNT = 16
} HkButton;

#endif
