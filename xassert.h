/* xassert.h */

#define xassert(expr) \
    ((void)((expr)|| (minitip_assert(#expr,__FILE__,__LINE__),1)))

void minitip_assert(const char *expr,const char *file, int line);

