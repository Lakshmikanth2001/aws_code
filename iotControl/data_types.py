from db_queries import DatabaseQueries


class DeviceControlType:
    def __init__(
        self,
        device_id: str,
        control_bits: str,
        timer_states: str,
        series_timer_states: str,
    ) -> None:
        self.db_quries = DatabaseQueries(device_id)
        self.device_id = device_id
        self.control_bits = control_bits
        self.timer_states = timer_states
        self.series_timer_states = series_timer_states
        pass
