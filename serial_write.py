import serial
import logging

logging.basicConfig(level=logging.INFO, filename="hardware.log", filemode="a")
if __name__ == "__main__":
    SERIAL_CONFIG = {
        "port": "COM3",
        "baudrate": 115200,
        "bytesize": 8,
        "timeout": 2,
        "stopbits": serial.STOPBITS_ONE,
    }
    while True:
        with serial.Serial(**SERIAL_CONFIG) as ser:
            # write to serial port and wait for response
            ic_log = ser.write("ok\n")
            if ic_log != b"[HTTPS] GET Request Ends...\n[HTTPS] GET Request begin...\n":
                logging.info(ic_log)
            logging.debug(ic_log)
