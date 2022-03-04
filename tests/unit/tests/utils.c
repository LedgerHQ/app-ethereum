#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

int local_strchr(char *string, char ch);

static void null_test_success(void **state) {
    assert_int_equal(local_strchr("salut", 'c'), -1);
    assert_int_equal(local_strchr("av", 'a'), 0);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(null_test_success),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}