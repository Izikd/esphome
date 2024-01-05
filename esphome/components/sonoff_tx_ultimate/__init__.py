import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart
from esphome import pins
from esphome.const import (
    CONF_ID,
)

DEPENDENCIES = ["uart"]
CODEOWNERS = ["@izikd"]

sonoff_tx_ultimate_ns = cg.esphome_ns.namespace("sonoff_tx_ultimate")
SonoffTXUltimate = sonoff_tx_ultimate_ns.class_(
    "SonoffTXUltimate", cg.Component, uart.UARTDevice
)

CONF_SONOFF_TX_ULTIMATE_ID = "sonoff_tx_ultimate_id"
CONF_POWER_PIN = "power_pin"

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(SonoffTXUltimate),
            cv.Required(CONF_POWER_PIN): pins.internal_gpio_output_pin_schema,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(uart.UART_DEVICE_SCHEMA)
)

FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    "sonoff_tx_ultimate", baud_rate=115200, require_tx=True, require_rx=True
)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    
    pin = await cg.gpio_pin_expression(config[CONF_POWER_PIN])
    cg.add(var.set_power_pin(pin))
