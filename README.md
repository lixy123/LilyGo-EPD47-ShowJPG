# LilyGo-EPD47-ShowJPG
LilyGo-EPD47-ShowJPG


工具: arduino 1.8.13 <br/>
用到库文件: <br/>
ESP32库 arduino-esp32 版本 1.0.6<br/>
墨水屏库 https://github.com/Xinyuan-LilyGO/LilyGo-EPD47 <br/>

LilyGo-EPD47 墨水屏没有提供显示JPG图片的函数，参考 epdiy项目中的显示JPG的示例代码适当修改了下，移植到了arduino上 <br/>
代码来源: <br/>
https://github.com/vroland/epdiy/blob/master/examples/www-image/main/jpg-render.c <br/>

需要用arduino的插件 ESP32 Sketch data Upload 上传图片到esp32的SPIFFS分区. <br/>
