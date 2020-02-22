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

#define BUFFER_SIZE 256
#define NUM_BUFFER 150

File myFile;

const unsigned long recorded_length = 160000;

volatile unsigned long totalRead = 0;

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



const String fileName = "test13.csv";

void setup() {
  Serial.begin(115200);
  while (!Serial);

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


  if (!SD.begin(SS)) {
    Serial.println("initialization failed!");
    while (1);
  }

  digitalWrite(A5, HIGH);
  //myFile = SD.open(fileName, FILE_WRITE);

  started = 1;
  
}

void loop() {  
  // wait for samples to be read

  if (started)
  {
      if (bufferStatus[sdIndex] == 2) {
         for (int j = 0; j < samplesRead[sdIndex]; j++) {
          //myFile.print(sampleBuffer[sdIndex][j]);
          //myFile.write(0x0A);
//          Serial.print(sampleBuffer[sdIndex][j]);
//          Serial.write(0x0A);
          delayMicroseconds(random(100,500));
        }

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
       totalRead++;
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


  if (started == 1 && totalRead * 256 > recorded_length) {
    started = 0;
    digitalWrite(A5, LOW);
//    myFile.close();
//
//    myFile = SD.open(fileName);
//  
//    if (myFile) {
//      // read from the file until there's nothing else in it:
//      while (myFile.available()) {
//        Serial.write(myFile.read());
//      }
//      myFile.close();
//      }
   }
  
}

void onPDMdata() {
  // query the number of bytes available
  int bytesAvailable = PDM.available();

  // read into the sample buffer

  if (started)
  {
    if (bufferStatus[pdmIndex] == 0) {
       bufferStatus[pdmIndex] = 1;
       PDM.read(sampleBuffer[pdmIndex], bytesAvailable);
       samplesRead[pdmIndex] = bytesAvailable / 2;
       bufferStatus[pdmIndex] = 2;
       
       pdmIndex++;
       if (pdmIndex == NUM_BUFFER) {
          pdmIndex = 0; 
       }
    }
    else {
      //Serial.println("data missing");
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
