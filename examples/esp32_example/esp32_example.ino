#include "sim7020_lib.h"

SIM7020 nb_modem(14, 13, 35, "28"); //Rx, Tx, Power, RF band

void setup() {
  Serial.begin(115200);
  delay(100);

  Serial.println("\nWait...");

  nb_modem.set_NetworkCredentials("virtueyes.com.br","virtu","virtu");

  nb_modem.HwInit();
  
  nb_modem.set_SleepPin(15);

  nb_modem.NbiotManager();
}

void loop() {
  Serial.println("flag");
  delay(5000);
}
