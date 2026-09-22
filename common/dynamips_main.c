/*
 * Cisco router simulation platform.
 * Copyright (c) 2005,2006 Christophe Fillot (cf@utc.fr)
 * Patched by Jeremy Grossmann for the GNS3 project (www.gns3.net)
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <dynamips/dynamips.h>

#include "cpu.h"
#include "dynamips.h"
#include "hypervisor.h"
#include "vm.h"

extern int hypervisor_mode;
extern int hypervisor_tcp_port;
extern char *hypervisor_ip_address;

int main(int argc, char *argv[]) {
  vm_instance_t *vm;

  dynamips_set_hypervisor_stop_handler(hypervisor_stopsig);
  if (dyn_runtime_init(argc, argv) != DYN_OK)
    return (EXIT_FAILURE);

  if (!hypervisor_mode) {
    /* Initialize the default instance */
    vm = vm_acquire("default");
    assert(vm != NULL);

    if (vm_init_instance(vm) == -1) {
      fprintf(stderr, "Unable to initialize router instance.\n");
      exit(EXIT_FAILURE);
    }

#if (DEBUG_INSN_PERF_CNT > 0) || (DEBUG_BLOCK_PERF_CNT > 0)
    {
      m_uint32_t counter, prev = 0, delta;
      while (vm->status == VM_STATUS_RUNNING) {
        counter = cpu_get_perf_counter(vm->boot_cpu);
        delta = counter - prev;
        prev = counter;
        printf("delta = %u\n", delta);
        sleep(1);
      }
    }
#else
    /* Start instance monitoring */
    vm_monitor(vm);
#endif

    /* Free resources used by instance */
    vm_release(vm);
  } else {
    hypervisor_tcp_server(hypervisor_ip_address, hypervisor_tcp_port);
  }

  dyn_runtime_shutdown();
  return (EXIT_SUCCESS);
}
