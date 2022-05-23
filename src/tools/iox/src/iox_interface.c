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

static bool iox_topic_supported (
  const struct dds_topic * topic);

static bool iox_qos_supported (
  const dds_qos_t * qos);

static ddsi_virtual_interface_topic_t* iox_topic_create (
  ddsi_virtual_interface_t * vi,
  struct dds_ktopic * cyclone_topic,
  struct ddsi_sertype * cyclone_sertype);

static bool iox_topic_destruct (
  ddsi_virtual_interface_topic_t *vi_topic);

static bool iox_vi_deinit (
  ddsi_virtual_interface_t * self);

static bool iox_serialization_required (
  ddsi_virtual_interface_topic_t * topic);

static ddsi_virtual_interface_pipe_t* iox_pipe_open (
  ddsi_virtual_interface_topic_t * topic,
  void * cdds_endpoint,
  virtual_interface_pipe_type_t pipe_type);

static bool iox_pipe_close (
  ddsi_virtual_interface_pipe_t * pipe);

static dds_loaned_sample_t* iox_request_loan (
  ddsi_virtual_interface_pipe_t * pipe,
  size_t size_requested);

static bool iox_ref_block (
  ddsi_virtual_interface_pipe_t * pipe,
  dds_loaned_sample_t * block);

static bool iox_unref_block (
  ddsi_virtual_interface_pipe_t * pipe,
  dds_loaned_sample_t * block);

static bool iox_sink (
  ddsi_virtual_interface_pipe_t * pipe,
  struct ddsi_serdata * serdata);

static struct ddsi_serdata* iox_source (
  ddsi_virtual_interface_pipe_t * pipe);

static dds_loaned_sample_t* iox_loan_origin (
  ddsi_virtual_interface_pipe_t * pipe,
  const void * sample);

static bool iox_set_on_source (
  ddsi_virtual_interface_pipe_t * pipe,
  struct dds_reader * reader);

/*definitions of function containers*/
static const ddsi_virtual_interface_ops_t v_ops = {
  .compute_locator = iox_compute_locator,
  .match_locator = iox_match_locator,
  .topic_supported = iox_topic_supported,
  .qos_supported = iox_qos_supported,
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
  .unref_block = iox_unref_block,
  .ref_block = iox_ref_block,
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

static bool iox_topic_supported (
  const struct dds_topic * topic)
{
  (void) topic;

  return true;
}

static bool iox_qos_supported (
  const dds_qos_t * qos)
{
  (void) qos;

  return true;
}


static ddsi_virtual_interface_topic_t* iox_topic_create (
  ddsi_virtual_interface_t * vi,
  struct dds_ktopic * cyclone_topic,
  struct ddsi_sertype * cyclone_sertype)
{
  (void) cyclone_topic;
  (void) cyclone_sertype;

  assert(vi);
  assert(cyclone_topic);
  assert(cyclone_sertype);

  ddsi_virtual_interface_topic_t *ptr = dds_alloc(sizeof(ddsi_virtual_interface_topic_t));

  if (!ptr)
    return ptr;

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

  while (vi_topic->pipes) {
    if (!remove_pipe_from_list(vi_topic->pipes->pipe, &vi_topic->pipes))
      return false;
  }

  dds_free(vi_topic);

  return true;
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
  void * cdds_endpoint,
  virtual_interface_pipe_type_t pipe_type)
{
  ddsi_virtual_interface_pipe_t *ptr = dds_alloc(sizeof(ddsi_virtual_interface_pipe_t));

  ptr->topic = topic;
  ptr->pipe_type = pipe_type;
  ptr->ops = p_ops;
  ptr->cdds_endpoint = cdds_endpoint;

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

  dds_free(pipe);

  return true;
}

static dds_loaned_sample_t* iox_request_loan (
  ddsi_virtual_interface_pipe_t * pipe,
  size_t size_requested)
{
  (void) pipe;
  (void) size_requested;

  return NULL;
}

static bool iox_ref_block (
  ddsi_virtual_interface_pipe_t * pipe,
  dds_loaned_sample_t * block)
{
  (void) pipe;
  (void) block;

  return true;
}

static bool iox_unref_block (
  ddsi_virtual_interface_pipe_t * pipe,
  dds_loaned_sample_t * block)
{
  (void) pipe;
  (void) block;

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

static struct ddsi_serdata* iox_source (
  ddsi_virtual_interface_pipe_t * pipe)
{
  (void) pipe;

  return NULL;
}

static dds_loaned_sample_t* iox_loan_origin (
  ddsi_virtual_interface_pipe_t * pipe,
  const void * sample)
{
  (void) pipe;
  (void) sample;

  return NULL;
}

static bool iox_set_on_source (
  ddsi_virtual_interface_pipe_t * pipe,
  struct dds_reader * reader)
{
  (void) pipe;
  (void) reader;

  return true;
}

static const char *interface_name = "iox";

bool iox_create_virtual_interface (
  ddsi_virtual_interface_t **virtual_interface,
  struct ddsi_domaingv *cyclone_domain,
  const char * configuration_string
)
{
  (void) configuration_string;
  (void) cyclone_domain;

  assert(virtual_interface);

  *virtual_interface = dds_alloc(sizeof(**virtual_interface));

  (*virtual_interface)->ops = v_ops;
  (*virtual_interface)->interface_name = interface_name;

  return true;
}
