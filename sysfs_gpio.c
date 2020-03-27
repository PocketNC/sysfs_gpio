/********************************************************************
* Description:  sysfs_gpio
*               This file, 'sysfs_gpio.c', is a HAL component that 
*               provides access to gpio pins using sysfs.
*
* Author: John Allwine <john@pocketnc.com>
* License: GPL Version 2
*    
* Copyright (c) 2020 Pocket NC Company All rights reserved.
*
********************************************************************/

#include "rtapi.h"              /* RTAPI realtime OS API */
#include "rtapi_app.h"          /* RTAPI realtime module decls */
#include "rtapi_errno.h"        /* EINVAL etc */
#include "hal.h"                /* HAL public API decls */
#include "sysfs_pin_map.h"

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>

MODULE_AUTHOR("John Allwine");
MODULE_DESCRIPTION("Driver for GPIO pins using sysfs");
MODULE_LICENSE("GPL");

#define MAX_PATH_LENGTH 30
#define MAX_GPIO_LENGTH 4
#define MAX_VALUE_LENGTH 4

typedef struct sysfs_pin_struct {
  hal_bit_t* value;
  hal_bit_t* invert;
  int pin;
  int gpio;
  int fd;
  struct sysfs_pin_struct *next;
} sysfs_pin_t;

static const char *modname = "sysfs_gpio";
static int comp_id;

sysfs_board_t board_id;
static char *board;
RTAPI_MP_STRING(board, "BBB, BBAI, OTHER (default)");

static char *input_pins;
RTAPI_MP_STRING(input_pins, "Comma separated list of pins configured as inputs. Use 8xx for P8 pins and 9xx for P9 pins.");

static char *output_pins;
RTAPI_MP_STRING(output_pins, "Comma separated list of pins configured as outputs. Use 8xx for P8 pins and 9xx for P9 pins.");

static sysfs_pin_t *input_pin_root = NULL;
static sysfs_pin_t *output_pin_root = NULL;

static void read_input_pins(void *arg, long period) {
  char value_str[MAX_VALUE_LENGTH];
  sysfs_pin_t *current = input_pin_root;
  while(current != NULL) {
    if(read(current->fd, value_str, 1) == -1) {
      rtapi_print_msg(RTAPI_MSG_ERR, "%s: ERROR: failed to read pin %d\n", modname, current->pin);
    }
    lseek(current->fd, 0, SEEK_SET);
    int value = value_str[0] != '0';
    *(current->value) = value ^ *(current->invert);
    current = current->next;
  }
}

static void write_output_pins(void *arg, long period) {
  char value_str[MAX_VALUE_LENGTH];
  sysfs_pin_t *current = output_pin_root;
  while(current != NULL) {
    value_str[0] = (*(current->value) ^ *(current->invert)) ? '1' : '0';
    if(write(current->fd, value_str, 1) == -1) {
      rtapi_print_msg(RTAPI_MSG_ERR, "%s: ERROR: failed to write pin %d\n", modname, current->pin);
    }
    lseek(current->fd, 0, SEEK_SET);
    current = current->next;
  }
}

int rtapi_app_main(void) {
  char name[HAL_NAME_LEN+1];
  int retval;
  char *data, *token;
  comp_id = hal_init(modname);
  if(comp_id < 0) {
    rtapi_print_msg(RTAPI_MSG_ERR, "%s: ERROR: hal_init() failed\n", modname);
    return -1;
  }

  if(board) {
    if(strncmp(board, "BBB", 3) == 0) {
      board_id = BBB;
    } else if(strncmp(board, "BBAI", 4)  == 0) {
      board_id = BBAI;
    } else {
      board_id = OTHER;
    }
  } else {
    board_id = OTHER;
  }

  char path[MAX_PATH_LENGTH];
  char gpioStr[MAX_GPIO_LENGTH];

  sysfs_pin_t *current = NULL;
  if (input_pins != NULL) {
    data = input_pins;
    while((token = strtok(data, ",")) != NULL) {
      if(current == NULL) {
	input_pin_root = hal_malloc(sizeof(sysfs_pin_t));
	current = input_pin_root;
      } else {
	current->next = hal_malloc(sizeof(sysfs_pin_t));
	current = current->next;
      }
      if(!current) {
	rtapi_print_msg(RTAPI_MSG_ERR, "%s: ERROR: hal_malloc() failed\n", modname);
	hal_exit(comp_id);
	return -1;
      }
      int pin = strtol(token, NULL, 10);
      int gpio = PIN2GPIO(board_id, pin);

      current->pin = pin;
      current->gpio = gpio;

      int r = O_RDONLY | O_SYNC;
      rtapi_snprintf(path, MAX_PATH_LENGTH, "/sys/class/gpio/gpio%d/value", gpio);
      rtapi_print_msg(RTAPI_MSG_DBG, "%s: path %s\n", modname, path);
      current->fd = open(path, r);

      if(current->fd == -1) {
	// If we failed to open the gpio value file descriptor
	// then perhaps we need to export it. 
	// Let's try it!
	int fd = open("/sys/class/gpio/export", O_WRONLY | O_SYNC);
	if(fd == -1) {
	  rtapi_print_msg(RTAPI_MSG_ERR, "%s: ERROR: unable to export pin %s\n", modname, token);
	  hal_exit(comp_id);
	  return -1;
	} else {
	  ssize_t bytes_written = rtapi_snprintf(gpioStr, MAX_GPIO_LENGTH, "%d", gpio);
	  rtapi_print_msg(RTAPI_MSG_DBG, "%s: attempting to export gpio %d to use pin %s, %s %d\n", modname, gpio, token, gpioStr, bytes_written);
	  write(fd, gpioStr, bytes_written);
	  close(fd);

	  // export successful, try opening the gpio value file again
	  current->fd = open(path, r);
	  if(current->fd == -1) {
	    // still failed, so error
	    rtapi_print_msg(RTAPI_MSG_ERR, "%s: ERROR: unable to open GPIO value file for pin %s\n", modname, token);
	    hal_exit(comp_id);
	    return -1;
	  }
	}
      }

      retval = hal_pin_bit_newf(HAL_OUT, &(current->value), comp_id, "sysfs_gpio.p%d", pin);
      if(retval < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR, "%s: ERROR: could not create pin sysfs_gpio.p%d", modname, pin);
	hal_exit(comp_id);
	return -1;
      }

      retval = hal_pin_bit_newf(HAL_IN, &(current->invert), comp_id, "sysfs_gpio.p%d.invert", pin);
      if(retval < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR, "%s: ERROR: could not create pin sysfs_gpio.p%d.invert", modname, pin);
	hal_exit(comp_id);
	return -1;
      }

      *(current->value) = 0;
      *(current->invert) = 0;

      data = NULL;
    }
  }

  current = NULL;
  if (output_pins != NULL) {
    data = output_pins;
    while((token = strtok(data, ",")) != NULL) {
      if(current == NULL) {
	output_pin_root = hal_malloc(sizeof(sysfs_pin_t));
	current = output_pin_root;
      } else {
	current->next = hal_malloc(sizeof(sysfs_pin_t));
	current = current->next;
      }
      if(!current) {
	rtapi_print_msg(RTAPI_MSG_ERR, "%s: ERROR: hal_malloc() failed\n", modname);
	hal_exit(comp_id);
	return -1;
      }
      int pin = strtol(token, NULL, 10);
      int gpio = PIN2GPIO(board_id, pin);

      current->pin = pin;
      current->gpio = gpio;

      int w = O_WRONLY | O_SYNC;
      rtapi_snprintf(path, MAX_PATH_LENGTH, "/sys/class/gpio/gpio%d/value", gpio);
      rtapi_print_msg(RTAPI_MSG_DBG, "%s: path %s\n", modname, path);
      current->fd = open(path, w);

      if(current->fd == -1) {
	// If we failed to open the gpio value file descriptor
	// then perhaps we need to export it. 
	// Let's try it!
	int fd = open("/sys/class/gpio/export", O_WRONLY | O_SYNC);
	if(fd == -1) {
	  rtapi_print_msg(RTAPI_MSG_ERR, "%s: ERROR: unable to export pin %s\n", modname, token);
	  hal_exit(comp_id);
	  return -1;
	} else {
	  ssize_t bytes_written = rtapi_snprintf(gpioStr, MAX_GPIO_LENGTH, "%d", gpio);
	  rtapi_print_msg(RTAPI_MSG_DBG, "%s: attempting to export gpio %d to use pin %s, %s %d\n", modname, gpio, token, gpioStr, bytes_written);
	  write(fd, gpioStr, bytes_written);
	  close(fd);

	  // export successful, try opening the gpio value file again
	  current->fd = open(path, w);
	  if(current->fd == -1) {
	    // still failed, so error
	    rtapi_print_msg(RTAPI_MSG_ERR, "%s: ERROR: unable to open GPIO value file for pin %s\n", modname, token);
	    hal_exit(comp_id);
	    return -1;
	  }
	}
      }

      retval = hal_pin_bit_newf(HAL_IN, &(current->value), comp_id, "sysfs_gpio.p%d", pin);
      if(retval < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR, "%s: ERROR: could not create pin sysfs_gpio.p%d", modname, pin);
	hal_exit(comp_id);
	return -1;
      }

      retval = hal_pin_bit_newf(HAL_IN, &(current->invert), comp_id, "sysfs_gpio.p%d.invert", pin);
      if(retval < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR, "%s: ERROR: could not create pin sysfs_gpio.p%d.invert", modname, pin);
	hal_exit(comp_id);
	return -1;
      }

      *(current->value) = 0;
      *(current->invert) = 0;

      data = NULL;
    }
  }

  rtapi_snprintf(name, sizeof(name), "sysfs_gpio.read");
  retval = hal_export_funct(name, read_input_pins, input_pin_root, 0, 0, comp_id);
  if(retval < 0) {
    rtapi_print_msg(RTAPI_MSG_ERR, "%s: ERROR: exporting read funct failed", modname);
    hal_exit(comp_id);
    return -1;
  }

  rtapi_snprintf(name, sizeof(name), "sysfs_gpio.write");
  retval = hal_export_funct(name, write_output_pins, output_pin_root, 0, 0, comp_id);
  if(retval < 0) {
    rtapi_print_msg(RTAPI_MSG_ERR, "%s: ERROR: exporting write funct failed", modname);
    hal_exit(comp_id);
    return -1;
  }

  rtapi_print_msg(RTAPI_MSG_INFO, "%s: installed driver\n", modname);
  hal_ready(comp_id);
  return 0;
}

void rtapi_app_exit(void) {
  hal_exit(comp_id);
}
