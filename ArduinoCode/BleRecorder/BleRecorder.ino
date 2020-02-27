/*
  This example reads audio data from the on-board PDM microphones, and prints
  out the samples to the Serial console. The Serial Plotter built into the
  Arduino IDE can be used to plot the audio data (Tools -> Serial Plotter)

  Please try with the Circuit Playground Bluefruit

  This example code is in the public domain.
*/

#include <PDM.h>

#include <SPI.h>
#include <SD.h>
#include <bluefruit.h>
#include <Adafruit_LittleFS.h>
#include <InternalFileSystem.h>

using namespace Adafruit_LittleFS_Namespace;

#define BUFFER_SIZE 256
#define NUM_BUFFER 50

#define CONFIG_FILENAME    "/CONFIG.TXT"

SDLib::File myFile;
Adafruit_LittleFS_Namespace::File configFile(InternalFS);

unsigned int file_num;
uint8_t buffer[64] = { 0 };

// BLE Service
BLEDfu  bledfu;  // OTA DFU service
BLEDis  bledis;  // device information
BLEUart bleuart; // uart over ble
BLEBas  blebas;  // battery


volatile unsigned long totalRead = 0;

short pdmBuffer[BUFFER_SIZE];

// buffer to read samples into, each sample is 16-bits
short sampleBuffer[NUM_BUFFER][BUFFER_SIZE];
// number of samples read
volatile int samplesRead[NUM_BUFFER];

volatile int bufferStatus[NUM_BUFFER];

volatile int sdIndex = 0;

volatile int pdmIndex = 0;

unsigned long startTS = 0;

volatile int started = 0;

volatile int samplesReadGen = 0;

unsigned short rc_length = 0;

char fileName[50];

uint8_t ble_ch;
uint8_t ble_cmd_buff[64];
uint8_t ble_cmd_index;
uint8_t _header;
uint8_t _length;
uint8_t _index;

void setup() {
  Serial.begin(115200);
  while (!Serial);

  pdm_init();
  Serial.println("pdm_init");
  delay(100);
  
  sd_init();
  Serial.println("sd_init");
  delay(100);
  
  ble_init();

  Serial.println("ble_init");
  delay(100);

  InternalFS.begin();
  init_internal_config();

  Serial.print("file_num=");
  Serial.println(file_num);
}

void sd_init() {
   if (!SD.begin(SS)) {
    Serial.println("initialization failed!");
    while (1);
  }

}

void init_internal_config() {

  configFile.open(CONFIG_FILENAME, FILE_O_READ);
  
  if ( configFile )
  {
      uint32_t readlen;
      
      readlen = configFile.read(buffer, sizeof(buffer));
      
      if (readlen == 4) {
        memcpy(&file_num, buffer, sizeof(unsigned int));  
      }  
  } 
  else {
    configFile.open(CONFIG_FILENAME, FILE_O_WRITE);
    file_num = 0;
    memcpy(buffer, &file_num, sizeof(unsigned int));
    configFile.write(buffer, sizeof(unsigned int));
  }
  configFile.close();
}

void file_num_inc() {
  if( configFile.open(CONFIG_FILENAME, FILE_O_WRITE) )
  {
    configFile.seek(0);
    file_num++;
    memcpy(buffer, &file_num, sizeof(unsigned int));
    configFile.write(buffer, sizeof(unsigned int));
    configFile.close();
  }
  
}

void pdm_init() {
  PDM.setPins(6,11,-1);

  pinMode(A5, OUTPUT);
  digitalWrite(A5, LOW);

  // configure the data receive callback
  PDM.onReceive(onPDMdata);

  // optionally set the gain, defaults to 20
  // PDM.setGain(30);

  // initialize PDM with:
  // - one channel (mono mode)
  // - a 16 kHz sample rate
  if (!PDM.begin(1, 16000)) {
    Serial.println("Failed to start PDM!");
    while (1);
  }
}

void ble_init() {
  // BLE configurations

  // Setup the BLE LED to be enabled on CONNECT
  // Note: This is actually the default behaviour, but provided
  // here in case you want to control this LED manually via PIN 19
  Bluefruit.autoConnLed(true);

  // Config the peripheral connection with maximum bandwidth 
  // more SRAM required by SoftDevice
  // Note: All config***() function must be called before begin()
  Bluefruit.configPrphBandwidth(BANDWIDTH_MAX);

  Bluefruit.begin();
  Bluefruit.setTxPower(4);    // Check bluefruit.h for supported values
  Bluefruit.setName("Bluefruit52");
  //Bluefruit.setName(getMcuUniqueID()); // useful testing with multiple central connections
  Bluefruit.Periph.setConnectCallback(connect_callback);
  Bluefruit.Periph.setDisconnectCallback(disconnect_callback);

  // To be consistent OTA DFU should be added first if it exists
  bledfu.begin();

  // Configure and Start Device Information Service
  bledis.setManufacturer("Adafruit Industries");
  bledis.setModel("Bluefruit Feather52");
  bledis.begin();

  // Configure and Start BLE Uart Service
  bleuart.begin();

  // Start BLE Battery Service
  blebas.begin();
  blebas.write(100);

  // Set up and start advertising
  startAdv();
}

void startAdv(void)
{
  // Advertising packet
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();

  // Include bleuart 128-bit uuid
  Bluefruit.Advertising.addService(bleuart);

  // Secondary Scan Response packet (optional)
  // Since there is no room for 'Name' in Advertising packet
  Bluefruit.ScanResponse.addName();
  
  /* Start Advertising
   * - Enable auto advertising if disconnected
   * - Interval:  fast mode = 20 ms, slow mode = 152.5 ms
   * - Timeout for fast mode is 30 seconds
   * - Start(timeout) with timeout = 0 will advertise forever (until connected)
   * 
   * For recommended advertising interval
   * https://developer.apple.com/library/content/qa/qa1931/_index.html   
   */
  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setInterval(32, 244);    // in unit of 0.625 ms
  Bluefruit.Advertising.setFastTimeout(30);      // number of seconds in fast mode
  Bluefruit.Advertising.start(0);                // 0 = Don't stop advertising after n seconds  
}

void loop() {

  while ( bleuart.available() )
  {
    ble_ch = (uint8_t) bleuart.read();

    //Serial.println(ble_ch, HEX);

    bleHandler();
  }
//
//  if (Serial.available() > 0) {
//    char c = Serial.read();
//
//    if (c == 's') {
//      // open new file and record for 10 seconds
//      Serial.println("start recording");
//      digitalWrite(A5, HIGH);
//      myFile = SD.open(fileName, FILE_WRITE);
//
//      startTS = millis();
//
//      started = 1;
//      
//    } else if (c = 'c') {
//      
//    }
//    
//  }
  
  // wait for samples to be read

  if (started)
  {

     // test
      
      if (bufferStatus[sdIndex] == 2) {
         myFile.write((const char*)sampleBuffer[sdIndex], 512);
//         for (int j = 0; j < samplesRead[sdIndex]; j++) {
//          myFile.print(sampleBuffer[sdIndex][j]);
//          myFile.write(0x0A);
//        }
        Serial.print("sdIndex=");
        Serial.print(sdIndex);

        Serial.print(",pdmIndex=");
        Serial.println(pdmIndex);
        bufferStatus[sdIndex] = 0;
        samplesRead[sdIndex] = 0;

        sdIndex++;
        if (sdIndex == NUM_BUFFER) {
          sdIndex = 0; 
       }
      }
  
//     for(int i = 0; i < NUM_BUFFER; i++) {
//      if (bufferStatus[i] == 2) { // ready for spi
//        for (int j = 0; j < samplesRead[i]; j++) {
//          myFile.print(sampleBuffer[i][j]);
//          myFile.write(0x0A);
//        }
//        Serial.print("write buffer=");
//        Serial.print(i);
//
//        Serial.print(",sampleRead=");
//        Serial.println(samplesRead[i]);
//        
//        bufferStatus[i] = 0;
//        samplesRead[i] = 0;
//      }
//    }
  } else {
    if (samplesReadGen)
    samplesReadGen = 0;
  }


  if (started == 1 && millis() - startTS > rc_length * 1000) {
    started = 0;
    Serial.println("close file1");
    delay(10);
    digitalWrite(A5, LOW);
    Serial.println("close file2");
    delay(10);
    myFile.close();
    Serial.println("close file3");
    delay(10);
   }
  
}

void process_recording() {
  
  unsigned short timestamp = 0;

  if (ble_cmd_buff[0] == 0x01 && started == 0) { // start recording, return file name
    memcpy(&rc_length, ble_cmd_buff + 1, sizeof(unsigned short));
    memcpy(&timestamp, ble_cmd_buff + 1 + sizeof(unsigned short), sizeof(unsigned short));

    file_num_inc();

    memset(fileName,0,sizeof(fileName));

    sprintf(fileName, "R%lu", file_num);

    Serial.println("start recording");
    Serial.println(fileName);
    digitalWrite(A5, HIGH);
    myFile = SD.open(fileName, FILE_WRITE);

    startTS = millis();

    started = 1;
  }
  else if (ble_cmd_buff[0] == 0x02) { // read recent recorded file

    Serial.println(fileName);
    myFile = SD.open(fileName);
  
    if (myFile) {
      Serial.println("file opened");
      delay(1);
  
      const uint16_t data_mtu = Bluefruit.Connection(0)->getMtu() - 3;

      uint8_t *ble_buffer = (uint8_t*)malloc(data_mtu);

      uint8_t ble_index = 0;
     
      // read from the file until there's nothing else in it:
      while (myFile.available()) {

        ble_buffer[ble_index] = myFile.read();
        ble_index++;

        if (ble_index == data_mtu) {
          if ( !bleuart.write(ble_buffer, data_mtu) ) break;
          ble_index = 0;
        }
      }

      free(ble_buffer);
      myFile.close();
    }
  }
}

void bleHandler() {

          
  switch(_index) {
      case 0:
        if (ble_ch == 0x8F || ble_ch == 0x90) {
          _index++;
          _header = ble_ch;
        }
        break;
      case 1:
        if (_length > 25) {
          _index = 0;
        }
        else 
        {
          _length = ble_ch;
          _index++;
        }
        break;
      default:

        ble_cmd_buff[_index - 2] = ble_ch;

        if (_length == _index - 1) {
          Serial.println("received cmd");
          delay(10);
          for(int i = 0; i < _length;i++){
            Serial.println(ble_cmd_buff[i], HEX);
            delay(10);
          }
          if (_header == 0x8F) {
              process_recording();
            } else {
              
            }
          
          _index  = 0;
          
        } else if (_index - 1 > _length) {
          // max out
          Serial.println("max out");
          _index  = 0;
        } else {

          Serial.print("index=");
          Serial.println(_index);
          delay(10);
   
          _index++;
        }
        break;
  }
}

// callback invoked when central connects
void connect_callback(uint16_t conn_handle)
{
  // Get the reference to current connection
  BLEConnection* connection = Bluefruit.Connection(conn_handle);

  char central_name[32] = { 0 };
  connection->getPeerName(central_name, sizeof(central_name));

  Serial.print("Connected to ");
  Serial.println(central_name);
}

/**
 * Callback invoked when a connection is dropped
 * @param conn_handle connection where this event happens
 * @param reason is a BLE_HCI_STATUS_CODE which can be found in ble_hci.h
 */
void disconnect_callback(uint16_t conn_handle, uint8_t reason)
{
  (void) conn_handle;
  (void) reason;
 
  Serial.println();
  Serial.print("Disconnected, reason = 0x"); Serial.println(reason, HEX);
}

void onPDMdata() {
  // query the number of bytes available
  int bytesAvailable = PDM.available();

  // read into the sample buffer

  if (started)
  {
     if (bufferStatus[pdmIndex] == 0)
     {
       bufferStatus[pdmIndex] = 1;
       PDM.read(sampleBuffer[pdmIndex], bytesAvailable);
       samplesRead[pdmIndex] = bytesAvailable / 2;
       bufferStatus[pdmIndex] = 2;
       
       pdmIndex++;
       if (pdmIndex == NUM_BUFFER) {
          pdmIndex = 0; 
       }
     } else {
       Serial.println("sd read too slow");
       PDM.read(sampleBuffer[pdmIndex], bytesAvailable);
     }

   
//    
//     for(int i = 0; i < NUM_BUFFER; i++) {
//      if (bufferStatus[i] == 0) {
//        bufferStatus[i] = 1;
//        PDM.read(sampleBuffer[i], bytesAvailable);
//        samplesRead[i] = bytesAvailable / 2;
//        bufferStatus[i] = 2;
//        break;
//      }
//    }
  }else {
    PDM.read(sampleBuffer[0], bytesAvailable);
    samplesReadGen = bytesAvailable / 2;
  }
  


  
}
