// Kalibrasi dari hasil library DF Robot yang di modifikasi

#include <EEPROM.h>
#include "GravityTDS.h"

#define TdsSensorPin 35

#define EEPROM_SIZE 512

GravityTDS gravityTds;

float temperature = 25, tdsValue, tdsCalibrated;
const float slope = (1.77 + 1.693)/2;
// const float slopeA = 1.782;
// const float slopeB = 1.661;
// const float slopeC = 1.209;
const float offset = 0.0;

float readTDS() {
  float sum = 0;
  const int samples = 10;
  for (int i = 0; i < samples; i++) {
    gravityTds.update();
    sum += gravityTds.getTdsValue();
    delay(50);
  }
  return sum / samples;
}

void setup()
{
    Serial.begin(115200);
    
    EEPROM.begin(EEPROM_SIZE);  //Initialize EEPROM
    
    gravityTds.setPin(TdsSensorPin);
    gravityTds.setAref(3.3);  //reference voltage on ADC, default 5.0V on Arduino UNO
    gravityTds.setAdcRange(4096);  //1024 for 10bit ADC;4096 for 12bit ADC
    gravityTds.begin();  //initialization
    
}

void loop()
{
    tdsValue = readTDS();
    //temperature = readTemperature();  //add your temperature sensor and read it
    gravityTds.setTemperature(temperature);  // set the temperature and execute temperature compensation
    gravityTds.update();  //sample and calculate
    tdsValue = gravityTds.getTdsValue();  // then get the value
    Serial.print("slope: ");
    Serial.println(slope);
    tdsCalibrated = (tdsValue * slope ) + offset;
    Serial.print("TDS Raw: ");
    Serial.print(tdsValue);
    Serial.print(" ppm | TDS Kalibrasi: ");
    Serial.print(tdsCalibrated);
    Serial.println(" ppm");
    delay(1000);
}
