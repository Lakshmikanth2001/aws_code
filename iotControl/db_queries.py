import json
from datetime import datetime
from datetime import timedelta


def sql_formate(method):
    def wrapper(*args, **kwargs):
        sql_string = method(*args, **kwargs)
        sql_string = sql_string.replace("\n", "")
        return sql_string.strip()

    return wrapper


class DatabaseQueries:
    def __init__(self, device_id) -> None:
        self.device_id = device_id

    @sql_formate
    def get_power_suply_status(self) -> str:
        return f"""
        SELECT power_supply, clear_wifi, control_bits, timer_states, series_timer_states
        FROM `user_devices` LEFT JOIN `device_control`
        ON user_devices.id = device_control.owner_id
        WHERE device_control.device_id = '{self.device_id}'"""

    @sql_formate
    def get_handshakes_timer_states(self) -> str:
        return f"""
        SELECT * FROM `device_timers`
        RIGHT JOIN `device_control`
        ON device_timers.device_id = device_control.device_id
        WHERE device_control.device_id = '{self.device_id}'
        """

    @sql_formate
    def get_series_timer_states(self, switch_index: int) -> str:
        return f"""
        SELECT * FROM `device_series_timers`
        WHERE device_id = '{self.device_id}' AND
        switch_index = {switch_index}
        """

    @sql_formate
    def update_wifi_status(self) -> str:
        return f"""
        UPDATE `user_devices`
        LEFT JOIN `device_control`
        ON user_devices.id = device_control.owner_id
        SET `clear_wifi` = false
        WHERE `device_id` = '{self.device_id}'"""

    @sql_formate
    def update_hardware_handshake_time(self, timezone, handshake_collection) -> str:
        current_time = datetime.now(timezone)
        if handshake_collection != None:
            handshake_collection = json.loads(handshake_collection)
            last_handshake_time: datetime = timezone.localize(handshake_collection[-1])
            if current_time - last_handshake_time > timedelta(seconds=10):
                handshake_collection.append(str(current_time))
        else:
            handshake_collection = [str(current_time)]

        return f"""UPDATE `device_control`
        SET `handshake_time` = '{current_time.strftime("%Y-%m-%d %H:%M:%S")}',
        `handshake_collection` = '{json.dumps(handshake_collection)}'
        WHERE `device_id` = '{self.device_id}'"""

    @sql_formate
    def update_after_timer_overflow(
        self, new_control_bits: str, new_timer_states: str
    ) -> str:
        return f"""
        UPDATE `device_control` SET `control_bits` = '{new_control_bits}',
        `timer_states` = '{new_timer_states}'
        WHERE `device_id` = '{self.device_id}'
        """

    @sql_formate
    def update_after_series_timer_overflow(
        self, new_control_bits: str, new_series_timer_states: str
    ) -> str:
        return f"""
        UPDATE `device_control` SET `control_bits` = '{new_control_bits}',
        `series_timer_states` = '{new_series_timer_states}'
        WHERE `device_id` = '{self.device_id}'
        """

    @classmethod
    @sql_formate
    def update_series_timer_info(
        cls, device_id: str, switch_index: int, timer_info: list[dict]
    ):
        return f"""
        UPDATE `device_series_timers` SET `timer_info` = '{json.dumps(timer_info)}'
        WHERE `device_id` = '{device_id}' AND `switch_index` = {switch_index}
        """

    @sql_formate
    def create_power_session(self, device_bit_id: str, trigger_time: str):
        return f"""
        INSERT INTO device_power_session(`device_bit_id`, `on_time`)
        VALUES ('{device_bit_id}', '{trigger_time}')
        """

    @sql_formate
    def complete_power_session(self, device_bit_id: str, trigger_time: str):
        # https://stackoverflow.com/questions/4429319/you-cant-specify-target-table-for-update-in-from-clause/14302701#14302701
        return f"""
        UPDATE `device_power_session` SET `off_time` = '{trigger_time}'
        WHERE `device_bit_id` = '{device_bit_id}' AND `off_time` IS NULL
        """
