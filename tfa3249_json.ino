/*
 * decode TFA Dostmann Thermo Hygro Sender 30.3249.02
 * send ook on 433.920 MHz
 * receivce test with aurel AC-RX-433 or MX-RM-5V
 * Data pin connect to Pin2 Arduino
 * Decode from https://github.com/merbanan/rtl_433/blob/master/src/devices/lacrosse_tx141x.c
 * Checksum Cal from https://github.com/merbanan/rtl_433/blob/master/src/devices/tfa_30_3221.c  
 * lfsr_digest8_reflect from https://github.com/merbanan/rtl_433/blob/master/src/util.c
 * Linear-feedback shift register see https://en.wikipedia.org/wiki/Linear-feedback_shift_register
 *  * 
 * send a preamble - 833 us high followed by 833 us low puls repeated 4 times, then a paket with 41 Bit, the preamble and the paket are repeated 4 time  
 * the hole sequenz is  afer 300ms, the next data is send after 50 sec.
 * Bit 41 seems to be always 1 and is not included in the checksum
 * 
 * Data Outout a Json Structure, Example
 *  {"model":"TFA3249","id":"E9","Channel":1,"temperature_C":21.3,"humidity":45,"dewpoint_C":8.8,"battery_low":0,"test":0,"temp_raw":713,"CRC":"CF.OK"}
 *  {"model":"TFA3249","id":"B4","Channel":3,"temperature_C":22.1,"humidity":46,"dewpoint_C":9.9,"battery_low":0,"test":0,"temp_raw":721,"CRC":"2B.OK"}
 *  The dewpoint is calculated from the temperature und humidity
 *  
 *  DEBUG 0 only Json Output
 *  DEBUG 1 the hole sequnce |0|1D215905B13|41|3||1|1D25905B13|41|4||2|1D25905B13|41|4||3|1D25905B13|41|4|#a179#s5#k3
 *  DEBUG 2 the first paket woh correct checksum in HEX an BIN B4 22 CF 2E 78 0000000000000000000000010110100001000101100111100101110011110000

 *  interrupt handling from https://github.com/jeelabs/jeelib/blob/master/examples/RF12/RFM12B_OOK/RFM12B_OOK.ino

 */

const char compile_date[] PROGMEM = __DATE__ " " __TIME__;
const char boardName[] PROGMEM = BOARD;

const char TFA3249_FormatJsonMwStr[] PROGMEM =  "{\"model\":\"TFA3249\",\"id\":\"%02X\",\"Channel\":%-d,\"temperature_C\":%s,\"humidity\":%d,\"dewpoint_C\":%s,\"battery_low\":%-d,\"test\":%-d,\"temp_raw\":%-d,\"CRC\":\"%02X.%s\"}";

#define RXDATAPIN 2
#define INTERRUPT 0  // for pin 2
#define DEBUG 0


struct
{
  word pulsNrAll;
  int state;
  int aktSeq;
  boolean WaitInitready;
  byte pulsNrInitSeq;
  word pulsNrSeq[4];
  word seqState[4];
  uint64_t seqData[4];
} TFA3249;



struct
{
  byte lastHigh;
  byte lastLow;
  word lastDauer;
} Plusdata;

byte tmpHPuls;
byte tmpLPuls;

const byte TFA3249DauerInitPlusSchwelle01time = 75 ;
const word TFA3249DauerInitPlus = 340;
const byte TFA3249DauerStartSeq = 165;

boolean revok = false;
uint64_t lastrevtime;

enum { UNKNOWN, RevSeq1, RevSeq2, RevSeq3, RevSeq4, DONE };




// **********************************************************************
void setup ()
{
  char str[30];
  Serial.begin(57600);
  Serial.print(F("Decode TFA 30.3249.02 "));
  strcpy_P(str,  compile_date);
  Serial.print(str);
  Serial.print(F(" "));
  strcpy_P(str,  boardName);
  Serial.println(str);
  pinMode(RXDATAPIN, INPUT);
  digitalWrite(RXDATAPIN, 1); // pull-up
  reset_TFA3249();
  /* prescaler 64 -> 250 KHz = 4 usec/count, max 1.024 msec (16 MHz clock) */
  TCNT2 = 0;    //reset timer
  TCCR2A = 0; //Timer2 Settings: WGM mode 0
  TCCR2B = _BV(CS22)  ;  //Timer2 Settings: Timer Prescaler /64
  TIMSK2 = _BV(TOIE2);   //Timer2 Overflow Interrupt Enable
  attachInterrupt (INTERRUPT, isr, CHANGE);   // interrupt 0 is pin 2
}



// **********************************************************************
static void reset_TFA3249()
{
  memset(&TFA3249, 0x00, sizeof(TFA3249));
  TFA3249.state = UNKNOWN;
  TFA3249.WaitInitready = false;
  TFA3249.aktSeq = -1;
}



// **********************************************************************
ISR(TIMER2_OVF_vect)
{
  if (TFA3249.state == UNKNOWN)
  {
    reset_TFA3249();
  }
  else
  {
    if ( TFA3249.pulsNrAll > 43 ) {
      if ((TFA3249.state == RevSeq4) & (TFA3249.pulsNrSeq[TFA3249.aktSeq] > 39 )) {
        TFA3249BitSet(tmpHPuls);
      }
      TFA3249.state = DONE;
    } else
    {
      reset_TFA3249();
    }
  }
  Plusdata.lastHigh = 0;
  Plusdata.lastLow = 0;
  Plusdata.lastDauer = 0;
}


// **********************************************************************
void TFA3249BitSet(byte dauer)
{
  byte bitvalue = 0;
  if ( dauer > TFA3249DauerInitPlusSchwelle01time  )
  {
    bitvalue = 1;
  } else {
    bitvalue = 0;
  }
  TFA3249.seqData[TFA3249.aktSeq] = (TFA3249.seqData[TFA3249.aktSeq] << 1) | bitvalue;
  TFA3249.pulsNrAll++;
  TFA3249.pulsNrSeq[TFA3249.aktSeq]++;
}


// **********************************************************************
void isr ()
{
  byte count = TCNT2;
  TCNT2 = 0;
  if (TFA3249.state == DONE)
  {
    return;
  }
  if (digitalRead(RXDATAPIN) == HIGH)
  {
    Plusdata.lastHigh = tmpHPuls;
    Plusdata.lastLow = count;
    Plusdata.lastDauer = count + tmpHPuls;
  }
  else
  {
    tmpHPuls = count;
  }
  if (Plusdata.lastDauer > 0)
  {
    if ( (Plusdata.lastDauer > TFA3249DauerInitPlus ) & (digitalRead(RXDATAPIN) == LOW)) {
      if ((TFA3249.pulsNrInitSeq  >= 2) & (TFA3249.WaitInitready == false)) {
        TFA3249.state++;
        TFA3249.aktSeq++;
        TFA3249.WaitInitready = true;
      }
      TFA3249.pulsNrInitSeq++;
      TFA3249.pulsNrAll++;
    }
    else
    {
      if (digitalRead(RXDATAPIN) == LOW)
      {
        if (  (TFA3249.state > UNKNOWN))
        {
          TFA3249BitSet(Plusdata.lastHigh);
          if (TFA3249.WaitInitready ) {
            TFA3249.seqState[TFA3249.aktSeq] = TFA3249.pulsNrInitSeq ;
          }
          TFA3249.pulsNrInitSeq = 0;
          TFA3249.WaitInitready = false;

        }
        else
        {
          reset_TFA3249();
        }
      }
    }
  }
}


// **********************************************************************
void print32bitasbin(uint32_t value)
{
  char str[33];
  byte tmp;
  uint32_t tmpmask = 0x80000000l;
  for (int i = 0; i < 32; i++)
  {
    tmp = 0x00;
    if ((value & tmpmask) > 0) {
      tmp = 0x01;
    }
    str[i] = 0x30 + tmp;
    tmpmask = tmpmask >> 1;
  }
  str[32] = 0x0;
  Serial.print(str);
}

void print64bitasbin(uint64_t value)
{
  print32bitasbin((uint32_t)((value >> 32) & 0xFFFFFFFF));
  print32bitasbin((uint32_t)(value & 0xFFFFFFFF));
}


// **********************************************************************
char* formatDouble( double value, byte precision, char* ascii, uint8_t ascii_len) {
  // formats val with number of decimal places determine by precision
  // precision is a number from 0 to 6 indicating the desired decimial places

  if ( value >= 0.0d )
  {
    snprintf(ascii, ascii_len, "%d", int(value));
  } else
  {
    snprintf(ascii, ascii_len, "-%d", int(value * -1.0d));
  }
  if ( precision > 0) {
    strcat(ascii, ".");
    unsigned long frac;
    unsigned long mult = 1;
    byte padding = precision - 1;
    while (precision--)
      mult *= 10;

    if (value >= 0)
      frac = (value - int(value)) * mult;
    else
      frac = (int(value) - value ) * mult;
    unsigned long frac1 = frac;
    while ( frac1 /= 10 )
      padding--;
    while (  padding--)
      strcat(ascii, "0");
    char str[7];
    snprintf(str, sizeof(str), "%lu", frac);
    strcat(ascii, str);
  }
  return 0x00;
}

// **********************************************************************
float gettaupunkt(float temp, float hum)
{
  float a = 17.271;
  float b = 237.7;
  float taupunktTemp = (a * temp) / (b + temp) + log(hum / 100);
  float p = (b * taupunktTemp) / (a - taupunktTemp);
  return p;

}

// **********************************************************************
void loop()
{
  // put your main code here, to run repeatedly:
  if (millis() - lastrevtime > 500) {
    revok = false;
  }
  if (TFA3249.state == DONE ) {
#if DEBUG >= 1
    outputrevBuffer();
#endif
    OutPutRevData();
  }
}


// **********************************************************************
void  OutPutRevData()
{
  byte b[5];
  byte i = 0;
  char FormatStrbuffer[200];
  char str[200];
  char tstr[6];
  char tpstr[6];

  do  {
    if ((TFA3249.pulsNrSeq[i] >= 41) & (revok == false) ) {
      b[4] = byte(TFA3249.seqData[i] >> 1);
      b[3] = byte(TFA3249.seqData[i] >> 9);
      b[2] = byte(TFA3249.seqData[i] >> 17);
      b[1] = byte(TFA3249.seqData[i] >> 25);
      b[0] = byte(TFA3249.seqData[i] >> 33);
      int observed_checksum = b[4];
      int computed_checksum = lfsr_digest8_reflect(b, 4, 0x31, 0xf4);
      if (observed_checksum == computed_checksum) {
        int id = b[0] & 0xff;
        int battery_low = (b[1] >> 7);
        int testbit     = (b[1] & 0x40) >> 6;
        int channel  = ((b[1] & 0x30) >> 4) + 1;
        int temp_raw = ((b[1] & 0x0F) << 8) | b[2];
        float temp_c = (temp_raw - 500) * 0.1f; // Temperature in C
        int humidity = b[3] & 0xff ;
        float tp = gettaupunkt(temp_c, float(humidity));
        formatDouble(temp_c, 1, tstr, sizeof(tstr));
        formatDouble(tp, 1, tpstr, sizeof(tpstr));
        str[0] = 0;
        strcpy_P(FormatStrbuffer, TFA3249_FormatJsonMwStr);
        snprintf(str, sizeof(str), FormatStrbuffer , id, channel, tstr, humidity, tpstr,  battery_low, testbit, temp_raw,computed_checksum, "OK");
        Serial.println(str);

#if DEBUG >= 2
        for (byte j = 0; j < 5; j++) {
          Serial.print(b[j], HEX);
          Serial.print(" ");
        }
        print64bitasbin(TFA3249.seqData[i]);  Serial.println(" ");
#endif
        i = 5;
        lastrevtime = millis();
        revok = true;
      }
    }
    i++;
  } while  (i < 4);
  reset_TFA3249();
}


// **********************************************************************
// from https://github.com/merbanan/rtl_433/blob/master/src/util.c
// **********************************************************************

uint8_t lfsr_digest8_reflect(uint8_t const message[], int bytes, uint8_t gen, uint8_t key)
{
  uint8_t sum = 0;
  // Process message from last byte to first byte (reflected)
  for (int k = bytes - 1; k >= 0; --k) {
    uint8_t data = message[k];
    // Process individual bits of each byte (reflected)
    for (int i = 0; i < 8; ++i) {
      // fprintf(stderr, "key is %02x\n", key);
      // XOR key into sum if data bit is set
      if ((data >> i) & 1) {
        sum ^= key;
      }

      // roll the key left (actually the lsb is dropped here)
      // and apply the gen (needs to include the dropped lsb as msb)
      if (key & 0x80)
        key = (key << 1) ^ gen;
      else
        key = (key << 1);
    }
  }
  return sum;
}




// **********************************************************************
void outputrevBuffer()
{
  for (byte i = 0; i < 4; i++) {
    Serial.print(F("|"));
    Serial.print(i);
    Serial.print(F("|"));
    Serial.print(((uint32_t)((TFA3249.seqData[i]  >> 32) & 0xFFFFFFFF)), HEX);
    Serial.print(( ((uint32_t)(TFA3249.seqData[i]  & 0xFFFFFFFF))), HEX);
    Serial.print(F("|"));
    Serial.print(TFA3249.pulsNrSeq[i]);
    Serial.print(F("|"));
    Serial.print(TFA3249.seqState[i]);
    Serial.print(F("|"));
  }
  Serial.print(F("#a"));
  Serial.print(TFA3249.pulsNrAll);
  Serial.print(F("#s"));
  Serial.print(TFA3249.state);
  Serial.print(F("#k"));
  Serial.print(TFA3249.aktSeq);
  Serial.println();
  Serial.flush();
}
