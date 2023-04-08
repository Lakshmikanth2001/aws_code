import os
import pytz
import json
import logging
import pymysql
from datetime import date
from datetime import datetime
from db_helper import Database
from db_queries import DatabaseQueries
from data_types import DeviceControlType

logger = logging.getLogger()
logger.setLevel(logging.DEBUG)
db = Database()
UTC_TIMEZONE = pytz.timezone("UTC")


def get_date(date_str: str, time_str: str):
    date = list(map(int, date_str.split("-")))
    time = list(map(int, time_str.split(":")))
    return datetime(*date, *time)


def manage_switch_power_seesion(
    old_control_bit: str,
    new_control_bit: str,
    device_id: str,
    switch_index: int,
    overflow_time: str,
) -> str:
    db_queries = DatabaseQueries(device_id)
    device_bit_id = f"{device_id}_{switch_index}"
    if old_control_bit == "1" and new_control_bit == "0":
        return db_queries.create_power_session(device_bit_id, overflow_time)
    elif old_control_bit == "0" and new_control_bit == "1":
        return db_queries.complete_power_session(device_bit_id, overflow_time)
    # no need to create or complete power session
    return None


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


def handle_series_timer(old_control_bit: str, device_id: str, switch_index: int):
    new_control_bit = old_control_bit
    db_queries = DatabaseQueries(device_id)
    sql = db_queries.get_series_timer_states(switch_index)
    overflow_flag = False

    result = db.run_qry(sql)[0]
    power_session_sqls = []
    user_timezone = pytz.timezone(result["timezone"])
    series_timers_info: list[dict] = json.loads(result["timer_info"])

    logger.debug(series_timers_info)

    all_timers_overflowed = True
    for index, timer_info in enumerate(series_timers_info):
        if timer_info.get("overflowed", False):
            # new_control_bit = old_control_bit
            # past timers which are stored
            continue

        overflow_date, overflow_time = timer_info["timer_overflow_time"].split(" ")
        overflow_datetime: datetime = user_timezone.localize(
            get_date(overflow_date, overflow_time)
        )
        current_utc_datetime = datetime.now(user_timezone)

        logger.info(f"Current UTC time {current_utc_datetime}")
        logger.info(f"Overflow time {overflow_datetime}")
        if overflow_datetime > current_utc_datetime:
            all_timers_overflowed = False
            continue

        # timer overflowed
        logger.info("timer overflowed")
        overflow_flag = True
        new_control_bit = "0" if timer_info["desired_state"] == "ON" else "1"
        power_sql = manage_switch_power_seesion(
            old_control_bit,
            new_control_bit,
            device_id,
            switch_index,
            overflow_datetime.astimezone(UTC_TIMEZONE).strftime("%Y-%m-%d %H:%M:%S"),
        )
        if power_sql != None:
            power_session_sqls.append(power_sql)

        timer_info["overflowed"] = "true"
        series_timers_info[index] = timer_info
        old_control_bit = new_control_bit

    return (
        new_control_bit,
        power_session_sqls,
        all_timers_overflowed,
        series_timers_info,
        overflow_flag,
    )


def collect_resolve_series_timers(
    device_id: str, control_bits: str, series_timer_states: str
):
    power_session_sqls = []
    series_timer_overflowed = False
    new_control_bits = ""
    new_series_timer_states = ""

    timerinfo_update_sqls = []

    for index, timer_state in enumerate(series_timer_states):
        if timer_state == "0":
            new_control_bits += control_bits[index]
            new_series_timer_states += timer_state
            continue

        (
            new_control_bit,
            power_sqls,
            all_timers_overflowed,
            series_timers_info,
            timer_overflowed,
        ) = handle_series_timer(control_bits[index], device_id, index)

        # if any one of the series time overflowed we should update the timer states
        if timer_overflowed:
            series_timer_overflowed = True

        timerinfo_update_sqls.append(
            DatabaseQueries.update_series_timer_info(
                device_id, index, series_timers_info
            )
        )
        new_control_bits += new_control_bit
        power_session_sqls.extend(power_sqls)

        # clear the series timer
        if all_timers_overflowed:
            new_series_timer_states += "0"
        else:
            new_series_timer_states += timer_state
    return (
        new_control_bits,
        new_series_timer_states,
        power_session_sqls,
        series_timer_overflowed,
        timerinfo_update_sqls,
    )


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

    if not result["power_supply"]:
        return "1" * len(result["control_bits"])

    timer_flag = False
    series_timer_flag = False
    # This sql is to collect timers data and handshake_times from esp8266 hardware
    sql = db_queries.get_handshakes_timer_states()
    device_control_timer_info = db.run_qry(sql)[0]
    timezone = pytz.timezone(device_control_timer_info["timezone"])
    handshake_collection = device_control_timer_info["handshake_collection"]

    # update hardware handshake time
    sql = db_queries.update_hardware_handshake_time(timezone, handshake_collection)
    db.run_qry(sql)
    # no need to collect result for UPDATE query

    for timer_state in result["timer_states"]:
        if timer_state == "1":
            timer_flag = True
            break

    for series_timer_state in result["series_timer_states"]:
        if series_timer_state == "1":
            series_timer_flag = True
            break

    if not (timer_flag or series_timer_flag):
        return result["control_bits"]

    timer_states = device_control_timer_info["timer_states"]
    control_bits = device_control_timer_info["control_bits"]
    new_control_bits = ""
    new_timer_states = ""
    series_timer_overflowed_flag = False
    power_start_switches: dict = {}
    power_end_switches: dict = {}

    if series_timer_flag:
        (
            control_bits,
            new_series_timer_states,
            series_timer_power_session_sqls,
            series_timer_overflowed_flag,
            timer_info_update_sqls,
        ) = collect_resolve_series_timers(
            device_id, control_bits, result["series_timer_states"]
        )
        # logger.debug(f"control bits after series_timer_overflow {control_bits}")

    current_time = datetime.now(timezone)
    timer_overflowed = False
    for i in range(len(control_bits)):
        trigger_time = device_control_timer_info[f"time_{i}"]

        if not (timer_states[i] == "1" and trigger_time != None):
            new_control_bits += control_bits[i]
            new_timer_states += "0"
            continue

        # if timer is active and trigger time is not None - Guard Clause

        # no need to localize current_time
        if current_time < timezone.localize(trigger_time):
            # prevent sql injection
            # assert control_bits[i] == "1" or control_bits[i] == "0"
            new_control_bits += control_bits[i]
            new_timer_states += "1"
            continue

        # guard clause for timer_overflow
        timer_overflowed = True
        # all power sessions are UTC configured
        if control_bits[i] == "0":
            power_end_switches[device_id + "_" + str(i)] = (
                timezone.localize(trigger_time)
                .astimezone(UTC_TIMEZONE)
                .strftime("%Y-%m-%d %H:%M:%S")
            )
            new_control_bits += "1"
        else:
            power_start_switches[device_id + "_" + str(i)] = (
                timezone.localize(trigger_time)
                .astimezone(UTC_TIMEZONE)
                .strftime("%Y-%m-%d %H:%M:%S")
            )
            new_control_bits += "0"
        # toggle the control bit and clear the timer state time
        new_timer_states += "0"

    power_session_sqls = []
    if power_start_switches:
        power_session_sqls = power_session_sqls + power_session_queries(
            db_queries, power_start_switches, "CREATE"
        )
    if power_end_switches:
        power_session_sqls = power_session_sqls + power_session_queries(
            db_queries, power_end_switches, "COMPLETE"
        )

    if timer_overflowed or series_timer_overflowed_flag:
        # for tracting the power consumed by each swicth after timer over flow
        device_control_data = DeviceControlType(
            device_id, new_control_bits, new_timer_states, new_series_timer_states
        )

        power_session_manager(
            device_control_data,
            timer_overflowed,
            series_timer_overflowed_flag,
            power_session_sqls,
            series_timer_power_session_sqls,
            timer_info_update_sqls,
        )

    return new_control_bits


# utility method to create or complete power sesion after timer overflow
def power_session_manager(
    device_control_data: DeviceControlType,
    timer_overflowed: bool,
    series_timer_overflowed: bool,
    power_session_sqls: list[str],
    series_timer_power_sqls: list[str],
    timer_info_update_sqls: list[str],
):

    db_queries = device_control_data.db_quries
    new_control_bits = device_control_data.control_bits
    new_timer_states = device_control_data.timer_states
    new_series_timer_states = device_control_data.series_timer_states

    # updating all power sessions after timer overflows
    # no need to collect the result as all are `UPDATE` queries
    total_power_session_sqls = power_session_sqls + series_timer_power_sqls

    if total_power_session_sqls:
        logger.debug(f"power session SQL from Normal Timer {power_session_sqls}")
        logger.debug(f"power session SQL from Series Timer {series_timer_power_sqls}")
        db.run_multiple_queries(total_power_session_sqls)

    # clear timer overflows after correcting the power sessions
    update_sqls = []
    if timer_overflowed:
        update_sqls.append(
            db_queries.update_after_timer_overflow(new_control_bits, new_timer_states)
        )
    if series_timer_overflowed:
        update_sqls.append(
            db_queries.update_after_series_timer_overflow(
                new_control_bits, new_series_timer_states
            )
        )
    logger.debug(f"Update SQLs : {update_sqls + timer_info_update_sqls}")
    # db.run_multiple_queries(update_sqls)
    db.run_multiple_queries(timer_info_update_sqls + update_sqls)


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


def get_response(event: dict, device_id: str):
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
            return get_response(event, device_id)

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
