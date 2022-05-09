#include "dds/dds.h"

#if defined (__cplusplus)
extern "C" {
#endif

DDS_EXPORT bool iox_create_virtual_interface (
  ddsi_virtual_interface_t **virtual_interface,
  struct ddsi_domaingv *cyclone_domain,
  const char * configuration_string
);

#if defined (__cplusplus)
}
#endif
