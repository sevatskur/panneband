
#include "mcc_generated_files/system/system.h"
#include "mcc_generated_files/system/pins.h" 
#include "mcc_generated_files/vref/vref.h"
#include <avr/io.h>
#include <util/delay.h>
#include <stdio.h>

#define PIN_PW (1 << 0) // PD0 for pw pin
#define PIN_PWM (1 << 2)  // PB2 for vibration motor
#define PERIOD 0x9c3      // pwm variable
#define N 4 //For 2^N
#define CIRCULAR_BUFFER_SIZE (1 << N) //Gir bufferstørrelse 2^N

typedef struct {
    int16_t * const buffer;
    unsigned writeIndex;
} circular_buffer_t;

float calculate_distance(uint16_t adc_value);
void circular_buffer_write(circular_buffer_t *c, int16_t data);
int16_t circular_buffer_read(circular_buffer_t *c, unsigned Xn);
int32_t accumolator(int16_t sample);
uint8_t map_distance_to_duty_cycle(uint16_t distance);

int main(void) {
    // Initial setup
    PORTD.DIRCLR = PIN_PW; // Set pw pin as input
    PORTB.DIRSET = PIN_PWM; // Set pwm pin as output
    SYSTEM_Initialize();

    while (1) {

        uint16_t adc_result = ADC0_GetConversion(ADC0_IO_PD0); // Hent ADC-verdi
        float adc_result_flat = (float)accumolator(adc_result)/CIRCULAR_BUFFER_SIZE;
        float distance_cm = calculate_distance(adc_result_flat); // Kalkuler avstand
        printf("Distance: %.2f cm\r\n", distance_cm);
        
        uint8_t duty_cycle = map_distance_to_duty_cycle((uint16_t) distance_cm); // Map distance to duty cycle
        TCA1.SINGLE.CMP2 = (PERIOD * duty_cycle) / 100;

        //printf("PWM Duty Cycle: %.2f \r\n", pwm_percent); // Skriv ut PWM-verdi
        
        //_delay_ms(100); // Kort forsinkelse for å unngå raske variasjoner
    }
}

//int16_t circular_buffer_t c = {
   //     .buffer = bufferDataSpace,
     //   .writeIndex = 0
    //};

void circular_buffer_write(circular_buffer_t *c, int16_t data) {
    c -> buffer[c->writeIndex] = data;
    c->writeIndex = (c->writeIndex+1)%CIRCULAR_BUFFER_SIZE;
}

int16_t circular_buffer_read(circular_buffer_t *c, unsigned Xn) {
    return c->buffer[(c->writeIndex-Xn+CIRCULAR_BUFFER_SIZE-1)%CIRCULAR_BUFFER_SIZE];
}

int32_t accumolator(int16_t sample){
    //Deklarerer en ringbuffer med dataområde gitt av CIRCULAR_BUFFER_SIZE
    static int16_t bufferDataSpace[CIRCULAR_BUFFER_SIZE];
    static circular_buffer_t c = {
        .buffer = bufferDataSpace,
        .writeIndex = 0
    };
    //skriver til ringbufffer
    circular_buffer_write(&c, sample);
    
    static int32_t sum = 0;
    static int16_t counter = 0;
    //Sjekker om ringbufferen har blitt lagt sammen en gang
    if(counter > CIRCULAR_BUFFER_SIZE){
        //trekker fra edlste verdi og legger deretter på nyeste verdi
        sum -= circular_buffer_read(&c, CIRCULAR_BUFFER_SIZE-1);
        sum += circular_buffer_read(&c, 0);
    }
    //legger sammen alle verdiene i ringbufferen ved første gjennomkjøring
    else{
        for(uint8_t n = 0; n < CIRCULAR_BUFFER_SIZE; n++) {
        sum += (int32_t) c.buffer[n];
        counter++;
    }
    }
    return sum;
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
