/*
PERHATIAN
Program ini tidak mendukung pengiriman System Exclusive.
Arduino UDP
Using hardwareSerial2 Sebagai input MIDI
Kandidat Pengirim 1
*/

#include <Arduino.h>

#define TIMER_MS 0              // Timer ticker, 0 untuk disable.
#define UDP_PORT 1112           // port UDP
#define DEBUG_THRU              //komen untuk tanpa debug thru
#define DEBUG_UDP_PACKET        //komen untuk tanpa debug udp packet
#define DEBUG_PACKET_COUNTER    //komen untuk tanpa debug perhitungan paket
#define ESP32_TICKER            //komen untuk menonaktifkan lib ESP32_ticker / nativ Ticker
#define LED_INDIKATOR 2         // pin LED proses
#define PIN_CEK_PAKET 23        // pin untuk triger penampilan debug perhitungan paket


#ifdef ESP32_TICKER
#include <ESP32Ticker.h>
#else
#include <Ticker.h>
#endif

#include "WiFi.h"
#include "AsyncUDP.h"
#include "midi_type.h"


const char * ssid = "MIDI";
const char * password = "MIDIMIDI";

bool timeToRead;

Ticker tickerBaca;
AsyncUDP udp;

#ifdef DEBUG_PACKET_COUNTER
unsigned int packetCounter=0;
unsigned int dataCounter=0;
#endif

void setup();
void loop();
void kirim();
void kosongkan();
MidiType getStatusType(byte status_);
int getStatus_dataCount (byte inStatus);
void readSerial_multy();
void tickerBacaInterupt();

void setup() {
  Serial2.begin(31250);
  Serial.begin(115200);

  pinMode(LED_INDIKATOR, OUTPUT);
  
  #ifdef DEBUG_PACKET_COUNTER
  pinMode(PIN_CEK_PAKET, INPUT_PULLUP);
  #endif

  delay(1000);

#ifdef DEBUG_ON
  Serial.println ("<+++++++++++++++ Debug ON +++++++++++++++++++++ >");
#endif

#ifdef ESP32_TICKER
Serial.println ("Menggunakan ESP32_TICKER");
#else
Serial.println ("Menggunakan TICKER Biasa");
#endif

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  WiFi.setSleep(false);
  if (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("WiFi Failed");
    while (1) {
      delay(1000);
    }
  }
  if (TIMER_MS){
    tickerBaca.attach_ms(TIMER_MS, tickerBacaInterupt);
    Serial.print("Ticker ON :");
    Serial.print(TIMER_MS);
    Serial.println(" ms"); 
  } else {
    Serial.println("Gak pakai ticker");
  }


  
  delay(1000);
    if (udp.connect(IPAddress(255, 255, 255, 255), UDP_PORT)) {
    Serial.println("\n UDP connected : 255,255,255,255");
  }

}


void loop() {
  if (TIMER_MS){   
    if (timeToRead){
      if (Serial2.available()) {
        readSerial_multy();
      }
    timeToRead= false;
    }
  } else {
    if (Serial2.available()) {
        readSerial_multy();
      }
  }

  #ifdef DEBUG_PACKET_COUNTER
  if (!digitalRead(PIN_CEK_PAKET)){
    Serial.println("\n\n===========================");
    Serial.print("Paket Terkirim = ");
    Serial.println(packetCounter);
    Serial.print("Paket Sebesar= ");
    Serial.println(dataCounter);
    Serial.println("===========================");
    delay(100);
    while(1)
    {
      if(digitalRead(PIN_CEK_PAKET)){
        break;
      } 
    }
  }
  #endif
}

void kirim() {
  digitalWrite(LED_INDIKATOR, HIGH);
  unsigned int ret;
#ifdef DEBUG_ON
  Serial.print ("panjangPesanTersimpan : "); Serial.println(panjangPesanTersimpan);
  Serial.print("jumlahPesan : "); Serial.println(jumlahPesan);
#endif

#ifdef DEBUG_UDP_PACKET
  ret = udp.write(_midiMessage, panjangPesanTersimpan);
  Serial.print(ret);
  Serial.print("   ");
#else
  ret = udp.write(_midiMessage, panjangPesanTersimpan);
#endif

  #ifdef DEBUG_PACKET_COUNTER
  if (ret){
    dataCounter += panjangPesanTersimpan;
    packetCounter ++;
  }
  #endif

#ifdef DEBUG_THRU
  for (int i = 0; i < panjangPesanTersimpan; i++) {

    Serial.print(_midiMessage[i], HEX);
  }
  Serial.println();
#endif

#ifdef DEBUG_ON
  Serial.println();
#endif
  digitalWrite(LED_INDIKATOR, LOW);
}

void kosongkan() {
  for (unsigned int i = 0; i < (MIDI_BUFFER_MAX); i++) {
    _midiMessage[i] = NULL;
  }
}

MidiType getStatusType(byte status_) {
  if ((status_ < 0x80) ||
      (status_ == 0xf4) ||
      (status_ == 0xf5) ||
      (status_ == 0xf9) ||
      (status_ == 0xfD))
  {

    return InvalidType;
  }
  if (status_ < 0xf0)
  {

    return MidiType(status_ & 0xf0);
  }
  return MidiType(status_);
}

int getStatus_dataCount (byte inStatus) {
  switch (getStatusType(inStatus))
  {

    case Start:
    case Continue:
    case Stop:
    case Clock:
    case ActiveSensing:
    case SystemReset:
    case TuneRequest:

      _dataCount = 0;
      return true;
      break;


    case ProgramChange:
    case AfterTouchChannel:
    case TimeCodeQuarterFrame:
    case SongSelect:

      _dataCount = 1;
      break;


    case NoteOn:
    case NoteOff:
    case ControlChange:
    case PitchBend:
    case AfterTouchPoly:
    case SongPosition:

      _dataCount = 2;
      break;


    case EndOfSystemExclusive:
      _flagEndSysExclusive = true;
      _dataCount = 0;

      break;

    case SystemExclusive:


      _dataCount = SYS_EX_MAX_SIZE + 3;
      _flagEndSysExclusive = false;
      break;

    case InvalidType:
    default:
      return false;
      break;
  }
}

void readSerial_multy() {
  byte _buff;
  while (1) {
    if (Serial2.available()) {
      _buff = Serial2.read();

#ifdef DEBUG_ON
      Serial.println("Masuk Serial multy");
#endif


      if (_buff & FLAG_BIT_STATUS) {

#ifdef DEBUG_ON
        Serial.println("Masuk Status");
#endif


        if (getStatus_dataCount(_buff)) {
          if (_flagEndSysExclusive) {
            _midiMessage[panjangPesanTersimpan] = _buff;
            panjangPesanTersimpan++;
            _flagEndSysExclusive = false;
          } else {

#ifdef DEBUG_ON
            Serial.println("Status Biasa");
#endif

            _midiMessage[panjangPesanTersimpan] = _buff;

            panjangPesanTersimpan++;
          }
        }

      } else {


#ifdef DEBUG_ON
        Serial.println("Masuk Data");
#endif

        if (_dataCount) {
          _midiMessage[panjangPesanTersimpan] = _buff;
          panjangPesanTersimpan++;
          _dataCount--;
        }
      }

      if (!_dataCount) {

        jumlahPesan++;
      }
      if (panjangPesanTersimpan > MIDI_BUFFER_MAX - 1) {


#ifdef DEBUG_ON
        Serial.println("Buffer Penuh");
#endif
        break;
      }
      if (jumlahPesan >= MIDI_MSG_MAX_UDP) {

        break;
      }
    } else {

      if (!_dataCount) {


#ifdef DEBUG_ON
        Serial.println("Tidak ada pesan lagi");
#endif

        break;
      }
    }
  }
  kirim();
  panjangPesanTersimpan = 0;
  jumlahPesan = 0;
}

void tickerBacaInterupt() {
  timeToRead =true;
}