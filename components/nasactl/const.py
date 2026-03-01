"""
NASA message code definitions for nasactl.

To add a new entity, add an entry to ENTITIES below. Each entry maps a YAML
config key to a NASA message definition. No C++ changes needed.

Supported types: sensor, number, select, switch, binary_sensor, text_sensor

Fields:
  type        (required) Entity type
  code        (required) NASA message hex code
  unit        (optional) Unit of measurement
  device_class(optional) HA device class
  state_class (optional) HA state class (measurement, total_increasing, etc.)
  accuracy    (optional) Decimal places, default 0
  icon        (optional) MDI icon
  divisor     (optional) Read: x/divisor, Write: x*divisor
  multiplier  (optional) Read: x*multiplier, Write: x/multiplier
  signed      (optional) If True, treat raw value as int16. Default False for
                         sensors, True for temperature sensors with divisor
  fsv         (optional) If True, this is a Field Setting Value (polled on
                         startup, not broadcast)
  min         (optional) Number min value
  max         (optional) Number max value
  step        (optional) Number step
  options     (optional) Select options list
  offset      (optional) Select offset (NASA value = option_index + offset)
  mapping     (optional) Text sensor value-to-string mapping dict
  entity_category (optional) "config" or "diagnostic"
"""

ENTITIES = {
    # =========================================================================
    # SENSORS (read-only numeric)
    # =========================================================================

    # --- Indoor / Hydro sensors ---
    "water_temperature": {
        "type": "sensor",
        "code": 0x4237,
        "unit": "°C",
        "device_class": "temperature",
        "state_class": "measurement",
        "accuracy": 1,
        "icon": "mdi:thermometer-water",
        "divisor": 10,
        "signed": True,
    },
    "phe_out_water_out": {
        "type": "sensor",
        "code": 0x4238,
        "unit": "°C",
        "device_class": "temperature",
        "state_class": "measurement",
        "accuracy": 1,
        "icon": "mdi:thermometer",
        "divisor": 10,
        "signed": True,
    },
    "phe_in_water_return": {
        "type": "sensor",
        "code": 0x4236,
        "unit": "°C",
        "device_class": "temperature",
        "state_class": "measurement",
        "accuracy": 1,
        "icon": "mdi:thermometer",
        "divisor": 10,
        "signed": True,
    },
    "eva_in_temp": {
        "type": "sensor",
        "code": 0x4205,
        "unit": "°C",
        "device_class": "temperature",
        "state_class": "measurement",
        "accuracy": 1,
        "icon": "mdi:thermometer",
        "divisor": 10,
        "signed": True,
    },
    "eva_out_temp": {
        "type": "sensor",
        "code": 0x4206,
        "unit": "°C",
        "device_class": "temperature",
        "state_class": "measurement",
        "accuracy": 1,
        "icon": "mdi:thermometer",
        "divisor": 10,
        "signed": True,
    },
    "water_flow": {
        "type": "sensor",
        "code": 0x42E9,
        "unit": "L/min",
        "state_class": "measurement",
        "accuracy": 1,
        "icon": "mdi:water-pump",
        "divisor": 10,
    },
    "water_law_target_flow_temp": {
        "type": "sensor",
        "code": 0x427F,
        "unit": "°C",
        "device_class": "temperature",
        "state_class": "measurement",
        "accuracy": 1,
        "icon": "mdi:thermometer-auto",
        "divisor": 10,
        "signed": True,
    },
    "indoor_power_consumption": {
        "type": "sensor",
        "code": 0x4284,
        "unit": "W",
        "device_class": "power",
        "state_class": "measurement",
        "accuracy": 1,
        "icon": "mdi:flash",
        "divisor": 10,
        "signed": True,
    },
    "generated_power_last_minute": {
        "type": "sensor",
        "code": 0x4426,
        "unit": "kWh",
        "device_class": "energy",
        "state_class": "total_increasing",
        "accuracy": 3,
        "icon": "mdi:flash",
    },
    "room_temperature": {
        "type": "sensor",
        "code": 0x4204,
        "unit": "°C",
        "device_class": "temperature",
        "state_class": "measurement",
        "accuracy": 1,
        "icon": "mdi:thermometer",
        "divisor": 10,
        "signed": True,
    },
    "indoor_eva_in_temperature": {
        "type": "sensor",
        "code": 0x4205,
        "unit": "°C",
        "device_class": "temperature",
        "state_class": "measurement",
        "accuracy": 1,
        "icon": "mdi:thermometer",
        "divisor": 10,
        "signed": True,
    },
    "indoor_eva_out_temperature": {
        "type": "sensor",
        "code": 0x4206,
        "unit": "°C",
        "device_class": "temperature",
        "state_class": "measurement",
        "accuracy": 1,
        "icon": "mdi:thermometer",
        "divisor": 10,
        "signed": True,
    },

    # --- Outdoor unit sensors ---
    "outdoor_temperature": {
        "type": "sensor",
        "code": 0x8204,
        "unit": "°C",
        "device_class": "temperature",
        "state_class": "measurement",
        "accuracy": 1,
        "icon": "mdi:thermometer",
        "divisor": 10,
        "signed": True,
    },
    "outdoor_instantaneous_power": {
        "type": "sensor",
        "code": 0x8413,
        "unit": "W",
        "device_class": "power",
        "state_class": "measurement",
        "accuracy": 0,
        "icon": "mdi:flash",
    },
    "outdoor_cumulative_energy": {
        "type": "sensor",
        "code": 0x8414,
        "unit": "kWh",
        "device_class": "energy",
        "state_class": "total_increasing",
        "accuracy": 1,
        "icon": "mdi:counter",
        "divisor": 10,
    },
    "outdoor_current": {
        "type": "sensor",
        "code": 0x8233,
        "unit": "A",
        "device_class": "current",
        "state_class": "measurement",
        "accuracy": 1,
        "icon": "mdi:current-ac",
        "divisor": 10,
    },
    "outdoor_voltage": {
        "type": "sensor",
        "code": 0x8234,
        "unit": "V",
        "device_class": "voltage",
        "state_class": "measurement",
        "accuracy": 0,
        "icon": "mdi:sine-wave",
    },

    # =========================================================================
    # NUMBERS (read-write numeric)
    # =========================================================================

    # --- Temperature targets ---
    "water_target_temperature": {
        "type": "number",
        "code": 0x4235,
        "min": 30, "max": 75, "step": 1,
        "unit": "°C",
        "device_class": "temperature",
        "icon": "mdi:thermometer-water",
        "divisor": 10,
        "signed": True,
    },
    "target_temperature": {
        "type": "number",
        "code": 0x4201,
        "min": 16, "max": 30, "step": 0.5,
        "unit": "°C",
        "device_class": "temperature",
        "icon": "mdi:thermometer",
        "divisor": 10,
        "signed": True,
    },
    "water_law_target_temp_shift": {
        "type": "number",
        "code": 0x4248,
        "min": -5, "max": 5, "step": 0.5,
        "unit": "°C",
        "device_class": "temperature",
        "icon": "mdi:thermometer-plus",
        "divisor": 10,
        "signed": True,
    },

    # --- FSV: Heating water outlet limits ---
    "heating_water_outlet_temp_upper": {
        "type": "number",
        "code": 0x424E,
        "min": 37, "max": 70, "step": 1,
        "unit": "°C",
        "device_class": "temperature",
        "icon": "mdi:thermometer-chevron-up",
        "divisor": 10,
        "signed": True,
        "fsv": True,
        "entity_category": "config",
    },
    "heating_water_outlet_temp_lower": {
        "type": "number",
        "code": 0x424F,
        "min": 15, "max": 37, "step": 1,
        "unit": "°C",
        "device_class": "temperature",
        "icon": "mdi:thermometer-chevron-down",
        "divisor": 10,
        "signed": True,
        "fsv": True,
        "entity_category": "config",
    },

    # --- FSV: Water law curve ---
    "heating_lower_outdoor_temp": {
        "type": "number",
        "code": 0x4254,
        "min": -20, "max": 5, "step": 1,
        "unit": "°C",
        "device_class": "temperature",
        "icon": "mdi:snowflake-thermometer",
        "divisor": 10,
        "signed": True,
        "fsv": True,
        "entity_category": "config",
    },
    "heating_upper_outdoor_temp": {
        "type": "number",
        "code": 0x4255,
        "min": 10, "max": 20, "step": 1,
        "unit": "°C",
        "device_class": "temperature",
        "icon": "mdi:sun-thermometer",
        "divisor": 10,
        "signed": True,
        "fsv": True,
        "entity_category": "config",
    },
    "heating_water_temp_cold_outdoor": {
        "type": "number",
        "code": 0x4256,
        "min": 17, "max": 65, "step": 1,
        "unit": "°C",
        "device_class": "temperature",
        "icon": "mdi:thermometer-high",
        "divisor": 10,
        "signed": True,
        "fsv": True,
        "entity_category": "config",
    },
    "heating_water_temp_warm_outdoor": {
        "type": "number",
        "code": 0x4257,
        "min": 17, "max": 65, "step": 1,
        "unit": "°C",
        "device_class": "temperature",
        "icon": "mdi:thermometer-low",
        "divisor": 10,
        "signed": True,
        "fsv": True,
        "entity_category": "config",
    },

    # --- FSV: DHW tank limits ---
    "dhw_tank_temp_upper": {
        "type": "number",
        "code": 0x4252,
        "min": 50, "max": 70, "step": 1,
        "unit": "°C",
        "device_class": "temperature",
        "icon": "mdi:thermometer-chevron-up",
        "divisor": 10,
        "signed": True,
        "fsv": True,
        "entity_category": "config",
    },
    "dhw_tank_temp_lower": {
        "type": "number",
        "code": 0x4253,
        "min": 30, "max": 40, "step": 1,
        "unit": "°C",
        "device_class": "temperature",
        "icon": "mdi:thermometer-chevron-down",
        "divisor": 10,
        "signed": True,
        "fsv": True,
        "entity_category": "config",
    },

    # --- FSV: DHW disinfection ---
    "dhw_disinfection_start_time": {
        "type": "number",
        "code": 0x4269,
        "min": 0, "max": 23, "step": 1,
        "unit": "h",
        "icon": "mdi:clock-start",
        "fsv": True,
        "entity_category": "config",
    },
    "dhw_disinfection_target_temp": {
        "type": "number",
        "code": 0x426A,
        "min": 40, "max": 70, "step": 5,
        "unit": "°C",
        "device_class": "temperature",
        "icon": "mdi:virus-off",
        "divisor": 10,
        "signed": True,
        "fsv": True,
        "entity_category": "config",
    },
    "dhw_disinfection_duration": {
        "type": "number",
        "code": 0x426B,
        "min": 5, "max": 60, "step": 5,
        "unit": "min",
        "icon": "mdi:timer",
        "fsv": True,
        "entity_category": "config",
    },
    "dhw_disinfection_max_time": {
        "type": "number",
        "code": 0x42CE,
        "min": 60, "max": 1440, "step": 60,
        "unit": "min",
        "icon": "mdi:timer-alert",
        "fsv": True,
        "entity_category": "config",
    },

    # --- FSV: DHW booster heater ---
    "dhw_booster_heater_delay": {
        "type": "number",
        "code": 0x4266,
        "min": 20, "max": 95, "step": 5,
        "unit": "min",
        "icon": "mdi:timer",
        "fsv": True,
        "entity_category": "config",
    },
    "dhw_booster_heater_overshoot": {
        "type": "number",
        "code": 0x4267,
        "min": 0, "max": 4, "step": 1,
        "unit": "°C",
        "device_class": "temperature",
        "icon": "mdi:thermometer-plus",
        "fsv": True,
        "entity_category": "config",
    },

    # --- FSV: DHW forced operation ---
    "dhw_forced_operation_time": {
        "type": "number",
        "code": 0x426C,
        "min": 30, "max": 300, "step": 10,
        "unit": "min",
        "icon": "mdi:timer",
        "multiplier": 10,
        "fsv": True,
        "entity_category": "config",
    },

    # --- FSV: HP tank control ---
    "hp_max_temp_alone": {
        "type": "number",
        "code": 0x4260,
        "min": 45, "max": 55, "step": 1,
        "unit": "°C",
        "device_class": "temperature",
        "icon": "mdi:heat-pump",
        "divisor": 10,
        "signed": True,
        "fsv": True,
        "entity_category": "config",
    },
    "hp_temp_diff_off": {
        "type": "number",
        "code": 0x4261,
        "min": 0, "max": 10, "step": 1,
        "unit": "°C",
        "device_class": "temperature",
        "icon": "mdi:thermometer-minus",
        "fsv": True,
        "entity_category": "config",
    },
    "hp_temp_diff_on": {
        "type": "number",
        "code": 0x4262,
        "min": 5, "max": 30, "step": 1,
        "unit": "°C",
        "device_class": "temperature",
        "icon": "mdi:thermometer-plus",
        "fsv": True,
        "entity_category": "config",
    },

    # --- FSV: Heating priority ---
    "heating_priority_changeover_temp": {
        "type": "number",
        "code": 0x426D,
        "min": -15, "max": 20, "step": 1,
        "unit": "°C",
        "device_class": "temperature",
        "icon": "mdi:swap-horizontal",
        "divisor": 10,
        "signed": True,
        "fsv": True,
        "entity_category": "config",
    },
    "heating_dhw_off_outdoor_temp": {
        "type": "number",
        "code": 0x426E,
        "min": 14, "max": 35, "step": 1,
        "unit": "°C",
        "device_class": "temperature",
        "icon": "mdi:thermometer-off",
        "divisor": 10,
        "signed": True,
        "fsv": True,
        "entity_category": "config",
    },

    # --- FSV: Inverter pump ---
    "heating_inverter_pump_target_delta": {
        "type": "number",
        "code": 0x428A,
        "min": 2, "max": 8, "step": 1,
        "unit": "°C",
        "device_class": "temperature",
        "icon": "mdi:delta",
        "fsv": True,
        "entity_category": "config",
    },

    # =========================================================================
    # SELECTS (read-write enum)
    # =========================================================================

    "water_heater_mode": {
        "type": "select",
        "code": 0x4066,
        "options": ["Eco", "Standard", "Power", "Force"],
        "icon": "mdi:water-boiler",
    },
    "mode": {
        "type": "select",
        "code": 0x4001,
        "options": ["Cool", "Dry", "Fan", "Heat", "Auto"],
        "icon": "mdi:hvac",
    },
    "dhw_operation_mode": {
        "type": "select",
        "code": 0x4097,
        "options": ["Off", "Thermo ON/OFF", "On demand"],
        "icon": "mdi:water-boiler",
        "fsv": True,
        "entity_category": "config",
    },
    "dhw_disinfection_day": {
        "type": "select",
        "code": 0x409A,
        "options": ["Sunday", "Monday", "Tuesday", "Wednesday",
                    "Thursday", "Friday", "Saturday", "Everyday"],
        "icon": "mdi:calendar",
        "fsv": True,
        "entity_category": "config",
    },
    "dhw_forced_operation_timer": {
        "type": "select",
        "code": 0x409B,
        "options": ["No timer", "Timer"],
        "icon": "mdi:timer-cog",
        "fsv": True,
        "entity_category": "config",
    },
    "heating_dhw_priority": {
        "type": "select",
        "code": 0x409E,
        "options": ["DHW", "Heating"],
        "icon": "mdi:priority-high",
        "fsv": True,
        "entity_category": "config",
    },
    "heating_inverter_pump_application": {
        "type": "select",
        "code": 0x40C2,
        "options": ["Off", "100% max", "70% max"],
        "icon": "mdi:pump",
        "fsv": True,
        "entity_category": "config",
    },

    # =========================================================================
    # SWITCHES (read-write boolean)
    # =========================================================================

    "water_heater_power": {
        "type": "switch",
        "code": 0x4065,
        "icon": "mdi:water-boiler",
    },
    "power": {
        "type": "switch",
        "code": 0x4000,
        "icon": "mdi:power",
    },
    "silence_mode": {
        "type": "switch",
        "code": 0x4046,
        "icon": "mdi:volume-off",
    },
    "dhw_disinfection_enable": {
        "type": "switch",
        "code": 0x4099,
        "icon": "mdi:virus-off",
        "fsv": True,
        "entity_category": "config",
    },
    "dhw_booster_heater": {
        "type": "switch",
        "code": 0x4098,
        "icon": "mdi:radiator",
        "fsv": True,
        "entity_category": "config",
    },

    # =========================================================================
    # BINARY SENSORS (read-only boolean)
    # =========================================================================

    "defrost_status": {
        "type": "binary_sensor",
        "code": 0x402E,
        "icon": "mdi:snowflake-melt",
    },
    "water_pump_status": {
        "type": "binary_sensor",
        "code": 0x4089,
        "icon": "mdi:pump",
    },
    "booster_heater_status": {
        "type": "binary_sensor",
        "code": 0x4087,
        "icon": "mdi:radiator",
    },

    # =========================================================================
    # TEXT SENSORS (read-only with value mapping)
    # =========================================================================

    "dhw_valve_direction": {
        "type": "text_sensor",
        "code": 0x4067,
        "icon": "mdi:valve",
        "mapping": {
            0: "Room",
            1: "Tank",
        },
    },
    "error_code": {
        "type": "text_sensor",
        "code": 0x8235,
        "icon": "mdi:alert-circle",
        "mapping": {
            0: "No Error",
            101: "Wire Connection Error (Control Kit/ODU)",
            102: "Wire Connection Error (IDU/ODU)",
            201: "Communication Error (IDU EEPROM)",
            202: "Communication Error (ODU Timeout)",
            301: "Outdoor Unit Sensor Error",
            302: "Outdoor Unit Sensor Error (Discharge)",
            303: "Outdoor Unit Sensor Error (Suction)",
            401: "Indoor Unit Sensor Error",
            402: "Indoor Unit Sensor Error (EVA In)",
            403: "Indoor Unit Sensor Error (EVA Out)",
            501: "System Error (Compressor)",
            601: "System Error (Overcurrent)",
            701: "System Error (High Pressure)",
            801: "System Error (Inverter)",
        },
    },
    "outdoor_operation_odu_mode": {
        "type": "text_sensor",
        "code": 0x8000,
        "icon": "mdi:information",
        "mapping": {
            0: "Off",
            1: "Cool",
            2: "Heat",
            3: "Auto",
        },
    },
    "outdoor_operation_heatcool": {
        "type": "text_sensor",
        "code": 0x8001,
        "icon": "mdi:information",
        "mapping": {
            0: "Off",
            1: "Cool",
            2: "Heat",
        },
    },
}

# Custom sensor support: allows arbitrary message codes via YAML
# These are created per-device and don't need entries in ENTITIES
CUSTOM_SENSOR_SCHEMA_KEY = "custom_sensor"
