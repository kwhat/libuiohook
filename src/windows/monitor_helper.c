#include "monitor_helper.h"

static LONG left = 0;
static LONG top = 0;

static BOOL CALLBACK enum_monitor_proc(HMONITOR h_monitor, HDC hdc, LPRECT lp_rect, LPARAM dwData) {
    MONITORINFO MonitorInfo = {0};
    MonitorInfo.cbSize = sizeof(MonitorInfo);
    if (GetMonitorInfo(h_monitor, &MonitorInfo)) {
        if (MonitorInfo.rcMonitor.left < left) {
            left = MonitorInfo.rcMonitor.left;
        }
        if (MonitorInfo.rcMonitor.top < top) {
            top = MonitorInfo.rcMonitor.top;
        }
    }
    return TRUE;
}

void enumerate_displays()
{
    // Reset coordinates because if a negative monitor moves to positive space,
    // it will still look like there is some monitor in negative space.
    left = 0;
    top = 0;

    EnumDisplayMonitors(NULL, NULL, enum_monitor_proc, 0);
}

LARGESTNEGATIVECOORDINATES get_largest_negative_coordinates()
{
    LARGESTNEGATIVECOORDINATES lnc = {
            .left = left,
            .top = top
    };
    return lnc;
}