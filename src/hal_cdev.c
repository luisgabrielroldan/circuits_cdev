#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/gpio.h>

#include "cdev_nif.h"

int hal_open(struct gpio_chip *chip, const char *chip_path, char *error_str) {

    *error_str = '\0';

    chip->fd = open(chip_path, O_RDWR);

    if (chip->fd < 0) {
        strcpy(error_str, "access_denied");
        return -1;
    }

    return 0;
}

void hal_chip_close(struct gpio_chip *chip) {
    close(chip->fd);
    chip->fd = -1;
}

void hal_line_close(struct gpiohandle_request *req) {
    close(req->fd);
}

void hal_event_request_close(struct gpioevent_request *event_req) {
    close(event_req->fd);
}
