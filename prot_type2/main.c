#include "mcc_generated_files/system/system.h"
#include "mcc_generated_files/system/pins.h" 
#include "mcc_generated_files/vref/vref.h"
#include <avr/io.h>
#include <util/delay.h>
#include <stdio.h>

#define PIN_PW (1 << 0)   // PD0 for pw pin
#define PIN_PW_1 (1 << 1) // PD1 for pw pin 
#define PIN_PW_2 (1 << 2)
#define PIN_PWM (1 << 2)  // PB2 for vibration motor
#define PERIOD 0x9c3      // pwm variable
#define N 4               // For 2^N
#define CIRCULAR_BUFFER_SIZE (1 << N) //Gir bufferstørrelse 2^N

uint16_t adc_result[3]; // array to store data for each sensor
float adc_result_flat[3];
float distance_cm[3]; // array for 3 different distances
int adc_pins[] = {ADC0_IO_PD0, ADC0_IO_PD1, ADC0_IO_PD2}; // Distance sensor pins

typedef struct {
    int16_t * const buffer1; // buffer 1
    unsigned writeIndex1; // write index to buffer 1

    int16_t * const buffer2;
    unsigned writeIndex2;

    int16_t * const buffer3;
    unsigned writeIndex3;
} circular_buffer_t;


float calculate_distance(uint16_t adc_value);
void circular_buffer_write(circular_buffer_t *c, int16_t data, int8_t buffer_number);
int16_t circular_buffer_read(circular_buffer_t *c, unsigned Xn, int8_t buffer_number);
int32_t accumolator(int16_t sample, int8_t buffer_number);
uint8_t map_distance_to_duty_cycle(uint16_t distance);

int main(void) {
    // Initial setup
    PORTD.DIRCLR = PIN_PW;   // input sensor 1
    PORTD.DIRCLR = PIN_PW_1; // input sensor 2
    PORTD.DIRCLR = PIN_PW_2; // input sensor 3
    PORTB.DIRSET = PIN_PWM;  // Set pwm pin as output
    SYSTEM_Initialize();

    while (1) {

        for (int i = 0; i <3; i++) {
            adc_result[i] = ADC0_GetConversion(adc_pins[i]); // Hent ADC-verdi
            //printf("heipådei %u \r\n", adc_result[0]);
            adc_result_flat[i] = (float) accumolator(adc_result[i], i); /* / CIRCULAR_BUFFER_SIZE;*/
            printf("sum1: %.2f \r\n", adc_result_flat[0]);
        }
    }
}

//int16_t circular_buffer_t c = {
//     .buffer = bufferDataSpace,
//   .writeIndex = 0
//};

void circular_buffer_write(circular_buffer_t *c, int16_t data, int8_t buffer_number) {
    if (buffer_number == 0) {
        c -> buffer1[c->writeIndex1] = data;
        c->writeIndex1 = (c->writeIndex1 + 1) % CIRCULAR_BUFFER_SIZE;
    } else if (buffer_number == 1) {
        c -> buffer2[c->writeIndex2] = data;
        c->writeIndex2 = (c->writeIndex2 + 1) % CIRCULAR_BUFFER_SIZE;
    } else if (buffer_number == 2) {
        c -> buffer3[c->writeIndex3] = data;
        c->writeIndex3 = (c->writeIndex3 + 1) % CIRCULAR_BUFFER_SIZE;
    }
}

int16_t circular_buffer_read(circular_buffer_t *c, unsigned Xn, int8_t buffer_number) {
    if (buffer_number == 0) {
        return c->buffer1[(c->writeIndex1 - Xn + CIRCULAR_BUFFER_SIZE - 1) % CIRCULAR_BUFFER_SIZE];
    } else if (buffer_number == 1) {
        return c->buffer2[(c->writeIndex2 - Xn + CIRCULAR_BUFFER_SIZE - 1) % CIRCULAR_BUFFER_SIZE];
    } else if (buffer_number == 2) {
        return c->buffer3[(c->writeIndex3 - Xn + CIRCULAR_BUFFER_SIZE - 1) % CIRCULAR_BUFFER_SIZE];
    }
    return 0;
}

int32_t accumolator(int16_t sample, int8_t buffer_number) {
    //Deklarerer en ringbuffer med dataområde gitt av CIRCULAR_BUFFER_SIZE
    static int16_t bufferDataSpace1[CIRCULAR_BUFFER_SIZE];
    static int16_t bufferDataSpace2[CIRCULAR_BUFFER_SIZE];
    static int16_t bufferDataSpace3[CIRCULAR_BUFFER_SIZE];
    static int32_t sum[3] = {0};

    static circular_buffer_t c = {
        .buffer1 = bufferDataSpace1,
        .buffer2 = bufferDataSpace2,
        .buffer3 = bufferDataSpace3,
        .writeIndex1 = 0,
        .writeIndex2 = 0,
        .writeIndex3 = 0
    };
    //skriver til ringbufffer
    circular_buffer_write(&c, sample, buffer_number);

    static int16_t counter[] = {0, 0, 0, 0}; // we use elements 1, 2, and 3 only

    //Sjekker om ringbufferen har blitt lagt sammen en gang
    if (counter[buffer_number] >= CIRCULAR_BUFFER_SIZE) {
        //trekker fra edlste verdi og legger deretter på nyeste verdi
        sum[buffer_number] -= circular_buffer_read(&c, CIRCULAR_BUFFER_SIZE - 1, buffer_number);
        sum[buffer_number] += circular_buffer_read(&c, 0, buffer_number);
    }//legger sammen alle verdiene i ringbufferen ved første gjennomkjøring
    else {
        if (buffer_number == 0) {
            for (uint16_t n = 0; n < CIRCULAR_BUFFER_SIZE; n++) {
                sum[buffer_number] += (int32_t) c.buffer1[n];
            }
            counter[1]++;
        } else if (buffer_number == 1) {
            for (uint16_t n = 0; n < CIRCULAR_BUFFER_SIZE; n++) {
                sum[buffer_number] += (int32_t) c.buffer2[n];
            }
            counter[2]++;
        } else if (buffer_number == 2) {
            for (uint16_t n = 0; n < CIRCULAR_BUFFER_SIZE; n++) {
                sum[buffer_number] += (int32_t) c.buffer3[n];
            }
            counter[3]++;
        }
        counter[buffer_number] = CIRCULAR_BUFFER_SIZE;
    }
    //printf("sum1 = %u \r\n", sum[1]/CIRCULAR_BUFFER_SIZE);
    return sum[buffer_number] / CIRCULAR_BUFFER_SIZE;
}

float calculate_distance(uint16_t adc_value) {
    float distance = adc_value * 0.107; // scaling factor tested irl
    //printf("Distance: %.2f cm\r\n", distance);
    return distance;
}

uint8_t map_distance_to_duty_cycle(uint16_t distance) {
    uint16_t max_distance = 300; // chosen max distance
    uint8_t duty_cycle = 100 - (distance * 100 / max_distance);

    // Corrected clamping for the duty cycle range
    if (duty_cycle > 100) duty_cycle = 0;
    else if (duty_cycle < 0) duty_cycle = 100;

    //printf("Duty_cycle: %u\r\n", duty_cycle);
    return duty_cycle;
}
