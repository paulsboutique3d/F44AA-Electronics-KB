/**
 * @file trigger.h
 * @brief Trigger Input Interface
 * 
 * Trigger switch detection for firing control.
 * 
 * @author PaulsBoutique3D
 * @date January 2026
 * @version 2.0
 */

#pragma once

#include <stdbool.h>

void trigger_init(void);
bool trigger_fired(void);
