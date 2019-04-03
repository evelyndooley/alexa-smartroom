import colorsys
from flask import Flask
import math
import os
import urllib
import yaml
import serial

app = Flask(__name__)

IR_CODES = {
    "power": "D7E84B1B",
    "brightness_up": "B3D4B87F",
    "brightness_down": "44490A7B",
    "#ff0000": "A8E05FBB",
    "#00ff00": "3954B1B7",
    "#0000ff": "E318261B",
    "#ffffff": "9716BE3F",
    "#ffa500": "5B83B61B",
    "#c6ff1e": "B5310E1F",
    "#33cc33": "B5310E1F",
    "#00008b": "73CEC633",
    "#ffbecb": "8C22657B",
    "#ff00ff": "8C22657B",
    "#ffff00": "B08CB7DF",
    "#ffd400": "B08CB7DF",
    "#007e7e": "410109DB",
    "#00ffff": "A23C94BF",
    "#a021ef": "DC0197DB",
    "#87cdf9": "E721C0DB",
    "#8b0000": "44C407DB"
}

LIGHT_INDEX = {
    "all the lights": 0,
    "wall": 1,
    "desk": 2,
    "mirror": 3,
    "bed": 4,
    "couch": 5,
    "pc": 6
}

OPERATIONS = [
    'power',
    'color'
]

port = serial.Serial('/dev/ttyS0', 9600)

def send_command(command):
    print(command)
    if len(command) == 12:
        port.write(command.encode())
    else:
        print('Bad command!')


def parse_request(request):
    req = urllib.parse.parse_qs(request)
    print(req)
    if req.get('secret')[0] == os.environ['SECRET_KEY']:
        operation = req.get('operation')[0]
        value = req.get('value')[0]
        index = LIGHT_INDEX.get(req.get('device')[0])
        print(index)
        if operation == 'power':
            power(index, value)
        elif operation == 'color':
           color_hsv(index, value)
        elif operation == 'colorTemp':
            colorTemp(index, value)
        elif operation == 'brightness':
            brightness(index, value)


def power(index, value):
    command = 'X'
    command += str(index)
    command += 'P'
    if 'ON' in value:
        command += '1'
    else:
        command += '0'
    command += '00000000'
    send_command(command)


def color(index, color_rgb):
    command = 'X'
    command += str(index)
    command += 'C'
    for c in color_rgb:
        s = str(c)
        while len(s) < 3:
            s = '0' + s
        command += str(s)
    send_command(command)


def color_hsv(index, value):
    color_hsv = yaml.load(value)
    color_rgb = colorsys.hsv_to_rgb(float(color_hsv.get('hue'))/360,
                                    float(color_hsv.get('saturation')),
                                    float(color_hsv.get('brightness')))

    color_rgb2 = (int(color_rgb[0] * 255),
                  int(color_rgb[1] * 255),
                  int(color_rgb[2] * 255))

    color(index, color_rgb2)


def colorTemp(index, value):
    red, green, blue = convert_K_to_RGB(int(value))
    color_rgb = [
        int(red),
        int(green),
        int(blue)
    ]
    color(index, color_rgb)


def brightness(index, value):
    command = 'X'
    command += str(index)
    command += 'B'
    while len(value) < 3:
        value = '0' + value
    command += str(value)
    command += '000000'
    send_command(command)


def rgb2hex(color_rgb):
    color_hex = '#%02x%02x%02x' % (int(rgb[0]),
                                   int(rgb[1]),
                                   int(rgb[2]))
    return color_hex


def convert_K_to_RGB(colour_temperature):
    """
    Converts from K to RGB, algorithm courtesy of
    http://www.tannerhelland.com/4435/convert-temperature-rgb-algorithm-code/
    """
    #range check
    if colour_temperature < 1000:
        colour_temperature = 1000
    elif colour_temperature > 40000:
        colour_temperature = 40000

    tmp_internal = colour_temperature / 100.0

    # red
    if tmp_internal <= 66:
        red = 255
    else:
        tmp_red = 329.698727446 * math.pow(tmp_internal - 60, -0.1332047592)
        if tmp_red < 0:
            red = 0
        elif tmp_red > 255:
            red = 255
        else:
            red = tmp_red

    # green
    if tmp_internal <=66:
        tmp_green = 99.4708025861 * math.log(tmp_internal) - 161.1195681661
        if tmp_green < 0:
            green = 0
        elif tmp_green > 255:
            green = 255
        else:
            green = tmp_green
    else:
        tmp_green = 288.1221695283 * math.pow(tmp_internal - 60, -0.0755148492)
        if tmp_green < 0:
            green = 0
        elif tmp_green > 255:
            green = 255
        else:
            green = tmp_green

    # blue
    if tmp_internal >=66:
        blue = 255
    elif tmp_internal <= 19:
        blue = 0
    else:
        tmp_blue = 138.5177312231 * math.log(tmp_internal - 10) - 305.0447927307
        if tmp_blue < 0:
            blue = 0
        elif tmp_blue > 255:
            blue = 255
        else:
            blue = tmp_blue

    return red, green, blue
