import os
import logging
import pymysql
from pymysql.constants import CLIENT

logger = logging.getLogger()
logger.setLevel(logging.DEBUG)


class Database:

    database_cred = {
        "host": os.environ["RDS_HOST"],
        "user": os.environ["RDS_USER"],
        "password": os.environ["RDS_PASSWORD"],
        "database": os.environ["RDS_DATABASE"],
        "cursorclass": pymysql.cursors.DictCursor,
    }

    def __init__(self, database_cred: dict = None):
        if database_cred:
            self.conn = pymysql.connect(**database_cred)
        else:
            self.conn = pymysql.connect(**self.database_cred)
        # end if

    def run_qry(self, sql: str):
        # to check DB Connection pin the instance
        self.conn.ping()
        with self.conn.cursor() as cursor:
            cursor.execute(sql)
            self.conn.commit()
            result = cursor.fetchall()
        # end with
        return result

    def run_multiple_queries(self, sql_statements: list[str]):
        if len(sql_statements) == 1:
            result = self.run_qry(sql_statements[0])
            logging.debug(f"[SQL_SINGLE]: {result}")
            return result

        new_db_cred = {"client_flag": CLIENT.MULTI_STATEMENTS, **self.database_cred}
        sql_statements = ";".join(sql_statements)

        sql_connetion = pymysql.connect(**new_db_cred)
        # to check DB Connection pin the instance
        sql_connetion.ping()
        with sql_connetion.cursor() as cursor:
            cursor.execute(sql_statements)
            self.conn.commit()
            result = cursor.fetchall()
        logging.debug(f"[SQL_MULTI]: {result}")
        return result
