from .log import Log
from .broker import MqttBroker
from .rule import Rule
from multiprocessing import Process, Queue
from json import dumps, loads
from struct import unpack, pack
from typing import List
import asyncio

_topic_registry = {}
_recv, _send = Queue(), Queue()
_topic = Queue()
_rpc_log = None

def dict2funcs() -> List[dict]:
    global _topic_registry
    data = _topic_registry

    functions = []

    type_signature_mapping = {
        'c': {'type': 'string', 'description': 'a character'},
        'i': {'type': 'integer', 'description': 'a number'},
        'f': {'type': 'number', 'description': 'a single-precision floating-point number'},
        'd': {'type': 'number', 'description': 'a double-precision floating-point number'},
    }

    for place, devices in data.items():
        for device_type, device_list in devices.items():
            for device in device_list:
                for service in device['services']:
                    function = {
                        'name': service['name'],
                        'description': service['desc'],
                        'parameters': {
                            'type': 'object',
                            'properties': {
                                'place': {
                                    'type': 'string',
                                    'description': 'where the device is located',
                                    'enum': [place]
                                },
                                'device_type': {
                                    'type': 'string',
                                    'description': 'what type of device',
                                    'enum': [device_type]
                                },
                                'device_id': {
                                    'type': 'integer',
                                    'description': 'the id of the device',
                                    'enum': [device['id']]
                                }
                            },
                            'required': ['place', 'device_type', 'device_id']
                        }
                    }
                    if service['input_type']:
                        function['parameters']['properties']['input_type'] = type_signature_mapping[service['input_type']]
                        function['parameters']['required'].append('input_type')
                    functions.append(function)
    return functions

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
            _rpc_log.log(f'Registered device: {_topic_registry}')

def deserialize_rpc_any(buffer: str, data_type: str) -> any:
        res = bytes.fromhex(buffer.ljust(16, '0'))

        if data_type == 'f':
            return unpack('f', res[:4])[0]
        elif data_type == 'i':
            return unpack('q', res[:8])[0]
        elif data_type == 'd':
            return unpack('d', res[:8])[0]
        elif data_type == 'c':
            ascii_value = unpack('B', res[:1])[0]
            return chr(ascii_value)
        else:
            raise ValueError('Unsupported data type: ' + data_type)

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

def _asyncio_loop(ip: str, log: bool, recv: Queue, send: Queue, topic: Queue):
    loop = asyncio.get_event_loop()
    rule_engine = Rule(loop, recv, send, topic, log)

    class Server(MqttBroker):
        def __init__(self, ip, log):
            super().__init__(loop, ip, log)

        def _init_task(self):
            rule_engine.listen()

    server = Server(ip, log)
    server.loop_forever()

class HomeRPC():
    @staticmethod
    def setup(ip: str = None, log: bool = True):
        global _rpc_log
        _rpc_log = Log(disable = not log)
        Process(target = _asyncio_loop, args = (ip, log, _recv, _send, _topic), daemon = True).start()

    @staticmethod
    def place(name: str):

        class Place:
            def device(self, device_name: str):
                
                class Device:
                    def id(self, device_id: int):

                        class Id:
                            def call(self, func: str, *args, **kwargs):
                                update_registry()
                                timeout = kwargs.get("timeout_s", 10) * 1000
                            
                                if name in _topic_registry and device_name in _topic_registry[name]:
                                    for device_info in _topic_registry[name][device_name]:
                                            if device_info['id'] != device_id:
                                                continue

                                            for service in device_info['services']:
                                                if service['name'] != func:
                                                    continue
                                                
                                                if len(args) != len(service['input_type']):
                                                    _rpc_log.log_error(f'Function {func} requires {len(service["input_type"])} arguments, but {len(args)} were given')
                                                    return None

                                                message = {
                                                    "callback": "/callback/master",
                                                }
                                                if len(args):
                                                    message["params"] = serialize_rpc_any(args, service['input_type'])
                                                
                                                _send.put(dumps({
                                                    "topic": f"/{name}/{device_name}/{device_id}/{func}",
                                                    "message": dumps(message)
                                                }))

                                                try:
                                                    data = loads(_recv.get(timeout = timeout))
                                                    return deserialize_rpc_any(data['result'], service['output_type'])
                                                except Exception as e:
                                                    _rpc_log.log_error(e)
                                                    return None
                            
                                _rpc_log.log_error(f'Device {name}/{device_name}/{device_id} does not exist')
                                return None
                        return Id()
                return Device()
        return Place()

    @staticmethod
    def funcs():
        update_registry()
        return dict2funcs()