#include <string.h>
#include <stdio.h>

short testme(const char *str)
{
    printf("%s with arg %p\n", __FUNCTION__, str);
    printf("len of '%s' is %d\n", str, strlen(str));

    return 42;
}

void doubletest(double d)
{
    printf("%s with arg %g (%g)\n", __FUNCTION__, (float) d, 3.1f);
}

void floattest(float d)
{
    printf("%s with arg %g (%g)\n", __FUNCTION__, (float) d, 3.1f);
}

void chartest(char d)
{
    printf("%s with arg %d\n", __FUNCTION__, (int) d);
}

struct test_t {
    int a;
    int b;
};

struct test_t structtest(int a, int b)
{
    struct test_t res = {a, b};
    printf("building a = %d, b = %d into %p\n", res.a, res.b, &res);
    return res;
}

void structtest2(struct test_t s)
{
    printf("a = %d, b = %d into %p\n", s.a, s.b, &s);
}

void structtest3(struct test_t *s)
{
    printf("a = %d, b = %d into %p\n", s->a, s->b, s);
}
