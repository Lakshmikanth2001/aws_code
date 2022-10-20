@ECHO OFF
SET COM_PORT=%1
arduino_debug ^
--upload ^
--board esp8266:esp8266:nodemcuv2:xtal=80,vt=flash,exception=disabled,stacksmash=disabled,ssl=all,mmu=3232,non32xfer=fast,eesz=4M2M,led=2,ip=lm2f,dbg=Disabled,lvl=None____,wipe=none,baud=115200 ^
--pref build.path=D:\\aws_code\\esp8266Build ^
--port %COM_PORT% ^
iot_control.ino