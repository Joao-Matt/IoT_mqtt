#include <Arduino.h>
#include <DHT.h>

#define DHTPIN 4
#define DHTTYPE DHT11 // DHT15 usually behaves like DHT11

DHT dht(DHTPIN, DHTTYPE);

void setup()
{
  Serial.begin(115200);
  dht.begin();
}

void loop()
{
  float humidity = dht.readHumidity();
  float temperature = dht.readTemperature(); // Celsius

  if (!isnan(temperature) && !isnan(humidity)) {
    // Serial Plotter: two columns
    Serial.print(temperature);
    Serial.print(" ");
    Serial.println(humidity);
  }
}
