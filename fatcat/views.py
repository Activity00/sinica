import json
from datetime import datetime

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


# ********************************** poll 相关接口 **********************************

COMMAND_QUEUE = []
COMMAND_RESULTS = []


def command_page(request):
    return render(request, 'fatcat/commands.html', {
        'commands': COMMAND_QUEUE,
        'results': reversed(COMMAND_RESULTS),
    })


@csrf_exempt
def add_command(request):
    if request.method == 'POST':
        data = json.loads(request.body)
        command = data.get('command')
        if command and command in ['photo', 'water']:
            COMMAND_QUEUE.append(command)
            return JsonResponse({'status': 'ok', 'message': f'Command "{command}" added.'})
    return JsonResponse({'status': 'error', 'message': 'Invalid command'})


@csrf_exempt
def get_commands(request):
    global COMMAND_QUEUE
    if request.GET.get('web'):
        return JsonResponse({'commands': COMMAND_QUEUE})

    cmds = COMMAND_QUEUE.copy()
    COMMAND_QUEUE = []  # 清空队列
    return JsonResponse({'commands': cmds})


@csrf_exempt
def get_results(request):
    # 返回全部结果或限定条数
    return JsonResponse({'results': list(reversed(COMMAND_RESULTS))})


@csrf_exempt
def report_result(request):
    if request.method == 'POST':
        try:
            data = json.loads(request.body)
        except json.JSONDecodeError:
            return JsonResponse({'status': 'error', 'message': 'Invalid JSON'})

        result = {
            'device_id': data.get('device_id'),
            'type': data.get('type'),
            'result': data.get('result'),
            'time': datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        }

        # 如果是拍照，还带有图片
        if data.get('type') == 'photo':
            result['photo_data'] = data.get('data')  # base64 string

        COMMAND_RESULTS.append(result)
        # 保持只保留最新10条记录
        if len(COMMAND_RESULTS) > 10:
            COMMAND_RESULTS.pop(0)  # 移除最旧的记录

        return JsonResponse({'status': 'ok'})

    return JsonResponse({'status': 'error'})
