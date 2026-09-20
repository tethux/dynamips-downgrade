#ifndef DYNAMIPS_PUBLIC_DYNAMIPS_H
#define DYNAMIPS_PUBLIC_DYNAMIPS_H

#ifdef __cplusplus
extern "C" {
#endif

const char *dyn_hello(void);

int dyn_runtime_init(int argc, char *argv[]);
void dyn_runtime_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif
