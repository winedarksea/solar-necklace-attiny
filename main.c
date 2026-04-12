/*
 * ATtiny416 LED Scintillation Firmware
 * Solar-powered moissanite necklace — drives 4 LEDs via NMOS FETs
 * in a pseudo-random pattern to simulate diamond scintillation.
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <util/delay.h>

/* ADC thresholds (10-bit, 2.5V internal reference) */
#define ADC_THRESHOLD_SHUTDOWN  205   /* 0.5V */
#define ADC_THRESHOLD_TURBO     737   /* 1.8V */

/* ADC check interval: every 8th PIT wake = ~2 seconds at 4Hz */
#define ADC_CHECK_INTERVAL  8

/* LED index mapping:
 *   0 = FET1 / PB3
 *   1 = FET2 / PA2
 *   2 = FET3 / PC2  (boot default)
 *   3 = FET4 / PC1
 */
#define BOOT_LED  2

/* ── Global state ────────────────────────────────────────────── */

static uint16_t lfsr;
static uint8_t  current_led = BOOT_LED;
static uint8_t  adc_counter;
static uint8_t  turbo_available;

/* ── PRNG ────────────────────────────────────────────────────── */

static uint8_t prng_next(void)
{
    uint8_t lsb = lfsr & 1;
    lfsr >>= 1;
    if (lsb)
        lfsr ^= 0xB400;   /* maximal-length 16-bit Galois LFSR taps */
    return (uint8_t)lfsr;
}

static uint8_t next_random_led(uint8_t current)
{
    uint8_t next;
    do {
        next = prng_next() & 0x03;
    } while (next == current);
    return next;
}

/* ── LED control ─────────────────────────────────────────────── */

static void led_on(uint8_t idx)
{
    switch (idx) {
    case 0: PORTB.OUTSET = PIN3_bm; break;
    case 1: PORTA.OUTSET = PIN2_bm; break;
    case 2: PORTC.OUTSET = PIN2_bm; break;
    case 3: PORTC.OUTSET = PIN1_bm; break;
    }
}

static void led_off(uint8_t idx)
{
    switch (idx) {
    case 0: PORTB.OUTCLR = PIN3_bm; break;
    case 1: PORTA.OUTCLR = PIN2_bm; break;
    case 2: PORTC.OUTCLR = PIN2_bm; break;
    case 3: PORTC.OUTCLR = PIN1_bm; break;
    }
}

static void all_leds_off(void)
{
    PORTB.OUTCLR = PIN3_bm;
    PORTA.OUTCLR = PIN2_bm;
    PORTC.OUTCLR = PIN1_bm | PIN2_bm;
}

/* ── ADC ─────────────────────────────────────────────────────── */

static uint16_t adc_read(void)
{
    ADC0.COMMAND = ADC_STCONV_bm;
    while (!(ADC0.INTFLAGS & ADC_RESRDY_bm))
        ;
    return ADC0.RES;
}

/* ── Shutdown ────────────────────────────────────────────────── */

static void shutdown(void)
{
    all_leds_off();
    PORTA.OUTCLR = PIN4_bm;              /* turbo off */

    ADC0.CTRLA &= ~ADC_ENABLE_bm;        /* disable ADC */

    while (RTC.STATUS & RTC_CTRLABUSY_bm)
        ;
    RTC.PITCTRLA = 0;                     /* disable PIT */

    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
    sleep_mode();
    /* MCU stays here until power cycles (power-on reset) */
}

/* ── PIT ISR (wake only) ─────────────────────────────────────── */

ISR(RTC_PIT_vect)
{
    RTC.PITINTFLAGS = RTC_PI_bm;
}

/* ── Initialization ──────────────────────────────────────────── */

static void init_gpio(void)
{
    /* LED gate outputs */
    PORTB.DIRSET = PIN3_bm;              /* PB3 = FET1 */
    PORTA.DIRSET = PIN2_bm;              /* PA2 = FET2 */
    PORTC.DIRSET = PIN1_bm | PIN2_bm;   /* PC1 = FET4, PC2 = FET3 */

    /* Turbo output */
    PORTA.DIRSET = PIN4_bm;              /* PA4 = TURBO */

    /* Boot LED on immediately (regulator needs load) */
    led_on(BOOT_LED);

    /* Disable digital input buffers on all pins for power saving.
     * Skip PA0 (UPDI). Outputs and analog pins don't need them;
     * unused pins must not float. */
    PORTA.PIN1CTRL = PORT_ISC_INPUT_DISABLE_gc;
    PORTA.PIN2CTRL = PORT_ISC_INPUT_DISABLE_gc;
    PORTA.PIN3CTRL = PORT_ISC_INPUT_DISABLE_gc;
    PORTA.PIN4CTRL = PORT_ISC_INPUT_DISABLE_gc;
    PORTA.PIN5CTRL = PORT_ISC_INPUT_DISABLE_gc;
    PORTA.PIN6CTRL = PORT_ISC_INPUT_DISABLE_gc;
    PORTA.PIN7CTRL = PORT_ISC_INPUT_DISABLE_gc;

    PORTB.PIN0CTRL = PORT_ISC_INPUT_DISABLE_gc;
    PORTB.PIN1CTRL = PORT_ISC_INPUT_DISABLE_gc;
    PORTB.PIN2CTRL = PORT_ISC_INPUT_DISABLE_gc;
    PORTB.PIN3CTRL = PORT_ISC_INPUT_DISABLE_gc;
    PORTB.PIN4CTRL = PORT_ISC_INPUT_DISABLE_gc;  /* analog — must disable */
    PORTB.PIN5CTRL = PORT_ISC_INPUT_DISABLE_gc;

    PORTC.PIN0CTRL = PORT_ISC_INPUT_DISABLE_gc;
    PORTC.PIN1CTRL = PORT_ISC_INPUT_DISABLE_gc;
    PORTC.PIN2CTRL = PORT_ISC_INPUT_DISABLE_gc;
    PORTC.PIN3CTRL = PORT_ISC_INPUT_DISABLE_gc;
}

static void init_adc(void)
{
    VREF.CTRLA = VREF_ADC0REFSEL_2V5_gc;
    ADC0.CTRLC = ADC_SAMPCAP_bm | ADC_REFSEL_INTREF_gc | ADC_PRESC_DIV16_gc;
    ADC0.MUXPOS = ADC_MUXPOS_AIN9_gc;   /* PB4 */
    ADC0.CTRLA = ADC_ENABLE_bm;
}

static void init_pit(void)
{
    while (RTC.STATUS & RTC_CTRLABUSY_bm)
        ;
    RTC.CLKSEL = RTC_CLKSEL_INT32K_gc;
    RTC.PITINTCTRL = RTC_PI_bm;
    RTC.PITCTRLA = RTC_PERIOD_CYC8192_gc | RTC_PITEN_bm;
}

static void seed_prng(void)
{
    uint16_t seed = adc_read();
    if (seed == 0)
        seed = 0xACE1;
    lfsr = seed;
}

/* ── Main ────────────────────────────────────────────────────── */

int main(void)
{
    init_gpio();
    init_adc();
    seed_prng();
    init_pit();

    set_sleep_mode(SLEEP_MODE_STANDBY);
    sei();

    while (1) {
        sleep_mode();

        /* Select next LED (guaranteed different from current) */
        uint8_t next = next_random_led(current_led);

        /* Make-before-break: new on, then old off */
        led_on(next);
        led_off(current_led);
        current_led = next;

        /* Periodic ADC voltage check */
        if (++adc_counter >= ADC_CHECK_INTERVAL) {
            adc_counter = 0;
            uint16_t voltage = adc_read();

            if (voltage < ADC_THRESHOLD_SHUTDOWN)
                shutdown();

            turbo_available = (voltage > ADC_THRESHOLD_TURBO);
        }

        /* Occasional turbo pulse (~12.5% chance per transition when available) */
        if (turbo_available && (prng_next() & 0x07) == 0) {
            PORTA.OUTSET = PIN4_bm;
            _delay_us(300);
            PORTA.OUTCLR = PIN4_bm;
        }
    }
}
