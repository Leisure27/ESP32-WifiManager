#include <Arduino.h>
#include <ArduinoJson.h>

#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <EEPROM.h>

char ssid[32] = {0};
char password[64] = {0};
const char *AP_Name = "ESP32_AP";
// char *ssid ="";
// char *password= "";

typedef struct
{
  char c_ssid[32];
  char c_pwd[64];
} config_type;
config_type config;
// EEPROM ==========================================================
/*保存配置信息*/
void saveConfig()
{
  Serial.println("save config");
  EEPROM.begin(sizeof(config));
  uint8_t *p = (uint8_t *)(&config);
  for (uint i = 0; i < sizeof(config); i++)
  {
    EEPROM.write(i, *(p + i));
  }
  EEPROM.commit(); // 此操作会消耗flash写入次数
}

/*加载配置信息*/
void loadConfig()
{
  Serial.println("load config");
  EEPROM.begin(sizeof(config));
  uint8_t *p = (uint8_t *)(&config);
  for (uint i = 0; i < sizeof(config); i++)
  {
    *(p + i) = EEPROM.read(i);
  }
  strcpy(ssid, config.c_ssid);
  strcpy(password, config.c_pwd);
}
// wEB配置 =================================================================
const char *page_html = "\
<!DOCTYPE html>\r\n\
<html lang='en'>\r\n\
<head>\r\n\
  <meta charset='UTF-8'>\r\n\
  <meta name='viewport' content='width=device-width, initial-scale=1.0'>\r\n\
  <title>Document</title>\r\n\
</head>\r\n\
<body>\r\n\
  <h1>ESP8266配置页</h1>\r\n\
  <form name='input' action='/' method='POST'>\r\n\
    WiFi名称:\r\n\
    <input type='text' name='ssid'><br>\r\n\
    WiFi密码:\r\n\
    <input type='password' name='password'><br>\r\n\
    目标日期:\r\n\
    <input type='date' name='date'><br>\r\n\
    时区(-12~12, 默认为8——北京时间):<br>\r\n\
    <input type='text' name='timezone' value='8'><br>\r\n\
    <input type='submit' value='提交'>\r\n\
    <br><br>\r\n\
    <a href='https://space.bilibili.com/751219'>FlyAkari</a>\r\n\
  </form>\r\n\
</body>\r\n\
</html>\r\n\
";

const byte DNS_PORT = 53;        // DNS端口号默认为53
IPAddress AP_IP(192, 168, 4, 1); // ESP32的AP模式IP地址
DNSServer dnsServer;             // 创建dnsServer实例
WebServer server(80);            // 创建WebServer

void ConnectWifi(); // 连接WIFI

/*访问主页回调函数*/
void handleRoot()
{
  server.send(200, "text/html", page_html);
}

// 当浏览器请求的网络资源无法在服务器找到时
void handleNotFound()
{
  server.send(404, "text/plain", "404: Not found");
}

/*post回调函数*/
void handleRootPost()
{
  Serial.println(F("通过Web获取配置信息中"));
  if (server.hasArg("ssid"))
  {
    Serial.print("ssid:");
    strcpy(ssid, server.arg("ssid").c_str()); // 获取ssid
    Serial.println(ssid);
  }
  if (server.hasArg("password"))
  {
    Serial.print("password:");
    strcpy(password, server.arg("password").c_str()); // 获取password
    Serial.println(password);
  }

  server.send(200, "text/plain", "<meta charset='UTF-8'>保存成功"); // 返回保存页面
  delay(1000);
  saveConfig();

  ConnectWifi(); // 连接wifi
}

void initSoftAP()
{
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(AP_IP, AP_IP, IPAddress(255, 255, 255, 0));
  if (WiFi.softAP(AP_Name))
  {
    Serial.println(F("AP模式建立成功"));
  }
}

/*初始化WebServer*/
void initWebServer(void)
{

  server.on("/", HTTP_GET, handleRoot); // 设置主页回调函数
  // server.onNotFound(handleNotFound);         // 测试了一下，这样无法强制门户
  server.onNotFound(handleRoot);             // 这样可用强制门户
  server.on("/", HTTP_POST, handleRootPost); // 设置Post请求回调函数
  server.begin();                            // 启动WebServer
  Serial.println("WebServer started!");
}

/*初始化DNS服务器*/
void initDNS(void)
{
  // 判断将所有地址映射到ESP32的ip上是否成功
  if (dnsServer.start(DNS_PORT, "*", AP_IP))
  {
    Serial.println("start dnsserver success.");
  }
  else
    Serial.println("start dnsserver failed.");
}

void ConnectWifi()
{
  WiFi.mode(WIFI_STA);       // 切换为STA模式
  WiFi.setAutoConnect(true); // 设置自动连接
  WiFi.begin(ssid, password);
  Serial.println("");
  Serial.print("Connect WiFi");

  int count = 0;
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(800);
    count++;
    if (count > 10)
    {               // 8s内没有连接上，就开启AP模式进行配网
      initSoftAP(); // 切换为AP模式
      initDNS();
      initWebServer(); // 获取Web输入信息
      break;           // 跳出，防止无限初始化
    }
    Serial.print(".");
  }
  Serial.println("");
  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println(F("wifi连接成功"));
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    server.stop();
    dnsServer.stop();
  }
}

void setup()
{
  Serial.begin(9600);
  WiFi.hostname("ESP32");

  ConnectWifi();
}

void loop()
{

  server.handleClient();          // 处理Web服务器的客户端连接请求(get、post)
  dnsServer.processNextRequest(); // 处理DNS请求
}