#include <Arduino.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>

char ssid[32] = {0};
char password[32] = {0};

const char *AP_Name = "ESP32_AP";
int Web_Status = 1;

const byte DNS_PORT = 53;
IPAddress AP_IP(192, 168, 4, 1); // ESP32的AP模式IP地址
DNSServer dnsServer;             // 创建dnsServer实例
WebServer server(80);            // 创建WebServer

void Save_Config(); // 保存配置信息
void Read_Config(); // 读取配置信息
void ConnectWifi(); // 连接WIFI

/*访问主页回调函数*/
void handleRoot()
{
  if (LittleFS.exists("/index.html"))
  {

    File html = LittleFS.open("/index.html", "r");
    server.streamFile(html, "text/html");
    html.close(); // 关闭闪存文件中的index.html
  }
  else
    Serial.println("不存在inde.html");
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
  strcpy(ssid, server.arg("ssid").c_str());         // 获取ssid
  strcpy(password, server.arg("password").c_str()); // 获取password
  Save_Config();
  server.send(200, "text/plain", "<meta charset='UTF-8'>保存成功");
  delay(200);
  ConnectWifi(); // 连接wifi
}

// 保存配置信息
void Save_Config()
{
  const size_t capacity = JSON_OBJECT_SIZE(2) + 80;
  DynamicJsonBuffer jsonBuffer(capacity);
  JsonObject &root = jsonBuffer.createObject();
  root["ssid"] = ssid;
  root["password"] = password;

  String jsonCode;
  root.printTo(jsonCode);

  File wifiConfig = LittleFS.open("/config.json", "w");
  wifiConfig.println(jsonCode); // 将数据写入config.json文件中
  wifiConfig.close();

  Serial.print("配置信息已保存\r");
  Serial.print("jsonCode: ");
  Serial.println(jsonCode); // {"ssid":"leisure","password":"shadow27"}
}

// 读取配置信息
void Read_Config()
{
  File ConfigJson = LittleFS.open("/config.json", "r"); // 读取配置文件中的JSON信息

  const size_t capacity = JSON_OBJECT_SIZE(2) + 80;
  DynamicJsonBuffer jsonBuffer(capacity);
  JsonObject &root = jsonBuffer.parseObject(ConfigJson);
  const char *ssid = root["ssid"];
  const char *password = root["password"];

  Serial.print("ssid:  ");
  Serial.println(ssid);
  Serial.print("password:  ");
  Serial.println(password);
  delay(100);

  /*进行wifi连接*/
  WiFi.mode(WIFI_STA);       // 切换为STA模式
  WiFi.setAutoConnect(true); // 设置自动连接
  WiFi.begin(ssid, password);
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
  server.begin(); // 启动WebServer
  Web_Status = 1;
  server.on("/", HTTP_GET, handleRoot); // 设置主页回调函数
  // server.onNotFound(handleNotFound);         // 测试了一下，这样无法强制门户
  server.onNotFound(handleRoot);             // 这样可用强制门户
  server.on("/", HTTP_POST, handleRootPost); // 设置Post请求回调函数
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
  Read_Config(); // 读取配置，并进行WIFI连接

  Serial.println("");
  Serial.println("Connecting  WIFI");

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
    Web_Status = 0;
  }
}

void setup()
{
  Serial.begin(9600);
  LittleFS.begin();
  WiFi.hostname("ESP32");

  ConnectWifi();
}

void loop()
{
  if (Web_Status == 1)
  {
    server.handleClient();          // 处理Web服务器的客户端连接请求(get、post)
    dnsServer.processNextRequest(); // 处理DNS请求
    delay(200);
  }
  else
  {
  }
}