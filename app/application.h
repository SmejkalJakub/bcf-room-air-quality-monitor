#ifndef _APPLICATION_H
#define _APPLICATION_H

#ifndef FIRMWARE
#define FIRMWARE "room-air-quality-monitor"
#endif

#ifndef VERSION
#define VERSION "2.0"
#endif

#include <bcl.h>

typedef struct
{
    uint8_t channel;
    float value;
    bc_tick_t next_pub;

} event_param_t;

typedef struct
{
    bc_tag_temperature_t self;
    event_param_t param;

} temperature_tag_t;

typedef struct
{
    bc_tag_humidity_t self;
    event_param_t param;

} humidity_tag_t;

static void temperature_tag_init(bc_i2c_channel_t i2c_channel, bc_tag_temperature_i2c_address_t i2c_address, temperature_tag_t *tag);
void temperature_tag_event_handler(bc_tag_temperature_t *self, bc_tag_temperature_event_t event, void *event_param);
static void humidity_tag_init(bc_tag_humidity_revision_t revision, bc_i2c_channel_t i2c_channel, humidity_tag_t *tag);
void humidity_tag_event_handler(bc_tag_humidity_t *self, bc_tag_humidity_event_t event, void *event_param);
void co2_event_handler(bc_module_co2_event_t event, void *event_param);
void battery_module_event_handler(bc_module_battery_event_t event, void *event_param);
void button_event_handler(bc_button_t *self, bc_button_event_t event, void *event_param);

#endif
