import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor

from .. import SonoffTXUltimate, sonoff_tx_ultimate_ns, CONF_SONOFF_TX_ULTIMATE_ID

CONF_CHANNELS = "channels"
CONF_PRESS_ONLY = "press_only"

DEPENDENCIES = ["sonoff_tx_ultimate"]

SonoffTXUltimateTouchBinarySensor = sonoff_tx_ultimate_ns.class_("SonoffTXUltimateTouchBinarySensor", binary_sensor.BinarySensor)

CONFIG_SCHEMA = binary_sensor.binary_sensor_schema(SonoffTXUltimateTouchBinarySensor).extend(
    {
        cv.GenerateID(CONF_SONOFF_TX_ULTIMATE_ID): cv.use_id(SonoffTXUltimate),
        cv.Required(CONF_CHANNELS): cv.ensure_list(
            cv.int_range(min=1, max=13)
        ),
        cv.Optional(CONF_PRESS_ONLY, default=False): cv.boolean,
    }
)

async def to_code(config):
    var = await binary_sensor.new_binary_sensor(config)
    SonoffTXUltimate = await cg.get_variable(config[CONF_SONOFF_TX_ULTIMATE_ID])

    for _, ch in enumerate(config[CONF_CHANNELS]):
        cg.add(var.add_channel(ch))

    cg.add(var.press_only_set(config[CONF_PRESS_ONLY]))

    cg.add(SonoffTXUltimate.register_touch_binary_sensor(var))
