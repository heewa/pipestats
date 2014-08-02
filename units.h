
#ifndef __UNITS_H__
#define __UNITS_H__

typedef enum Unit {
    Human = 0,
    Bytes = 1,
    Kilobytes = 1024,
    Megabytes = 1024 * 1024,
    Gigabytes = 1024 * 1024 * 1024,
} Unit;

double adjust_unit(double bytes, Unit target_unit);

const char* unit_name(double bytes, Unit target_unit);

Unit find_unit(double bytes);

#endif

