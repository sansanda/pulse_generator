/*
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

#include <LiquidCrystal.h>
#include <Keypad.h>

//Crear el objeto LCD con los números correspondientes (rs, en, d19, d18, d17, d16)
LiquidCrystal lcd(14, 15, 19, 18, 17, 16);
const unsigned int DISPLAY_N_COLUMNS = 20;
const unsigned int DISPLAY_N_ROWS = 4;

#define             pinOC1A 9   //timer 1 OC1A (Pulse output)
#define             pinLed 2    //Led output pin para señalizar la generación del pulso

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

char const START_STOP_KEY = 'A';
char const ENTER_MENU_KEY = 'B';
char const RETURN_MENU_KEY = 'C';
char const  RESET_COUNTER_KEY = "D";

// Generator Status
byte OUTPUT_STATUS = 0;
// DISPLAY_STATUS
byte const DISPLAY_IN_MAIN_WINDOW = 1;
byte const DISPLAY_IN_MAIN_MENU = 2;
byte const DISPLAY_IN_SUBMENU_1 = 4;
byte const DISPLAY_IN_SUBMENU_2 = 8;
byte const DISPLAY_IN_SUBMENU_3 = 16;
byte DISPLAY_STATUS = DISPLAY_IN_MAIN_WINDOW;
bool FIRST_DIGIT = true;

// Timer 1 initial config
unsigned const int  timer1Prescaler = 1;              //valid values are: {1, 8, 64, 256, 1024}
unsigned const long desired_freq = 40000;             //40000Hz -> T=25us, 25000Hz -> 40us. Valid freq 245 Hz - 8000000 . Recommended 245 - 1000000
const byte          desired_dutycycle = 10;           // valid values [1,99]. Remember that is inverted. pe 10 is 10% off

// Generator running params
unsigned int        T = 1000;                         //period in milli_seconds
unsigned int const  MIN_T = 500;
unsigned int const  MAX_T = 100000;
unsigned long       np = 1000;                        //number of programmed pulses to generate
unsigned long       MAX_np = 100000000;
unsigned long       MIN_np = 0;
unsigned int        ton = 10;                         //ton of the generated pulse (in useconds)
unsigned int const  MIN_TON = 1;
unsigned int const  MAX_TON = 100;
unsigned long       generated_pulses_counter = 0;     //Auto described

unsigned const long debounceDelay = 5000;             // in ms
unsigned long       lastTimeButtonPressed = 0;        // in ms
bool                lastButtonState = HIGH;

void initPortAsOutput(const byte pinNumber,
                      const bool initialState) {
  //Initialize pins
  pinMode(pinNumber, OUTPUT);
  digitalWrite(pinNumber, initialState);
}

void initPortAsInput( const byte pinNumber,
                      const byte mode) {
  //Initialize pins
  pinMode(pinNumber, mode);
}

void initTimer1(unsigned const long cpu_freq,
                unsigned const int  prescaler,
                unsigned const long freq,
                const byte          dutycicle) {

  TCCR1A = 0;
  TCCR1B = 0;

  // Set output compare mode
  //TCCR1A |= (1 << COM1A1); //non-inverting mode

  //inverting mode. We work as inverting because in non-inverting mode we have
  //indesirable side effects like a last pulse that shouldn't be there or
  //an indesirable elongation of the first pulse on every burst.
  //Then, you have to take into account that duty_cyce must be also inverted
  TCCR1A |= (1 << COM1A1) | (1 << COM1A0);

  //Configure the operation mode (fast PWM Mode 14)
  TCCR1A |= (1 << WGM11);
  TCCR1B |= (1 << WGM12);
  TCCR1B |= (1 << WGM13);

  //Configure the output compare register A and I
  // Set PWM frequency/TOP value

  ICR1 = (cpu_freq / (prescaler * freq)) - 1;
  //Serial.print("ICR1 = ");
  //Serial.println(ICR1);
  // Set the duty cycle
  OCR1A = ICR1 * (dutycicle / 100.0) ;
  //Serial.print("OCR1A = ");
  //Serial.println(OCR1A);
  //Enable Timer1 overflow Interrupt
  TIMSK1 = TIMSK1 | (1 << TOIE1);
}

void startTimer1(unsigned int prescaler) {
  TCCR1B = TCCR1B | (1 << CS10);            //prescaler a 1. fclk de 16.000.000 Hz (0.0625us)
  //TCCR1B = TCCR1B | (1<<CS11);              //prescaler a 8. fclk de  2.000.000 Hz (0.5us)
  //TCCR1B = TCCR1B | (1<<CS11) | (1<<CS10);  //prescaler a 64. fclk de   250.000 Hz (4us)
  //TCCR1B = TCCR1B | (1<<CS12);              //prescaler a 256. fclk de   62.500 Hz (16us)
  //TCCR1B = TCCR1B | (1<<CS12) | (1<<CS10);    //prescaler a 1024. fclk de  15.625 Hz (64us)
}


//Timer1 Overflow ISR
//Este timer es el que genera el burst o tren de pulsos

ISR(TIMER1_OVF_vect)
{
  //Existe un pequeño retardo hasta que la salida se desconecta.
  //Esto es debido a que el timer sigue contando cuando entramos en esta función y en consecuencia
  //la la salida se pone a 1.
  //Eso da lugar a un pequeño pulso

  //En resumen. Al llegar a la siguiente linea el TCNT1 ya va aprox por 72 counts lo
  //que da lugar a un pulso de aprox 0.0625us x 72 = 4.5us aprox.
  /*
    if (n_pulses_per_burst>=desired_n_pulses_per_burst)
    {
    TCCR1A &= 0b00111111;         //Disconnect the output
    TCCR1B = TCCR1B & 0b11111110; //stop the timer1
    n_pulses_per_burst = 0;
    TCNT1 = 0;                    //reset the timer
    //Here we have to switchoff the IR Led
    digitalWrite(pinIR, LOW);
    }
    else
    {
    n_pulses_per_burst++;
    }
  */
}

void startGenerator(
  const byte          pinNumber,
  unsigned const long cpu_freq,
  unsigned const int  timer_prescaler,
  unsigned const long desired_freq,
  const byte          desired_dutycycle
) {
  cli();  // disable all interrupts
  initPortAsOutput(pinNumber, LOW); //the pulse generator pin output
  initTimer1(cpu_freq, timer_prescaler, desired_freq, desired_dutycycle);
  sei(); // enable global interrupts
  startTimer1(timer_prescaler);
}

void setup() {
  // initialize serial communication at 9600 bits per second:
  Serial.begin(9600);
  // Inicializar el LCD con el número de  columnas y filas del LCD
  lcd.begin(DISPLAY_N_COLUMNS, DISPLAY_N_ROWS);
  lcd.clear();
  initPortAsOutput(pinOC1A, LOW);
  initPortAsOutput(pinLed, LOW);
  OUTPUT_STATUS = 0;
  DISPLAY_STATUS = DISPLAY_IN_MAIN_WINDOW;
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
        OUTPUT_STATUS = !OUTPUT_STATUS;
      } else if (key == ENTER_MENU_KEY && OUTPUT_STATUS == 0) {
        DISPLAY_STATUS = DISPLAY_IN_MAIN_MENU;
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
    }
    else if (DISPLAY_STATUS == DISPLAY_IN_SUBMENU_2) {
      // Getting T
      if (key == RETURN_MENU_KEY) {
        if (T < MIN_T) {
          T = MIN_T;
        }
        else if (T > MAX_T) {
          T = MAX_T;
        }
        FIRST_DIGIT = true;
        DISPLAY_STATUS = DISPLAY_IN_MAIN_MENU;
      }
      else if (isDigit(key)) {
        if (FIRST_DIGIT) {
          T = int(key - '0');
          FIRST_DIGIT = false;
        }
        else {
          T = T * 10 + int(key - '0');
        }
      }
    }
    else if (DISPLAY_STATUS == DISPLAY_IN_SUBMENU_3) {
      // Getting ton
      if (key == RETURN_MENU_KEY) {
        if (ton < MIN_TON) {
          ton = MIN_TON;
        }
        else if (ton > MAX_TON) {
          ton = MAX_TON;
        }
        FIRST_DIGIT = true;
        DISPLAY_STATUS = DISPLAY_IN_MAIN_MENU;
      }
      else if (isDigit(key)) {
        if (FIRST_DIGIT) {
          ton = int(key - '0');
          FIRST_DIGIT = false;
        }
        else {
          ton = ton * 10 + int(key - '0');
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
    lcd.print(T);
    lcd.print("ms");
    // indicates the state of the generator with blinking efect
    String text = "    STOPPED";
    if (OUTPUT_STATUS) {
      if ((millis()%1000)>500){
        text = "           ";
      }else{
        text = "    RUNNING";
      }  
    }
    lcd.print(text);
    // Second row
    lcd.setCursor(0, 1); // (columna, fila)
    lcd.print("ton=");
    lcd.print(ton);
    lcd.print("us");
    // Third row
    lcd.setCursor(0, 2); // (columna, fila)
    if (np == 0) {
      lcd.print("N_PULSES=INF");
    } else {
      lcd.print("N_PULSES=");
      lcd.print(np);
    }
    // Fourth row
    lcd.setCursor(0, 3); // (columna, fila)
    lcd.print("A=");
    lcd.print(generated_pulses_counter);
    lcd.print(",");
    lcd.print("R=");
    lcd.print(np - generated_pulses_counter);
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
    if ((millis()%1000)>500){
      lcd.print("              ");
    }else{
      if (np == 0){
        lcd.print("INFINITE");  
      }
      else{
        lcd.print(np);
      } 
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
    if ((millis()%1000)>500){
      lcd.print("              ");
    }else{
      lcd.print(T + String("ms"));
    } 
    lcd.setCursor(0, 1); // (columna, fila)
    lcd.print(String(RETURN_MENU_KEY) + ".Return");
  }
  if (displayStatus == DISPLAY_IN_SUBMENU_3) {
    // SUBMENU_3
    // ton=XXXus
    // *.Return
    lcd.setCursor(0, 0); // (columna, fila)
    lcd.print("ton=");
    // indicates ton blinking
    if ((millis()%1000)>500){
      lcd.print("              ");
    }else{
      lcd.print(ton + String("us"));
    }
    lcd.setCursor(0, 1); // (columna, fila)
    lcd.print(String(RETURN_MENU_KEY) + ".Return");
  }
}