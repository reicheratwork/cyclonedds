#include "../inc/iox_interface.h"

#include <assert.h>

/*forward declarations of functions*/
static bool iox_compute_locator (
  ddsi_virtual_interface_t * self,
  struct ddsi_locator ** locator,
  struct ddsi_domaingv * gv);

static bool iox_match_locator (
  ddsi_virtual_interface_t * self,
  const struct ddsi_locator * locator);

static bool iox_topic_and_qos_supported (
  const struct dds_topic * topic,
  const dds_qos_t * qos);

static ddsi_virtual_interface_topic_t* iox_topic_create (
  ddsi_virtual_interface_t * vi,
  struct dds_topic * cyclone_topic);

static bool iox_topic_destruct (
  ddsi_virtual_interface_topic_t *vi_topic);

static bool iox_vi_deinit (
  ddsi_virtual_interface_t * self);

static bool iox_serialization_required (
  ddsi_virtual_interface_topic_t * topic);

static ddsi_virtual_interface_pipe_t* iox_pipe_open (
  ddsi_virtual_interface_topic_t * topic,
  void * cdds_counterpart);

static bool iox_pipe_close (
  ddsi_virtual_interface_pipe_t * pipe);

static memory_block_t* iox_request_loan (
  ddsi_virtual_interface_pipe_t * pipe,
  size_t size_requested);

static bool iox_return_block (
  ddsi_virtual_interface_pipe_t * pipe,
  memory_block_t * block);

static bool iox_return_loan (
  ddsi_virtual_interface_pipe_t * pipe,
  void * loan);

static bool iox_sink (
  ddsi_virtual_interface_pipe_t * pipe,
  struct ddsi_serdata * serdata);

static memory_block_t* iox_source (
  ddsi_virtual_interface_pipe_t * pipe);

static memory_block_t* iox_loan_origin (
  ddsi_virtual_interface_pipe_t * pipe,
  const void * sample);

static bool iox_set_on_source (
  ddsi_virtual_interface_pipe_t * pipe,
  ddsi_virtual_interface_on_data_func * on_data_func);

/*definitions of function containers*/
static const ddsi_virtual_interface_ops_t v_ops = {
  .compute_locator = iox_compute_locator,
  .match_locator = iox_match_locator,
  .topic_and_qos_supported = iox_topic_and_qos_supported,
  .topic_create = iox_topic_create,
  .topic_destruct = iox_topic_destruct,
  .deinit = iox_vi_deinit
};

static const ddsi_virtual_interface_topic_ops_t t_ops = {
  .serialization_required = iox_serialization_required,
  .pipe_open = iox_pipe_open,
  .pipe_close = iox_pipe_close
};

static const ddsi_virtual_interface_pipe_ops_t p_ops = {
  .request_loan = iox_request_loan,
  .return_loan = iox_return_loan,
  .return_block = iox_return_block,
  .originates_loan = iox_loan_origin,
  .sink_data = iox_sink,
  .source_data = iox_source,
  .set_on_source = iox_set_on_source
};

/*implementation of functions*/
static bool iox_compute_locator (
  ddsi_virtual_interface_t * self,
  struct ddsi_locator ** locator,
  struct ddsi_domaingv * gv)
{
  (void) self;
  (void) locator;
  (void) gv;
  return true;
}

static bool iox_match_locator (
  ddsi_virtual_interface_t * self,
  const struct ddsi_locator * locator)
{
  (void) self;
  (void) locator;
  return true;
}

static bool iox_topic_and_qos_supported (
  const struct dds_topic * topic,
  const dds_qos_t * qos)
{
  (void) topic;
  (void) qos;
  return true;
}


static ddsi_virtual_interface_topic_t* iox_topic_create (
  ddsi_virtual_interface_t * vi,
  struct dds_topic * cyclone_topic)
{
  assert(vi);
  assert(cyclone_topic);

  ddsi_virtual_interface_topic_t *ptr = dds_alloc(sizeof(ddsi_virtual_interface_topic_t));

  ptr->cyclone_topic = cyclone_topic;
  ptr->ops = t_ops;
  ptr->pipes = NULL;
  ptr->supports_loan = true;
  ptr->virtual_interface = vi;

  if (!add_topic_to_list(ptr, &vi->topics)) {
    dds_free(ptr);
    ptr = NULL;
  }

  return ptr;
}

static bool iox_topic_destruct (
  ddsi_virtual_interface_topic_t *vi_topic)
{
  assert(vi_topic);

  bool val = remove_topic_from_list(vi_topic, &vi_topic->virtual_interface->topics);

  dds_free(vi_topic);

  return val;
}

static bool iox_vi_deinit (
  ddsi_virtual_interface_t * self)
{
  while (self->topics) {
    if (!remove_topic_from_list(self->topics->topic, &self->topics))
      return false;
  }

  dds_free(self);

  return true;
}

static bool iox_serialization_required (
  ddsi_virtual_interface_topic_t * topic)
{
  (void) topic;

  return true;
}

static ddsi_virtual_interface_pipe_t* iox_pipe_open (
  ddsi_virtual_interface_topic_t * topic,
  void * cdds_counterpart)
{
  ddsi_virtual_interface_pipe_t *ptr = dds_alloc(sizeof(ddsi_virtual_interface_pipe_t));

  ptr->topic = topic;
  ptr->ops = p_ops;
  ptr->cdds_counterpart = cdds_counterpart;

  if (!add_pipe_to_list(ptr, &topic->pipes)) {
    dds_free(ptr);
    ptr = NULL;
  }

  return ptr;
}

static bool iox_pipe_close (
  ddsi_virtual_interface_pipe_t * pipe)
{
  assert(pipe);

  bool val = remove_pipe_from_list(pipe, &pipe->topic->pipes);

  dds_free(pipe);

  return val;
}

static memory_block_t* iox_request_loan (
  ddsi_virtual_interface_pipe_t * pipe,
  size_t size_requested)
{
  (void) pipe;
  (void) size_requested;

  return NULL;
}

static bool iox_return_block (
  ddsi_virtual_interface_pipe_t * pipe,
  memory_block_t * block)
{
  (void) pipe;
  (void) block;

  return true;
}

static bool iox_return_loan (
  ddsi_virtual_interface_pipe_t * pipe,
  void * loan)
{
  (void) pipe;
  (void) loan;

  return true;
}

static bool iox_sink (
  ddsi_virtual_interface_pipe_t * pipe,
  struct ddsi_serdata * serdata)
{
  (void) pipe;
  (void) serdata;

  return true;
}

static memory_block_t* iox_source (
  ddsi_virtual_interface_pipe_t * pipe)
{
  (void) pipe;

  return NULL;
}

static memory_block_t* iox_loan_origin (
  ddsi_virtual_interface_pipe_t * pipe,
  const void * sample)
{
  (void) pipe;
  (void) sample;

  return NULL;
}

static bool iox_set_on_source (
  ddsi_virtual_interface_pipe_t * pipe,
  ddsi_virtual_interface_on_data_func * on_data_func)
{
  (void) pipe;
  (void) on_data_func;

  return true;
}

static const char *interface_name = "iox";

bool iox_create_virtual_interface (
  ddsi_virtual_interface_t **virtual_interface,
  struct ddsi_domaingv *cyclone_domain,
  const char * configuration_string
)
{
  (void)configuration_string;
  assert(virtual_interface);
  *virtual_interface = dds_alloc(sizeof(**virtual_interface));

  (*virtual_interface)->ops = v_ops;
  (*virtual_interface)->cyclone_domain = cyclone_domain;
  (*virtual_interface)->interface_name = interface_name;
  return true;
}