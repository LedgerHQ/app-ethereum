#include <string.h>

int local_strchr_demo(char *string, char ch) {
    unsigned int length = strlen(string);
    unsigned int i;
    for (i = 0; i < length; i++) {
        if (string[i] == ch) {
            return i;
        }
    }
    return -1;
}
