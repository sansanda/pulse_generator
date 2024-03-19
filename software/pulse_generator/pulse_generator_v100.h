/*
  pulse_generator_v1.0.0
  Este programa ha sido creado para generar, a través de la salida digital 9 de un Arduino UNO v3,
  una señal correspondiente a un pulso de 5V repetitivo de ton ajustable y un periodo tambien ajustable (T).
  Por otro lado, tambien se desea controlar el número de pulsos (np) a generar pudiendo ser entre 1 e infinito.

  Como periféricos tendremos un keypad de 4x4 y un dispkay de 4 filas de 20 caracteres cada una (2004).

  Los parámetros a ajustar serán:
  1. ton (mínimo 1us, máximo 10us)
  2. T (mínimo 0.5s, máximo 100000s)
  3. np (mínimo 1, máximo infinito)


  En este ejemplo, utilizando el timer1 en modo de trabajo fastPWM se ha implementando
  una funcion startBurst que permite enviar un tren de impulsos (n_pulses)
  a una determinada frecuencia y duty_cycle. El estado inicial y final del tren de pulsos es LOW.
  Tambien se ha implementado una primera aproximación de la función startBurtsTrain que permite
  enviar un tren de bursts separados un determinado tiempo. Esta primera aproximación es un
  poco burda ya que hace uso de la función delay lo que provoca desajustes temporales entre
  trenes de pulsos, especialmente cuando el delay entre trenes se aproxima a 1 ms.

  pinUS (9) es la salida US
  pinIR (2) es el pin IR
  pinVIS (7) es el pin VIS
  pinButtonStart (2) es el start

*/
/*
  Hardware: Arduino UNO v3

*/

// standard libraries
#include <LiquidCrystal.h>
#include <Keypad.h>

//Crear el objeto LCD con los números correspondientes (rs, en, d19, d18, d17, d16)
LiquidCrystal lcd(14, 15, 19, 18, 17, 16);
const unsigned int  DISPLAY_N_COLUMNS = 20;
const unsigned int  DISPLAY_N_ROWS = 4;

#define             pinPulse  PIND2     //salida de pulso

//Crear un objeto keypad
const int KEYPAD_N_ROWS = 4; //four rows
const int KEYPAD_N_COLUMNS = 4; //four columns

char keys[KEYPAD_N_ROWS][KEYPAD_N_COLUMNS] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'0', 'F', 'E', 'D'}
};

byte pin_rows[KEYPAD_N_ROWS] = {5, 6, 7, 8}; //connect to the row pinouts of the keypad
byte pin_columns[KEYPAD_N_COLUMNS] = {10, 11, 12, 13}; //connect to the column pinouts of the keypad

Keypad keypad = Keypad( makeKeymap(keys), pin_rows, pin_columns, KEYPAD_N_ROWS, KEYPAD_N_COLUMNS);

char const  START_STOP_KEY = 'A';
char const  ENTER_MENU_KEY = 'B';
char const  RETURN_MENU_KEY = 'C';
char const  RESET_COUNTER_KEY = 'E';
char const  RESET_ACC_COUNTER_KEY = 'F';


// Generator Status
byte GENERATOR_STATUS = 0; // 0 --> STOPPED, 1 --> RUNNING

// DISPLAY_STATUS
byte const DISPLAY_IN_MAIN_WINDOW = 1;
byte const DISPLAY_IN_MAIN_MENU = 2;
byte const DISPLAY_IN_SUBMENU_1 = 4;
byte const DISPLAY_IN_SUBMENU_2 = 8;
byte const DISPLAY_IN_SUBMENU_3 = 16;
byte DISPLAY_STATUS = DISPLAY_IN_MAIN_WINDOW;
bool FIRST_DIGIT = true;

// Generator running params

//pulse control paremeters
unsigned const int time_base_ms       = 100;    //  the period at which timer1 will generate an interruption (TIMER1_COMPA_vect) due to the arrival of the 
                                                //  TCNT1 to the value specified in the OCR1A

unsigned int desired_pulse_period_ms  = time_base_ms * 10;    // pulse period in milliseconds (min 500, max 25000) (also T)
unsigned int const  MIN_T = time_base_ms * 5;                 // MIN_T = 500;
unsigned int const  MAX_T = time_base_ms * 1000;
unsigned long       np = 1000;                  // number of programmed pulses to generate
unsigned long       MAX_np = 100000000;
unsigned long       MIN_np = 0;
unsigned int const  MIN_TON = 1;                // in us  
unsigned int        ton_us = MIN_TON *10;       // ton of the generated pulse (in useconds)
unsigned int const  MAX_TON = 100;              // in us

unsigned long       generated_pulses = 0;       // auto described counter
unsigned long       accumulated_pulses = 0;     // auto described counter

unsigned long program_timestamp_ms = 0;         // multiple of time_base_ms 

void setup() {
  // initialize serial communication at 9600 bits per second:
  Serial.begin(9600);
  GENERATOR_STATUS = 0;

  cli();  // disable all interrupts
  initPinModesAndStates();
  // initialize timers 1 and 0 parameters
  initTimers();
  resetTimersCounters();
  initDisplay();
  sei(); // enable global interrupts
}

void loop() {
  manageKeyboardEvent(keypad.getKey());
  updateDisplay(DISPLAY_STATUS);
}

void manageKeyboardEvent(char key) {
  if (key) {
    lcd.clear();
    if (DISPLAY_STATUS == DISPLAY_IN_MAIN_WINDOW) {
      // Manage the start/stop of the output
      if (key == START_STOP_KEY) {
        GENERATOR_STATUS = !GENERATOR_STATUS;
        GENERATOR_STATUS ? startTimers() : stopTimers();
      } else if (key == ENTER_MENU_KEY && GENERATOR_STATUS == 0) {
        DISPLAY_STATUS = DISPLAY_IN_MAIN_MENU;
      }
      if (key == RESET_COUNTER_KEY) {
        generated_pulses = 0;
      }
      if (key == RESET_ACC_COUNTER_KEY) {
        accumulated_pulses = 0;
      }    
    }
    else if (DISPLAY_STATUS == DISPLAY_IN_MAIN_MENU) {
      // MAIN_MENU
      // 1. Set N_PULSES
      // 2. Set T
      // 3. Set ton
      // *. Return
      if (key == '1') {
        DISPLAY_STATUS = DISPLAY_IN_SUBMENU_1;
      }
      else if (key == '2') {
        DISPLAY_STATUS = DISPLAY_IN_SUBMENU_2;
      }
      else if (key == '3') {
        DISPLAY_STATUS = DISPLAY_IN_SUBMENU_3;
      }
      else if (key == RETURN_MENU_KEY) {
        DISPLAY_STATUS = DISPLAY_IN_MAIN_WINDOW;
      }
    }
    else if (DISPLAY_STATUS == DISPLAY_IN_SUBMENU_1) {
      // Getting N_PULSES
      if (key == RETURN_MENU_KEY) {
        FIRST_DIGIT = true;
        DISPLAY_STATUS = DISPLAY_IN_MAIN_MENU;
      }
      else if (isDigit(key)) {
        if (FIRST_DIGIT) {
          np = long(key - '0');
          FIRST_DIGIT = false;
        }
        else {
          np = np * 10 + long(key - '0');
        }
        if (np > MAX_np){
          np = MAX_np;
        }
      }
      generated_pulses = 0;
    }
    else if (DISPLAY_STATUS == DISPLAY_IN_SUBMENU_2) {
      // Getting T
      if (key == RETURN_MENU_KEY) {
        if (desired_pulse_period_ms < MIN_T) {
          desired_pulse_period_ms = MIN_T;
        }
        else if (desired_pulse_period_ms > MAX_T) {
          desired_pulse_period_ms = MAX_T;
        }
        // round to the nearest multiple of 100 ms
        byte rest = desired_pulse_period_ms % time_base_ms;
        (rest > 50) ? desired_pulse_period_ms = desired_pulse_period_ms + (100 - rest) : desired_pulse_period_ms = desired_pulse_period_ms - rest; 
        //
        FIRST_DIGIT = true;
        DISPLAY_STATUS = DISPLAY_IN_MAIN_MENU;
      }
      else if (isDigit(key)) {
        if (FIRST_DIGIT) {
          desired_pulse_period_ms = int(key - '0');
          FIRST_DIGIT = false;
        }
        else {
          desired_pulse_period_ms = desired_pulse_period_ms * 10 + int(key - '0');
        }
      }
    }
    else if (DISPLAY_STATUS == DISPLAY_IN_SUBMENU_3) {
      // Getting ton
      if (key == RETURN_MENU_KEY) {
        if (ton_us < MIN_TON) {
          ton_us = MIN_TON;
        }
        else if (ton_us > MAX_TON) {
          ton_us = MAX_TON;
        }
        FIRST_DIGIT = true;
        DISPLAY_STATUS = DISPLAY_IN_MAIN_MENU;
      }
      else if (isDigit(key)) {
        if (FIRST_DIGIT) {
          ton_us = int(key - '0');
          FIRST_DIGIT = false;
        }
        else {
          ton_us = ton_us * 10 + int(key - '0');
        }
      }
    }
  }
}

void updateDisplay(byte displayStatus) {
  if (displayStatus == DISPLAY_IN_MAIN_WINDOW)
  {
    // MAIN WINDOW
    // First row
    lcd.setCursor(0, 0); // (columna, fila)
    lcd.print("T=");
    lcd.print(desired_pulse_period_ms);
    lcd.print("ms");
    // indicates the state of the generator with blinking efect
    String text = "    STOPPED";
    if (GENERATOR_STATUS) {
      (millis()%500) > 250 ? text = "           " : text = "    RUNNING";
    }
    lcd.print(text);
    // Second row
    lcd.setCursor(0, 1); // (columna, fila)
    lcd.print("ton=");
    lcd.print(ton_us);
    lcd.print("us");
    // Third row
    lcd.setCursor(0, 2); // (columna, fila)
    if (np == 0) {
      lcd.print("N_PULSES=INFINITE");
    } else {
      lcd.print("N_PULSES=");
      lcd.print(np);
    }
    // Fourth row
    lcd.setCursor(0, 3); // (columna, fila)
    String remaining_pulses = "";
    np == 0 ? remaining_pulses = "INFINITE" : remaining_pulses = String(np - generated_pulses);
    text = "A=" + String(accumulated_pulses) + ",R=" + remaining_pulses;
    lcd.print(text);
    // remove other characters on the right
    byte n_spaces = 20-text.length();
    lcd.setCursor(text.length(), 3);
    for (byte i=0;i<n_spaces;i++){
      lcd.print(" ");  
    }
  }
  if (displayStatus == DISPLAY_IN_MAIN_MENU) {
    // MAIN_MENU
    lcd.setCursor(0, 0); // (columna, fila)
    lcd.print("1.Set N_PULSES");
    lcd.setCursor(0, 1); // (columna, fila)
    lcd.print("2.Set T");
    lcd.setCursor(0, 2); // (columna, fila)
    lcd.print("3.Set ton");
    lcd.setCursor(0, 3); // (columna, fila)
    lcd.print(String(RETURN_MENU_KEY) + ".Return");
  }
  if (displayStatus == DISPLAY_IN_SUBMENU_1) {
    // SUBMENU_1
    // N_PULSES=XXXXXX
    // *.Return
    lcd.setCursor(0, 0); // (columna, fila)
    lcd.print("N_PULSES=");
    // indicates the number of pulse blinking
    if ((millis()%1000) > 500){
      lcd.print("              ");
    }else{
      np == 0 ? lcd.print("INFINITE") : lcd.print(np);    
    }  
    lcd.setCursor(0, 1); // (columna, fila)
    lcd.print(String(RETURN_MENU_KEY) + ".Return");
  }
  if (displayStatus == DISPLAY_IN_SUBMENU_2) {
    // SUBMENU_2
    // T=XXXXXXms
    // *.Return
    lcd.setCursor(0, 0); // (columna, fila)
    lcd.print("T=");
    // indicates T blinking
    (millis()%1000) > 500 ? lcd.print("              ") : lcd.print(desired_pulse_period_ms + String("ms"));  
    lcd.setCursor(0, 1); // (columna, fila)
    lcd.print(String(RETURN_MENU_KEY) + ".Return");
  }
  if (displayStatus == DISPLAY_IN_SUBMENU_3) {
    // SUBMENU_3
    // ton_us=XXXus
    // *.Return
    lcd.setCursor(0, 0); // (columna, fila)
    lcd.print("ton=");
    // indicates ton blinking
    (millis()%1000) > 500 ? lcd.print("              ") : lcd.print(ton_us + String("us"));
    lcd.setCursor(0, 1); // (columna, fila)
    lcd.print(String(RETURN_MENU_KEY) + ".Return");
  }
}

void initPinModesAndStates(){
  DDRD |= (1<<pinPulse);    // set pinPulse as output              
  PORTD &= ~(1<<pinPulse);  // set pinPulse low
  DDRD &= ~(1<<PIND3);      // set PIND3 (3) as input, unused and tied to gnd
  DDRD &= ~(1<<PIND4);      // set PIND4 (4) as input, unused and tied to gnd 
  DDRB &= ~(1<<PINB1);      // set PINB1 (9) as input, unused and tied to gnd 
}

void initDisplay(){
  lcd.begin(DISPLAY_N_COLUMNS, DISPLAY_N_ROWS);
  lcd.clear();
  DISPLAY_STATUS = DISPLAY_IN_MAIN_WINDOW;
}

void initTimers(){
    //timer #1 toggles OCR1A on TCNT1=0 in turn toggling T0
  TCCR1A = (1<<COM1A0); //TCCR1A toggle OC1A on compare match
  TCCR1B = 0;
  TCCR1C = 0;
  // output compare register on division ratio of 12500
  // if the timer1 clock is prescaled on 64 then timer1 clock = 4us ocr1a = 25000 will be filled in 25000 * 4 us = 100ms, so 
  // the OCR1A will generate an interrupt every 100ms. Then, ww will increase a counter every 100ms which will be the base time for the pulse generation.  
  OCR1A = (time_base_ms * 10E-3) / (4 * 10E-6);       
 
  //timer 1 enable interrupt TIMER1_COMPA_vect
  TIMSK1 |= (1<<OCIE1A);
}

void startTimers(){
    //configure ctc and reset timers
  TCCR1B |= (1<<WGM12); // timer1 CTC mode 4
  TCCR1B |= (1<<CS11) | (1<<CS10); //timer1 64 prescaler
}

void stopTimers(){
  TCCR1B &= 0x11111000; //timer1 64 prescaler
}

void resetTimersCounters(){
  TCNT1 = 0UL;
}

ISR(TIMER1_COMPA_vect)
{
  program_timestamp_ms = program_timestamp_ms + time_base_ms;
  if ((program_timestamp_ms % desired_pulse_period_ms) == 0){
    bool generate_pulse = false;
    // np = 0, infinite case
    (np == 0 or (np-generated_pulses) > 0) ? generate_pulse = true : generate_pulse = false;
    if (generate_pulse){
      PORTD |= (1<<pinPulse); // set to one
      delayMicroseconds(ton_us);
      PORTD &= ~(1<<pinPulse); //set D4 low
      generated_pulses++;
      accumulated_pulses++;
    }
    else{
      GENERATOR_STATUS = 0;
      GENERATOR_STATUS ? startTimers() : stopTimers();
      generated_pulses = 0;
    }
  }
}