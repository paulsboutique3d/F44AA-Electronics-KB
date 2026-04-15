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

// Magazine management functions
void magazine_init(void);

// Magazine state
bool magazine_is_inserted(void);
bool magazine_check_reload_event(void);
void magazine_set_override(bool inserted);
void magazine_clear_test_mode(void);
bool magazine_is_test_mode(void);
bool magazine_is_physically_inserted(void);

#ifdef __cplusplus
}
#endif

#endif // MAGAZINE_H
