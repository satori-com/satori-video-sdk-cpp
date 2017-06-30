#pragma once
#include <cbor.h>

extern "C" {
int bot_cmd_check_action(cbor_item_t *command, const char *name);
const char *bot_cmd_get_str_value(cbor_item_t *command, const char *name,
                                  const char *default_value);
}
