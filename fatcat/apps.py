from django.apps import AppConfig


class FatcatConfig(AppConfig):
    default_auto_field = 'django.db.models.BigAutoField'
    name = 'fatcat'

    def ready(self):
        pass
