#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include "embled.h"

typedef struct
{
    uint8_t state_num;
    uint16_t num_repetitions;
    uint16_t state_duration[EMBLED_MAX_STATES];
} embled_profiles_t;

typedef struct embled_params_s
{
    bool enabled;
    uint16_t pin;
    embled_driver_mode_t driver_mode;
    bool start_level;
    embled_modes_t mode;
    embled_levels_t level;
} embled_params_t;

typedef struct embled_led_ctrl_s
{
    embled_params_t active;
    embled_params_t backup;
    embled_params_t shadow;
    volatile bool shadow_pending;
    uint8_t current_state;
    uint32_t state_counter;
    uint16_t repetition_counter;
} embled_led_ctrl_t;

typedef struct
{
    bool running;
    embled_led_ctrl_t leds[EMBLED_NUM_LEDS];
    embled_callbacks_t* cbks;
} embled_ctrl_t;

embled_ctrl_t embled_control = {0};

const bool logic_levels[2][2] = {{true, false}, {false, true}};

embled_profiles_t embled_profile[EMBLED_MAX_MODES] = {
    [EMBLED_MODE_ON] = {.state_num = 1, .num_repetitions = 0, .state_duration = {1}},                                /* on */
    [EMBLED_MODE_OFF] = {.state_num = 1, .num_repetitions = 0, .state_duration = {0}},                               /* off */
    [EMBLED_MODE_PULSE_FAST_ONCE] = {.state_num = 1, .num_repetitions = 0, .state_duration = {2}},                   /* on once and off after */
    [EMBLED_MODE_PULSE_SLOW_ONCE] = {.state_num = 1, .num_repetitions = 0, .state_duration = {6}},                   /* on once and off after */
    [EMBLED_MODE_PULSE_FAST_DOUBLE_ONCE] = {.state_num = 2, .num_repetitions = 2, .state_duration = {1, 2}},         /* 2 quick blinks on and off after */
    [EMBLED_MODE_PULSE_SINGLE] = {.state_num = 2, .num_repetitions = 0xFFFF, .state_duration = {1, 19}},             /* 1 quick blink in an infinite 2s cycle */
    [EMBLED_MODE_PULSE_DOUBLE] = {.state_num = 4, .num_repetitions = 0xFFFF, .state_duration = {1, 2, 1, 16}},       /* 2 quick blinks in an infinite 2s cycle */
    [EMBLED_MODE_PULSE_TRIPLE] = {.state_num = 6, .num_repetitions = 0xFFFF, .state_duration = {1, 2, 1, 2, 1, 13}}, /* 3 quick blinks in an infinite 2s cycle */
    [EMBLED_MODE_BLINK_SLOW] = {.state_num = 2, .num_repetitions = 0xFFFF, .state_duration = {10, 10}},              /* blink 1Hz: 1s on and 1s off */
    [EMBLED_MODE_BLINK_MEDIUM] = {.state_num = 2, .num_repetitions = 0xFFFF, .state_duration = {5, 5}},              /* blink 2Hz: 0.5s on and 0.5s off */
    [EMBLED_MODE_BLINK_FAST] = {.state_num = 2, .num_repetitions = 0xFFFF, .state_duration = {2, 2}},                /* blink 5Hz: 0.2s on and 0.2s off */
    [EMBLED_MODE_DYNAMIC_1] = {.state_num = 1, .num_repetitions = 0, .state_duration = {0}},                         /* default off */
    [EMBLED_MODE_DYNAMIC_2] = {.state_num = 1, .num_repetitions = 0, .state_duration = {0}},                         /* default off */
    [EMBLED_MODE_DYNAMIC_3] = {.state_num = 1, .num_repetitions = 0, .state_duration = {0}},                         /* default off */
    [EMBLED_MODE_DYNAMIC_4] = {.state_num = 1, .num_repetitions = 0, .state_duration = {0}},                         /* default off */
    [EMBLED_MODE_DYNAMIC_5] = {.state_num = 1, .num_repetitions = 0, .state_duration = {0}},                         /* default off */
};

/**
 * @brief  	    Ends the control process of an LED, if there are no more LEDs running, pauses the task in the scheduler
 *
 * @param[in]	Pointer to the LED control structure
 * @param[in]	Index of the finished LED
 * @return 	    Null
 */
static void embled_finish(embled_ctrl_t* embled_ctrl, uint32_t led);

/**
 * @brief  	    Activates the LED according to the configured mode
 *
 * @param[in]	Pointer to the LED control structure
 * @param[in]	Index of the finished LED
 * @param[in]	State activated or not
 * @return 	    Null
 */
static void embled_driver_activate(embled_ctrl_t* embled_ctrl, uint32_t id, bool state);


static void embled_finish(embled_ctrl_t* embled_ctrl, uint32_t led)
{
    if(embled_ctrl->leds[led].backup.enabled)
    {
        embled_ctrl->leds[led].active = embled_ctrl->leds[led].backup;
        memset(&embled_ctrl->leds[led].backup, 0, sizeof(embled_params_t));
        embled_ctrl->leds[led].repetition_counter = 0;
        embled_ctrl->leds[led].state_counter = 0;
    }
    else
        embled_ctrl->leds[led].active.enabled = false;
}

static void embled_driver_activate(embled_ctrl_t* embled_ctrl, uint32_t id, bool state)
{
    if(embled_ctrl->leds[id].active.driver_mode == EMBLED_DRIVER_MODE_DIGITAL)
    {
        if(embled_ctrl->cbks->write_gpio != NULL)
            embled_ctrl->cbks->write_gpio(embled_ctrl->leds[id].active.pin, logic_levels[embled_ctrl->leds[id].active.level][(state)]);
    }
    else if(embled_ctrl->leds[id].active.driver_mode == EMBLED_DRIVER_MODE_PWM)
    {
        if(state)
        {
            if(embled_ctrl->cbks->start_pwm != NULL)
                embled_ctrl->cbks->start_pwm(embled_ctrl->leds[id].active.pin);
        }
        else
        {
            if(embled_ctrl->cbks->stop_pwm != NULL)
                embled_ctrl->cbks->stop_pwm(embled_ctrl->leds[id].active.pin);
        }
    }
}

static inline void embled_reset_dynamic(embled_modes_t mode)
{
    if(mode >= EMBLED_START_DYNAMIC)
        memset(embled_profile[mode].state_duration, 0, sizeof(embled_profile[mode].state_duration));
}

void embled_task(void* argument)
{
    uint8_t mode;

    for(uint8_t led = 0; led < EMBLED_NUM_LEDS; led++)
    {
        embled_led_ctrl_t* ctrl = &embled_control.leds[led];

        if(ctrl->shadow_pending)
        {
            ctrl->active = ctrl->shadow;
            ctrl->current_state = 0;
            ctrl->state_counter = 0;
            ctrl->repetition_counter = 0;
            ctrl->shadow.enabled = false;
            ctrl->shadow_pending = false;

            if(ctrl->active.mode >= EMBLED_MODE_PULSE_FAST_ONCE)    /* always init the pin in ON mode */
                embled_driver_activate(&embled_control, led, true);
        }
        else if(!ctrl->active.enabled)
            continue;

        mode = ctrl->active.mode;
        if(mode == EMBLED_MODE_ON)
        {
            embled_driver_activate(&embled_control, led, true);
            continue;
        }
        else if(mode == EMBLED_MODE_OFF)
        {
            embled_driver_activate(&embled_control, led, false);
            embled_finish(&embled_control, led);
            embled_reset_dynamic(ctrl->active.mode);
            continue;
        }

        uint16_t state_duration_ticks = embled_profile[mode].state_duration[ctrl->current_state];
        if(++ctrl->state_counter < state_duration_ticks)
            continue;

        if(embled_profile[mode].state_num == 1)
        {
            embled_driver_activate(&embled_control, led, false);
            embled_finish(&embled_control, led);
            embled_reset_dynamic(ctrl->active.mode);
            ctrl->state_counter = state_duration_ticks; /* prevents reactivation */
        }
        else if(embled_profile[mode].num_repetitions == EMBLED_INFINITE || ctrl->repetition_counter < embled_profile[mode].num_repetitions)
        {
            ctrl->state_counter = 0;
            if(++ctrl->current_state >= embled_profile[mode].state_num)
            {
                ctrl->repetition_counter++;
                ctrl->current_state = 0;
            }
            /* even indices are related to off state, odd to on state */
            embled_driver_activate(&embled_control, led, logic_levels[ctrl->active.level][((ctrl->current_state & 0x01) ? 0 : 1)]);
        }
        else if(embled_profile[mode].num_repetitions != EMBLED_INFINITE)
        {
            if(ctrl->repetition_counter == embled_profile[mode].num_repetitions)
            {
                embled_driver_activate(&embled_control, led, ctrl->active.start_level);
                embled_finish(&embled_control, led);
                embled_reset_dynamic(ctrl->active.mode);
            }
        }
    }
}

bool embled_set_mode(uint16_t pin, embled_driver_mode_t driver_mode, embled_modes_t new_mode, embled_levels_t level, bool return_last_mode)
{
    uint32_t led;
    int32_t index = -1;
    bool embled_overwrite = false;
    bool start_level = false;

    if((!embled_control.running) || (new_mode >= EMBLED_MAX_MODES))
        return false;
    if(driver_mode == EMBLED_DRIVER_MODE_DIGITAL && embled_control.cbks->write_gpio == NULL)
        return false;
    if(driver_mode == EMBLED_DRIVER_MODE_PWM && (embled_control.cbks->start_pwm == NULL || embled_control.cbks->stop_pwm == NULL))
        return false;

    for(led = 0; led < EMBLED_NUM_LEDS; led++)
    {
        if((embled_control.leds[led].active.enabled == true) && (embled_control.leds[led].active.pin == pin))
        {
            embled_overwrite = true;
            break;
        }
        else if(embled_control.leds[led].active.enabled == false && embled_control.leds[led].shadow.enabled == false)
        {
            index = led;
        }
    }
    if(embled_overwrite && return_last_mode)
        embled_control.leds[led].backup = embled_control.leds[led].active;

    if(led == EMBLED_NUM_LEDS) /* new pin, allocate a new slot */
    {
        if(index == -1)
            return false;
        led = index;

        if(driver_mode == EMBLED_DRIVER_MODE_DIGITAL && embled_control.cbks->read_gpio != NULL)
            start_level = embled_control.cbks->read_gpio(pin);
        embled_control.leds[led].shadow.start_level = start_level;
    }
    else if(new_mode != embled_control.leds[led].active.mode || embled_profile[new_mode].state_num == 1) /* pin already allocated, update mode pending */
    {
        if(embled_control.leds[led].shadow_pending) /* block update, shadow pending yet */
            return false;
        if(embled_control.leds[led].active.driver_mode != driver_mode) /* cannot change driver mode of an allocated and running pin */
            return false;
        embled_control.leds[led].shadow.start_level = embled_control.leds[led].active.start_level;
    }

    embled_control.leds[led].shadow.mode = new_mode;
    embled_control.leds[led].shadow.driver_mode = driver_mode;
    embled_control.leds[led].shadow.pin = pin;
    embled_control.leds[led].shadow.level = level;
    embled_control.leds[led].shadow.enabled = true;
    embled_control.leds[led].shadow_pending = true;

    return true;
}

int8_t embled_new_profile(uint8_t num_states, uint16_t num_repetitions, uint16_t* duration_ms)
{
    int8_t index = -1;
    uint8_t state_cnt;
    if(num_states > EMBLED_MAX_STATES)
        return -1;

    for(uint8_t pos = EMBLED_START_DYNAMIC; pos < EMBLED_MAX_MODES; pos++)
    {
        if((embled_profile[pos].state_num == num_states) && (embled_profile[pos].num_repetitions == num_repetitions))
        {
            state_cnt = 0;

            for(uint8_t state = 0; state < num_states; state++)
            {
                if(duration_ms[state] == embled_profile[pos].state_duration[state] * EMBLED_CYCLE_TIME_MS)
                    state_cnt++;
            }
            if(state_cnt == num_states)
                return pos;
        }
    }
    for(uint8_t pos = EMBLED_START_DYNAMIC; pos < EMBLED_MAX_MODES; pos++)
    {
        if(embled_profile[pos].state_duration[0] == 0)
        {
            index = pos;

            embled_profile[pos].state_num = num_states;
            embled_profile[pos].num_repetitions = num_repetitions;
            for(uint8_t state = 0; state < num_states; state++)
            {
                embled_profile[pos].state_duration[state] = EMBLED_CALC_TICKS(*duration_ms);
                duration_ms++;
            }
            break;
        }
    }

    return index;
}

void embled_init(embled_callbacks_t* cbks)
{
    for(uint8_t id = 0; id < EMBLED_NUM_LEDS; id++)
    {
        embled_control.leds[id].active.enabled = false;
        embled_control.leds[id].shadow.enabled = false;
        embled_control.leds[id].backup.enabled = false;
        embled_control.leds[id].shadow_pending = false;
    }

    embled_control.cbks = cbks;
    embled_control.running = true;
}