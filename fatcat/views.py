import json

from asgiref.sync import async_to_sync
from channels.layers import get_channel_layer

from django.http import JsonResponse
from django.shortcuts import render
from django.views.decorators.csrf import csrf_exempt

from fatcat.consumers import online_devices


def index(request):
    return render(request, 'fatcat/index.html', {})


def get_devices(request):
    return JsonResponse({"devices": [{'device_id': device_id} for device_id in list(online_devices.keys())]})


# 启动摄像头流
@csrf_exempt
def start_stream(request):
    if request.method != 'POST':
        return JsonResponse({"status": "error", "message": "仅支持POST请求"})

    data = json.loads(request.body)
    device_id = data.get('device_id')
    if not device_id:
        return JsonResponse({"status": "error", "message": "缺少device_id参数"})

    # 获取设备的 channel_name
    channel_name = online_devices.get(device_id)
    if not channel_name:
        return JsonResponse({"status": "error", "message": f"设备 {device_id} 不在线"})

    # 构建 WebSocket 消息
    command = {
        "type": "send_message",
        "text": '{"action": "start_stream"}',  # 可自定义 JSON 命令格式
    }

    try:
        channel_layer = get_channel_layer()
        async_to_sync(channel_layer.send)(channel_name, command)
        return JsonResponse({"status": "ok", "message": f"start_stream 命令已发送到 {device_id}"})
    except Exception as e:
        return JsonResponse({"status": "error", "message": str(e)})


# 停止摄像头流
@csrf_exempt
def stop_stream(request):
    if request.method != 'POST':
        return JsonResponse({"status": "error", "message": "仅支持POST请求"})

    data = json.loads(request.body)
    device_id = data.get('device_id')
    if not device_id:
        return JsonResponse({"status": "error", "message": "缺少device_id参数"})

    # 获取设备的 channel_name
    channel_name = online_devices.get(device_id)
    if not channel_name:
        return JsonResponse({"status": "error", "message": f"设备 {device_id} 不在线"})

    # 构建 WebSocket 消息
    command = {
        "type": "send_message",
        "text": '{"action": "stop_stream"}',  # 可自定义 JSON 命令格式
    }

    try:
        channel_layer = get_channel_layer()
        async_to_sync(channel_layer.send)(channel_name, command)
        return JsonResponse({"status": "ok", "message": f"stop_stream 命令已发送到 {device_id}"})
    except Exception as e:
        return JsonResponse({"status": "error", "message": str(e)})


@csrf_exempt
def take_photo(request):
    if request.method != 'POST':
        return JsonResponse({"status": "error", "message": "仅支持POST请求"})

    data = json.loads(request.body)
    device_id = data.get('device_id')
    if not device_id:
        return JsonResponse({"status": "error", "message": "缺少device_id参数"})

    # 获取设备的 channel_name
    channel_name = online_devices.get(device_id)
    if not channel_name:
        return JsonResponse({"status": "error", "message": f"设备 {device_id} 不在线"})

    # 构建 WebSocket 消息
    command = {
        "type": "send_message",
        "text": '{"action": "take_photo"}',  # 可自定义 JSON 命令格式
    }

    try:
        channel_layer = get_channel_layer()
        async_to_sync(channel_layer.send)(channel_name, command)
        return JsonResponse({"status": "ok", "message": f"拍照命令已发送到 {device_id}"})
    except Exception as e:
        return JsonResponse({"status": "error", "message": str(e)})


@csrf_exempt
def start_water(request):
    if request.method != 'POST':
        return JsonResponse({"status": "error", "message": "仅支持POST请求"})

    data = json.loads(request.body)
    device_id = data.get('device_id')
    if not device_id:
        return JsonResponse({"status": "error", "message": "缺少device_id参数"})

    # 获取设备的 channel_name
    channel_name = online_devices.get(device_id)
    if not channel_name:
        return JsonResponse({"status": "error", "message": f"设备 {device_id} 不在线"})

    # 构建 WebSocket 消息
    command = {
        "type": "send_message",
        "text": '{"action": "start_water", "ts": 15}',
    }

    try:
        channel_layer = get_channel_layer()
        async_to_sync(channel_layer.send)(channel_name, command)
        return JsonResponse({"status": "ok", "message": f"浇水命令已发送到 {device_id}"})
    except Exception as e:
        return JsonResponse({"status": "error", "message": str(e)})


@csrf_exempt
def get_soil(request):
    if request.method != 'POST':
        return JsonResponse({"status": "error", "message": "仅支持POST请求"})

    data = json.loads(request.body)
    device_id = data.get('device_id')
    if not device_id:
        return JsonResponse({"status": "error", "message": "缺少device_id参数"})

    # 获取设备的 channel_name
    channel_name = online_devices.get(device_id)
    if not channel_name:
        return JsonResponse({"status": "error", "message": f"设备 {device_id} 不在线"})

    # 构建 WebSocket 消息
    command = {
        "type": "send_message",
        "text": '{"action": "soil_data"}',
    }

    try:
        channel_layer = get_channel_layer()
        async_to_sync(channel_layer.send)(channel_name, command)
        return JsonResponse({"status": "ok", "message": f"获取土壤湿度命令已发送到 {device_id}"})
    except Exception as e:
        return JsonResponse({"status": "error", "message": str(e)})
