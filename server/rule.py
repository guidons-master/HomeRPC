from .log import Log
import asyncio
import json
import struct
from amqtt.client import MQTTClient
from amqtt.mqtt.constants import QOS_1, QOS_2
from multiprocessing import Queue

class Rule:
    def __init__(self, loop: asyncio.AbstractEventLoop = None, \
                 recv: Queue = None, send: Queue = None, topic: Queue = None, log: Log = True):
        self.log = Log(disable = not log)
        self.loop = loop or asyncio.get_event_loop()
        (self.recv, self.send) = (recv, send)
        self.topic = topic

    async def _subscribe(self):
        self.client = MQTTClient(
            client_id = "self",
            config = {
                "keep_alive": 60,
                "reconnect_max_interval": 5,
                "reconnect_retries": 5,
                "ping_delay": 2,
            })
        try:
            await self.client.connect('ws://127.0.0.1:3000/')
            await self.client.subscribe([
                ('/topics/register', QOS_1),
                ('/callback/master', QOS_1),
            ])
        except Exception as ce:
            self.log.log_error(ce)
    
    async def _listen_mqtt(self):
        while True:
            message = await self.client.deliver_message()
            packet = message.publish_packet
            try:
                data = packet.payload.data.decode('utf-8')
                json_data = json.loads(data)
                self.log.log(json_data)
                if (packet.variable_header.topic_name == "/topics/register"):
                    self.loop.run_in_executor(None, self.topic.put, json_data)
                elif (packet.variable_header.topic_name == "/callback/master"):
                    self.loop.run_in_executor(None, self.recv.put, data)
            except Exception as e:
                self.log.log_error(e)
    
    async def _listen_queue(self):
        while True:
            data = await self.loop.run_in_executor(None, self.send.get)
            data = json.loads(data)
            self.log.log(data)
            await self._call(data['topic'], bytearray(data['message'], 'utf-8'))
    
    async def _call(self, topic, message):
        await self.client.subscribe([
            (topic, QOS_1),
        ])
        await self.client.publish(topic, message, qos = QOS_1)

    def listen(self):
        self.loop.run_until_complete(self._subscribe())
        self.loop.create_task(self._listen_mqtt())
        self.loop.create_task(self._listen_queue())