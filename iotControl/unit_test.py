from db_queries import DatabaseQueries
from db_helper import Database

db_queries = DatabaseQueries(None)
db = Database()


class TestPowerSession:
    def __init__(self, device_switch_id: str) -> None:
        self.device_switch_id = device_switch_id
        self.trigger_on_time = "2022-11-19 05:05:05"  # format "%Y-%m-%d %H:%M:%S"
        self.trigger_off_time = "2022-11-19 05:15:05"

    def create_power_session(self):
        sql_query = db_queries.create_power_session(
            self.device_switch_id, self.trigger_on_time
        )
        db.run_qry(sql_query)

    def complete_power_session(self):
        sql_query = db_queries.complete_power_session(
            self.device_switch_id, self.trigger_off_time
        )
        db.run_qry(sql_query)
