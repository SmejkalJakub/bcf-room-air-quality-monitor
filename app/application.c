#include <application.h>
#include <radio.h>

#define TEMPERATURE_TAG_PUB_NO_CHANGE_INTEVAL (5 * 60 * 1000)
#define TEMPERATURE_TAG_PUB_VALUE_CHANGE 0.1f
#define TEMPERATURE_TAG_UPDATE_INTERVAL (1 * 1000)

#define HUMIDITY_TAG_PUB_NO_CHANGE_INTEVAL (5 * 60 * 1000)
#define HUMIDITY_TAG_PUB_VALUE_CHANGE 1.0f
#define HUMIDITY_TAG_UPDATE_INTERVAL (1 * 1000)

#define CO2_PUB_NO_CHANGE_INTERVAL (5 * 60 * 1000)
#define CO2_PUB_VALUE_CHANGE 50.0f
#define CO2_UPDATE_INTERVAL (1 * 60 * 1000)
#define CO2_CALIBRATION_DELAY (10 * 60 * 1000)

#define BATTERY_UPDATE_INTERVAL (60 * 60 * 1000)

bc_led_t led;

static struct
{
    float_t temperature;
    float_t humidity;
    float_t co2_concentation;
} values;

void application_init(void)
{
    bc_led_init(&led, BC_GPIO_LED, false, false);

    bc_radio_init();

    static bc_button_t button;
    bc_button_init(&button, BC_GPIO_BUTTON, BC_GPIO_PULL_DOWN, false);
    bc_button_set_event_handler(&button, button_event_handler, NULL);

    // Temperature Tag
    static temperature_tag_t temperature_tag_0_0;
    temperature_tag_init(BC_I2C_I2C0, BC_TAG_TEMPERATURE_I2C_ADDRESS_DEFAULT, &temperature_tag_0_0);

    // Humidity Tag
    static humidity_tag_t humidity_tag_0_2;
    humidity_tag_init(BC_TAG_HUMIDITY_REVISION_R2, BC_I2C_I2C0, &humidity_tag_0_2);

    // CO2 Module
    static event_param_t co2_event_param = { .next_pub = 0 };
    bc_module_co2_init();
    bc_module_co2_set_update_interval(CO2_UPDATE_INTERVAL);
    bc_module_co2_set_event_handler(co2_event_handler, &co2_event_param);

    // Battery Module
    bc_module_battery_init(BC_MODULE_BATTERY_FORMAT_STANDARD);
    bc_module_battery_set_event_handler(battery_module_event_handler, NULL);
    bc_module_battery_set_update_interval(BATTERY_UPDATE_INTERVAL);

    // LCD Module
    memset(&values, 0xff, sizeof(values));
    bc_module_lcd_init(&_bc_module_lcd_framebuffer);

    static bc_button_t lcd_left;
    bc_button_init_virtual(&lcd_left, BC_MODULE_LCD_BUTTON_LEFT, bc_module_lcd_get_button_driver(), false);
    bc_button_set_event_handler(&lcd_left, lcd_button_event_handler, NULL);
}

void application_task(void)
{
    if (!bc_module_lcd_is_ready())
    {
        return;
    }

    int w;
    char str[32];

    bc_module_core_pll_enable();

    bc_module_lcd_clear();

    bc_module_lcd_set_font(&bc_font_ubuntu_28);
    snprintf(str, sizeof(str), "%.1f", values.temperature);
    w = bc_module_lcd_draw_string(25, 10, str, true);
    bc_module_lcd_set_font(&bc_font_ubuntu_15);
    w = bc_module_lcd_draw_string(w, 20, "\xb0" "C", true);

    bc_module_lcd_set_font(&bc_font_ubuntu_28);
    snprintf(str, sizeof(str), "%.1f", values.humidity);
    w = bc_module_lcd_draw_string(25, 50, str, true);
    bc_module_lcd_set_font(&bc_font_ubuntu_15);
    bc_module_lcd_draw_string(w, 60, "%", true);

    bc_module_lcd_set_font(&bc_font_ubuntu_28);
    snprintf(str, sizeof(str), "%.0f", values.co2_concentation);
    w = bc_module_lcd_draw_string(20, 90, str, true);
    bc_module_lcd_set_font(&bc_font_ubuntu_15);
    bc_module_lcd_draw_string(w, 100, "ppm", true);

    bc_module_core_pll_disable();

    bc_module_lcd_update();
}

void button_event_handler(bc_button_t *self, bc_button_event_t event, void *event_param)
{
    (void) self;
    (void) event_param;

    if (event == BC_BUTTON_EVENT_PRESS)
    {
        bc_led_pulse(&led, 100);

        static uint16_t event_count = 0;

        bc_radio_pub_push_button(&event_count);

        event_count++;
    }
    else if (event == BC_BUTTON_EVENT_HOLD)
    {
        bc_radio_enroll_to_gateway();

        bc_led_pulse(&led, 1000);
    }
}

static void temperature_tag_init(bc_i2c_channel_t i2c_channel, bc_tag_temperature_i2c_address_t i2c_address, temperature_tag_t *tag)
{
    memset(tag, 0, sizeof(*tag));

    tag->param.number = (i2c_channel << 7) | i2c_address;

    bc_tag_temperature_init(&tag->self, i2c_channel, i2c_address);

    bc_tag_temperature_set_update_interval(&tag->self, TEMPERATURE_TAG_UPDATE_INTERVAL);

    bc_tag_temperature_set_event_handler(&tag->self, temperature_tag_event_handler, &tag->param);
}

void temperature_tag_event_handler(bc_tag_temperature_t *self, bc_tag_temperature_event_t event, void *event_param)
{
    float value;
    event_param_t *param = (event_param_t *)event_param;

    if (event != BC_TAG_TEMPERATURE_EVENT_UPDATE)
    {
        return;
    }

    if (bc_tag_temperature_get_temperature_celsius(self, &value))
    {
        if ((fabs(value - param->value) >= TEMPERATURE_TAG_PUB_VALUE_CHANGE) || (param->next_pub < bc_scheduler_get_spin_tick()))
        {
            bc_radio_pub_thermometer(param->number, &value);
            param->value = value;
            param->next_pub = bc_scheduler_get_spin_tick() + TEMPERATURE_TAG_PUB_NO_CHANGE_INTEVAL;

            values.temperature = value;
            bc_scheduler_plan_now(0);
        }
    }
}

static void humidity_tag_init(bc_tag_humidity_revision_t revision, bc_i2c_channel_t i2c_channel, humidity_tag_t *tag)
{
    uint8_t address;

    memset(tag, 0, sizeof(*tag));

    if (revision == BC_TAG_HUMIDITY_REVISION_R1)
    {
        address = 0x5f;
    }
    else if (revision == BC_TAG_HUMIDITY_REVISION_R2)
    {
        address = 0x40;
    }
    else if (revision == BC_TAG_HUMIDITY_REVISION_R3)
    {
        address = 0x40 | 0x0f; // 0x0f - hack
    }
    else
    {
        return;
    }

    tag->param.number = (i2c_channel << 7) | address;

    bc_tag_humidity_init(&tag->self, revision, i2c_channel, BC_TAG_HUMIDITY_I2C_ADDRESS_DEFAULT);

    bc_tag_humidity_set_update_interval(&tag->self, HUMIDITY_TAG_UPDATE_INTERVAL);

    bc_tag_humidity_set_event_handler(&tag->self, humidity_tag_event_handler, &tag->param);
}

void humidity_tag_event_handler(bc_tag_humidity_t *self, bc_tag_humidity_event_t event, void *event_param)
{
    float value;
    event_param_t *param = (event_param_t *)event_param;

    if (event != BC_TAG_HUMIDITY_EVENT_UPDATE)
    {
        return;
    }

    if (bc_tag_humidity_get_humidity_percentage(self, &value))
    {
        if ((fabs(value - param->value) >= HUMIDITY_TAG_PUB_VALUE_CHANGE) || (param->next_pub < bc_scheduler_get_spin_tick()))
        {
            bc_radio_pub_humidity(param->number, &value);
            param->value = value;
            param->next_pub = bc_scheduler_get_spin_tick() + HUMIDITY_TAG_PUB_NO_CHANGE_INTEVAL;

            values.humidity = value;
            bc_scheduler_plan_now(0);
        }
    }
}

void co2_event_handler(bc_module_co2_event_t event, void *event_param)
{
    event_param_t *param = (event_param_t *) event_param;
    float value;

    if (event == BC_MODULE_CO2_EVENT_UPDATE)
    {
        if (bc_module_co2_get_concentration(&value))
        {
            if ((fabs(value - param->value) >= CO2_PUB_VALUE_CHANGE) || (param->next_pub < bc_scheduler_get_spin_tick()))
            {
                bc_radio_pub_co2(&value);
                param->value = value;
                param->next_pub = bc_scheduler_get_spin_tick() + CO2_PUB_NO_CHANGE_INTERVAL;

                values.co2_concentation = value;
                bc_scheduler_plan_now(0);
            }
        }
    }
}

void co2_calibration_task(void *param)
{
    (void) param;

    bc_led_set_mode(&led, BC_LED_MODE_OFF);

    bc_module_co2_calibration(BC_CO2_SENSOR_CALIBRATION_BACKGROUND_FILTERED);

    bc_scheduler_unregister(bc_scheduler_get_current_task_id());
}

void battery_module_event_handler(bc_module_battery_event_t event, void *event_param)
{
    (void) event;
    (void) event_param;

    float voltage;

    if (bc_module_battery_get_voltage(&voltage))
    {
        bc_radio_pub_battery(0, &voltage);
    }
}

void lcd_button_event_handler(bc_button_t *self, bc_button_event_t event, void *event_param)
{
    (void) self;
    (void) event_param;

    if (event != BC_BUTTON_EVENT_CLICK)
    {
        return;
    }

    bc_led_set_mode(&led, BC_LED_MODE_ON);

    bc_scheduler_register(co2_calibration_task, NULL, bc_tick_get() + CO2_CALIBRATION_DELAY);
}
