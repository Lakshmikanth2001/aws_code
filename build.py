import os
import serial
import logging
logging.basicConfig(level=logging.DEBUG)
#!T(0r#d@T@

def change_device_id(device_id: str):
    # we are changing the source code file and dumping it to the IC every time
    with open("iot_control.ino", 'r') as iot_config_file:
        iot_config_file_content = iot_config_file.readlines()
        for i, line in enumerate(iot_config_file_content):
            if line.find("#define DEVICE_ID") != -1:
                iot_config_file_content[i] = f'#define DEVICE_ID "{device_id}"\n'
                break
        else:
            raise AssertionError("Device ID not found in iot_control.ino")

    with open("iot_control.ino", 'w') as iot_config_file:
        iot_config_file.writelines(iot_config_file_content)

if __name__ == "__main__":
    SERIAL_CONFIG = {
        'port': 'COM4',
        'baudrate': 115200,
        'bytesize': 8,
        'timeout': 2,
        'stopbits': serial.STOPBITS_ONE,
    }
    DEVICE_IDS = [600]
    DECORATION_SIZE = 40

    logging.debug("DEVICE IDS LIST {}".format(DEVICE_IDS))

    for device_id in DEVICE_IDS:
        # detect which COM port is connected
        command = input("Press Enter to continue the build for {} or Q to stop: ".format(device_id))

        initial_port = 3
        portFound = False

        while not portFound:
            try:
                SERIAL_CONFIG['port'] = f'COM{initial_port}'
                with serial.Serial(**SERIAL_CONFIG) as ser:
                    # write to serial port and wait for response
                    ser.write("test".encode('ascii'))
                    portFound = True
            except Exception as e:
                # if serail write didnt respond for PORT-4 it is probably connected to PORT-5
                # esp8266 chose COM5 as serial port
                initial_port += 1

        print(DECORATION_SIZE*"**")
        logging.info("Building for device id: {}".format(device_id))
        print(DECORATION_SIZE*"**")
        if command == "Q":
            break
        elif command == "":
            change_device_id(device_id)
            exit_code = os.system(f'build.bat {SERIAL_CONFIG["port"]}')
            print(DECORATION_SIZE*"**")
            logging.debug(f"device id {device_id} is build with Exit code: {exit_code}")
    print(DECORATION_SIZE*"**")
