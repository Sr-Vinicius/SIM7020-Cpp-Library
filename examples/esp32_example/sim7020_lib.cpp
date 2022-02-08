/*
  SIM7020 C++ PORTABLE LIBRARY
*/

#include <string>
#include "arduino.h"
#include "sim7020_lib.h"


std::string command_response;


std::string at_CommandWithReturn(String command, uint16_t timeout){
  std::string res;
  Serial_AT.println(command);
  while(Serial_AT.available() == 0);
  
  unsigned long lastRead = millis();
  while(millis() - lastRead < timeout){   
    while(Serial_AT.available()){
      res = Serial_AT.readString().c_str();
      #ifdef DEBUG_MODE
      printf(res.c_str());
      #endif
      lastRead = millis();
    }
  }
  return res;
}


void at_command(String command, uint32_t timeout) {
  Serial_AT.println(command);
  while (Serial_AT.available() == 0);

  unsigned long lastRead = millis();
  while(millis() - lastRead < timeout) {
    while(Serial_AT.available()) {
      Serial.println(Serial_AT.readString());
      lastRead = millis();
    }
  }
}


void SIM7020::HwInit(){
  std::string aux_string;

  #ifdef DEBUG_MODE
  at_command("AT", 500);
  at_command("ATE1", 500);
  #endif

  #ifndef DEBUG_MODE
  at_command("ATE0", 500);
  #endif
  
  command_response = at_CommandWithReturn("AT+CPIN?", 1000);
  while( (command_response.find("ERROR")) !=  std::string::npos){  
    digitalWrite(pwr, LOW);
    delay(10000);
    digitalWrite(pwr, HIGH);
    command_response = at_CommandWithReturn("AT+CPIN?", 1000);
  }

  command_response.clear();

  aux_string = "AT*MCGDEFCONT=\"IP\",\"" + apn + "\",\"" + user + "\",\"" + psswd + "\"";
  
  at_command("AT+CFUN=0", 1000); //turn-off rf
  at_command("AT+CREG=2", 500);
  at_command(aux_string.c_str(), 2000);
  at_command("AT+CFUN=1", 1000); //turn-on rf

  aux_string.clear();
  
  aux_string = "AT+CBAND=" + rf_band;
  at_command(aux_string.c_str(), 1000);
  
  at_command("AT+COPS=0,0", 1000);
  at_command("AT+CGCONTRDP", 1000);

  #ifdef DEBUG_MODE
  at_command("AT+CMEE=2",500);
  Serial.print("SIM7020 firmware version: ");
  at_command("AT+CGMR", 500);
  Serial.print("APN settings: ");
  at_command("AT*MCGDEFCONT?", 500);
  Serial.print("Banda: ");
  at_command("AT+CBAND?", 500);
  Serial.print("Internet register status: ");
  at_command("AT+CGREG?", 500);
  Serial.print("Network Information: ");
  at_command("AT+COPS?", 500);
  Serial.print("Signal quality: ");
  at_command("AT+CSQ", 500);
  Serial.print("GPRS service attachment: ");
  at_command("AT+CGATT?", 500);
  Serial.print("PDP context definition: ");
  at_command("AT+CGDCONT?", 500);
  Serial.println("\n\nDiagnostic completed!");
  #endif
}


void SIM7020::NbiotManager(){
  socket_host = "www.blinkenlichten.info";
  socket_port = "80";
  http_page = "/print/origin.html";
  app_layer_method = "GET";
  app_layer_protocol = "mqtt"; //Choose between http and mqtt
  
  while(1){
    switch(eNextState){
      case PDP_DEACT:
        eNextState = SIM7020::NetworkAttachHandler();
        break;
      
      case IP_INITIAL:
        eNextState = SIM7020::StartTaskHandler(apn, user, psswd);
        break;

      case IP_START:
        eNextState = SIM7020::BringUpGprsHandler();
        break;
        
      case IP_CONFIG:
        eNextState = SIM7020::WaitGprsHandler();
        break;  
        
      case IP_GPRSACT:
        eNextState = SIM7020::GetLocalIpHandler();
        break;

      case IP_STATUS:
        eNextState = SIM7020::SocketConnectHandler(app_layer_protocol,socket_host, socket_port);
        break;        

      case TCP_CONNECTING:
		    eNextState = SIM7020::WaitSocketHandler();
        break;

      case CONNECT_OK:
        //eNextState = SIM7020::DataSendHandler(app_layer_protocol, app_layer_method ,socket_host, socket_port);
        eNextState = SIM7020::SSL_Connection();
        break;

      case TCP_CLOSING:
        eNextState = SIM7020::WaitSocketCloseHandler();  
        break;

      case TCP_CLOSED:
      Serial.println("Estado ainda n implementado - flag");
	    return;
        break;
    }
  }
}


SIM7020::eNbiotStateMachine SIM7020::NetworkAttachHandler(){
  at_command("AT+CIPSHUT", 10000);
  at_command("AT+CIPMUX=0", 1000);

  command_response = at_CommandWithReturn("AT+CGATT=1", 20000);
  while( (command_response.find("ERROR")) !=  std::string::npos){  //procurar pela substring ERROR na resp de AT+CGATT
    at_command("AT+CGATT=0", 5000);
    command_response = at_CommandWithReturn("AT+CGATT=1", 20000);
    }

  command_response = at_CommandWithReturn("AT+CIPSTATUS", 500);

  if((command_response.find("IP INITIAL")) !=  std::string::npos){
    command_response.clear();
    return IP_INITIAL;
  }
}


SIM7020::eNbiotStateMachine SIM7020::StartTaskHandler(std::string apn, std::string user, std::string psswd){
  std::string aux_string;
  aux_string = "AT+CSTT=\"" + apn + "\",\"" + user + "\",\"" + psswd + "\"";
  at_command(aux_string.c_str(), 2000);
  aux_string.clear();
  command_response = at_CommandWithReturn("AT+CIPSTATUS", 500);
  if((command_response.find("IP START")) !=  std::string::npos)
    return IP_START;
  command_response.clear();   
}


SIM7020::eNbiotStateMachine SIM7020::BringUpGprsHandler(){
  at_command("AT+CIICR", 1000);
  command_response = at_CommandWithReturn("AT+CIPSTATUS", 500);
  if((command_response.find("IP CONFIG")) !=  std::string::npos)
    return IP_CONFIG;
  else if((command_response.find("IP GPRSACT")) !=  std::string::npos)
    return IP_GPRSACT;
  else if((command_response.find("PDP DEACT")) !=  std::string::npos)
    return PDP_DEACT;
  command_response.clear();
}


SIM7020::eNbiotStateMachine SIM7020::WaitGprsHandler(){
  delay(1000);
  command_response = at_CommandWithReturn("AT+CIPSTATUS", 500);
  if((command_response.find("IP GPRSACT")) !=  std::string::npos){
    command_response.clear();
    return IP_GPRSACT;
  }
}


SIM7020::eNbiotStateMachine SIM7020::GetLocalIpHandler(){
  at_command("AT+CIFSR", 1000);
  command_response = at_CommandWithReturn("AT+CIPSTATUS", 500);
  if((command_response.find("IP STATUS")) !=  std::string::npos)
    command_response.clear();
    return IP_STATUS;
}


SIM7020::eNbiotStateMachine SIM7020::SocketConnectHandler(std::string app_protocol,std::string host, std::string port){
  std::string aux_string;
    if(app_layer_protocol.find("http") != std::string::npos){
    aux_string = "AT+CIPSTART=\"TCP\",\"" + host + "\"," + port;
    at_command(aux_string.c_str(), 2000);
    aux_string.clear();
    command_response = at_CommandWithReturn("AT+CIPSTATUS", 500);
    if((command_response.find("CONNECT OK")) !=  std::string::npos)
      return CONNECT_OK;
    else if((command_response.find("TCP CONNECTING")) !=  std::string::npos){
      command_response.clear();
      return TCP_CONNECTING;
      }
  }

  else if(app_layer_protocol.find("mqtt") != std::string::npos){
    std::string host = "broker.emqx.io";
    std::string mqtt_port = "1883";
    // aux_string = "AT+CMQNEW=\"" + host + "\",\"" + mqtt_port + "\",\"9000\",\"100\"";
    // at_command(aux_string.c_str(), 9000);
    // aux_string.clear();

    // aux_string = "AT+CMQCON=\"0\",\"3\",\"myclient\",\"600\",\"0\", \"0\"";
    // command_response = at_CommandWithReturn(aux_string.c_str(), 5000);
    
    //if((command_response.find("OK")) !=  std::string::npos){
    command_response.clear();

    // aux_string = "AT+CMQSUB=\"0\",\"esp32/NbioT\",\"1\"";
    // at_command(aux_string.c_str(), 5000);
    // aux_string.clear();

    // aux_string = "AT+CMQPUB=\"0\",\"esp32/NbioT\",\"1\",\"0\",\"0\",\"8\",\"12345678\"";
    // at_command(aux_string.c_str(), 5000);
    // aux_string.clear();

    return CONNECT_OK;
    //}
  }
}

SIM7020::eNbiotStateMachine SIM7020::Config_TLS()
{
  std::string aux_string, host, port, serve_ca, serve_ca2, client_ca,aux_string_client, private_key,mqtt_port;

  at_command("AT+CMQTTSNEW?", 500);
  host = "broker.emqx.io";
  port = "1883";

  if(command_response.find("null") == std::string::npos)
  {
    aux_string = "AT+CMQNEW=\"" + host + "\",\"" + port + "\",\"9000\",\"100\"";
    at_command(aux_string.c_str(), 9000);
    aux_string.clear();
  }

  
  host = "mqtt.googleapis.com";
  mqtt_port = "8883";
  //OPC A
  serve_ca = "-----BEGIN CERTIFICATE-----MIIB4TCCAYegAwIBAgIRKjikHJYKBN5CsiilC+g0mAIwCgYIKoZIzj0EAwIwUDEkMCIGA1UECxMbR2xvYmFsU2lnbiBFQ0MgUm9vdCBDQSAtIFI0MRMwEQYDVQQKEwpHbG9iYWxTaWduMRMwEQYDVQQDEwpHbG9iYWxTaWduMB4XDTEyMTExMzAwMDAwMFoXDTM4MDExOTAzMTQwN1owUDEkMCIGA1UECxMbR2xvYmFsU2lnbiBFQ0MgUm9vdCBDQSAtIFI0MRMwEQYDVQQKEwpHbG9iYWxTaWduMRMwEQYDVQQDEwpHbG9iYWxTaWduMFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEuMZ5049sJQ6fLjkZHAOkrprlOQcJFspjsbmG+IpXwVfOQvpzofdlQv8ewQCybnMO/8ch5RikqtlxP6jUuc6MHaNCMEAwDgYDVR0PAQH/BAQDAgEGMA8GA1UdEwEB/wQFMAMBAf8wHQYDVR0OBBYEFFSwe61FuOJAf/sKbvu+M8k8o4TVMAoGCCqGSM49BAMCA0gAMEUCIQDckqGgE6bPA7DmxCGXkPoUVy0D7O48027KqGx2vKLeuwIgJ6iFJzWbVsaj8kfSt24bAgAXqmemFZHe+pTsewv4n4Q=-----END CERTIFICATE-----";
  //OPC B
  //serve_ca = "-----BEGIN CERTIFICATE-----MIIB3DCCAYOgAwIBAgINAgPlfvU/k/2lCSGypjAKBggqhkjOPQQDAjBQMSQwIgYDVQQLExtHbG9iYWxTaWduIEVDQyBSb290IENBIC0gUjQxEzARBgNVBAoTCkdsb2JhbFNpZ24xEzARBgNVBAMTCkdsb2JhbFNpZ24wHhcNMTIxMTEzMDAwMDAwWhcNMzgwMTE5MDMxNDA3WjBQMSQwIgYDVQQLExtHbG9iYWxTaWduIEVDQyBSb290IENBIC0gUjQxEzARBgNVBAoTCkdsb2JhbFNpZ24xEzARBgNVBAMTCkdsb2JhbFNpZ24wWTATBgcqhkjOPQIBBggqhkjOPQMBBwNCAAS4xnnTj2wlDp8uORkcA6SumuU5BwkWymOxuYb4ilfBV85C+nOh92VC/x7BALJucw7/xyHlGKSq2XE/qNS5zowdo0IwQDAOBgNVHQ8BAf8EBAMCAYYwDwYDVR0TAQH/BAUwAwEB/zAdBgNVHQ4EFgQUVLB7rUW44kB/+wpu+74zyTyjhNUwCgYIKoZIzj0EAwIDRwAwRAIgIk90crlgr/HmnKAWBVBfw147bmF0774BxL4YSFlhgjICICadVGNA3jdgUM/I2O2dgq43mLyjj0xMqTQrbO/7lZsm-----END CERTIFICATE-----";
  //serve_ca =   "-----BEGIN CERTIFICATE-----MIIC0TCCAnagAwIBAgINAfQKmcm3qFVwT0+3nTAKBggqhkjOPQQDAjBEMQswCQYDVQQGEwJVUzEiMCAGA1UEChMZR29vZ2xlIFRydXN0IFNlcnZpY2VzIExMQzERMA8GA1UEAxMIR1RTIExUU1IwHhcNMTkwMTIzMDAwMDQyWhcNMjkwNDAxMDAwMDQyWjBEMQswCQYDVQQGEwJVUzEiMCAGA1UEChMZR29vZ2xlIFRydXN0IFNlcnZpY2VzIExMQzERMA8GA1UEAxMIR1RTIExUU1gwWTATBgcqhkjOPQIBBggqhkjOPQMBBwNCAARr6/PTsGoOg9fXhJkj3CAk6C6DxHPnZ1I+ER40vEe290xgTp0gVplokojbN3pFx07fzYGYAX5EK7gDQYuhpQGIo4IBSzCCAUcwDgYDVR0PAQH/BAQDAgGGMB0GA1UdJQQWMBQGCCsGAQUFBwMBBggrBgEFBQcDAjASBgNVHRMBAf8ECDAGAQH/AgEAMB0GA1UdDgQWBBSzK6ugSBx+E4rJCMRAQiKiNlHiCjAfBgNVHSMEGDAWgBQ+/v/MUuu/ND4980DQ5CWxX7i7UjBpBggrBgEFBQcBAQRdMFswKAYIKwYBBQUHMAGGHGh0dHA6Ly9v";
  //serve_ca2 = "Y3NwLnBraS5nb29nL2d0c2x0c3IwLwYIKwYBBQUHMAKGI2h0dHA6Ly9wa2kuZ29vZy9ndHNsdHNyL2d0c2x0c3IuY3J0MDgGA1UdHwQxMC8wLaAroCmGJ2h0dHA6Ly9jcmwucGtpLmdvb2cvZ3RzbHRzci9ndHNsdHNyLmNybDAdBgNVHSAEFjAUMAgGBmeBDAECATAIBgZngQwBAgIwCgYIKoZIzj0EAwIDSQAwRgIhAPWeg2v4yeimG+lzmZACDJOlalpsiwJR0VOeapY8/7aQAiEAiwRsSQXUmfVUW+N643GgvuMH70o2Agz8w67fSX+k+Lc=-----END CERTIFICATE-----";
  aux_string.clear();
  



  //LIMPAR E ESCREVE O SERVE_CA
  aux_string = "AT+CSETCA=\"0\",\"0\",\"0\",\"0\",\"0\"";
  at_command(aux_string.c_str(), 500);
  aux_string.clear();

  aux_string = "AT+CSETCA=\"0\",\"702\",\"0\",\"0\",\"" + serve_ca + "\"";
  at_command(aux_string.c_str(), 500);
  aux_string.clear();

  
  //LIMPAR E ESCREVE O CLIENT_CA
  aux_string = "AT+CSETCA=\"1\",\"0\",\"0\",\"0\",\"0\"";
  at_command(aux_string.c_str(), 500);
  aux_string.clear();
  client_ca = "-----BEGIN PUBLIC KEY-----MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEQOVGC+fbMK45MCJscsIknKvV21bzNRCvP12Sf204pxhuf+iqa8zMuPFgPCb7d+bNd82uUlDMnvZYlKkrItXSiw==-----END PUBLIC KEY-----";
  aux_string_client = "AT+CSETCA=\"1\",\"176\",\"0\",\"0\",\"" + client_ca + "\"";
  at_command(aux_string_client.c_str(), 500);
  aux_string_client.clear();

  //LIMPAR E ESCREVE O PRIVATE_KEY
  aux_string = "AT+CSETCA=\"2\",\"0\",\"0\",\"0\",\"0\"";
  at_command(aux_string.c_str(), 500);
  aux_string.clear();
  private_key = "-----BEGIN EC PRIVATE KEY-----MHcCAQEEIEf7pG64E9BqMylniTrkm84zmCfO885apmpLHb5dybHmoAoGCCqGSM49AwEHoUQDQgAEQOVGC+fbMK45MCJscsIknKvV21bzNRCvP12Sf204pxhuf+iqa8zMuPFgPCb7d+bNd82uUlDMnvZYlKkrItXSiw==-----END EC PRIVATE KEY-----";
  aux_string = "AT+CSETCA=\"2\",\"224\",\"0\",\"0\",\"" + private_key + "\"";
  at_command(aux_string.c_str(), 500);
  aux_string.clear();

  at_command("AT+CMQTTSNEW?", 500);
  at_command("AT+CMQCON=?", 500);




  // aux_string = "AT+CMQNEW=\"0\",\"0\",\"9000\",\"100\"";
  // at_command(aux_string.c_str(), 500);
  // aux_string.clear();
  aux_string = "AT+CMQNEW=\"" + host + "\",\"" + mqtt_port + "\",\"9000\",\"100\"";
  at_command(aux_string.c_str(), 9000);
    at_command("AT+CMQTTSNEW?", 500);
  // aux_string.clear();

  // at_command("AT+CMQNEW?", 500);

  aux_string = "AT+CMQCON=\"0\",\"3\",\"projects/iot-test-328120/locations/us-central1/registries/test-registry/devices/nb_iot\",\"600\",\"0\", \"0\"";
  at_command(aux_string.c_str(), 5000);
  aux_string.clear();
  
  at_command("AT+CMQCON=?", 500);
  // aux_string.clear();
  // aux_string = "AT+CMQTTSNEW=\"" + host + "\",\"" + port + "\",\"9000\",\"100\"";
  // at_command(aux_string.c_str(), 9000);
  // aux_string.clear();

}


SIM7020::eNbiotStateMachine SIM7020::WaitSocketHandler(){
  delay(5000);
  command_response = at_CommandWithReturn("AT+CIPSTATUS", 500);
  if((command_response.find("CONNECT OK")) !=  std::string::npos){
    command_response.clear();
    return CONNECT_OK;
  }
}


SIM7020::eNbiotStateMachine SIM7020::DataSendHandler(std::string app_protocol, std::string app_method, std::string page_file_topic, std::string host){
  std::string aux_string, cipsend_str, aux_command_before, aux_command_pub;
  at_command("AT+CIPSTATUS", 500);
  if(app_protocol.find("http") != std::string::npos)
  {
    aux_string = app_method + " " + page_file_topic + " HTTP/1.0\r\nHost: " + host + "\r\n\r\n";
    at_command("AT+CIPSEND", 10000);
    Serial_AT.write(aux_string.c_str());
    Serial_AT.write(26);
    at_command("AT+CIPCLOSE", 1000);
    command_response = at_CommandWithReturn("AT+CIPSTATUS", 500);
    if((command_response.find("TCP CLOSED")) !=  std::string::npos)
      return TCP_CLOSED;
    else if((command_response.find("CONNECT OK")) !=  std::string::npos)
    {
        at_command("AT+CIPCLOSE", 1000);
        return TCP_CLOSED;
   }  
  }
  else if(app_protocol.find("mqtt") != std::string::npos)
  {
    // std::string host = "broker.emqx.io";
    // std::string mqtt_port = "8883";
    // aux_string = "AT+CMQNEW=\"" + host + "\",\"" + mqtt_port + "\",\"9000\",\"100\"";
    // at_command(aux_string.c_str(), 9000);
    // aux_string.clear();
    aux_command_before = "AT+CMQSUB=\"0\",\"esp32/NbioT\",\"1\"";
    at_command(aux_command_before.c_str(), 5000);
    aux_command_pub = "AT+CMQPUB=\"0\",\"esp32/NbioT\",\"1\",\"0\",\"0\",\"8\",\"12345678\"";
    at_command(aux_command_pub.c_str(), 5000);
  }
}

SIM7020::eNbiotStateMachine SIM7020::MQTT_Connection()
{
  std::string aux_string, aux_command_request, aux_command_pub_test, aux_command_sub_test, host, port;
  host = "broker.hivemq.com";
  port = 1883;

  aux_string = "AT+CMQNEW=\"" + host + "\",\"" + "1883" + "\",\"12000\",\"100\"";
  at_command(aux_string.c_str(), 12000);

  aux_command_request = "AT+CMQCON=\"0\",\"3\",\"my_id\",\"600\",\"0\", \"0\"";
  at_command(aux_command_request.c_str(), 12000);

  aux_command_sub_test = "AT+CMQSUB=\"0\",\"esp32/NbioT\",\"1\"";
  at_command(aux_command_sub_test.c_str(), 5000);

  aux_command_pub_test = "AT+CMQPUB=\"0\",\"esp32/NbioT\",\"1\",\"0\",\"0\",\"8\",\"12345678\"";
  at_command(aux_command_pub_test.c_str(), 5000);
}

SIM7020::eNbiotStateMachine SIM7020::SSL_Connection()
{
  std::string aux_string, root_ca, root_ca_b, root_total, serve_ca, private_key, broker, port;

  broker = "mqtt.2030.ltsapis.goog";
  port = "8883";

  at_command("AT+CSETCA?", 500);

  serve_ca = "-----BEGIN CERTIFICATE-----MIIB4TCCAYegAwIBAgIRKjikHJYKBN5CsiilC+g0mAIwCgYIKoZIzj0EAwIwUDEkMCIGA1UECxMbR2xvYmFsU2lnbiBFQ0MgUm9vdCBDQSAtIFI0MRMwEQYDVQQKEwpHbG9iYWxTaWduMRMwEQYDVQQDEwpHbG9iYWxTaWduMB4XDTEyMTExMzAwMDAwMFoXDTM4MDExOTAzMTQwN1owUDEkMCIGA1UECxMbR2xvYmFsU2lnbiBFQ0MgUm9vdCBDQSAtIFI0MRMwEQYDVQQKEwpHbG9iYWxTaWduMRMwEQYDVQQDEwpHbG9iYWxTaWduMFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEuMZ5049sJQ6fLjkZHAOkrprlOQcJFspjsbmG+IpXwVfOQvpzofdlQv8ewQCybnMO/8ch5RikqtlxP6jUuc6MHaNCMEAwDgYDVR0PAQH/BAQDAgEGMA8GA1UdEwEB/wQFMAMBAf8wHQYDVR0OBBYEFFSwe61FuOJAf/sKbvu+M8k8o4TVMAoGCCqGSM49BAMCA0gAMEUCIQDckqGgE6bPA7DmxCGXkPoUVy0D7O48027KqGx2vKLeuwIgJ6iFJzWbVsaj8kfSt24bAgAXqmemFZHe+pTsewv4n4Q=-----END CERTIFICATE-----";

  aux_string = "AT+CSETCA=\"0\",\"0\",\"0\",\"0\",\"0\"";
  at_command(aux_string.c_str(), 500);
  aux_string.clear();

  aux_string = "AT+CSETCA=\"2\",\"0\",\"0\",\"0\",\"0\"";
  at_command(aux_string.c_str(), 500);
  aux_string.clear();

  at_command("AT+CSETCA?", 500);

  root_ca = "-----BEGIN CERTIFICATE-----MIIC0TCCAnagAwIBAgINAfQKmcm3qFVwT0+3nTAKBggqhkjOPQQDAjBEMQswCQYDVQQGEwJVUzEiMCAGA1UEChMZR29vZ2xlIFRydXN0IFNlcnZpY2VzIExMQzERMA8GA1UEAxMIR1RTIExUU1IwHhcNMTkwMTIzMDAwMDQyWhcNMjkwNDAxMDAwMDQyWjBEMQswCQYDVQQGEwJVUzEiMCAGA1UEChMZR29vZ2xlIFRydXN0IFNlcnZpY2VzIExMQzERMA8GA1UEAxMIR1RTIExUU1gwWTATBgcqhkjOPQIBBggqhkjOPQMBBwNCAARr6/PTsGoOg9fXhJkj3CAk6C6DxHPnZ1I+ER40vEe290xgTp0gVplokojbN3pFx07fzYGYAX5EK7gDQYuhpQGIo4IBSzCCAUcwDgYDVR0PAQH/BAQDAgGGMB0GA1UdJQQWMBQGCCsGAQUFBwMBBggrBgEFBQcDAjASBgNVHRMBAf8ECDAGAQH/AgEAMB0GA1UdDgQWBBSzK6ugSBx+E4rJCMRAQiKiNlHiCjAfBgNVHSMEGDAWgBQ+/v/MUuu/ND4980DQ5CWxX7i7UjBpBggrBgEFBQcBAQRdMFswKAYIKwYBBQUHMAGGHGh0dHA6Ly9vY3NwLnBraS5nb29nL2d0c2x0c3IwLwYIKwYBBQUHMAKGI2h0dHA6Ly9wa2kuZ29vZy9ndHNsdHNyL2d0c2x0c3IuY3J0MDgGA1UdHwQxMC8wLaAroCmGJ2h0dHA6Ly9jcmwucGtpLmdvb2cvZ3RzbHRzc";
  
  root_ca_b = "i9ndHNsdHNyLmNybDAdBgNVHSAEFjAUMAgGBmeBDAECATAIBgZngQwBAgIwCgYIKoZIzj0EAwIDSQAwRgIhAPWeg2v4yeimG+lzmZACDJOlalpsiwJR0VOeapY8/7aQAiEAiwRsSQXUmfVUW+N643GgvuMH70o2Agz8w67fSX+k+Lc=-----END CERTIFICATE-----";

  private_key = "-----BEGIN EC PRIVATE KEY-----MHcCAQEEIEf7pG64E9BqMylniTrkm84zmCfO885apmpLHb5dybHmoAoGCCqGSM49AwEHoUQDQgAEQOVGC+fbMK45MCJscsIknKvV21bzNRCvP12Sf204pxhuf+iqa8zMuPFgPCb7d+bNd82uUlDMnvZYlKkrItXSiw==-----END EC PRIVATE KEY-----";



  aux_string = "AT+CSETCA=\"0\",\"1024\",\"0\",\"0\",\"" + root_ca + "\"";
  at_command(aux_string.c_str(), 500);
  aux_string.clear();

  aux_string = "AT+CSETCA=\"0\",\"1024\",\"1\",\"0\",\"" + root_ca_b + "\"";
  at_command(aux_string.c_str(), 500);
  aux_string.clear();

  aux_string = "AT+CSETCA=\"2\",\"224\",\"1\",\"0\",\"" + private_key + "\"";
  at_command(aux_string.c_str(), 500);
  aux_string.clear();

  at_command("AT+CMQTTSNEW=?", 500);
  at_command("AT+CMQTTSNEW?", 500);

  aux_string = "AT+CMQNEW=\"" + broker + "\",\"" + port + "\",\"12000\",\"1024\"";
  at_command(aux_string.c_str(), 2000);
  aux_string.clear();

  at_command("AT+CMQTTSNEW?", 500);

  aux_string = "AT+CMQCON=\"0\",\"4\",\"projects/iot-test-328120/locations/us-central1/registries/test-registry/devices/nb_iot\",\"60000\",\"1\",\"0\"";
  at_command(aux_string.c_str(), 2000);
  at_command("AT+CMQCON?", 500);
  aux_string.clear();

  at_command("AT+CMQCON=?", 500);


  aux_string = "AT+CMQSUB=\"0\",\"projects/iot-test-328120/subscriptions/testesub-sub\",\"0\"";
  at_command(aux_string.c_str(), 2000);
  aux_string.clear();
  
  at_command("AT+CMQSUB?", 500);

  aux_string = "AT+CMQDISCON=\"0\"";
  at_command(aux_string.c_str(), 2000);
  aux_string.clear();

  return TCP_CLOSED;
}

SIM7020::eNbiotStateMachine SIM7020::WaitSocketCloseHandler(){
  delay(5000);
  command_response = at_CommandWithReturn("AT+CIPSTATUS", 500);
  if((command_response.find("TCP CLOSED")) !=  std::string::npos){
    command_response.clear();
    return TCP_CLOSED;
    }
}


void SIM7020::set_NetworkCredentials(std::string user_apn, std::string username, std::string user_psswd){
  apn = user_apn;
  user = username;
  psswd = user_psswd;
}


void SIM7020::set_RFBand(std::string band){
	rf_band = band;
}