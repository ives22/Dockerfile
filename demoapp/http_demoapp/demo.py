#!/usr/bin/python3

# 参考 马哥教育 开发，提供支持arm架构

from flask import Flask, request, jsonify
import socket, sys, os, random, logging
import argparse

app = Flask(__name__)

# Read the version number from the environment variable or use the default value "v2.0"
app_version = os.getenv('VERSION', 'v2.0')

# Global variables to store the status of livez and readyz
livez_status = "OK"  # Default value is "OK"
readyz_status = "OK"  # Default value is "OK"

# Get the service name from the environment variable
current_service_name = os.environ.get('APP_NAME', 'demoapp')

# Randomly choose a color to use when the program starts
random.seed()  # Use system time as the random seed to increase randomness
random_color = "#{:06x}".format(random.randint(0, 0xFFFFFF))

# Disable Flask's built-in logging
app.logger.disabled = True
log = logging.getLogger('werkzeug')
log.disabled = True

# Set up custom logger
logger = logging.getLogger('customLogger')
logger.setLevel(logging.INFO)
console_handler = logging.StreamHandler()
console_handler.setFormatter(logging.Formatter(
    '%(asctime)s %(message)s', datefmt='%d/%b/%Y:%H:%M:%S %z'))
logger.addHandler(console_handler)

@app.before_request
def log_request_info():
    forwarded_for = request.headers.get('X-Forwarded-For', '-')
    remote_addr = request.remote_addr
    method = request.method
    path = request.full_path
    user_agent = request.user_agent.string
    protocol = request.environ.get('SERVER_PROTOCOL')

    log_message = f'{remote_addr} - - [{request.date}] "{method} {path} {protocol}" - "{user_agent}" "{forwarded_for}"'
    logger.info(log_message)

@app.route('/')
def welcome():
    server_name = socket.gethostname()
    client_ip = request.remote_addr
    server_ip = socket.gethostbyname(server_name)
    user_agent = request.headers.get('User-Agent', '')

    if 'curl' in user_agent or 'wget' in user_agent or 'elinks' in user_agent:
        welcome_message = f"Demoapp by iKubernetes! App Version: {app_version}, Client IP: {client_ip}, Server Name: {server_name}, Server IP: {server_ip} ~\n"
        return welcome_message, 200
    else:
        server_name_html = f'<span style="color: {random_color};">{server_name}</span>'
        server_version_html = f'<span style="color: red;">{app_version}</span>'
        welcome_message = f"""
        <html>
        <head><title>Welcome</title></head>
        <body>
            <p>Demoapp by iKubernetes! App Version: {server_version_html}, Client IP: {client_ip}, Server Name: {server_name_html}, Server IP: {server_ip} ~</p>
        </body>
        </html>
        """
        return welcome_message

def handle_health_check(status, method):
    if method == 'POST':
        status_value = request.form.get(status, 'OK')
        globals()[f"{status}_status"] = status_value
        return jsonify({status: status_value}), 200
    elif method == 'GET':
        status_value = globals()[f"{status}_status"]
        return (status_value, 200) if status_value == "OK" else (status_value, 506)
    elif method == 'HEAD':
        status_value = globals()[f"{status}_status"]
        return '', 200 if status_value == "OK" else '', 506

@app.route('/livez', methods=['POST', 'GET', 'HEAD'])
def livez():
    return handle_health_check('livez', request.method)

@app.route('/readyz', methods=['POST', 'GET', 'HEAD'])
def readyz():
    return handle_health_check('readyz', request.method)

@app.route('/api/get_service', methods=['GET'])
def get_service_name():
    server_name = socket.gethostname()
    return jsonify({'service_name': current_service_name, 'instance_name': server_name, 'app_version': app_version})

@app.route('/hostname')
def hostname():
    return f'ServerName: {socket.gethostname()}\n'

@app.route('/configs')
def configs():
    return f'DEPLOYENV: {os.environ.get("DEPLOYENV")}\nRELEASE: {os.environ.get("RELEASE")}\n'

@app.route("/user-agent")
def view_user_agent():
    return f'User-Agent: {request.headers.get("User-Agent")}\n'

def main(argv):
    host = os.getenv('HOST', '0.0.0.0')
    port = int(os.getenv('PORT', 80))
    debug = False
    
    parser = argparse.ArgumentParser()
    parser.add_argument('-p', '--port', type=int, help='Port number')
    parser.add_argument('-l', '--host', help='Host address')
    parser.add_argument('-v', '--verbose', action='store_true', help='Enable debug mode')

    args = parser.parse_args(argv)
    if args.port:
        port = args.port
    if args.host:
        host = args.host
    if args.verbose:
        debug = True


    app.run(host=host, port=port, debug=debug)

if __name__ == "__main__":
    main(sys.argv[1:])
