from flask import Flask
from flask import render_template
from flask import jsonify
from flask import Response

import db
import hike

app = Flask(__name__, static_url_path='/images', static_folder='images')
hdb = db.HubDatabase()

@app.route('/')
def get_home():
    sessions = hdb.get_sessions()
    return render_template('index.html')

@app.route('/sessions')
def get_sessions():
    sessions = hdb.get_sessions()
    return render_template('sessions.html', sessions=sessions)

@app.route('/sessions/<id>')
def get_session_by_id(id):
    session = hdb.get_session(id)
    if session is None:
        return render_template('404.html'), 404  # Render a custom 404 template
    return render_template('session.html', session=session)

@app.route('/sessions/<id>/delete')
def delete_session(id):
    session = hdb.get_session(id)
    if session is None:
        return render_template('404.html'), 404  # Render a custom 404 template
    hdb.delete(id)
    print(f'DELETED SESSION WITH ID: {id}')

    return Response(status=202)

@app.route('/', defaults={'path': ''})
@app.route('/<path:path>')
def catch_all(path):
    return render_template('404.html'), 404
if __name__ == "__main__":
    app.run('0.0.0.0', debug=True)