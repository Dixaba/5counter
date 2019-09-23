#include <avr/io.h>
#include <avr/sleep.h>
#include <avr/interrupt.h>

// We want low power consumption
#undef F_CPU
#define F_CPU 128000L
#include <util/delay.h>

// Pin ports
#define PWM PB0
#define WHITE PB4
#define BUTTON PB1

// How long will be fade periods
#define DELAY 2

volatile bool pressed, released;
volatile uint8_t intime, time, result, BtnState;
uint8_t payCount;
bool lighting;

// Timer overflow interrupt
ISR(TIM0_OVF_vect)
{
  // Debounce code
  uint8_t state = !(PINB & (1 << BUTTON));
  static uint8_t currState = state;
  static uint8_t lastState = state;
  static uint16_t count = 0;

  //Just random fluctuations - skip it
  if (state != lastState)
    {
      count = 0;
      lastState = state;
      return;
    }

  count++;

  // It is real button state change
  if (count > 60 && state != currState)
    {
      // Which transition it is?
      if (state > 0)
        {
          pressed = true;
        }
      else
        {
          released = true;
        }

      currState = state;
    }

  lastState =  state;

  // Count fades to determine desired action
  if (pressed)
    {
      intime++;

      if (intime == DELAY)
        {
          intime = 0;

          if (++time == 255)
            {
              BtnState++;
            }
        }
    }
  else
    {
      intime = 0;
      time = 0;
      BtnState = 0;
    }
}

// External interrupt vector
// It basically wakes MCU up and disables itself
ISR(INT0_vect)
{
  GIMSK &= ~_BV(INT0);
}

// Long flashes to show count
void flash(uint8_t count)
{
  for (uint8_t i = 0; i < count; i++)
    {
      PORTB &= ~_BV(PWM);
      _delay_ms(500);
      PORTB |= _BV(PWM);
      _delay_ms(500);
    }
}

int main()
{
  // Set IO port mode
  DDRB |= _BV(PWM) | _BV(WHITE) ;
  PORTB |= _BV(BUTTON);
  // Timer
  TCCR0B |= _BV(CS00); // No prescaling
  TCCR0A = _BV(COM0A1) | _BV(WGM00); // Toggle OC0A, phase correct PWM
  TCNT0 = 0; //Timer value
  OCR0A = 0; // Compare value
  TIMSK0 |= _BV(TOIE0); // Enable interrupt
  TIFR0 |= _BV(TOV0);
  payCount = intime = time = 0;
  lighting = pressed = released = false;
  sei(); // Enable interrupts

  while (1)
    {
      PORTB |= _BV(PWM) | _BV(WHITE); // Disable output

      // If button is pressed, we just show fading LED
      // Brightness is increasing on every timer interrupt
      if (pressed)
        {
          OCR0A = 255 - time;
        }

      // Main logic block
      if (released)
        {
          pressed = false;
          released = false;

          // Was flashlight active?
          // If so - we need to disable it
          if (lighting)
            {
              TIMSK0 &= ~_BV(TOIE0); // Disable interrupt
              PORTB |= _BV(PWM);
              _delay_ms(250);
              PORTB |= _BV(WHITE);
              lighting = false;
              GIMSK |= _BV(INT0); //Ee-enable external interrupt
              set_sleep_mode(SLEEP_MODE_PWR_DOWN);
              sleep_mode();
              TIMSK0 |= _BV(TOIE0); // Enable interrupt
              sei();
            }
          else
            {
              TIMSK0 &= ~_BV(TOIE0); // Disable interrupt
              TCCR0A = 0; // Disable PWM
              TCNT0 = 0; //Timer value
              OCR0A = 0; // Compare value

              switch (BtnState) // Select action
                {
                  // 1st fade
                  // Show remaining value
                  case 0:
                  {
                    // Turn LED off
                    PORTB |= _BV(PWM);
                    _delay_ms(500);

                    // Did we reach value of 5?
                    if (payCount == 4)
                      {
                        // If so, just one long flash
                        PORTB &= ~_BV(PWM);
                        _delay_ms(1500);
                        PORTB |= _BV(PWM);
                      }
                    else
                      {
                        // If not - show how much left
                        flash(4 - payCount);
                      }

                    break;
                  }

                  // 2nd fade
                  // Increase value
                  case 1:
                  {
                    payCount++;

                    // If it is 5 - reset
                    if (payCount == 5)
                      {
                        payCount = 0;
                      }

                    // Two short and one long flashes
                    PORTB |= _BV(PWM);
                    _delay_ms(200);
                    PORTB &= ~_BV(PWM);
                    _delay_ms(200);
                    PORTB |= _BV(PWM);
                    _delay_ms(200);
                    PORTB &= ~_BV(PWM);
                    _delay_ms(200);
                    PORTB |= _BV(PWM);
                    _delay_ms(200);
                    PORTB &= ~_BV(PWM);
                    _delay_ms(500);
                    PORTB |= _BV(PWM);
                    break;
                  }

                  // 3 or more fades - just turn flashlight on
                  default:
                  {
                    PORTB &= ~_BV(WHITE);
                    lighting = true;
                    break;
                  }
                }

              // And now sleep!
              GIMSK |= _BV(INT0); //Re-enable external interrupt
              set_sleep_mode(SLEEP_MODE_PWR_DOWN);
              sleep_mode();
              // Here we wake up
              // It happens ONLY when button is pressed
              BtnState = 0; // Reset press state
              TIMSK0 |= _BV(TOIE0); // Enable interrupt
              PORTB |= _BV(PWM);
              TCNT0 = 0; //Timer value
              OCR0A = 0; /// Compare value
              TCCR0A = _BV(COM0A1) | _BV(WGM00); // Re-enable timer
              sei(); // Re-enable interrupts
            }
        }
    }
}
