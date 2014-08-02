
#include "units.h"


double adjust_unit(double bytes, Unit target_unit) {
    if (target_unit != Human) {
        return bytes / (double) target_unit;
    }
    return bytes / (double) find_unit(bytes);
}


Unit find_unit(double bytes) {
    if (bytes >= Gigabytes) {
        return Gigabytes;
    } else if (bytes >= Megabytes) {
        return Megabytes;
    } else if (bytes >= Kilobytes) {
        return Kilobytes;
    }
    return Bytes;
}


const char* unit_name(double bytes, Unit target_unit) {
    if (target_unit == Human) {
        target_unit = find_unit(bytes);
    }

    switch (target_unit) {
    case Gigabytes:
        return "GB";
        break;

    case Megabytes:
        return "MB";
        break;

    case Kilobytes:
        return "KB";
        break;

    case Bytes:
        return "B";
        break;

    default:
        break;
    }

    return "??";
}
