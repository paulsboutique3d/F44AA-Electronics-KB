/**
 * @file magazine.h
 * @brief Magazine Detection Interface
 * 
 * Hall effect sensor magazine detection and ammo management.
 * GPIO: 38 (active low)
 * 
 * @author PaulsBoutique3D
 * @date January 2026
 * @version 2.0
 */

#ifndef MAGAZINE_H
#define MAGAZINE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Magazine capacity and state definitions
#define MAGAZINE_FULL_CAPACITY 400
#define MAGAZINE_EMPTY 0

// Magazine management functions
void magazine_init(void);
void magazine_update(void);

// Ammo count management
int magazine_get_current_ammo(void);
void magazine_set_current_ammo(int ammo);
void magazine_consume_ammo(int rounds);
void magazine_reload(void);

// Magazine state management
bool magazine_is_inserted(void);
bool magazine_check_reload_event(void);
void magazine_set_override(bool inserted);  // Override for web interface testing
void magazine_clear_test_mode(void);        // Clear test mode and return to sensor
bool magazine_is_test_mode(void);           // Check if in test mode
bool magazine_is_physically_inserted(void); // Check real physical magazine (bypasses test mode)
int magazine_get_stored_ammo(void);
void magazine_clear_stored_ammo(void);

// Magazine event callbacks
void magazine_on_inserted(void);
void magazine_on_removed(void);

#ifdef __cplusplus
}
#endif

#endif // MAGAZINE_H
