/*
  chronometer and thermometer
  by F.Palmonari Sep.2021

  using 2 photoresistors to trigger start and stop time
  reading a DS18B20 for the temperature
  //
  // SENSORE DS18B20 (TEMPERATURA)
  // ROSSO = 5V
  // GIALLO = GROUND
  // VERDE = DIGITAL PIN
  //
  // QUESTO SENSORE HA BISOGNO DI 1 kOhm tra Digital e 5V
  //
  //
  // FOTORESISTENZE (CRONOMETRO)
  //
  // OGNI RESISTENZA HA BISOGNO DI 220 Ohm tra Analog e GND
  //
  //
*/
#include <LiquidCrystal.h>
LiquidCrystal lcd(8, 9, 4, 5, 6, 7);

#include <LiquidCrystal.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <SPI.h>
#include <SoftwareSerial.h>
#include <avr/pgmspace.h>
#include <string.h>

// Assign to pin XX of your Arduino to the DS18B20
#define ONE_WIRE_BUS 2

#define btnRIGHT  0
#define btnUP     1
#define btnDOWN   2
#define btnLEFT   3
#define btnSELECT 4
#define btnNONE   5

// Create a onewire instanace
OneWire oneWire(ONE_WIRE_BUS);

// Declare a DS18B20 Instance and assing the OneWire reference to it.
DallasTemperature sensors(&oneWire);

// Global message buffers shared by Serial
#define  BUF_SIZE 128
char curMessage[BUF_SIZE];
char newMessage[BUF_SIZE];
bool newMessageAvailable = false;

// simbolo per Celsius
byte celsius[8] = {
  B00110,
  B01001,
  B01001,
  B00110,
  B00000,
  B00000,
  B00000,
  B00000
};
// simbolo per Delta
byte delta[8] = {
  B00100,
  B01010,
  B01010,
  B10001,
  B10001,
  B10001,
  B11111,
};


// char array to print to the screen
char sensorPrintout[2];
//
int photoRPin[] = {1, 2};
int minLight[2];          //Used to calibrate the readings
int maxLight[2];          //Used to calibrate the readings
int lightLevel[2];
int newlightLevel[2];
int rms[2];
int avg[2];
int probe[2];
int dummy;
int sum = 0;
//                       // triggers x periodo
boolean trigger01 = false;
boolean trigger02 = false;
boolean trigger11 = false;
boolean trigger12 = false;
boolean trigger = false; // trigger x cronometro
boolean first = true;
boolean prog = false; // per definizione parte la temperatura (prog == false)
boolean periodic = false; // per definizione parte il cronometro (periodic == false)
boolean testprobes = false; // per il test delle fotoresistenze all'inizio
unsigned long timezero = 0;
unsigned long ztime = 0;
unsigned long deltatime = 0;
//
double tstart = 0.0;
double mytime = 0.0;
double tempold = 0.0;
double told = 0.0;
double deltat = 0.0;
double deltatemp = 0.0;
int page = 0;

// ======================================== FUNZIONE PER LEGGERE IL TEASTIERINO DEL DISPLAY
int read_buttons() {
  int adc_key_in = analogRead(0);
  if (adc_key_in > 1000) return btnNONE;
  if (adc_key_in < 50)   return btnRIGHT;
  if (adc_key_in < 195)  return btnUP;
  if (adc_key_in < 380)  return btnDOWN;
  if (adc_key_in < 555)  return btnLEFT;
  if (adc_key_in < 790)  return btnSELECT;
}


void setup() {
  Serial.begin(38400);
  lcd.createChar(0, celsius);
  lcd.createChar(1, delta);
  lcd.begin(16, 2);

  // ============================== SPLASH SCREEN
  lcd.setCursor(0, 0);
  lcd.print("LABORATORIO - FP");
  lcd.setCursor(0, 1);
  lcd.print("DI FISICA - 2022");
  delay(2500);

  // ============================== PAGINA #0
  lcd.clear();
  lcd.setCursor(0, 0);

  lcd.print("UP=   PROSEGUI");
  lcd.setCursor(0, 1);
  lcd.print("DOWN=TEST PROBE");
  int buttons = read_buttons();
  while (1) {
    buttons = read_buttons();
    if (buttons == btnUP) {
      break;
    }
    if (buttons == btnDOWN) {
      testprobes = true; // fai il test delle porbes dopo la calibrazione
      break;
    }
    delay(2);
  }
  delay(200);
  // ============================== PAGINA #1
  if (testprobes) {
    prog = true;
    delay(600);
  } else {
    lcd.clear();
    lcd.setCursor(0, 0);

    lcd.print("UP=   CRONOMETRO");
    lcd.setCursor(0, 1);
    lcd.print("DOWN= TERMOMETRO");
    buttons = read_buttons();
    while (1) {
      buttons = read_buttons();
      if (buttons == btnUP) {
        prog = true; // cronometro
        break;
      }
      if (buttons == btnDOWN) {
        prog = false; // temperatura
        break;
      }
      delay(2);
    }
  }
  lcd.clear();
  lcd.setCursor(0, 0);
  // ============================== PAGINA #2 (se cronometro, scelta tipo di cronometro)
  if (prog) {
    if (testprobes) {
      delay(200);
    } else {
      lcd.clear();
      lcd.setCursor(0, 0);

      lcd.print("UP=  t-PERIODICO");
      lcd.setCursor(0, 1);
      lcd.print("DOWN=  t-SINGOLO");
      delay(300);
      int buttons = read_buttons();
      while (1) {
        buttons = read_buttons();
        if (buttons == btnUP) {
          periodic = true; // periodico
          break;
        }
        if (buttons == btnDOWN) {
          periodic = false; // cronometro
          break;
        }
        delay(2);
      }
    }
    // ============================== PAGINA #2 calibrazione comune a tutti

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("..calibrazione..");

    avg[0] = 0;
    avg[1] = 0;
    for (int i = 0; i < 2; i++) {
      rms[i] = 0;
      lightLevel[i] = 0;
      minLight[i] = 1024;
      maxLight[i] = 0;
      for (int j = 0; j < 10000; j++) {
        //Setup the starting light level limits
        dummy = analogRead(photoRPin[i]);
        avg[i] = 0.5 * (avg[i] + dummy);
        rms[i] += (dummy - avg[i]) * (dummy - avg[i]);
        if (dummy < minLight[i]) {
          minLight[i] = dummy;
        }
        if (dummy > maxLight[i]) {
          maxLight[i] = dummy;
        }
        //Serial.println(dummy);
      }
      lightLevel[i] = max(0.0001 * sqrt(rms[i]), 0.5 * (maxLight[i] - minLight[i]));
      if (lightLevel[i] > 5) {
        rms[i] = lightLevel[i];
      } else {
        rms[i] = 5;
      }
    }

    // ============================== PAGINA #3 inizio delle misure

    // clear the screen with a black background
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("PROBE-1:");
    lcd.setCursor(10, 0);
    lcd.print(lightLevel[0]);
    lcd.setCursor(0, 1);
    lcd.print("PROBE-2:");
    lcd.setCursor(10, 1);
    lcd.print(lightLevel[1]);
    delay(3000);
    if (testprobes) {
      delay(200);
    } else {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("SELECT = start");
      lcd.setCursor(0, 1);
      lcd.print("UP  = re-start");
      lcd.setCursor(0, 0);
      buttons = read_buttons();
      while (1) {
        buttons = read_buttons();
        if (buttons == btnSELECT) {
          break;
        }
        delay(2);
      }
    }
    lcd.clear();
    lcd.setCursor(0, 0);

  } else {
    strcpy(curMessage, "   ");
    newMessage[0] = '\0';

    // Start the DallasTemperature Library
    sensors.begin();

    // ============================ SPIEGAZIONI PAGINE TERMOMETRO
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("SELECT=(RE)START");
    lcd.setCursor(0, 1);
    lcd.print("UP=PAG1  DW=PAG2");
    lcd.setCursor(0, 0);
    int buttons = read_buttons();
    while (1) {
      buttons = read_buttons();
      if (buttons == btnSELECT) {
        break;
      }
      delay(2);
    }

    // ============================ INIZIALIZZAZIONE TERMOMETRO
    lcd.clear();
    lcd.setCursor(0, 0);

    tstart = millis();
    told = (millis() - tstart) / 1000.0;
    sensors.requestTemperatures(); // RICHIEDE LE TEMPERATURE AL DS18B20
    delay(400);
    tempold = sensors.getTempCByIndex(0); // PRIMA MISURA DI TEMPERATURA COME RIFERIMENTO PER DELTA-T
  }

  // ============================== TEST DELLE PORBES
  if (testprobes) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("OGGETTO DAVANTI ");
    lcd.setCursor(0, 1);
    lcd.print("PROBE           ");
    while (1) {
      for (int j = 0; j < 2; j++) {
        readmyprobe(j);
        lightLevel[j] = sum;
      }
      delay(1);
      for (int j = 0; j < 2; j++) {
        readmyprobe(j);
        newlightLevel[j] = sum;
      }
      if ( abs(newlightLevel[0] - lightLevel[0]) > rms[0]) {
        lcd.setCursor(10, 1);
        lcd.print("[1]");
        delay(300);
        lcd.setCursor(10, 1);
        lcd.print("   ");
      }
      delay(1);
      if ( abs(newlightLevel[1] - lightLevel[1]) > rms[1]) {
        lcd.setCursor(10, 1);
        lcd.print("[2]");
        delay(300);
        lcd.setCursor(10, 1);
        lcd.print("   ");
      }
    }
  }
}

void loop() {
  if (prog) {
    //==================== QUI INIZIA IL CRONOMETRO PER FENOMENI PERIODICI ======================
    if (periodic) {
      float periodo = 0;
      int nperiodo = 0;
      float psum;
      while (1) {
        for (int j = 0; j < 2; j++) {
          readmyprobe(j);
          lightLevel[j] = sum;
        }
        delay(1);
        for (int j = 0; j < 2; j++) {
          readmyprobe(j);
          newlightLevel[j] = sum;
        }
        if ( abs(newlightLevel[0] - lightLevel[0]) > rms[0]) {
          if ( first) {
            timezero = millis();
            trigger01 = true;
            first = false;
          }
        }
        if ( trigger01 && abs(newlightLevel[1] - lightLevel[1]) > rms[1]) {
          trigger11 = true;
        }
        if ( trigger01 && trigger11 && abs(newlightLevel[1] - lightLevel[1]) > rms[1]) {
          trigger12 = true;
        }
        if ( trigger01 && trigger11 && trigger12 && abs(newlightLevel[0] - lightLevel[0]) > rms[0]) {
          trigger02 = true;
        }
        if ( trigger01 && trigger11 && trigger12 && trigger02 && abs(newlightLevel[0] - lightLevel[0]) > rms[0]) {
          ztime = millis();
          periodo = (float)(0.001 * (ztime - timezero));
          psum += periodo;
          nperiodo++;
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("PERIODO=");
          lcd.setCursor(9, 0);
          lcd.print(periodo);
          lcd.setCursor(16, 0);
          lcd.write("s");
          lcd.setCursor(0, 1);
          lcd.write("T-medio=");
          lcd.setCursor(9, 1);
          lcd.print((float)(psum / nperiodo));
          lcd.setCursor(16, 1);
          lcd.write("s");
          first = true;
          trigger01 = false;
          trigger02 = false;
          trigger11 = false;
          trigger12 = false;
        }
      }
    } else {
      //=================================== QUI INIZIA IL CRONOMETRO SINGOLO ======================
      for (int j = 0; j < 2; j++) {
        readmyprobe(j);
        lightLevel[j] = sum;
      }
      delay(1);
      for (int j = 0; j < 2; j++) {
        readmyprobe(j);
        newlightLevel[j] = sum;
      }
      if ( abs(newlightLevel[0] - lightLevel[0]) > rms[0]) {
        if ( first) {
          timezero = millis();
          trigger = true;
          first = false;
        }
      }
      if (trigger) {
        if (abs(newlightLevel[1] - lightLevel[1]) > rms[1]) {
          ztime = millis();
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("--- misurato ---");
          //      lcd.print(timezero);
          //      lcd.setCursor(8, 0);
          //      lcd.print(ztime);
          lcd.setCursor(0, 1);
          lcd.write(byte(1));
          lcd.setCursor(1, 1);
          lcd.write("t= ");
          lcd.setCursor(5, 1);
          lcd.print(ztime - timezero);
          lcd.setCursor(12, 1);
          lcd.write("ms");
          first = false;
          trigger = false;
        }
      }
      int buttons = read_buttons();
      if (buttons == btnUP) {
        lcd.clear();
        first = true;
        trigger = false;
      }
    }

  } else {
    //
    //=================================== QUI CALCOLA TEMPERATURA ==============================
    //
    sensors.requestTemperatures(); // Tell the DS18B20 to get make a measurement
    delay(400);
    float mytemp = sensors.getTempCByIndex(0);
    double val = (double)(mytemp);
    sendToPC(&val);
    //  Serial.println(mytemp,4); // Get that temperature and print it.
    //  delay(1000);


    // CALCOLA TEMPO
    mytime = (millis() - tstart) / 1000.0;
    if (mytime > 86400) {
      tstart = millis();
    }

    // LEGGE TASTIERA

    // UP == DISPLAY temp/temperatura
    int buttons = read_buttons();
    if (buttons == btnUP) {
      page = 0;
      lcd.clear();
    }

    // SELECT == AZZERA CRONOMETRO E TEMPERATURA RIFERIMENTO
    if (buttons == btnSELECT) {
      lcd.clear();
      page = 0;
      tstart = millis();
      tempold = mytemp;
      told = mytime;
    }

    // DOWN == VALUTA DTemp/DT E RE-IMPOSTA told
    if (buttons == btnDOWN) {
      lcd.clear();
      page = 1;
      deltat = mytime - told;
      deltatemp = mytemp - tempold;
      // UPDATE told and tempold
      told = mytime;
      //tempold = mytemp;
    }

    switch (page) {
      case (0):
        lcd.setCursor(0, 0);
        lcd.print("T=");
        lcd.setCursor(3, 0);
        lcd.print("     ");
        lcd.setCursor(3, 0);
        lcd.print(mytemp, 1);
        lcd.setCursor(14, 0);
        lcd.write(byte(0));
        lcd.setCursor(15, 0);
        lcd.print("C");

        lcd.setCursor(0, 1);
        lcd.print("tempo=");
        lcd.setCursor(7, 1);
        lcd.print("        ");
        lcd.setCursor(7, 1);
        lcd.print(mytime, 0);
        lcd.setCursor(15, 1);
        lcd.print("s");
        break;
      case (1):
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.write(byte(1));
        lcd.print("T = ");
        lcd.setCursor(5, 0);
        lcd.print(deltatemp);
        lcd.setCursor(14, 0);
        lcd.write(byte(0));
        lcd.setCursor(15, 0);
        lcd.print("C");

        lcd.setCursor(0, 1);
        lcd.write(byte(1));
        lcd.print("t = ");
        lcd.setCursor(5, 1);
        lcd.print(deltat);
        lcd.setCursor(15, 1);
        lcd.print("s");
        break;
    }
  }
}

// ==============================================================
// ========== SUBROUTINES =======================================
// ==============================================================


void readmyprobe(int channel) {
  sum = analogRead(photoRPin[channel]);
}

void sendToPC(double * data)
{
  byte* byteData = (byte*)(data);
  Serial.write(byteData, 4);
}
