
  decode TFA Dostmann Thermo Hygro Sender 30.3249.02 that   send ook on 433.920 MHz
  

  
  receivce test with aurel AC-RX-433 or MX-RM-5V
  
  Data pin connect to Pin2 Arduino
  
  Decode from https://github.com/merbanan/rtl_433/blob/master/src/devices/lacrosse_tx141x.c
  
  Checksum Cal from https://github.com/merbanan/rtl_433/blob/master/src/devices/tfa_30_3221.c  
  
  lfsr_digest8_reflect from https://github.com/merbanan/rtl_433/blob/master/src/util.c
  
  Linear-feedback shift register see https://en.wikipedia.org/wiki/Linear-feedback_shift_register
  
  
  send a preamble - 833 us high followed by 833 us low puls repeated 4 times, then a paket with 41 Bit, the preamble and the paket are repeated 4 time  
  
  the hole sequenz is  afer 300ms, the next data is send after 50 sec.
  
  Bit 41 seems to be always 1 and is not included in the checksum
  
  
  Data Outout a Json Structure, Example
  
   {"model":"TFA3249","id":"E9","Channel":1,"temperature_C":21.3,"humidity":45,"dewpoint_C":8.8,"battery_low":0,"test":0,"temp_raw":713,"CRC":"CF.OK"}
   
   {"model":"TFA3249","id":"B4","Channel":3,"temperature_C":22.1,"humidity":46,"dewpoint_C":9.9,"battery_low":0,"test":0,"temp_raw":721,"CRC":"2B.OK"}
   
   The dewpoint is calculated from the temperature und humidity
   
   
   DEBUG 0 only Json Output 
   
   DEBUG 1 the hole sequnce |0|1D215905B13|41|3||1|1D25905B13|41|4||2|1D25905B13|41|4||3|1D25905B13|41|4|#a179#s5#k3
   
   DEBUG 2 the first paket woh correct checksum in HEX an BIN B4 22 CF 2E 78 0000000000000000000000010110100001000101100111100101110011110000

   interrupt handling from https://github.com/jeelabs/jeelib/blob/master/examples/RF12/RFM12B_OOK/RFM12B_OOK.ino

 
