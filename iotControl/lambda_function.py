import os
import pytz
import json
import logging
import pymysql
from datetime import date
from datetime import datetime
from db_helper import Database


logging.basicConfig(level=logging.INFO)
db = Database()


def get_device_control_bits(device_id: str):
    # This sql statemate is to collect `power_supply` `clear wifi` etc
    sql = f"""SELECT power_supply, clear_wifi, control_bits, timer_states
    FROM `user_devices` LEFT JOIN `device_control`
    ON user_devices.id = device_control.owner_id
    WHERE `device_id` = '{device_id}';"""
    result = db.run_qry(sql)[0]

    if result["clear_wifi"]:
        # update clear_wifi
        sql = f"""UPDATE `user_devices`
        LEFT JOIN `device_control`
        ON user_devices.id = device_control.owner_id
        SET `clear_wifi` = false
        WHERE `device_id` = '{device_id}';"""
        # no need to collect result for UPDATE query
        db.run_qry(sql)
        return -1

    if result["power_supply"]:
        timer_flag = False
        # This sql is to collect timers data and handshake_times from esp8266 hardware
        sql = f"""SELECT * FROM `device_timers`
        RIGHT JOIN `device_control`
        ON device_timers.device_id_id = device_control.device_id
        WHERE `device_id` = '{device_id}';"""
        timer_result = db.run_qry(sql)[0]
        timezone = pytz.timezone(timer_result["timezone"])

        # update hardware handshake time
        current_time = datetime.now(timezone)
        sql = f"""UPDATE `device_control`
        SET `handshake_time` = '{current_time.strftime("%Y-%m-%d %H:%M:%S")}'
        WHERE `device_id` = '{device_id}'"""
        db.run_qry(sql)
        # no need to collect result for UPDATE query

        for timer_state in result["timer_states"]:
            if timer_state == "1":
                timer_flag = True
                break
            else:
                continue
        if timer_flag:
            # sql = f"""SELECT * FROM `device_timers`
            # RIGHT JOIN `device_control`
            # ON device_timers.device_id_id = device_control.device_id
            # WHERE `device_id` = '{device_id}';"""
            # timer_result = db.run_qry(sql)[0]
            timer_states = timer_result["timer_states"]
            control_bits = timer_result["control_bits"]
            current_time = datetime.now(timezone)
            new_control_bits = ""
            new_timer_states = ""
            updated_switch_index: list[int] = []

            timer_overflowed = False
            for i in range(len(control_bits)):
                trigger_time = timer_result[f"time_{i}"]
                if timer_states[i] == "1" and trigger_time != None:
                    # no need to localize current_time
                    if current_time < timezone.localize(trigger_time):
                        # prevent sql injection
                        assert control_bits[i] == "1" or control_bits[i] == "0"
                        new_control_bits += control_bits[i]
                        new_timer_states += "1"
                    else:
                        timer_overflowed = True
                        updated_switch_index.append(i)
                        if control_bits[i] == "0":
                            new_control_bits += "1"
                        else:
                            new_control_bits += "0"
                        # toggle the control bit and clear the timer state time
                        new_timer_states += "0"
                else:
                    new_control_bits += control_bits[i]
                    new_timer_states += "0"

            if timer_overflowed:
                sql = f"""UPDATE `device_control` SET `control_bits` = '{new_control_bits}', `timer_states` = '{new_timer_states}'  WHERE `device_id` = '{device_id}';"""
                # sql = f"""UPDATE `device_control` SET `control_bits` = '{new_control_bits}'  WHERE `device_id` = '{device_id}';"""
                result = db.run_qry(sql)
            return new_control_bits
        else:
            return result["control_bits"]
    else:
        return "1" * len(result["control_bits"])


def get_device_labels(device_id: str):
    sql = f"""SELECT `device_description` FROM `device_control` WHERE `device_id` = '{device_id}'"""
    result = db.run_qry(sql)[0]

    return result["device_description"]


def responce_structure(status_code: int, body: dict):
    body["date"] = str(date.today())
    body["time"] = str(datetime.now())
    return {
        "statusCode": status_code,
        "body": json.dumps(body),
        "headers": {
            "Access-Control-Allow-Headers": "*",
            "Access-Control-Allow-Origin": "*",
            "Access-Control-Allow-Methods": "GET, POST",
        },
    }


def validate_mac_id(mac_id: list[str]):
    for char in mac_id.split(":"):
        # prevent sql injection
        if not char.isalnum():
            return False
    return True


def lambda_handler(event, context):

    # TODO implement
    # dynamodb.put_item(TableName = 'iot_control_data', Item = {
    #     "device_id":{
    #         "S":"524"
    #     },
    #     "control_bits":{
    #         "S":"1010"
    #     }
    # })
    device_provided = True
    try:
        _, _, device_id = event["path"].split("/")
    except KeyError:
        device_id = "524"
        device_provided = False
    except ValueError:
        device_id = "524"
        device_provided = False

    # prevent sql injection
    assert device_id.isalnum()

    if event.get("httpMethod") == "GET":
        if event.get("queryStringParameters") and device_provided:
            if event["queryStringParameters"].get("hardware") == "esp8266":
                # sql = f"""SELECT `control_bits` FROM `device_control` WHERE `device_id` = {device_id}"""
                # result = db.run_qry(sql)[0]
                return {
                    "statusCode": 200,
                    "body": get_device_control_bits(device_id),
                    "headers": {"content-type": "text/plain"},
                }
            elif event["queryStringParameters"].get("fetch") == "finger_print":
                return {
                    "statusCode": 200,
                    "body": os.environ["THUMBPRINT"],
                    "headers": {"content-type": "text/plain"},
                }
            else:
                return responce_structure(400, "Bad Request")

        elif device_provided:
            sql = f"""SELECT `control_bits`,`device_description` FROM `device_control` WHERE `device_id` = '{device_id}'"""
            result = db.run_qry(sql)[0]
            return responce_structure(200, result)

        else:
            return {"statusCode": 200, "body": json.dumps(event)}
    elif event.get("httpMethod") == "POST":
        # esp8266 will make a post request with its MAC ID as payload
        mac_id = event.get("body")

        # if valid mac_id is not provide it will throw an exception
        assert validate_mac_id(mac_id)

        sql = f"""INSERT INTO device_info(`device_id`, `mac_id`) VALUES('{device_id}', '{mac_id}');"""
        try:
            db.run_qry(sql)
        except pymysql.err.IntegrityError as e:
            # duplicate mac ids was sent
            logging.debug(e)
        return {"statusCode": 200, "body": json.dumps(mac_id)}
