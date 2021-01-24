#!/usr/bin/env python
 
# ServicePython 
# is part of the Microservice demo MSBasics
# This python program implements a WebSocket server that:
# - receives timestamp and counter value from ServiceC
# - calculates the moving average over the last max 10 events
# - delivers the result to ApiGateway via REST POST

import asyncio
import websockets
import atexit
import json
import requests

summary = []

async def handler(websocket, path):
    data_in = await websocket.recv()
    jdata = json.loads(data_in)
    tmp = jdata["data"]["counter"]
    if isinstance(tmp, int):
        summary.append(tmp)
    else:
        try:
            summary.append(int(tmp))
        except ValueError:
            # ignore any invalid counter value
            pass
    if len(summary) > 10:
        summary.remove(summary[0])
    tmp = f"{(sum(summary) / float(len(summary))):.2f}"
    try:
        resp = requests.post('http://apigateway:8080/summary', json={"data": {"summary" : tmp }})
        if resp.status_code != 200:
            print("(MSBasics-ServicePython) Error delivering summary")
    except:
        print("(MSBasics-ServicePython) Error connecting to apigateway")

def gracefulexit():
    print("(MSBasics-ServicePython) service terminated")
    loop.stop()
    start_server.ws_server.close()

start_server = websockets.serve(handler, "0.0.0.0", 8083)
atexit.register(gracefulexit)
loop = asyncio.get_event_loop()
loop.run_until_complete(start_server)
print("(MSBasics-ServicePython) service started")
try:
    loop.run_forever()
except KeyboardInterrupt:
    loop.stop()
    start_server.ws_server.close()
