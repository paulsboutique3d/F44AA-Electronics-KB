/**
 * @file system_config.h
 * @brief Centralized System Constants for F44AA Pulse Rifle
 *
 * All shared constants in one place. Individual modules should
 * include this instead of defining their own copies.
 *
 * @author PaulsBoutique3D
 * @date January 2026
 * @version 2.0
 */

#pragma once

// ── Weapon Parameters ──
#define F44AA_FIRE_RATE_RPM     900
#define F44AA_FIRE_DELAY_MS     (60000 / F44AA_FIRE_RATE_RPM)  // ~66.67ms
#define F44AA_MAX_AMMO          400
#define F44AA_BAR_COUNT         6

// ── Polling Intervals ──
#define F44AA_TRIGGER_POLL_MS   10
#define F44AA_MAGAZINE_CHECK_MS 50
