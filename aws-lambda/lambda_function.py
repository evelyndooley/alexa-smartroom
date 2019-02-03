# -*- coding: utf-8 -*-

# Copyright 2017 Amazon.com, Inc. or its affiliates. All Rights Reserved.
#
# Licensed under the Amazon Software License (the "License"). You may not use this file except in
# compliance with the License. A copy of the License is located at
#
#    http://aws.amazon.com/asl/
#
# or in the "license" file accompanying this file. This file is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, express or implied. See the License for the specific
# language governing permissions and limitations under the License.

"""Alexa Smart Home Lambda Function Sample Code.

This file demonstrates some key concepts when migrating an existing Smart Home skill Lambda to
v3, including recommendations on how to transfer endpoint/appliance objects, how v2 and vNext
handlers can be used together, and how to validate your v3 responses using the new Validation
Schema.

Note that this example does not deal with user authentication, only uses virtual devices, omits
a lot of implementation and error handling to keep the code simple and focused.
"""

import colorsys
import logging
import time
import json
import os
import urllib
import uuid

# Imports for v3 validation
from validation import validate_message

# Setup logger
logger = logging.getLogger()
logger.setLevel(logging.INFO)

# Class for creating appliances, in this case smart lights
class AlexaHomeApp:
    def __init__(self, name, desc, id):
        self.applianceId = id
        self.manufacturerName = "evelyn corp"
        self.modelName = "Smart Light"
        self.version = "3"
        self.friendlyName = name
        self.friendlyDescription = desc
        self.isReachable = True
        self.actions = [
            "turnOn",
            "turnOff",
            "setPercentage",
            "incrementPercentage",
            "decrementPercentage",
            "setColor"
        ]
        self.additionalApplianceDetails = {}

    def __getitem__(self, key):
        return getattr(self, key)

    # Return JSON that Alexa understands
    def toJSON(self):
        return json.dumps(self, default=lambda o: o.__dict__)

class AlexaResponse:
    def __init__(self, namespace, name, value, endpoint_id, message_id, correlation_token):
       self.response = {
            "context": {
                     "properties": [
                    {
                        "namespace": namespace,
                        "name": name,
                        "value": value,
                        "timeOfSample": get_utc_timestamp(),
                        "uncertaintyInMilliseconds": 500
                    }]
                 },
                 "event": {
                     "header": {
                         "namespace": "Alexa",
                         "name": "Response",
                         "payloadVersion": "3",
                         "messageId": message_id,
                         "correlationToken": correlation_token
                     },
                     "endpoint": {
                         "scope": {
                             "type": "BearerToken",
                             "token": "access-token-from-Amazon"
                         },
                         "endpointId": endpoint_id
                     },
                     "payload": {}
                 }
             }

    def json(self):
        return self.response


# URL to post data to
base_url = os.environ['BASE_URL']

# Current list of conntrollable lights in my room
wall_light = AlexaHomeApp("wall", "wall only", "wall")
desk_light = AlexaHomeApp("desk", "desk only", "desk")
mirror_light = AlexaHomeApp("mirror", "mirror only", "mirror")
bed_light = AlexaHomeApp("bed", "bed only", "bed")
couch_light = AlexaHomeApp("couch", "couch only", "couch")
pc_light = AlexaHomeApp("pc", "pc only", "pc")
all_lights = AlexaHomeApp("all the lights", "all lights", "all the lights")

# Iterable list of appliances
appliances = [wall_light,
              desk_light,
              mirror_light,
              bed_light,
              couch_light,
              pc_light,
              all_lights]



def lambda_handler(request, context):
    """Main Lambda handler.

    Since you can expect both v2 and v3 directives for a period of time during the migration
    and transition of your existing users, this main Lambda handler must be modified to support
    both v2 and v3 requests.
    """

    try:
        logger.info("Directive:")
        logger.info(json.dumps(request, indent=4, sort_keys=True))

        version = get_directive_version(request)

        if version == "3":
            logger.info("Received v3 directive!")
            if request["directive"]["header"]["name"] == "Discover":
                response = handle_discovery_v3(request)
            else:
                response = handle_non_discovery_v3(request)


        logger.info("Response:")
        logger.info(json.dumps(response, indent=4, sort_keys=True))

        #if version == "3":
            #logger.info("Validate v3 response")
            #validate_message(request, response)

        return response
    except ValueError as error:
        logger.error(error)
        raise


# v3 handlers
def handle_discovery_v3(request):
    endpoints = []
    for appliance in appliances:
        endpoints.append(get_endpoint_from_v2_appliance(appliance))

    response = {
        "event": {
            "header": {
                "namespace": "Alexa.Discovery",
                "name": "Discover.Response",
                "payloadVersion": "3",
                "messageId": get_uuid()
            },
            "payload": {
                "endpoints": endpoints
            }
        }
    }
    return response


def handle_non_discovery_v3(request):
    request_namespace = request["directive"]["header"]["namespace"]
    request_name = request["directive"]["header"]["name"]
    device = request['directive']['endpoint']['endpointId']
    correlation_token = request["directive"]["header"]["correlationToken"]
    message_id = get_uuid()
    value = ""

    # Turn the light off or on
    if request_namespace == "Alexa.PowerController":
        if request_name == "TurnOn":
            value = "ON"
        else:
            value = "OFF"
        sendBatchRequest(device, 'power', value)

    # Adjust the brighness of light
    elif request_namespace == "Alexa.BrightnessController":
        if request_name =='Alexa.AdjustBrightness':
            value = request['directive']['payload']['brightnessDelta']
            sendBatchRequest(device, 'brightnessDelta', value)

        # Set the brightness to a level
        elif request_name == 'Alexa.SetBrightness':
            value = request['directive']['payload']['brighness']
            sendBatchRequest(device, 'brightness', value)

     # Change the light color
    elif request_namespace == "Alexa.ColorController":
        if request_name == "setColor":
            value = request['directive']['payload']['color']
            color_hue = value['hue']
            color_sat = value['saturation']
            color_val = value['brightness']
            color = colorsys.hsv_to_rgb(color_hue, color_sat, color_val)
            sendBatchRequest(device, 'color', color)

    # Authorization Response
    elif request_namespace == "Alexa.Authorization":
        if request_name == "AcceptGrant":
            response = {
                "event": {
                    "header": {
                        "namespace": "Alexa.Authorization",
                        "name": "AcceptGrant.Response",
                        "payloadVersion": "3",
                        "messageId": "5f8a426e-01e4-4cc9-8b79-65f8bd0fd8a4"
                    },
                    "payload": {}
                }
            }
            return response

    # Create the final response and send it
    response = AlexaResponse(request_namespace, request_name, value,
                                 device, message_id, correlation_token)
    return response.json()


# v3 utility functions
def get_endpoint_from_v2_appliance(appliance):
    endpoint = {
        "endpointId": appliance["applianceId"],
        "manufacturerName": appliance["manufacturerName"],
        "friendlyName": appliance["friendlyName"],
        "description": appliance["friendlyDescription"],
        "displayCategories": [],
        "cookie": appliance["additionalApplianceDetails"],
        "capabilities": []
    }
    endpoint["displayCategories"] = get_display_categories_from_v2_appliance(appliance)
    endpoint["capabilities"] = get_capabilities_from_v2_appliance(appliance)
    return endpoint


def get_directive_version(request):
    try:
        return request["directive"]["header"]["payloadVersion"]
    except:
        try:
            return request["header"]["payloadVersion"]
        except:
            return "-1"


def get_endpoint_by_endpoint_id(endpoint_id):
    appliance = get_appliance_by_appliance_id(endpoint_id)
    if appliance:
        return get_endpoint_from_v2_appliance(appliance)
    return None


def get_display_categories_from_v2_appliance(appliance):
    model_name = appliance["modelName"]
    if model_name == "Smart Switch": displayCategories = ["SWITCH"]
    elif model_name == "Smart Light": displayCategories = ["LIGHT"]
    elif model_name == "Smart White Light": displayCategories = ["LIGHT"]
    elif model_name == "Smart Thermostat": displayCategories = ["THERMOSTAT"]
    elif model_name == "Smart Lock": displayCategories = ["SMARTLOCK"]
    elif model_name == "Smart Scene": displayCategories = ["SCENE_TRIGGER"]
    elif model_name == "Smart Activity": displayCategories = ["ACTIVITY_TRIGGER"]
    elif model_name == "Smart Camera": displayCategories = ["CAMERA"]
    else: displayCategories = ["OTHER"]
    return displayCategories


def get_capabilities_from_v2_appliance(appliance):
    model_name = appliance["modelName"]
    if model_name == "Smart Light":
        capabilities = [
            {
                "type": "AlexaInterface",
                "interface": "Alexa.PowerController",
                "version": "3",
                "properties": {
                    "supported": [
                        { "name": "powerState" }
                    ],
                    "proactivelyReported": True,
                    "retrievable": True
                }
            },
            {
                "type": "AlexaInterface",
                "interface": "Alexa.ColorController",
                "version": "3",
                "properties": {
                    "supported": [
                        { "name": "color" }
                    ],
                    "proactivelyReported": True,
                    "retrievable": True
                }
            },
            {
                "type": "AlexaInterface",
                "interface": "Alexa.ColorTemperatureController",
                "version": "3",
                "properties": {
                    "supported": [
                        { "name": "colorTemperatureInKelvin" }
                    ],
                    "proactivelyReported": True,
                    "retrievable": True
                }
            },
            {
                "type": "AlexaInterface",
                "interface": "Alexa.BrightnessController",
                "version": "3",
                "properties": {
                    "supported": [
                        { "name": "brightness" }
                    ],
                    "proactivelyReported": True,
                    "retrievable": True
                }
            },
            {
                "type": "AlexaInterface",
                "interface": "Alexa.PowerLevelController",
                "version": "3",
                "properties": {
                    "supported": [
                        { "name": "powerLevel" }
                    ],
                    "proactivelyReported": True,
                    "retrievable": True
                }
            },
            {
                "type": "AlexaInterface",
                "interface": "Alexa.PercentageController",
                "version": "3",
                "properties": {
                    "supported": [
                        { "name": "percentage" }
                    ],
                    "proactivelyReported": True,
                    "retrievable": True
                }
            }
        ]

    # additional capabilities that are required for each endpoint
    endpoint_health_capability = {
        "type": "AlexaInterface",
        "interface": "Alexa.EndpointHealth",
        "version": "3",
        "properties": {
            "supported":[
                { "name":"connectivity" }
            ],
            "proactivelyReported": True,
            "retrievable": True
        }
    }
    alexa_interface_capability = {
        "type": "AlexaInterface",
        "interface": "Alexa",
        "version": "3"
    }
    capabilities.append(endpoint_health_capability)
    capabilities.append(alexa_interface_capability)
    return capabilities


def get_utc_timestamp(seconds=None):
    return time.strftime("%Y-%m-%dT%H:%M:%S.00Z", time.gmtime(seconds))


def get_uuid():
    return str(uuid.uuid4())


def sendBatchRequest(device, operation, value):
    data_dict = {
        "device": device,
        "operation": operation,
        "value": value
    }
    url = base_url + "/api/update"
    data = urllib.parse.urlencode(data_dict).encode()
    req = urllib.request.Request(url, data=data)
    try:
        urllib.request.urlopen(req)
    except urllib.error.HTTPError:
        pass
