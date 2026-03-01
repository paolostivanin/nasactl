"""nasactl — ESPHome component for Samsung HVAC NASA protocol."""

import re

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import (
    binary_sensor,
    climate,
    number,
    select,
    sensor,
    switch,
    text_sensor,
    uart,
)
from esphome.const import (
    CONF_ID,
    CONF_NAME,
)

from .const import ENTITIES
from .nasa import (
    CONTROLLER_MODE_CONTROL,
    CONTROLLER_MODE_FSV,
    CONTROLLER_MODE_STATUS,
    NasaClient,
    NasaController,
    NasaDevice,
    NasactlBinarySensor,
    NasactlClimate,
    NasactlNumber,
    NasactlSelect,
    NasactlSensor,
    NasactlSwitch,
    NasactlTextSensor,
)

DEPENDENCIES = ["uart"]
AUTO_LOAD = ["sensor", "number", "select", "switch", "binary_sensor",
             "text_sensor", "climate"]

# Config keys
CONF_DEVICES = "devices"
CONF_ADDRESS = "address"
CONF_DEVICE_TYPE = "type"
CONF_CLIMATE = "climate"
CONF_CUSTOM_SENSOR = "custom_sensor"
CONF_MESSAGE = "message"
CONF_FLOW_CONTROL_PIN = "flow_control_pin"
CONF_SILENCE_INTERVAL = "silence_interval"
CONF_RETRY_INTERVAL = "retry_interval"
CONF_MIN_RETRIES = "min_retries"
CONF_SEND_TIMEOUT = "send_timeout"
CONF_DEBUG_LOG_MESSAGES = "debug_log_messages"
CONF_DEBUG_LOG_UNDEFINED = "debug_log_undefined"
CONF_FSV_READ = "fsv_read"
CONF_FSV_STARTUP_DELAY = "startup_delay"
CONF_FSV_BATCH_SIZE = "batch_size"
CONF_FSV_BATCH_DELAY = "batch_delay"
CONF_FSV_INTERVAL = "interval"
CONF_CLIENT_ID = "client_id"


# ---------------------------------------------------------------------------
# Build entity schemas dynamically from ENTITIES dict
# ---------------------------------------------------------------------------

def _sensor_schema(edef):
    kwargs = {}
    if "accuracy" in edef:
        kwargs["accuracy_decimals"] = edef["accuracy"]
    if "unit" in edef:
        kwargs["unit_of_measurement"] = edef["unit"]
    if "device_class" in edef:
        kwargs["device_class"] = edef["device_class"]
    if "state_class" in edef:
        kwargs["state_class"] = edef["state_class"]
    if "icon" in edef:
        kwargs["icon"] = edef["icon"]
    if edef.get("entity_category"):
        kwargs["entity_category"] = edef["entity_category"]
    return sensor.sensor_schema(NasactlSensor, **kwargs)


def _number_schema(edef):
    kwargs = {}
    if "icon" in edef:
        kwargs["icon"] = edef["icon"]
    if edef.get("entity_category"):
        kwargs["entity_category"] = edef["entity_category"]
    if "unit" in edef:
        kwargs["unit_of_measurement"] = edef["unit"]
    if "device_class" in edef:
        kwargs["device_class"] = edef["device_class"]
    return number.number_schema(NasactlNumber, **kwargs)


def _select_schema(edef):
    kwargs = {}
    if "icon" in edef:
        kwargs["icon"] = edef["icon"]
    if edef.get("entity_category"):
        kwargs["entity_category"] = edef["entity_category"]
    return select.select_schema(NasactlSelect, **kwargs)


def _switch_schema(edef):
    kwargs = {}
    if "icon" in edef:
        kwargs["icon"] = edef["icon"]
    if edef.get("entity_category"):
        kwargs["entity_category"] = edef["entity_category"]
    return switch.switch_schema(NasactlSwitch, **kwargs)


def _binary_sensor_schema(edef):
    kwargs = {}
    if "icon" in edef:
        kwargs["icon"] = edef["icon"]
    if edef.get("entity_category"):
        kwargs["entity_category"] = edef["entity_category"]
    if "device_class" in edef:
        kwargs["device_class"] = edef["device_class"]
    return binary_sensor.binary_sensor_schema(NasactlBinarySensor, **kwargs)


def _text_sensor_schema(edef):
    kwargs = {}
    if "icon" in edef:
        kwargs["icon"] = edef["icon"]
    if edef.get("entity_category"):
        kwargs["entity_category"] = edef["entity_category"]
    return text_sensor.text_sensor_schema(NasactlTextSensor, **kwargs)


SCHEMA_BUILDERS = {
    "sensor": _sensor_schema,
    "number": _number_schema,
    "select": _select_schema,
    "switch": _switch_schema,
    "binary_sensor": _binary_sensor_schema,
    "text_sensor": _text_sensor_schema,
}


def _build_device_entity_schemas():
    schemas = {}
    for key, edef in ENTITIES.items():
        entity_type = edef["type"]
        builder = SCHEMA_BUILDERS.get(entity_type)
        if builder:
            schemas[cv.Optional(key)] = builder(edef)
    return schemas


# Custom sensor schema for arbitrary message codes
CUSTOM_SENSOR_SCHEMA = sensor.sensor_schema(
    NasactlSensor,
    accuracy_decimals=1,
).extend({
    cv.Required(CONF_MESSAGE): cv.hex_int,
    cv.Optional("divisor"): cv.float_,
    cv.Optional("multiplier"): cv.float_,
    cv.Optional("signed", default=False): cv.boolean,
})

# FSV read configuration
FSV_READ_SCHEMA = cv.Schema({
    cv.Optional(CONF_FSV_STARTUP_DELAY, default="5s"):
        cv.positive_time_period_milliseconds,
    cv.Optional(CONF_FSV_BATCH_SIZE, default=10):
        cv.int_range(1, 50),
    cv.Optional(CONF_FSV_BATCH_DELAY, default="200ms"):
        cv.positive_time_period_milliseconds,
    cv.Optional(CONF_FSV_INTERVAL, default="0s"):
        cv.positive_time_period_milliseconds,
})

# Address format validator
ADDRESS_RE = re.compile(r"^[0-9a-fA-F]{2}\.[0-9a-fA-F]{2}\.[0-9a-fA-F]{2}$")


def _validate_address(value):
    value = cv.string_strict(value)
    if not ADDRESS_RE.match(value):
        raise cv.Invalid(
            f"Invalid address '{value}'. Must be XX.XX.XX (e.g. 20.00.00)")
    return value


# Device schema
DEVICE_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(NasaDevice),
    cv.Required(CONF_ADDRESS): _validate_address,
    cv.Required(CONF_DEVICE_TYPE): cv.one_of("hydro", "ac", "outdoor", upper=False),
    cv.Optional(CONF_CLIMATE): climate.climate_schema(NasactlClimate),
    cv.Optional(CONF_CUSTOM_SENSOR, default=[]):
        cv.ensure_list(CUSTOM_SENSOR_SCHEMA),
    **_build_device_entity_schemas(),
})

# Main component schema
CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(NasaController),
    cv.GenerateID(CONF_CLIENT_ID): cv.declare_id(NasaClient),
    cv.Optional(CONF_FLOW_CONTROL_PIN): pins.gpio_output_pin_schema,
    cv.Optional(CONF_SILENCE_INTERVAL, default=100): cv.int_range(50, 1000),
    cv.Optional(CONF_RETRY_INTERVAL, default=500): cv.int_range(200, 5000),
    cv.Optional(CONF_MIN_RETRIES, default=1): cv.int_range(1, 10),
    cv.Optional(CONF_SEND_TIMEOUT, default=4000): cv.int_range(1000, 10000),
    cv.Optional(CONF_DEBUG_LOG_MESSAGES, default=False): cv.boolean,
    cv.Optional(CONF_DEBUG_LOG_UNDEFINED, default=False): cv.boolean,
    cv.Optional(CONF_FSV_READ, default={}): FSV_READ_SCHEMA,
    cv.Required(CONF_DEVICES): cv.ensure_list(DEVICE_SCHEMA),
}).extend(uart.UART_DEVICE_SCHEMA).extend(cv.polling_component_schema("30s"))


# ---------------------------------------------------------------------------
# Code generation helpers
# ---------------------------------------------------------------------------

def _controller_mode(edef):
    if edef.get("fsv"):
        return CONTROLLER_MODE_FSV
    if edef["type"] in ("number", "select", "switch"):
        return CONTROLLER_MODE_CONTROL
    return CONTROLLER_MODE_STATUS


async def _create_sensor(config, edef, dev, ctrl):
    code = edef["code"]
    mode = _controller_mode(edef)
    label = str(config.get(CONF_NAME, ""))

    var = await sensor.new_sensor(config, label, code, mode, dev)
    cg.add(var.set_parent(ctrl))
    if "divisor" in edef:
        cg.add(var.set_divisor(edef["divisor"]))
    if "multiplier" in edef:
        cg.add(var.set_multiplier(edef["multiplier"]))
    if edef.get("signed", False):
        cg.add(var.set_signed(True))
    cg.add(ctrl.register_component(var))


async def _create_number(config, edef, dev, ctrl):
    code = edef["code"]
    mode = _controller_mode(edef)
    label = str(config.get(CONF_NAME, ""))

    var = await number.new_number(
        config, label, code, mode, dev,
        min_value=edef.get("min", 0),
        max_value=edef.get("max", 100),
        step=edef.get("step", 1),
    )
    cg.add(var.set_parent(ctrl))
    if "divisor" in edef:
        cg.add(var.set_divisor(edef["divisor"]))
    if "multiplier" in edef:
        cg.add(var.set_multiplier(edef["multiplier"]))
    if edef.get("signed", False):
        cg.add(var.set_signed(True))
    cg.add(ctrl.register_component(var))


async def _create_select(config, edef, dev, ctrl):
    code = edef["code"]
    mode = _controller_mode(edef)
    label = str(config.get(CONF_NAME, ""))

    var = await select.new_select(
        config, label, code, mode, dev,
        options=edef["options"],
    )
    cg.add(var.set_parent(ctrl))
    if "offset" in edef:
        cg.add(var.set_offset(edef["offset"]))
    cg.add(ctrl.register_component(var))


async def _create_switch(config, edef, dev, ctrl):
    code = edef["code"]
    mode = _controller_mode(edef)
    label = str(config.get(CONF_NAME, ""))

    var = await switch.new_switch(config, label, code, mode, dev)
    cg.add(var.set_parent(ctrl))
    cg.add(ctrl.register_component(var))


async def _create_binary_sensor(config, edef, dev, ctrl):
    code = edef["code"]
    mode = _controller_mode(edef)
    label = str(config.get(CONF_NAME, ""))

    var = await binary_sensor.new_binary_sensor(config, label, code, mode, dev)
    cg.add(var.set_parent(ctrl))
    cg.add(ctrl.register_component(var))


async def _create_text_sensor(config, edef, dev, ctrl):
    code = edef["code"]
    mode = _controller_mode(edef)
    label = str(config.get(CONF_NAME, ""))

    var = await text_sensor.new_text_sensor(config, label, code, mode, dev)
    cg.add(var.set_parent(ctrl))
    if "mapping" in edef:
        for val, txt in edef["mapping"].items():
            cg.add(var.add_mapping(val, txt))
    cg.add(ctrl.register_component(var))


ENTITY_CREATORS = {
    "sensor": _create_sensor,
    "number": _create_number,
    "select": _create_select,
    "switch": _create_switch,
    "binary_sensor": _create_binary_sensor,
    "text_sensor": _create_text_sensor,
}


# ---------------------------------------------------------------------------
# Main code generation
# ---------------------------------------------------------------------------

async def to_code(config):
    import os
    from esphome.core import CORE

    # Add the source component directory to the compiler include path so that
    # subdirectory headers (sensor/, number/, climate/, etc.) are found.
    # ESPHome only copies top-level component files to the build tree.
    # Subdirectory .cpp files are pulled in via nasactl_entities.cpp (#include).
    src_base = os.path.join(CORE.config_dir, "components", "nasactl")
    cg.add_platformio_option(
        "build_flags",
        [f"-I{src_base}"],
    )

    cg.add_global(cg.RawExpression('#include "esphome/components/nasactl/nasactl.h"'))

    # Create NasaClient (UART communication layer)
    client_var = cg.new_Pvariable(config[CONF_CLIENT_ID])
    await cg.register_component(client_var, {})
    await uart.register_uart_device(client_var, config)

    if CONF_FLOW_CONTROL_PIN in config:
        pin = await cg.gpio_pin_expression(config[CONF_FLOW_CONTROL_PIN])
        cg.add(client_var.set_flow_control_pin(pin))

    cg.add(client_var.set_silence_interval(config[CONF_SILENCE_INTERVAL]))
    cg.add(client_var.set_retry_interval(config[CONF_RETRY_INTERVAL]))
    cg.add(client_var.set_min_retries(config[CONF_MIN_RETRIES]))
    cg.add(client_var.set_send_timeout(config[CONF_SEND_TIMEOUT]))

    # Create NasaController (message routing + FSV polling)
    ctrl = cg.new_Pvariable(config[CONF_ID], client_var)
    await cg.register_component(ctrl, config)

    cg.add(ctrl.set_debug_log_messages(config[CONF_DEBUG_LOG_MESSAGES]))
    cg.add(ctrl.set_debug_log_undefined(config[CONF_DEBUG_LOG_UNDEFINED]))

    # FSV polling config
    fsv = config[CONF_FSV_READ]
    cg.add(ctrl.set_fsv_startup_delay(
        int(fsv[CONF_FSV_STARTUP_DELAY].total_milliseconds)))
    cg.add(ctrl.set_fsv_batch_size(fsv[CONF_FSV_BATCH_SIZE]))
    cg.add(ctrl.set_fsv_batch_delay(
        int(fsv[CONF_FSV_BATCH_DELAY].total_milliseconds)))
    cg.add(ctrl.set_fsv_interval(
        int(fsv[CONF_FSV_INTERVAL].total_milliseconds)))

    # Process each device
    for dev_conf in config[CONF_DEVICES]:
        address = dev_conf[CONF_ADDRESS]
        address_class = int(address.split(".")[0], 16)

        dev = cg.new_Pvariable(dev_conf[CONF_ID], address, address_class)
        cg.add(ctrl.register_device(dev))

        # Create entities from ENTITIES dict
        for key, edef in ENTITIES.items():
            if key not in dev_conf:
                continue
            creator = ENTITY_CREATORS.get(edef["type"])
            if creator:
                await creator(dev_conf[key], edef, dev, ctrl)

        # Create climate entity for AC devices
        if CONF_CLIMATE in dev_conf:
            clim_conf = dev_conf[CONF_CLIMATE]
            clim = cg.new_Pvariable(clim_conf[CONF_ID])
            await cg.register_component(clim, clim_conf)
            await climate.register_climate(clim, clim_conf)
            cg.add(clim.set_controller(ctrl))
            cg.add(clim.set_device(dev))

            # Message routers are created in NasactlClimate::setup()

        # Create custom sensors
        for cust in dev_conf.get(CONF_CUSTOM_SENSOR, []):
            msg_code = cust[CONF_MESSAGE]
            label = str(cust.get(CONF_NAME, ""))

            var = await sensor.new_sensor(
                cust, label, msg_code, CONTROLLER_MODE_STATUS, dev)
            cg.add(var.set_parent(ctrl))
            if "divisor" in cust:
                cg.add(var.set_divisor(cust["divisor"]))
            if "multiplier" in cust:
                cg.add(var.set_multiplier(cust["multiplier"]))
            if cust.get("signed", False):
                cg.add(var.set_signed(True))
            cg.add(ctrl.register_component(var))
