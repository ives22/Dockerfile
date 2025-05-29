#!/usr/bin/python3
#
from flask import Flask, request, abort, Response, jsonify as flask_jsonify, make_response
import argparse 
import sys, os, getopt, socket, json, time

app = Flask(__name__)

@app.route('/')
def index():
    return ('vvoo demoapp v1.1 !! ClientIP: {}, ServerName: {}, '
          'ServerIP: {}!\n'.format(request.remote_addr, socket.gethostname(),
                                  socket.gethostbyname(socket.gethostname())))

@app.route('/hostname')
def hostname():
    return ('ServerName: {}\n'.format(socket.gethostname()))

health_status = {'livez': 'OK', 'readyz': 'OK'}
probe_count = {'livez': 0, 'readyz': 0} 

@app.route('/livez', methods=['GET','POST'])
def livez():
    if request.method == 'POST':
        status = request.form['livez']
        health_status['livez'] = status
        return ''

    else:
        if probe_count['livez'] == 0:
            time.sleep(5)
        probe_count['livez'] += 1
        if health_status['livez'] == 'OK':
            return make_response((health_status['livez']), 200)
        else:
            return make_response((health_status['livez']), 506)

@app.route('/readyz', methods=['GET','POST'])
def readyz():
    if request.method == 'POST':
        status = request.form['readyz']
        health_status['readyz'] = status
        return ''

    else:
        if probe_count['readyz'] == 0:
            time.sleep(15)
        probe_count['readyz'] += 1
        if health_status['readyz'] == 'OK':
            return make_response((health_status['readyz']), 200)
        else:
            return make_response((health_status['readyz']), 507)

@app.route('/configs')
def configs():
    return ('DEPLOYENV: {}\nRELEASE: {}\n'.format(os.environ.get('DEPLOYENV'), os.environ.get('RELEASE')))

@app.route("/user-agent")
def view_user_agent():
    # user_agent=request.headers.get('User-Agent')
    return('User-Agent: {}\n'.format(request.headers.get('user-agent'))) 

def main(argv):
    port = 80
    host = '0.0.0.0'
    debug = False

    if os.environ.get('PORT') is not None:
        port = os.environ.get('PORT')

    if os.environ.get('HOST') is not None:
        host = os.environ.get('HOST')

    try:
        opts, args = getopt.getopt(argv,"vh:p:",["verbose","host=","port="])
    except getopt.GetoptError:
        print('server.py -p <portnumber>')
        sys.exit(2)
    for opt, arg in opts:
        if opt in ("-p", "--port"):
            port = arg
        elif opt in ("-h", "--host"):
            host = arg
        elif opt in ("-v", "--verbose"):
            debug = True

    app.run(host=str(host), port=int(port), debug=bool(debug))


if __name__ == "__main__":
    main(sys.argv[1:])
