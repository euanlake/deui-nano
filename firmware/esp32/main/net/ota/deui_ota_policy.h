#pragma once

#include <stdbool.h>

#include "deui_ota.h"

bool deui_ota_has_internet(void);
bool deui_ota_setup_session_active(void);
bool deui_ota_policy_should_defer_auto(void);
bool deui_ota_policy_should_defer(deui_ota_trigger_t trigger);
bool deui_ota_policy_can_download(deui_ota_trigger_t trigger);
