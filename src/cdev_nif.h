#ifndef CDEV_NIF_H
#define CDEV_NIF_H

#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/gpio.h>

#include "erl_nif.h"

struct cdev_priv
{
    ERL_NIF_TERM atom_ok;
    ErlNifResourceType *gpio_chip_rt;
    ErlNifResourceType *gpiohandle_request_rt;
    ErlNifResourceType *gpioevent_request_rt;
    ErlNifResourceType *gpioevent_data_rt;
};

struct gpio_chip
{
    int fd;
};


int hal_open(struct gpio_chip *chip, const char *chip_path, char *error_str);
void hal_chip_close(struct gpio_chip *chip);
void hal_line_close(struct gpiohandle_request *req);
void hal_event_request_close(struct gpioevent_request *event_req);

ERL_NIF_TERM make_error_tuple(ErlNifEnv *env, const char *reason);
ERL_NIF_TERM make_ok_tuple(ErlNifEnv *env, ERL_NIF_TERM value);

#endif // CDEV_NIF_H
