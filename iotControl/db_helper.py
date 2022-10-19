import os
import pymysql


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
        self.conn.ping()
        with self.conn.cursor() as cursor:
            cursor.execute(sql)
            self.conn.commit()
            result = cursor.fetchall()
        # end with
        return result
