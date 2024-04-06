from .log import Log
from .broker import MqttBroker
from .rule import Rule
from inspect import signature
from multiprocessing import Process, Queue
from json import dumps
from struct import unpack, pack
from typing import List
import asyncio

_func_registry = {}
_rule_registry = {}
_topic_registry = {}
log = Log()
_recv, _send = Queue(), Queue()
_topic = Queue()

def _wrapper(func: callable, interval_ms: int) -> asyncio.coroutine:
    async def _():
        while True:
            try:
                await func()
                if interval_ms <= 0:
                    break
            except Exception as e:
                log.log_error(e)

            await asyncio.sleep(interval_ms / 1000)
    return _()

def register_device(device: dict):
    place = device['place']
    type_ = device['type']
    id_ = device['id']
    services = device['services']

    if place not in _topic_registry:
        _topic_registry[place] = {}

    if type_ not in _topic_registry[place]:
        _topic_registry[place][type_] = []

    _topic_registry[place][type_].append({'id': id_, 'services': services})

def update_registry():
    while True:
        try:
            topic = dict(_topic.get_nowait())
        except Exception:
            break
        else:
            register_device(topic)
            print(_topic_registry)

def deserialize_rpc_any(buffer: List[str], data_type: str) -> List[any]:
    res = []

    for data, _type in zip(buffer, data_type):
        data = bytes.fromhex(data.ljust(16, '0'))

        if _type == 'f':
            res.append(unpack('f', data[:4])[0])
        elif _type == 'i':
            res.append(unpack('q', data[:8])[0])
        elif _type == 'd':
            res.append(unpack('d', data[:8])[0])
        elif _type == 'c':
            ascii_value = unpack('B', data[:1])[0]
            res.append(chr(ascii_value))
        else:
            raise ValueError('Unsupported data type: ' + _type)
    
    return res

def serialize_rpc_any(data_list: List[any], data_type: str) -> List[str]:
    res = []

    for data, _type in zip(data_list, data_type):
        if _type == 'f':
            res.append(pack('f', data).hex().ljust(8, '0'))
        elif _type == 'i':
            res.append(pack('q', data).hex().ljust(16, '0'))
        elif _type == 'd':
            res.append(pack('d', data).hex().ljust(16, '0'))
        elif _type == 'c':
            res.append(pack('B', ord(data)).hex().ljust(2, '0'))
        else:
            raise ValueError('Unsupported data type: ' + _type)
    
    return res

def _asyncio_loop(port: int, log: bool, recv: Queue, send: Queue, topic: Queue):
    class Server(MqttBroker):
        def __init__(self, port: int = 3000 , log: bool = True):
            super().__init__(loop = loop, port = port, log = log)

        def _init_task(self):
            rule_engine.listen()
            for rule in _rule_registry:
                loop.create_task(_rule_registry[rule])

    loop = asyncio.get_event_loop()
    rule_engine = Rule(loop, recv, send, topic)
    server = Server(port, log)
    server.loop_forever()

class HomeRPC():
    @staticmethod
    def setup(port: int = 3000, log: bool = True):
        Process(target = _asyncio_loop, args = (port, log, _recv, _send, _topic), daemon = True).start()

    @staticmethod
    def addFunc(allow_overwrite: bool = False):
        def decorator(func):
            if func.__name__ in _func_registry:
                if allow_overwrite:
                    log.log(f'Function `{func.__name__}` already exists! Overwriting...')
                else:
                    raise ValueError(
                        f'Function `{func.__name__}` already exists! Use `allow_overwrite=True` to overwrite.'
                    )
            _func_registry[func.__name__] = {
                'func': func,
                'signature': signature(func)
            }
            return func
        return decorator
    
    @staticmethod
    def addRule(desc: str, interval_ms: int = 1000, allow_overwrite: bool = False):
        def decorator(func):
            if desc in _rule_registry:
                if allow_overwrite:
                    log.log(f'Rule `{desc}` already exists! Overwriting...')
                else:
                    raise ValueError(
                        f'Rule `{desc}` already exists! Use `allow_overwrite=True` to overwrite.'
                    )
            _rule_registry[desc] = _wrapper(func, interval_ms)
            return func
        return decorator

    @staticmethod
    def place(name: str):

        class Place:
            def device(self, device_name: str):
                
                class Device:
                    def id(self, device_id: int):

                        class Id:
                            def call(self, func: str, *args, **kwargs):
                                update_registry()
                                timeout = kwargs.get("timeout", 10)
                            
                                if name in _topic_registry and device_name in _topic_registry[name]:
                                    for device_info in _topic_registry[name][device_name]:
                                            if device_info['id'] != device_id:
                                                continue

                                            for service in device_info['services']:
                                                if service['name'] != func:
                                                    continue

                                                _send.put(dumps({
                                                    "topic": f"/{name}/{device_name}/{device_id}/{func}",
                                                    "message": dumps({
                                                        "callback": "/callback/master",
                                                        "params": serialize_rpc_any(args, service['input_type'])
                                                    })
                                                }))

                                                try:
                                                    data = _recv.get(timeout = timeout)
                                                    return deserialize_rpc_any(data['params'], service['output_type'])[0]
                                                except Exception as e:
                                                    log.log_error(e)
                                                    return None
                            
                                log.log_error(f'Device {name}/{device_name}/{device_id} does not exist')
                                return None
                        return Id()
                return Device()
        return Place()