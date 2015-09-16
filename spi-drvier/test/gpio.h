#ifndef ZML_GPIO_H
#define ZML_GPIO_H

#define IN              0
#define OUT             1
#define LOW_LEVEL       0
#define HIGH_LEVEL      1

#define PIN_AS_MODE         3   //1
#define PIN_EPCS_CONFIG     9   //0
#define PIN_SPI_EN0         37  //0


#define BUFFER_MAX      3
#define DIRECTION_MAX   48


int gpio_export(int pin);
int gpio_unexport(int pin);
int gpio_direction(int pin, int dir);
int gpio_read(int pin);
int gpio_write(int pin, int value);


#endif
