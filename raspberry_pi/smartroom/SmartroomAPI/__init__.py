from flask import Flask, request
from SmartroomAPI import utils

app = Flask(__name__)

@app.route("/", methods=["GET"])
def root():
    return "Hello World!"

@app.route("/api/update", methods=["POST", "GET"])
def update():
    utils.parse_request(request.get_data().decode('utf-8'))
    return "200"
