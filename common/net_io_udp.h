#pragma once

#include <stddef.h>
#include <sys/types.h>

struct netio_desc;
typedef int (*netio_udp_handler)(struct netio_desc *, unsigned char *, ssize_t,
                                 void *, void *);

#ifdef __cplusplus
extern "C" {
#endif
int netio_udp_rxl_init(void);
void netio_udp_rxl_shutdown(void);
int netio_udp_rxl_add(struct netio_desc *, netio_udp_handler, void *, void *);
int netio_udp_rxl_remove(struct netio_desc *, int all);
int netio_udp_rxl_connect(struct netio_desc *, char *, int);
ssize_t netio_receive_dispatch(struct netio_desc *, void *, ssize_t);
#ifdef __cplusplus
}

#include <dynamips/macros.h>
#include <expected>
#include <system_error>

namespace dynamips::nio {
fn listen_udp(netio_desc &endpoint, netio_udp_handler handler, void *arg1,
                void *arg2) noexcept -> std::expected<void, std::error_code>;
fn unlisten_udp(netio_desc &endpoint, bool all = false) noexcept
    -> std::expected<void, std::error_code>;
} // namespace dynamips::nio
#endif
