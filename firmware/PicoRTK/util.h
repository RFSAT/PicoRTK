// util.h - debug printing. USB carries the receiver stream when the USB bridge is on,
// so debug text goes to USB only when the bridge is off.
#pragma once
void dbg(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
