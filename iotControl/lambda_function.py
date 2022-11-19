import os
import pytz
import json
import logging
import pymysql
from datetime import date
from datetime import datetime
from db_helper import Database
from db_queries import DatabaseQueries

logger = logging.getLogger()
logger.setLevel(logging.DEBUG)
db = Database()


def power_session_queries(
    db_queries: DatabaseQueries, power_switches: dict, power_session_type: str
) -> list[str]:
    if not ((power_session_type == "CREATE") or (power_session_type == "COMPLETE")):
        raise ValueError(
            "Invalid Power Session, it must be either `CREATE` or `COMPLETE`"
        )

    if power_session_type == "CREATE":
        power_session_method = db_queries.create_power_session
    else:
        power_session_method = db_queries.complete_power_session
    update_queries = []
    for switch_id, trigger_time in power_switches.items():
        update_queries.append(power_session_method(switch_id, trigger_time))
    return update_queries


def get_device_control_bits(device_id: str):
    # This sql statemate is to collect `power_supply` `clear wifi` etc
    db_queries = DatabaseQueries(device_id)
    sql = db_queries.get_power_suply_status()
    result = db.run_qry(sql)[0]

    if result["clear_wifi"]:
        # update clear_wifi
        sql = db_queries.update_wifi_status()
        db.run_qry(sql)
        return -1

    if result["power_supply"]:
        timer_flag = False
        # This sql is to collect timers data and handshake_times from esp8266 hardware
        sql = db_queries.get_handshakes_timer_states()
        timer_result = db.run_qry(sql)[0]
        timezone = pytz.timezone(timer_result["timezone"])
        utc_timezone = pytz.timezone('UTC')

        # update hardware handshake time
        sql = db_queries.update_hardware_handshake_time(timezone)
        db.run_qry(sql)
        # no need to collect result for UPDATE query

        for timer_state in result["timer_states"]:
            if timer_state == "1":
                timer_flag = True
                break
            else:
                continue
        if timer_flag:
            timer_states = timer_result["timer_states"]
            control_bits = timer_result["control_bits"]
            current_time = datetime.now(timezone)
            new_control_bits = ""
            new_timer_states = ""
            power_start_switches: dict = {}
            power_end_switches: dict = {}

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
                        # all power sessions are UTC configured
                        if control_bits[i] == "0":
                            power_end_switches[
                                device_id + "_" + str(i)
                            ] = utc_timezone.localize(trigger_time).strftime(
                                "%Y-%m-%d %H:%M:%S"
                            )
                            new_control_bits += "1"
                        else:
                            power_start_switches[
                                device_id + "_" + str(i)
                            ] = utc_timezone.localize(trigger_time).strftime(
                                "%Y-%m-%d %H:%M:%S"
                            )
                            new_control_bits += "0"
                        # toggle the control bit and clear the timer state time
                        new_timer_states += "0"
                else:
                    new_control_bits += control_bits[i]
                    new_timer_states += "0"

            if timer_overflowed:
                # for tracting the power consumed by each swicth after timer over flow
                power_session_sqls = []
                if power_start_switches:
                    power_session_sqls = power_session_sqls + power_session_queries(
                        db_queries, power_start_switches, "CREATE"
                    )
                if power_end_switches:
                    power_session_sqls = power_session_sqls + power_session_queries(
                        db_queries, power_end_switches, "COMPLETE"
                    )

                # updating all power sessions after timer overflows
                # no need to collect the result as all are `UPDATE` queries
                logger.debug(power_session_sqls)
                if power_session_sqls:
                    db.run_multiple_queries(power_session_sqls)

                # clear timer overflows after correcting the power sessions
                sql = db_queries.update_after_timer_overflow(
                    new_control_bits, new_timer_states
                )
                db.run_qry(sql)
            return new_control_bits
        else:
            return result["control_bits"]
    else:
        return "1" * len(result["control_bits"])


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


def validate_mac_id(mac_id: str):
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
            logger.error(e)
        return {"statusCode": 200, "body": json.dumps(mac_id)}
    elif event.get("httpMethod") == "TEST":
        from unit_test import TestPowerSession

        test_device_switch_id = event.get("device_switch_id")
        t = TestPowerSession(test_device_switch_id)
        if event.get("testCase") == "total_power_session":
            t.create_power_session()
            t.complete_power_session()
            return {
                "statusCode": 200,
                "body": "Total Power Session (create and complete) are working",
            }
        if event.get("testCase") == "create_power_session":
            t.create_power_session()
            return {
                "statusCode": 200,
                "body": "Create Power Session are working",
            }
        if event.get("testCase") == "complete_power_session":
            t.complete_power_session()
            return {
                "statusCode": 200,
                "body": "Complete Power Session are working",
            }

