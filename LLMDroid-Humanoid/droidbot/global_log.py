import logging
import logging.config


class ColorFormatter(logging.Formatter):
    # Define ansi color codes
    class Colors:

        RESET = '\033[0m'
        RED = '\033[31m'
        GREEN = '\033[32m'
        YELLOW = '\033[33m'
        BLUE = '\033[34m'
        MAGENTA = '\033[35m'    # purple
        CYAN = '\033[36m'   # cyan

    # Mapping log levels to colors
    LOG_LEVEL_COLORS = {
        logging.DEBUG: Colors.MAGENTA,
        logging.INFO: Colors.BLUE,
        logging.WARNING: Colors.YELLOW,
        logging.ERROR: Colors.RED,
        logging.CRITICAL: Colors.MAGENTA
    }

    def format(self, record):
        # Get the color of the log level
        level_color = self.LOG_LEVEL_COLORS.get(record.levelno, self.Colors.RESET)

        # Format timestamp in green
        timestamp = f"{self.Colors.GREEN}{self.formatTime(record, self.datefmt)}{self.Colors.RESET}"

        # Format log level
        levelname = f"{level_color}{record.levelname:8}{self.Colors.RESET}"

        # Format calling modules and functions in cyan
        module_func = f"{self.Colors.CYAN}{record.module}:{record.funcName}:{record.lineno}{self.Colors.RESET}"

        # Format information section, the color is the same as the log level
        message = f"{level_color}{record.getMessage()}{self.Colors.RESET}"

        # Combine all parts
        return f"{timestamp} | {levelname} | {module_func} - {message}"


def get_logger(name='droidbot'):
    # logging.config.fileConfig("logging.conf")
    return logging.getLogger(name)
