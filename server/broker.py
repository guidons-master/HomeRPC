import asyncio
from amqtt.broker import Broker
from zeroconf import ServiceInfo, Zeroconf, IPVersion
from .log import Log
from socket import inet_aton

class MqttBroker:
    def __init__(self, loop: asyncio.AbstractEventLoop = None, ip: str = None , log: bool = True):
        self.zeroconf = Zeroconf(ip_version = IPVersion.All)
        self.info = ServiceInfo(
            "_mqtt._tcp.local.",
            "broker._mqtt._tcp.local.",
            addresses = [inet_aton(ip),],
            port = 3000,
            properties = { "name": "MQTT Broker", "path": "/mqtt" },
            server = "homerpc.local.",
        )

        self.log = Log(disable = not log)

        self.loop = loop or asyncio.get_event_loop()

    async def _broker_coro(self):
        broker = Broker(config = {
            "listeners": {
                "default": {
                    "type": "ws",
                    "bind": "0.0.0.0:%d" % self.info.port,
                    "max_connections": 0
                }
            },
            "sys_interval": 10,
            "auth": {
                "allow-anonymous": True,
                "plugins": ['auth.anonymous']
            },
            "topic-check": {
                "enabled": False
            }
        })
        await broker.start()

    def _register_service(self):
        self.zeroconf.register_service(self.info)

    def _init_task(self):
        raise NotImplementedError

    def loop_forever(self):
        self._register_service()

        try:
            self.loop.run_until_complete(self._broker_coro())
            self._init_task()
            self.loop.run_forever()
        except Exception as e:
            self.loop.stop()
            self.log.log_error(e)