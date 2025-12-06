import sqlite3
from typing import List
import hike
import threading

DB_FILE_NAME = 'sessions.db'

DB_SESSION_TABLE = {
    "name": "sessions",
    "cols": [
        "session_id integer PRIMARY KEY",
        "km integer",
        "steps integer",
        "burnt_kcal integer"
    ]
}


# lock object so multithreaded use of the same
# HubDatabase object 

class HubDatabase:
    lock = threading.Lock()

    def __init__(self):
        self.con = sqlite3.connect(DB_FILE_NAME, check_same_thread=False)
        self.cur = self.con.cursor()

        create_table_sql = f"CREATE TABLE IF NOT EXISTS {DB_SESSION_TABLE['name']} ({', '.join(DB_SESSION_TABLE['cols'])})"
        self.cur.execute(create_table_sql)

        self.con.commit()

        # Insert example values into the table

    def insert_example_values(self):
        # Get the maximum session_id currently in the table
        max_session_id = self.cur.execute(f"SELECT MAX(session_id) FROM {DB_SESSION_TABLE['name']}").fetchone()[0]
        next_session_id = max_session_id + 1 if max_session_id is not None else 1

        example_data = [
            (next_session_id, 10, 2000, 100),
            (next_session_id + 1, 8, 1500, 80),
            (next_session_id + 2, 12, 2500, 120)
        ]
        insert_sql = f"INSERT INTO {DB_SESSION_TABLE['name']} VALUES (?, ?, ?, ?)"
        self.cur.executemany(insert_sql, example_data)
        self.con.commit()

    def save(self, s: hike.HikeSession):
        sessions = self.get_sessions()

        if len(sessions) > 0:
            s.id = sorted(sessions, key=lambda sess: sess.id)[-1].id + 1
        else:
            s.id = 1

        try:
            self.lock.acquire()

            try:
                self.cur.execute(f"INSERT INTO {DB_SESSION_TABLE['name']} VALUES ({s.id}, {s.km}, {s.steps}, {s.kcal})")
            except sqlite3.IntegrityError:
                print("WARNING: Session ID already exists in database! Aborting saving current session.")


            self.con.commit()
        finally:
            self.lock.release()

    def delete(self, session_id: int):
        try:
            self.lock.acquire()

            self.cur.execute(f"DELETE FROM {DB_SESSION_TABLE['name']} WHERE session_id = {session_id}")
            self.con.commit()
        finally:
            self.lock.release()

    def get_sessions(self) -> List[hike.HikeSession]:
        try:
            self.lock.acquire()
            rows = self.cur.execute(f"SELECT * FROM {DB_SESSION_TABLE['name']}").fetchall()
        finally:
            self.lock.release()

        return list(map(lambda r: hike.from_list(r), rows))

    def get_session(self, session_id: int) -> hike.HikeSession:
        try:
            self.lock.acquire()
            rows = self.cur.execute(f"SELECT * FROM {DB_SESSION_TABLE['name']} WHERE session_id = {session_id}").fetchall()
        finally:
            self.lock.release()

        if rows:  # Check if rows is not empty
            return hike.from_list(rows[0])
        else:
            return None  # Return None if session does not exist


    def __del__(self):
        self.cur.close()
        self.con.close()