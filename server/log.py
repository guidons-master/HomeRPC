import logging
import colorlog
import traceback

class Log:
    def __init__(self, log_file=None, log_level=logging.INFO):
        log_format = (
            "%(log_color)s[%(asctime)s] :: %(levelname)s :: %(name)s :: %(message)s%(reset)s"
        )
        colorlog.basicConfig(
            filename=log_file,
            level=log_level,
            format=log_format,
            log_colors={
                "DEBUG": "green",
                "INFO": "cyan",
                "WARNING": "yellow",
                "ERROR": "red",
                "CRITICAL": "red,bg_white",
            },
        )

    def log(self, message):
        logging.info(message)

    def log_error(self, message):
        error_info = traceback.format_exc()
        logging.error(f"{message} => {error_info}")