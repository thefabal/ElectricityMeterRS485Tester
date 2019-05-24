#include <Arduino.h>

LiquidCrystal lcd(22, 23, 25, 18, 19, 21);

enum programState {NONE, IDENTIFICATION, MODE_CHANGE, SERIAL_NUMBER, REQUEST_MODE};
enum flagCode {UNK, AEL, LUN, MSY, VIK, BYL, LGZ};

int sizeof_n( char text[] ) {
  if( strlen(text) == 0 ) {
    return 0;
  }
  
  int i = 0;
  while( text[i] != '\00' ) {
    i++;
  }

  return i;
}

byte getBCC(char text[]) {
  byte bcc = 0x00;

  int i = 0;
  do {
    i++;

    if( text[i] == '\00' || i > strlen(text) ) {
      return 0;
    }
    
    bcc ^= (unsigned char)text[i];
  } while( text[i] != '\03' );

  return bcc;  
}

int getBaudRate(int baudChar) { 
  switch ( baudChar ) {
    case 0: return 300; break;
    case 1: return 600; break;
    case 2: return 1200; break;
    case 3: return 2400; break;
    case 4: return 4800; break;
    case 5: return 9600; break;
    case 6: return 19200; break;
    default: return -1;
  }
}

flagCode getMeterBrand( char flag[] ) {
  if( strcmp( flag, "LUN") == 0 ) {
    return LUN;
  } else if( strcmp( flag, "MSY") == 0 ) {
    return MSY;
  } else if( strcmp( flag, "VIK") == 0 ) {
    return VIK;
  } else if( strcmp( flag, "AEL") == 0 ) {
    return AEL;
  } else if( strcmp( flag, "BYL") == 0 ) {
    return BYL;
  } else if( strcmp( flag, "LGZ") == 0 ) {
    return LGZ;
  } else {
    return UNK;
  }
}

void nonsenseData() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print( "ANLAMSIZ VERI" );
  lcd.setCursor(0, 1);
  lcd.print( "TEKRAR DENEYIN" );
}