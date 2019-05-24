#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal.h>
#include "functions.cpp"

#define DEBUG_MODE
#define RS485 Serial2

#define RXD2 16
#define TXD2 17

const int buttonPin = 14;
const int ledPin = 5;
const int enablePin = 15;
const int lcdLightPin = 12;

bool invert = false;

int buttonState = 0;
int receivedChar;
int p_pos = -1;
int r_pos = -1;
int baudChar = -1;
int baudRate = -1;

uint32_t serial_config = SERIAL_7E1;
unsigned int start_baudRate = 300;
unsigned long start_time;
unsigned long sleep_time;

char flag_code[4];
char meter_ser[9];
char response[128];
char request[128];

programState ps = NONE;
flagCode fc = UNK;

void setup() {
  Serial.begin(115200);

  pinMode(RXD2, INPUT);
  pinMode(TXD2, OUTPUT);

  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(ledPin, OUTPUT);
  pinMode(enablePin, OUTPUT);
  pinMode(lcdLightPin, OUTPUT);

  digitalWrite(ledPin, HIGH);
  digitalWrite(lcdLightPin, HIGH);

  esp_sleep_enable_ext0_wakeup((gpio_num_t)buttonPin, LOW);

  lcd.begin(16, 2);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print( "TEST ICIN" );
  lcd.setCursor(0, 1);
  lcd.print( "TUSA BASIN" );

  flag_code[3] = '\0';
  meter_ser[8] = '\0';

  sleep_time = millis();
}

void loop() {
  buttonState = digitalRead(buttonPin);

  if( buttonState == LOW ) {
    invert = false;
    serial_config = SERIAL_7E1;
    start_baudRate = 300;

    memset(flag_code, 0, sizeof(flag_code));
    memset(meter_ser, 0, sizeof(meter_ser));
    memset(response, 0, sizeof(response));
    memset(request, 0, sizeof(request));
    
    while( true ) {
      if( ps == NONE ) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print( "SAYAC TESTI" );
        lcd.setCursor(0, 1);
        lcd.print( "YAPILIYOR" );

        RS485.begin(start_baudRate, serial_config, RXD2, TXD2, invert);
        digitalWrite(ledPin, LOW);
        
        Serial.println("Request identification");

        digitalWrite(enablePin, HIGH);
        RS485.print( "\1B0\3q" );
        RS485.flush();
        RS485.print( "/?!\r\n" );
        RS485.flush();
        digitalWrite(enablePin, LOW);
        
        ps = IDENTIFICATION;
        start_time = millis();
      } else {
        sleep_time = millis();
        receivedChar = RS485.read();
        if( receivedChar > 127 ) {
          receivedChar -= 128;
        }

        /**
         * Timeout check.
         **/
        if( millis() - start_time > 5000 ) {
          baudRate = -1;
          baudChar = -1;

          if( ps == IDENTIFICATION && invert == false ) {
            /**
             * If meter does not response within a defined period
             * and if identification of meter can not get yet,
             * invert serial port to check if connection is wrong
             * and try again.
             **/
            ps = NONE;
            invert = true;

            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print( "TERS BAGLANTI" );
            lcd.setCursor(0, 1);
            lcd.print( "DENENIYOR" );

            Serial.println( "Identification timeout" );
            Serial.println( "Inverting RS485 connection" );
          } else {
            /**
             * If meter does not response within a defined period
             * and if identification of meter already get or 
             * already try to insert serial port, close the port. 
             **/
            ps = NONE;
            invert = false;

            if( serial_config == SERIAL_7E1 ) {
              serial_config = SERIAL_8N1;
              start_baudRate = 9600;

              lcd.clear();
              lcd.setCursor(0, 0);
              lcd.print( "LANDIS SAYAC" );
              lcd.setCursor(0, 1);
              lcd.print( "DENENIYOR" );

              Serial.println( "Trying for Landis meters" );
            } else {
              digitalWrite(enablePin, HIGH);
              RS485.print( "\1B0\3q" );
              RS485.flush();
              digitalWrite(enablePin, LOW);

              lcd.clear();
              lcd.setCursor(0, 0);
              lcd.print( "SAYAC" );
              lcd.setCursor(0, 1);
              lcd.print( "OKUNAMADI" );

              digitalWrite(ledPin, HIGH);
              Serial.println("Can not read meter");

              return;
            }
          }          
        }

        if( receivedChar != - 1) {
          response[ ++r_pos ] = (char)receivedChar;      

          if( ps == IDENTIFICATION ) {
            if ( receivedChar == 47) {
              p_pos = 0;
            }

            if ( p_pos >= 0 ) {
              /**
               * Check if received char is expected
               * 
               * MSY : <0x01>P0<0x02>(75410231)<0x03>g
               * LGZ : <0x01>P0<0x02>(DD28D823)<0x03><0x23>
               * BYL : <0x01>P0<0x02>(50000062)<0x03>a
               * LUN : <0x01>P0<0x02>()<0x03>`
               **/
              if( (receivedChar < 33 || receivedChar > 126) && receivedChar != 10 && receivedChar != 13 ) {
                nonsenseData();

                ps = NONE;
                invert = false;
                baudRate = -1;
                baudChar = -1;

                return;
              }
            
              p_pos++;
            }

            if( p_pos > 1 && p_pos < 5 ) {
              flag_code[p_pos - 2] = receivedChar;
            }

            if ( p_pos == 5 ) {
              baudChar = receivedChar;
              baudRate = getBaudRate( receivedChar - 48 );
            }

            if ( receivedChar == 10 ) {
              if( baudRate == -1 ) {
                nonsenseData();
                
                ps = NONE;
                invert = false;
                baudRate = -1;
                baudChar = -1;

                return;
              }

              fc = getMeterBrand( flag_code );

              delay(200);
              digitalWrite(enablePin, HIGH);
              RS485.print( String( String( (char)6 ) + "0" + String( (char)baudChar ) + "1\r\n" ) );
              RS485.flush();
              digitalWrite(enablePin, LOW);

              if( fc != AEL ) {
                RS485.end();
                Serial.println( "" );
                Serial.println( "Speed changed" );
                RS485.begin(baudRate, serial_config, RXD2, TXD2, invert);              
              }

              ps = MODE_CHANGE;
              p_pos = -1;
              r_pos = -1;

              lcd.clear();
              lcd.setCursor(0, 0);
              lcd.print("Marka : ");
              lcd.setCursor(8, 0);
              lcd.print( flag_code );

              Serial.print("Flag Code : ");
              Serial.println( flag_code );
              Serial.print("Identification : ");
              Serial.println( response );

              memset(response, 0, sizeof(response));
            }
          } else if( ps == MODE_CHANGE ) {
            if( (fc == MSY || fc == LUN || fc == BYL || fc == LGZ) && receivedChar == 3) {
              p_pos = 0;
            } else if( (fc == AEL || fc == VIK) && receivedChar == 6) {
              if( fc == AEL ) {
                RS485.end();
                Serial.println( "" );
                Serial.println( "Speed changed" );
                RS485.begin(baudRate, serial_config, RXD2, TXD2, invert);                 
              }
            
              p_pos = 1;
            }

            if( p_pos >= 0 ) {
              p_pos++; 
            }

            if ( p_pos == 2 ) {
              Serial.println("");
              Serial.print("Mode Change : ");
              Serial.println( response );
              p_pos = -1;
              r_pos = -1;      

              delay(100);
              memset(response, 0, sizeof(response));

              if( fc == LGZ ) {
                strcpy(request, "\001R5\0021-0:0.0.0()\003\000\000");
              } else {
                strcpy(request, "\001R2\0020.0.0()\003\000\000");
              }

              request[ sizeof_n(request) ] = getBCC(request);

              digitalWrite(enablePin, HIGH);
              RS485.print( request );
              RS485.flush();
              digitalWrite(enablePin, LOW);
            
              ps = SERIAL_NUMBER;
            }
          } else if( ps == REQUEST_MODE || ps == SERIAL_NUMBER ) {
            // <0x02>0.0.0(65002077)<0x03>3
            // <0x02>1-0:0.0.0(50715527)<0x03>"
            if( 
              ( fc == LGZ && (
                (r_pos == 0 && receivedChar != 2) ||
                (r_pos == 1 && receivedChar != 49) ||
                (r_pos == 2 && receivedChar != 45) ||
                (r_pos == 4 && receivedChar != 58) ||
                (r_pos == 10 && receivedChar != 40) ||
                (r_pos == 19 && receivedChar != 41) ||
                ( (r_pos == 6 || r_pos == 8) && receivedChar != 46) ||
                ( (r_pos == 3 || r_pos == 5 || r_pos == 7 || r_pos == 9) && receivedChar != 48) ||
                ( (r_pos > 10 || r_pos < 19) && (receivedChar < 48 && receivedChar > 57 ) )
              )) ||
              ( fc != LGZ && (
                (r_pos == 0 && receivedChar != 2) ||
                (r_pos == 6 && receivedChar != 40) ||
                (r_pos == 15 && receivedChar != 41) ||
                ( (r_pos == 2 || r_pos == 4) && receivedChar != 46 ) ||
                ( (r_pos == 1 || r_pos == 3 || r_pos == 5) && receivedChar != 48 ) ||
                ( (r_pos > 6 || r_pos < 15) && (receivedChar < 48 && receivedChar > 57 ) )))
            ) {
              nonsenseData();

              return;
            }
            
            if( ps == SERIAL_NUMBER ) {
              if ( receivedChar == 40 ) {
                p_pos = 0;
              }

              if ( p_pos >= 0 ) {
                p_pos++;
              }
      
              if( p_pos > 1 && p_pos < 10 ) {
                meter_ser[p_pos - 2] = receivedChar;
              }

              if ( receivedChar == 41 ) {
                p_pos = -1;
                ps = REQUEST_MODE;
              }
            } else {
              if ( receivedChar == 3 ) {
                p_pos = 0;
              }
              
              if ( p_pos >= 0 ) {
                p_pos++;
              }

              if ( ps == REQUEST_MODE && p_pos == 2 ) {
                RS485.end();
              
                digitalWrite(ledPin, HIGH);
                ps = NONE;

                lcd.setCursor(0, 1);
                lcd.print("SeriNo: ");
                lcd.setCursor(8, 1);
                lcd.print( meter_ser );

                if( invert == true ) {
                  lcd.setCursor(15, 0);
                  lcd.print("r");
                }

                Serial.println( "" );
                Serial.print("Meter Serial Number : ");
                Serial.println( meter_ser );
                Serial.println( "Reading finished." );

                Serial.print("Programming Mode : ");
                Serial.println( response );

                return;
              }              
            }
          }

          start_time = millis();
        }
      } 
    }
  }

  if( millis() - sleep_time > 25000 ) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Entering");
    lcd.setCursor(0, 1);
    lcd.print( "Deepsleep Mode" );

    sleep_time = millis();
    delay(1000);
    lcd.noDisplay();

    digitalWrite(lcdLightPin, LOW);
    esp_deep_sleep_start();
  }
}