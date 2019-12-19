#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/gpio.h>
#include <string.h>
#include <unistd.h>

#include <errno.h>
#include <stdio.h>

#include "erl_nif.h"

struct cdev_priv {
    ErlNifResourceType *gpio_chip_rt;
    ErlNifResourceType *gpiohandle_request_rt;
};

struct gpio_chip {
    int fd;
};

static void gpio_chip_dtor(ErlNifEnv *env, void *obj)
{
    struct gpio_chip *chip = (struct gpio_chip*) obj;

    close(chip->fd);
}

static void linehandle_request_dtor(ErlNifEnv *env, void *obj)
{
    struct gpiohandle_request *req = (struct gpiohandle_request*) obj;

    close(req->fd);
}

static int load(ErlNifEnv *env, void **priv_data, const ERL_NIF_TERM info)
{
    (void) info;
    struct cdev_priv *priv = enif_alloc(sizeof(struct cdev_priv));

    if (!priv) {
        return 1;
    }

    priv->gpio_chip_rt = enif_open_resource_type(env, NULL, "gpio_chip", gpio_chip_dtor, ERL_NIF_RT_CREATE, NULL);
    priv->gpiohandle_request_rt = enif_open_resource_type(env, NULL, "gpiohandle_request", linehandle_request_dtor, ERL_NIF_RT_CREATE, NULL);

    if (priv->gpio_chip_rt == NULL) {
        return 2;
    }

    *priv_data = (void *) priv;

    return 0;
}

static ERL_NIF_TERM open_chip(ErlNifEnv *env, int argc, const ERL_NIF_TERM argv[])
{
    struct cdev_priv *priv = enif_priv_data(env);
    char chip_path[16];
    memset(&chip_path, '\0', sizeof(chip_path));
    if (!enif_get_string(env, argv[0], chip_path, sizeof(chip_path), ERL_NIF_LATIN1)) {
        return enif_make_badarg(env);
    }

    struct gpio_chip *chip = enif_alloc_resource(priv->gpio_chip_rt, sizeof(struct gpio_chip));

    chip->fd = open(chip_path, O_RDWR);

    ERL_NIF_TERM chip_resource = enif_make_resource(env, chip);
    enif_release_resource(chip);

    ERL_NIF_TERM ok_atom = enif_make_atom(env, "ok");

    return enif_make_tuple2(env, ok_atom, chip_resource);
}

static ERL_NIF_TERM get_info_nif(ErlNifEnv *env, int argc, const ERL_NIF_TERM argv[])
{
    struct cdev_priv *priv = enif_priv_data(env);
    struct gpio_chip *chip;
    struct gpiochip_info info;

    if (argc != 1 || !enif_get_resource(env, argv[0], priv->gpio_chip_rt, (void **) &chip))
        return enif_make_badarg(env);

    int rv = ioctl(chip->fd, GPIO_GET_CHIPINFO_IOCTL, &info);

    if (rv < 0) {
        return enif_make_atom(env, "error");
    }

    ERL_NIF_TERM chip_name = enif_make_string(env, info.name, ERL_NIF_LATIN1);
    ERL_NIF_TERM chip_label = enif_make_string(env, info.label, ERL_NIF_LATIN1);
    ERL_NIF_TERM number_lines = enif_make_int(env, (int) info.lines);

    return enif_make_tuple3(env, chip_name, chip_label, number_lines);
}

static ERL_NIF_TERM close_chip_nif(ErlNifEnv *env, int argc, const ERL_NIF_TERM argv[])
{
    struct cdev_priv *priv = enif_priv_data(env);
    struct gpio_chip *chip;

    if (argc != 1 || !enif_get_resource(env, argv[0], priv->gpio_chip_rt, (void **) &chip))
        return enif_make_badarg(env);

    close(chip->fd);
    chip->fd = -1;

    return enif_make_atom(env, "ok");
}

static ERL_NIF_TERM get_line_info_nif(ErlNifEnv *env, int argc, const ERL_NIF_TERM argv[])
{
    struct cdev_priv *priv = enif_priv_data(env);
    struct gpio_chip *chip;
    struct gpioline_info info;
    int rv, offset;

    if (argc != 2 || !enif_get_resource(env, argv[0], priv->gpio_chip_rt, (void **) &chip) || !enif_get_int(env, argv[1], &offset))
        return enif_make_badarg(env);

    info.line_offset = offset;

    rv = ioctl(chip->fd, GPIO_GET_LINEINFO_IOCTL, &info);

    ERL_NIF_TERM flags = enif_make_int(env, info.flags);
    ERL_NIF_TERM name = enif_make_string(env, info.name, ERL_NIF_LATIN1);
    ERL_NIF_TERM consumer = enif_make_string(env, info.consumer, ERL_NIF_LATIN1);

    return enif_make_tuple3(env, flags, name, consumer);
}

/**
 * Will update to do many offsets, just going this route while doing
 * initial development
 */
static ERL_NIF_TERM request_linehandle_nif(ErlNifEnv *env, int argc, const ERL_NIF_TERM argv[])
{
    struct cdev_priv *priv = enif_priv_data(env);
    struct gpio_chip *chip;
    char consumer[32];
    int rv, lineoffset, flags, default_value;

    if (argc != 5
            || !enif_get_resource(env, argv[0], priv->gpio_chip_rt, (void **) &chip)
            || !enif_get_int(env, argv[1], &lineoffset)
            || !enif_get_int(env, argv[2], &default_value)
            || !enif_get_int(env, argv[3], &flags)
            || !enif_get_string(env, argv[4], consumer, sizeof(consumer), ERL_NIF_LATIN1))
        return enif_make_badarg(env);

    struct gpiohandle_request *req = enif_alloc_resource(priv->gpiohandle_request_rt, sizeof(struct gpiohandle_request));

    memset(req, 0, sizeof(struct gpiohandle_request));

    req->flags = flags;
    req->lines = 1;
    req->lineoffsets[0] = lineoffset;
    req->default_values[0] = default_value;
    strncpy(req->consumer_label, consumer, sizeof(req->consumer_label) - 1);

    rv = ioctl(chip->fd, GPIO_GET_LINEHANDLE_IOCTL, req);

    if (rv < 0)
        return enif_make_atom(env, "error"); // make better

    ERL_NIF_TERM linehandle_resource = enif_make_resource(env, req);
    enif_release_resource(req);

    ERL_NIF_TERM ok_atom = enif_make_atom(env, "ok");

    return enif_make_tuple2(env, ok_atom, linehandle_resource);
}

static ERL_NIF_TERM set_value_nif(ErlNifEnv *env, int argc, const ERL_NIF_TERM argv[])
{
    struct cdev_priv *priv = enif_priv_data(env);
    struct gpiohandle_request *req;
    struct gpiohandle_data data;
    int rv, new_value;

    if (argc != 2 || !enif_get_resource(env, argv[0], priv->gpiohandle_request_rt, (void **) &req) || !enif_get_int(env, argv[1], &new_value))
        return enif_make_badarg(env);

    data.values[0] = new_value;

    rv = ioctl(req->fd, GPIOHANDLE_SET_LINE_VALUES_IOCTL, &data);

    if (rv < 0)
        return enif_make_atom(env, "error"); // make better

    return enif_make_atom(env, "ok");
}

static ERL_NIF_TERM get_value_nif(ErlNifEnv *env, int argc, const ERL_NIF_TERM argv[])
{
    struct cdev_priv *priv = enif_priv_data(env);
    struct gpiohandle_request *req;
    struct gpiohandle_data data;
    int rv;

    if (argc != 1 || !enif_get_resource(env, argv[0], priv->gpiohandle_request_rt, (void **) &req))
        return enif_make_badarg(env);

    memset(&data, 0, sizeof(data));

    rv = ioctl(req->fd, GPIOHANDLE_GET_LINE_VALUES_IOCTL, &data);

    if (rv < 0)
        return enif_make_atom(env, "error"); // make better

    ERL_NIF_TERM value = enif_make_int(env, data.values[0]);
    ERL_NIF_TERM ok_atom = enif_make_atom(env, "ok");

    return enif_make_tuple2(env, ok_atom, value);
}

static ERL_NIF_TERM request_linehandle_multi_nif(ErlNifEnv *env, int argc, const ERL_NIF_TERM argv[])
{
    struct cdev_priv *priv = enif_priv_data(env);
    struct gpio_chip *chip;
    char consumer[32];
    int rv, flags, offset_list_len;

    if (!argc != 5)
        return enif_make_badarg(env);

    if (!enif_get_list_length(env, argv[1], &offset_list_len))
        return enif_make_atom(env, "bad_offset_list");

    if (!enif_get_int(env, argv[3], &flags))
        return enif_make_atom(env, "bad_flags");

    struct gpiohandle_request *req = enif_alloc_resource(priv->gpiohandle_request_rt, sizeof(struct gpiohandle_request));

    memset(req, 0, sizeof(struct gpiohandle_request));

    req->flags = flags;
    req->lines = offset_list_len;
    strncpy(req->consumer_label, consumer, sizeof(req->consumer_label) - 1);

    for (int i = 0; i < offset_list_len; i++) {
        ERL_NIF_TERM head;
        ERL_NIF_TERM tail;
        int offset;

        if (!enif_get_list_cell(env, argv[1], &head, &tail))
            return enif_make_atom(env, "offset_list");

        if (!enif_get_int(env, head, &offset))
            return enif_make_atom(env, "offset_head");

        req->lineoffsets[i] = offset;
    }

    for (int i = 0; i < offset_list_len; i++) {
        ERL_NIF_TERM head;
        ERL_NIF_TERM tail;
        int default_value;

        if (!enif_get_list_cell(env, argv[2], &head, &tail))
            return enif_make_atom(env, "default_list");

        if (!enif_get_int(env, head, &default_value))
            return enif_make_atom(env, "default_head");

        req->default_values[i] = default_value;
    }

    if (!enif_get_resource(env, argv[0], priv->gpio_chip_rt, (void **) &chip))
        return enif_make_atom(env, "bad_chip");

    rv = ioctl(chip->fd, GPIO_GET_LINEHANDLE_IOCTL, req);
    
    if (rv < 0)
        return enif_make_atom(env, "error"); // make better

    ERL_NIF_TERM linehandle_resource = enif_make_resource(env, req);
    enif_release_resource(req);

    ERL_NIF_TERM ok_atom = enif_make_atom(env, "ok");

    return enif_make_tuple2(env, ok_atom, linehandle_resource);
}

static ErlNifFunc nif_funcs[] = {
    {"open", 1, open_chip},
    {"close", 1, close_chip_nif},
    {"get_info", 1, get_info_nif},
    {"get_line_info", 2, get_line_info_nif},
    {"request_linehandle", 5, request_linehandle_nif},
    {"set_value", 2, set_value_nif},
    {"get_value", 1, get_value_nif},
    {"request_linehandle_multi", 5, request_linehandle_multi_nif}
};

ERL_NIF_INIT(Elixir.Circuits.GPIO.Chip.Nif, nif_funcs, load, NULL, NULL, NULL)


