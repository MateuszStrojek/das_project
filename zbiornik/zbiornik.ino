#include <Arduino.h>
#include "SimpleModbusSlave.h"

// - - - - - - - - - - - - - - d e f i n e s - - - - - - - - - - - - - - - - -|
//                         < =  m o d b u s
#define SLAVE_ID 2
#define COM_BUADRATE 9600

//                         < =  l e d    p i n s
#define LED_LVL_WATER_1 9   //1 dioda niebieska pwm
#define LED_LVL_WATER_2 6   //2 dioda niebieska pwm
#define LED_LVL_WATER_3 5   //3 dioda niebieska pwm
#define LED_LVL_WATER_4 3   //4 dioda niebieska pwm
#define LED_TEMP_LVL 10      //dioda pomaranczowa pwm

#define LED_SENS_X1 7       //1 dioda czerwona
#define LED_SENS_X2 4       //2 dioda czerwona
#define LED_SENS_X3 2       //3 dioda czerwona
#define LED_SENS_TEMP 8     //4 dioda czerwona

#define LED_PUMP_Z1 11       //1 dioda zielona - pompa z1
#define LED_PUMP_Z2 12       //2 dioda zielona - pompa z2
#define LED_HEATER_G 13      //3 dioda zielona - grzalka


//* 4 diody niebieskie = stan wody
//* 1 dioda pomarancozwa = temperatura
//* 4 diody czerwone = czujniki wody/temp
//* 3 diody zielone = pompy/grzalka

// - - - - - - - - - - - - - v a r i a b l e s - - - - - - - - - - - - - - - -|

int X1 = 0;
int X2 = 0;
int X3 = 0;
int Z1 = 0;
int Z2 = 0;
int G = 0;
int T = 0;

enum
{
    manual,
    automatic
} ControlType;

enum
{
    HR_LVL_WATER,
    HR_TEMP_WATER,
    HR_SENS_X1,
    HR_SENS_X2,
    HR_SENS_X3,
    HR_SENS_TEMP,
    HR_PUMP_Z1,
    HR_PUMP_Z2,
    HR_HEATER_G,
    HR_SP_LVL_WATER,
    HR_SP_TEMP_WATER,
    HR_CONTROL_TYPE,
    HR_SIZE
} HR_vars;

int leds_array[] = {
    LED_LVL_WATER_1,
    LED_LVL_WATER_2,
    LED_LVL_WATER_3,
    LED_LVL_WATER_4,
    LED_SENS_X1,
    LED_SENS_X2,
    LED_SENS_X3,
    LED_PUMP_Z1,
    LED_PUMP_Z2,
    LED_HEATER_G};

unsigned int holdingRegs[HR_SIZE];

int max_lvl_water = 1024;
int min_lvl_water = 0;

volatile uint8_t timer_cnt = 0;

// - - - - - - - - - - - - - - - s e t u p - - - - - - - - - - - - - - - - - -|

void setup()
{
    modbus_init();
    open_leds_pins();
    values_init();
    timer_init();
    //pinMode(9, OUTPUT);
    //digitalWrite(9, HIGH);
}

// - - - - - - - - - - - - - - - l o o p  - - - - - - - - - - - - - - - - - -|

int state = 0;

void loop()
{
    modbus_update();
    user_function();
    //automatic_control();
    modbus_frame_update();
    simualtion_leds();
}

// - - - - - - - - - - - - - f u n c t i o n s - - - - - - - - - - - - - - - -|

void modbus_init()
{
    modbus_configure(&Serial, COM_BUADRATE, SERIAL_8N2, SLAVE_ID, 1,
                     HR_SIZE, holdingRegs);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -|

void open_leds_pins()
{
    int leds_array_size = sizeof(leds_array) / sizeof(leds_array[0]);
    for (int i = 0; i < leds_array_size; i++)
    {
        pinMode(leds_array[i], OUTPUT);
    }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -|

void values_init()
{
    holdingRegs[HR_LVL_WATER] = 0;
    holdingRegs[HR_TEMP_WATER] = 10;
    holdingRegs[HR_SENS_X1] = 1;
    holdingRegs[HR_SENS_X2] = 1;
    holdingRegs[HR_SENS_X3] = 1;
    holdingRegs[HR_SENS_TEMP] = 0;
    holdingRegs[HR_PUMP_Z1] = 0;
    holdingRegs[HR_PUMP_Z2] = 1;
    holdingRegs[HR_HEATER_G] = 0;
    holdingRegs[HR_SP_LVL_WATER] = 500;
    holdingRegs[HR_SP_TEMP_WATER] = 10;
    holdingRegs[HR_CONTROL_TYPE] = manual;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -|

void timer_init()
{
    cli();
    TCCR1A = 0;
    TCCR1B = 0;
    // 5 sec
    TCNT1 = 49910;
    TCCR1B |= (1 << CS12) | (1 << CS10);
    TIMSK1 |= (1 << TOIE1);
    sei();
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -|

ISR(TIMER1_OVF_vect)
{
    TCNT1 = 49910;
    timer_cnt++;

    if (timer_cnt > 4)
    {
        //digitalWrite(9, !digitalRead(9));
        simulation_values();
        timer_cnt = 0;
    }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -|

void simulation_values()
{
    simulation_water_lvl();
    simulation_water_temp();
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -|

void simulation_water_lvl()
{
    if (!holdingRegs[HR_PUMP_Z1] && holdingRegs[HR_PUMP_Z2])
    {
        if (holdingRegs[HR_LVL_WATER] > 0)
        {
            change_water_parameter(HR_LVL_WATER, '-', 50);
        }
    }
    else if (holdingRegs[HR_PUMP_Z1] && !holdingRegs[HR_PUMP_Z2])
    {
        change_water_parameter(HR_LVL_WATER, '+', 50);
    }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -|

void simulation_water_temp()
{
    if (holdingRegs[HR_HEATER_G])
    {
        change_water_parameter(HR_TEMP_WATER, '+', 2);
    }
    else if (!holdingRegs[HR_HEATER_G] && holdingRegs[HR_TEMP_WATER] > 10)
    {
        change_water_parameter(HR_TEMP_WATER, '-', 1);
    }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -|

void change_water_parameter(int hr_param, char sign, int change_lvl)
{
    if (sign == '-')
    {
        holdingRegs[hr_param] = holdingRegs[hr_param] - change_lvl < 0
                                    ? 0
                                    : holdingRegs[hr_param] - change_lvl;
    }
    if (sign == '+')
    {
        holdingRegs[hr_param] = holdingRegs[hr_param] + change_lvl;
    }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -|

void simualtion_leds()
{
    water_lvl_led_shine(LED_LVL_WATER_4, 3);
    water_lvl_led_shine(LED_LVL_WATER_3, 2);
    water_lvl_led_shine(LED_LVL_WATER_2, 1);
    water_lvl_led_shine(LED_LVL_WATER_1, 0);

    temp_lvl_led_shine(LED_TEMP_LVL);

    device_led_state();
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -|

void water_lvl_led_shine(uint8_t led_pin, int lvl_formula)
{
    int new_lvl = holdingRegs[HR_LVL_WATER] - (lvl_formula * 255);

    if (new_lvl > 255)
    {
        new_lvl = 255;
    }
    else if (new_lvl < 0)
    {
        new_lvl = 0;
    }
    analogWrite(led_pin, new_lvl);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -|

void temp_lvl_led_shine(uint8_t led_pin)
{
    int new_lvl = map(holdingRegs[HR_TEMP_WATER], 0,
                      holdingRegs[HR_SP_TEMP_WATER], 0, 255);

    analogWrite(led_pin, new_lvl);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -|

void device_led_state()
{
    digitalWrite(LED_SENS_X1, holdingRegs[HR_SENS_X1]);
    digitalWrite(LED_SENS_X2, holdingRegs[HR_SENS_X2]);
    digitalWrite(LED_SENS_X3, holdingRegs[HR_SENS_X3]);

    digitalWrite(LED_PUMP_Z1, holdingRegs[HR_PUMP_Z1]);
    digitalWrite(LED_PUMP_Z2, holdingRegs[HR_PUMP_Z2]);

    digitalWrite(LED_HEATER_G, holdingRegs[HR_HEATER_G]);

    digitalWrite(LED_SENS_TEMP, holdingRegs[HR_SENS_TEMP]);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -|

void modbus_frame_update()
{
    holdingRegs[HR_SENS_TEMP] =
        holdingRegs[HR_TEMP_WATER] >= holdingRegs[HR_SP_TEMP_WATER];
    holdingRegs[HR_SENS_X1] = holdingRegs[HR_LVL_WATER] >= 200;
    holdingRegs[HR_SENS_X2] = holdingRegs[HR_LVL_WATER] >= 600;
    holdingRegs[HR_SENS_X3] = holdingRegs[HR_LVL_WATER] >= 900;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -|

void automatic_control()
{
    if (holdingRegs[HR_CONTROL_TYPE] == 1)
    {
        if (holdingRegs[HR_LVL_WATER] > holdingRegs[HR_SP_LVL_WATER])
        {
            holdingRegs[HR_PUMP_Z1] = 0;
            holdingRegs[HR_PUMP_Z2] = 1;
        }
        else if (holdingRegs[HR_LVL_WATER] < holdingRegs[HR_SP_LVL_WATER])
        {
            holdingRegs[HR_PUMP_Z1] = 1;
            holdingRegs[HR_PUMP_Z2] = 0;
        }
        else if (holdingRegs[HR_LVL_WATER] == holdingRegs[HR_SP_LVL_WATER])
        {
            holdingRegs[HR_PUMP_Z1] = 0;
            holdingRegs[HR_PUMP_Z2] = 0;
        }

        if (holdingRegs[HR_TEMP_WATER] >= holdingRegs[HR_SP_TEMP_WATER])
        {
            holdingRegs[HR_HEATER_G] = 0;
        }
        else if (holdingRegs[HR_TEMP_WATER] < holdingRegs[HR_SP_TEMP_WATER])
        {
            holdingRegs[HR_HEATER_G] = 1;
        }
    }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -|

void print_debug_log()
{
    Serial.println((String) "wl " + holdingRegs[HR_LVL_WATER] 
                    + " | wt " + holdingRegs[HR_TEMP_WATER] 
                    + " | z1 " + holdingRegs[HR_PUMP_Z1] 
                    + " | z2 " + holdingRegs[HR_PUMP_Z2] 
                    + " | x1 " + holdingRegs[HR_SENS_X1] 
                    + " | x2 " + holdingRegs[HR_SENS_X2] 
                    + " | x3 " + holdingRegs[HR_SENS_X3] 
                    + " | g " + holdingRegs[HR_HEATER_G] 
                    + " | st " + holdingRegs[HR_SENS_TEMP] 
                    + " | spl " + holdingRegs[HR_SP_LVL_WATER] 
                    + " | spt " + holdingRegs[HR_SP_TEMP_WATER] 
                    + " | ct " + holdingRegs[HR_CONTROL_TYPE]);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -|

void mapping_io()
{
    X1 = holdingRegs[HR_SENS_X1];
    X2 = holdingRegs[HR_SENS_X2];
    X3 = holdingRegs[HR_SENS_X3];
    T = holdingRegs[HR_TEMP_WATER];

    holdingRegs[HR_PUMP_Z1] = Z1;
    holdingRegs[HR_PUMP_Z2] = Z2;
    holdingRegs[HR_HEATER_G] = G;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -|

void user_function()
{
    if (holdingRegs[HR_CONTROL_TYPE] == automatic)
    {
        mapping_io();
        switch (state)
        {
        case 0:
            Z1 = 1;
            Z2 = 0;
            G = 0;
            if (X1)
                state = 1;
            break;
        case 1:
            Z1 = 1;
            Z2 = 0;
            G = 0;
            if (!X1)
                state = 0;
            if (X2)
                state = 2;
            break;
        case 2:
            Z1 = 1;
            Z2 = 0;
            G = 1;
            if (!X2)
                state = 1;
            if (X3)
                state = 3;
            break;
        case 3:
            Z1 = 0;
            Z2 = 0;
            G = 1;
            if (!X3)
                state = 2;
            if (T > 15)
                state = 4;
            break;
        case 4:
            Z1 = 0;
            Z2 = 1;
            G = 0;
            if (!X1)
                state = 0;
            break;
        }
    }
}
