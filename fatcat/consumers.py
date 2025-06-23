import base64
import json
import logging

from urllib.parse import parse_qs

from channels.generic.websocket import AsyncWebsocketConsumer

logger = logging.getLogger(__name__)


class DeviceConsumer(AsyncWebsocketConsumer):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.group_name = 'monitor'

    async def connect(self):
        await self.channel_layer.group_add(self.group_name, self.channel_name)
        await self.accept()

        logger.info(f"web连接: {self.channel_name}")

    async def disconnect(self, close_code):
        await self.channel_layer.group_discard(self.group_name, self.channel_name)
        logger.info(f"web断开: {self.channel_name}")

    async def receive(self, text_data=None, bytes_data=None):
        if not text_data:
            logger.warning("接收到空的消息")
            return

        try:
            data = json.loads(text_data)
            logger.info(f"来自 {self.channel_name} 的消息: {data}")
        except json.JSONDecodeError:
            logger.warning(f"无法解析JSON: {text_data}")

    async def device_frame(self, event):
        """广播视频帧到监控组"""
        await self.send(text_data=json.dumps({
            'type': 'frame',
            'device_id': event['device_id'],
            'frame': event['frame'],
        }))

    async def device_soil(self, event):
        """广播土壤数据到监控组"""
        await self.send(text_data=json.dumps({
            'type': 'soil',
            'device_id': event['device_id'],
            'soil_data': event['soil_data'],
        }))


online_devices = {}


class Esp32CamConsumer(AsyncWebsocketConsumer):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.device_id = None

    async def connect(self):
        # 解析设备ID（客户端WebSocket连接时需带上 ?device_id=xxx）
        query_params = parse_qs(self.scope["query_string"].decode())
        self.device_id = query_params.get("device_id", [None])[0]

        if not self.device_id:
            # 如果没有传 device_id，拒绝连接
            logger.warning("Connection rejected: device_id is required")
            await self.close()
            return

        # 注册设备到在线列表
        online_devices[self.device_id] = self.channel_name

        logger.info(f"ESP32Cam connected: device_id={self.device_id}, channel_name={self.channel_name}")
        logger.info(f"Online devices: {online_devices}")
        # 将连接加入组，方便广播
        await self.channel_layer.group_add("esp32_cam", self.channel_name)
        await self.accept()

    async def disconnect(self, close_code):
        current_channel = getattr(self, "channel_name", None)
        current_device_id = getattr(self, "device_id", None)

        logger.info(f"ESP32Cam disconnected: device_id={current_device_id}, channel_name={current_channel}")
        logger.info(f"Online devices before disconnect: {dict(online_devices)}")

        if current_device_id in online_devices:
            # 只有当前 channel_name 是注册的那个，才清除
            if online_devices[current_device_id] == current_channel:
                online_devices.pop(current_device_id)
                logger.info(f"Removed device_id={current_device_id} from online_devices")
            else:
                logger.info(f"Not removing device_id={current_device_id}: still connected via a newer channel")

        await self.channel_layer.group_discard("esp32_cam", self.channel_name)

        logger.info(f"Online devices after disconnect: {dict(online_devices)}")

    async def receive(self, text_data=None, bytes_data=None):
        if text_data:
            logger.info(f"Received text data from {self.device_id}: {text_data}")
            # 你可以在这里解析消息，比如控制命令，或者转发给其它设备
            try:
                data = json.loads(text_data)
                # 处理接收到的 JSON 数据
                logger.info(f"Parsed data from {self.device_id}: {data}")
            except json.JSONDecodeError:
                logger.warning(f"Invalid JSON received from {self.device_id}: {text_data}")
                return
            await self.channel_layer.group_send(
                "monitor",
                {
                    "type": "device_soil",  # 这里匹配的是 DeviceConsumer 中的 device_frame
                    "device_id": self.device_id,
                    "soil_data": data['data'],
                }
            )

        # 如果是二进制数据，转发到监控组
        if bytes_data:
            logger.info(f"Received binary data of length {len(bytes_data)} bytes from {self.device_id}")

            data_str = base64.b64encode(bytes_data).decode('utf-8')
            # 转发帧到监控组
            await self.channel_layer.group_send(
                "monitor",
                {
                    "type": "device_frame",  # 这里匹配的是 DeviceConsumer 中的 device_frame
                    "device_id": self.device_id,
                    "frame": data_str,
                }
            )

    async def send_message(self, event):
        await self.send(text_data=event['text'])
