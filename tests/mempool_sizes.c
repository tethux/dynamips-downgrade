#include "registry.h"
#include "atm.h"
#include "frame_relay.h"

size_t mempool_test_record_size(unsigned index) {
  const size_t sizes[] = {sizeof(registry_entry_t), sizeof(atmsw_vp_conn_t),
                          sizeof(atmsw_vc_conn_t), sizeof(frsw_conn_t)};
  return sizes[index % 4];
}
