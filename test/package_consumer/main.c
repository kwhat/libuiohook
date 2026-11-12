#include <uiohook.h>

// This target verifies that the exported include and link interfaces work in an
// independent project. It does not install a hook or require runtime permissions.
int main(void) {
    return sizeof(uiohook_event) == 0;
}
