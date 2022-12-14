from datetime import datetime


def sql_formate(method):
    def wrapper(*args, **kwargs):
        sql_string = method(*args, **kwargs)
        sql_string = sql_string.replace("\n", "")
        return sql_string

    return wrapper


class DatabaseQueries:
    def __init__(self, device_id) -> None:
        self.device_id = device_id

    @sql_formate
    def get_power_suply_status(self) -> str:
        return f"""
        SELECT power_supply, clear_wifi, control_bits, timer_states
        FROM `user_devices` LEFT JOIN `device_control`
        ON user_devices.id = device_control.owner_id
        WHERE `device_id` = '{self.device_id}';"""

    @sql_formate
    def get_handshakes_timer_states(self) -> str:
        return f"""
        SELECT * FROM `device_timers`
        RIGHT JOIN `device_control`
        ON device_timers.device_id_id = device_control.device_id
        WHERE `device_id` = '{self.device_id}';
        """

    @sql_formate
    def update_wifi_status(self) -> str:
        return f"""
        UPDATE `user_devices`
        LEFT JOIN `device_control`
        ON user_devices.id = device_control.owner_id
        SET `clear_wifi` = false
        WHERE `device_id` = '{self.device_id}';"""

    @sql_formate
    def update_hardware_handshake_time(self, timezone) -> str:
        current_time = datetime.now(timezone)
        return f"""UPDATE `device_control`
        SET `handshake_time` = '{current_time.strftime("%Y-%m-%d %H:%M:%S")}'
        WHERE `device_id` = '{self.device_id}'"""

    @sql_formate
    def update_after_timer_overflow(
        self, new_control_bits: str, new_timer_states: str
    ) -> str:
        return f"""
        UPDATE `device_control` SET `control_bits` = '{new_control_bits}',
        `timer_states` = '{new_timer_states}'
        WHERE `device_id` = '{self.device_id}';
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
