from django.urls import path
from . import views

urlpatterns = [
    path('', views.index, name='index'),
    path('api/devices/', views.get_devices, name='api-cameras'),
    path('api/start_stream/', views.start_stream, name='api-start-stream'),
    path('api/stop_stream/', views.stop_stream, name='api-stop-stream'),
    path('api/take_photo/', views.take_photo, name='api-take-photo'),
    path('api/start_water/', views.start_water, name='api-start-water'),
    path('api/get_soil/', views.get_soil, name='api-get-soil'),

    path('commands/', views.command_page, name='command-page'),           # 页面视图
    path('api/add_command/', views.add_command, name='api-add-command'),  # 添加命令
    path('api/get_results/', views.get_results, name='api-get-results'),
    path('api/get_commands/', views.get_commands, name='api-get-commands'),  # 给 ESP32 的命令队列
    path('api/report_result/', views.report_result, name='api-report-result'),  # ESP32 执行后上报
]
