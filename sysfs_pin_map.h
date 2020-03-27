#ifndef __SYSFS_PINMAP_H__
#define __SYSFS_PINMAP_H__

typedef struct {
  int pin;
  int gpio;
} sysfs_pin_map_t;

typedef enum {
  BBB,
  BBAI,
  OTHER
} sysfs_board_t;

#include "sysfs_bbai_pin_map.h"
#include "sysfs_bbb_pin_map.h"

int PIN2GPIO(sysfs_board_t board, int pin) {
  sysfs_pin_map_t *pin_map;
  int count;
  switch(board) {
    case BBB:
      pin_map = sysfs_bbb_pin_map;
      count = sizeof(sysfs_bbb_pin_map)/sizeof(sysfs_pin_map_t);
      break;
    case BBAI:
      pin_map = sysfs_bbai_pin_map;
      count = sizeof(sysfs_bbai_pin_map)/sizeof(sysfs_pin_map_t);
      break;
    default:
      pin_map = NULL;
  }
  if(pin_map) {
    for(int i = 0; i < count; i++) {
      if(pin_map[i].pin == pin) {
	return pin_map[i].gpio;
      }
    }
  }
  return pin;
}

#endif
