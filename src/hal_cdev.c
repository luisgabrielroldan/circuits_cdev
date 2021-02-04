#include <sys/ioctl.h>

#include "cdev_nif.h"

int hal_chip_open(struct gpio_chip *chip, const char *chip_path, char *error_str)
{

    *error_str = '\0';

    chip->fd = open(chip_path, O_RDWR);

    if (chip->fd < 0) {
        strcpy(error_str, "access_denied");
        return -1;
    }

    return 0;
}

void hal_chip_close(struct gpio_chip *chip)
{
    close(chip->fd);
    chip->fd = -1;
}

void hal_line_close(struct gpiohandle_request *req)
{
    close(req->fd);
}

void hal_event_request_close(struct gpioevent_request *event_req)
{
    close(event_req->fd);
}

ERL_NIF_TERM hal_getinfo(ErlNifEnv *env, struct gpio_chip *chip)
{
    struct gpiochip_info info;

    if (ioctl(chip->fd, GPIO_GET_CHIPINFO_IOCTL, &info) < 0) {
        return make_error_tuple(env, "get_chipinfo");
    }

    ERL_NIF_TERM chip_name = enif_make_string(env, info.name, ERL_NIF_LATIN1);
    ERL_NIF_TERM chip_label = enif_make_string(env, info.label, ERL_NIF_LATIN1);
    ERL_NIF_TERM number_lines = enif_make_int(env, (int)info.lines);

    return enif_make_tuple3(env, chip_name, chip_label, number_lines);
}

ERL_NIF_TERM hal_get_lineinfo(ErlNifEnv *env, struct gpio_chip *chip, int offset)
{
    struct gpioline_info info;

    info.line_offset = offset;

    if (ioctl(chip->fd, GPIO_GET_LINEINFO_IOCTL, &info) < 0) {
        return make_error_tuple(env, "get_lineinfo");
    }

    ERL_NIF_TERM flags = enif_make_int(env, info.flags);
    ERL_NIF_TERM name = enif_make_string(env, info.name, ERL_NIF_LATIN1);
    ERL_NIF_TERM consumer = enif_make_string(env, info.consumer, ERL_NIF_LATIN1);

    return enif_make_tuple3(env, flags, name, consumer);
}
