#include <LiquidCrystal.h>

//Crear el objeto LCD con los números correspondientes (rs, en, d19, d18, d17, d16)
LiquidCrystal lcd(15, 14, 19, 18, 17, 16);

void setup() {
  // Inicializar el LCD con el número de  columnas y filas del LCD
  lcd.begin(16, 2);
  // Escribimos el Mensaje en el LCD.
  lcd.print("Hola Mundo");
}

void loop() {
   // Ubicamos el cursor en la primera posición(columna:0) de la segunda línea(fila:1)
  lcd.setCursor(0, 1);
   // Escribimos el número de segundos trascurridos
  lcd.print(millis()/1000);
  lcd.print(" Segundos");
  delay(100);
}