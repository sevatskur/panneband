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
float map_to_pwm(uint16_t pwm_value);
void circular_buffer_write(circular_buffer_t *c, int16_t data);
int16_t circular_buffer_read(circular_buffer_t *c, unsigned Xn);
int32_t accumolator(int16_t sample);

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
        
        // Juster PWM duty cycle basert på avstand
        uint16_t pwm_value = (distance_cm <= 100) ? (uint16_t)(distance_cm) : 100; // Avgrens til 100% PWM
        float pwm_percent = map_to_pwm(pwm_value); // Sett PWM

        //printf("PWM Duty Cycle: %.2f \r\n", pwm_percent); // Skriv ut PWM-verdi
        
        _delay_ms(100); // Kort forsinkelse for å unngå raske variasjoner
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

float map_to_pwm(uint16_t pwm_value) {

    if (pwm_value > 100) {
        pwm_value = 100;
    }

    uint16_t current_pwm_value = (PERIOD * pwm_value) / 100;

    TCA1_Compare2Set(current_pwm_value); //sets pwm to channel 2

    float pwm = (float) current_pwm_value / PERIOD * 100;

    return pwm;
}
