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
]
