"""C++ class references for ESPHome code generation."""

import esphome.codegen as cg

nasactl_ns = cg.esphome_ns.namespace("nasactl")

# Core classes
NasaClient = nasactl_ns.class_("NasaClient", cg.Component)
NasaController = nasactl_ns.class_("NasaController", cg.PollingComponent)
NasaDevice = nasactl_ns.class_("NasaDevice")

# Enums
ControllerMode = nasactl_ns.enum("ControllerMode", is_class=True)
CONTROLLER_MODE_STATUS = ControllerMode.enum("Status")
CONTROLLER_MODE_CONTROL = ControllerMode.enum("Control")
CONTROLLER_MODE_FSV = ControllerMode.enum("FSV")

# Entity classes
NasactlSensor = nasactl_ns.class_(
    "NasactlSensor", cg.esphome_ns.class_("sensor::Sensor"))
NasactlNumber = nasactl_ns.class_(
    "NasactlNumber", cg.esphome_ns.class_("number::Number"))
NasactlSelect = nasactl_ns.class_(
    "NasactlSelect", cg.esphome_ns.class_("select::Select"))
NasactlSwitch = nasactl_ns.class_(
    "NasactlSwitch", cg.esphome_ns.class_("switch_::Switch"))
NasactlBinarySensor = nasactl_ns.class_(
    "NasactlBinarySensor", cg.esphome_ns.class_("binary_sensor::BinarySensor"))
NasactlTextSensor = nasactl_ns.class_(
    "NasactlTextSensor", cg.esphome_ns.class_("text_sensor::TextSensor"))
NasactlClimate = nasactl_ns.class_(
    "NasactlClimate", cg.esphome_ns.class_("climate::Climate"), cg.Component)
