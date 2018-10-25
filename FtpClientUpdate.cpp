#include "FtpClientUpdate.h"

ESPFTPCLIENTUpdate::ESPFTPCLIENTUpdate(void)
{
}

//----------------- FTP fail
void ESPFTPCLIENTUpdate::efail(WiFiClient& _Client)
{
  byte thisByte = 0;
  uint16_t To=0;
  _Client.println(F("QUIT"));

  while (!_Client.available()){
    delay(1);
    if(++To > 1000){
      break;
    }
  } 

#ifdef DEBUG_ESP_FTPCLIENT_PORT
  while (_Client.available())
  {
    thisByte = _Client.read();
    DEBUG_ESP_FTPCLIENT_PORT.write(thisByte);
  }
#endif

  _Client.stop();
  DEBUG_FTPCLIENT_UPDATE("Command disconnected");
  DEBUG_FTPCLIENT_UPDATE("SD closed");
} // efail

//-------------- FTP receive
bool ESPFTPCLIENTUpdate::eRcv(WiFiClient& _Client, char *BufRx, byte Len)
{
  byte outCount = 0;
  byte respCode;
  byte thisByte;
  uint16_t To=0;
  while (!_Client.available()){
    delay(1);
    if(++To > 2000){
      DEBUG_FTPCLIENT_UPDATE("Rx Timeout");
       efail(_Client);
       return 0;
    }
  }    

  respCode = _Client.peek();
  DEBUG_FTPCLIENT_UPDATE("Rx: ");
  while (_Client.available())
  {
    thisByte = _Client.read();
#ifdef DEBUG_ESP_FTPCLIENT_PORT
    DEBUG_ESP_FTPCLIENT_PORT.write(thisByte);
#endif
    if (outCount < Len)
    {
      BufRx[outCount++] = thisByte;
    }
  }
  BufRx[outCount] = 0;

  if (respCode >= '4')
  {
    efail(_Client);
    return 0;
  }
  return 1;
} // eRcv()

bool ESPFTPCLIENTUpdate::update(const String& host, const String& user, const String& pass, const String& path, String& md5) {
  size_t FileSize = 0;
  size_t Total = 0;
  WiFiClient client;
  WiFiClient dclient;
  char Buf[128];

  if (client.connect(host, 21))
  { // 21 = FTP server
    DEBUG_FTPCLIENT_UPDATE("Command connected");
  }
  else
  {
    DEBUG_FTPCLIENT_UPDATE("Command connection failed");
    _error = FTP_UPDATE_ERROR_COMMAND_SERVER;
    return 0;
  }
  if (!eRcv(client,Buf,sizeof(Buf)-1)){
    _error = FTP_UPDATE_ERROR_SUPPORT_SERVER;
    return 0;
  }
  
  DEBUG_FTPCLIENT_UPDATE("Send USER");
  client.println("USER " + user);
  if (!eRcv(client,Buf,sizeof(Buf)-1)){
    _error = FTP_UPDATE_ERROR_USER_SERVER;
    return 0;
  }
  
  DEBUG_FTPCLIENT_UPDATE("Send PASSWORD");
  client.println("PASS " + pass);
  if (!eRcv(client,Buf,sizeof(Buf)-1)){
    _error = FTP_UPDATE_ERROR_PASS_SERVER;
    return 0;
  }
  
  DEBUG_FTPCLIENT_UPDATE("Send SYST");
  client.println(F("SYST"));
  if (!eRcv(client,Buf,sizeof(Buf)-1)){
    _error = FTP_UPDATE_ERROR_SYST_SERVER;
    return 0;
  }
  
  DEBUG_FTPCLIENT_UPDATE("Send Type I");
  client.println(F("Type I"));
  if (!eRcv(client,Buf,sizeof(Buf)-1)){
    _error = FTP_UPDATE_ERROR_TYPE_SERVER;
    return 0;
  }
  
  DEBUG_FTPCLIENT_UPDATE("Send PASV");
  client.println(F("PASV"));
  if (!eRcv(client,Buf,sizeof(Buf)-1)){
    _error = FTP_UPDATE_ERROR_PASV_SERVER;
    return 0;
  }

  //227 Entering Passive Mode (192,168,31,199,171,83).
  char *tStr = strtok(Buf, "(,");
  int array_pasv[6];
  for (int i = 0; i < 6; i++)
  {
    tStr = strtok(NULL, "(,");
    array_pasv[i] = atoi(tStr);
    if (tStr == NULL)
    {
      DEBUG_FTPCLIENT_UPDATE("Bad PASV Answer");
    }
  }
  
  uint16_t hiPort, loPort;
  hiPort = array_pasv[4] << 8;
  loPort = array_pasv[5] & 255;
  IPAddress ip(array_pasv[0],array_pasv[1],array_pasv[2],array_pasv[3]);
  
  hiPort = hiPort | loPort;
  DEBUG_FTPCLIENT_UPDATE("Data port: %u",hiPort);

  if (dclient.connect(ip, hiPort))
  {
    DEBUG_FTPCLIENT_UPDATE("Data connected");
  }
  else
  {
    DEBUG_FTPCLIENT_UPDATE("Data connection failed");
    _error = FTP_UPDATE_ERROR_DATA_SERVER;
    client.stop();
    return 0;
  }

  DEBUG_FTPCLIENT_UPDATE("Send RETR %s",path.c_str());
  client.print(F("RETR "));
  client.println(path);

  if (!eRcv(client,Buf,sizeof(Buf)-1))
  {
    _error = FTP_UPDATE_ERROR_DATA_SERVER;
    dclient.stop();
    return 0;
  }
  
  //150 Opening BINARY mode data connection for BaseStation/ftpupdateV1.bin (271664 bytes)  
  char *dSize = strtok(Buf, "(");
  if(dSize != NULL){
    dSize = strtok(NULL, " ");
    if(dSize != NULL){
      FileSize = atol(dSize);
      DEBUG_FTPCLIENT_UPDATE("FileSize: %u",FileSize);
    }
  }
  
  uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
  DEBUG_FTPCLIENT_UPDATE("SketchSpace: %u", maxSketchSpace);
  if(FileSize==0 || FileSize> maxSketchSpace){
    client.stop();
    dclient.stop();
    _error = FTP_UPDATE_ERROR_SIZE;
    return 0;
  }
  
  if (!Update.begin(FileSize,U_FLASH))
  { //start with max available size
    #ifdef DEBUG_ESP_FTPCLIENT_PORT
    Update.printError(DEBUG_ESP_FTPCLIENT_PORT);
    #endif
    _error = Update.getError();
    client.stop();
    dclient.stop();
    return 0;
  }
  DEBUG_FTPCLIENT_UPDATE("SketchSpace OK");
  //String Md5 = "84950FB41AC0515F0EADA7517A148D3A";
  md5.toLowerCase();
  Update.setMD5(md5.c_str());
  
  uint8_t DcConnect;
  TimeOutEvent ToDownload(3000);
  do
  {
    DcConnect = dclient.connected();
    if (dclient.available())
    {
      uint8_t *Buf = (uint8_t*)malloc(4096);
      size_t Rbyte  = dclient.readBytes(Buf, 4096);
      Total += Rbyte;
      if (Update.write(Buf, Rbyte) != Rbyte) {
        #ifdef DEBUG_ESP_FTPCLIENT_PORT
        Update.printError(DEBUG_ESP_FTPCLIENT_PORT);
        #endif
      } else {
        DEBUG_FTPCLIENT_UPDATE("Rec: %u OK",Rbyte);
      }
      free(Buf);
      ToDownload.ToEUpdate(3000);
    }
    if(ToDownload.ToEExpired()){
      DEBUG_FTPCLIENT_UPDATE("Download Timeout");
      break;
    }
    yield();
  } while (DcConnect);

  dclient.stop();
  DEBUG_FTPCLIENT_UPDATE("Data disconnected");

  if (!eRcv(client,Buf,sizeof(Buf)-1)){
    _error = FTP_UPDATE_ERROR_STOP_DATA_SERVER;
    return 0; 
  }    

  DEBUG_FTPCLIENT_UPDATE("Send QUIT");
  client.println(F("QUIT"));
  if (!eRcv(client,Buf,sizeof(Buf)-1)){
    _error = FTP_UPDATE_ERROR_QUIT_SERVER;
    return 0; 
  }    
  client.stop();
  DEBUG_FTPCLIENT_UPDATE("Command disconnected");
  DEBUG_FTPCLIENT_UPDATE("Download: %u",Total);

  if(FileSize != Total){
    DEBUG_FTPCLIENT_UPDATE("File Download Size Error");
    _error = FTP_UPDATE_ERROR_SIZE_FILE;
    return 0;
  }

  if (Update.end(true))
  { //true to set the size to the current progress
    DEBUG_FTPCLIENT_UPDATE("Update Success: %u\nReboot to finish\n",Total);
  }
  else
  {
    #ifdef DEBUG_ESP_FTPCLIENT_PORT
    Update.printError(DEBUG_ESP_FTPCLIENT_PORT);
    #endif
    _error = Update.getError();
    return 0;
  }
  _error = FTP_UPDATE_ERROR_OK;
  return 1;
}

#if !defined(NO_GLOBAL_INSTANCES) && !defined(NO_GLOBAL_FTPCLIENTUPDATE)
ESPFTPCLIENTUpdate FtpClientUpdate;
#endif
