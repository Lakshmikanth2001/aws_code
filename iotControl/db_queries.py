from db_helper import Database
from datetime import datetime

class DatabaseQueries:
    def __init__(self, device_id) -> None:
        self.db_instance = Database()
        self.device_id = device_id

    def get_power_suply_status(self) -> str:
        return f"""
        SELECT power_supply, clear_wifi, control_bits, timer_states
        FROM `user_devices` LEFT JOIN `device_control`
        ON user_devices.id = device_control.owner_id
        WHERE `device_id` = '{self.device_id}';"""

    def get_handshakes_timer_states(self) -> str:
        return f"""
        SELECT * FROM `device_timers`
        RIGHT JOIN `device_control`
        ON device_timers.device_id_id = device_control.device_id
        WHERE `device_id` = '{self.device_id}';
        """

    def update_wifi_status(self) -> str:
        return f"""
        UPDATE `user_devices`
        LEFT JOIN `device_control`
        ON user_devices.id = device_control.owner_id
        SET `clear_wifi` = false
        WHERE `device_id` = '{self.device_id}';"""

    def update_hardware_handshake_time(self, timezone) -> str:
        current_time = datetime.now(timezone)
        return f"""UPDATE `device_control`
        SET `handshake_time` = '{current_time.strftime("%Y-%m-%d %H:%M:%S")}'
        WHERE `device_id` = '{self.device_id}'"""

    def update_after_timer_overflow(
        self, new_control_bits: str, new_timer_states: str
    ) -> str:
        return f"""
        UPDATE `device_control` SET `control_bits` = '{new_control_bits}',
        `timer_states` = '{new_timer_states}'
        WHERE `device_id` = '{self.device_id}';
        """
