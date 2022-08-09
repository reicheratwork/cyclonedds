#include "dds/ddsc/dds_loan.h"

#include "dds__entity.h"
#include "dds__types.h"
#include "dds__loan.h"
#include "dds/ddsi/q_entity.h"

#include <string.h>

bool dds_loaned_sample_fini(
  dds_loaned_sample_t *to_fini)
{
  assert(to_fini);

  if (to_fini->ops.fini)
    return to_fini->ops.fini(to_fini);
  else
    return true;
}

bool dds_loaned_sample_incr_refs(
  dds_loaned_sample_t *to_incr)
{
  assert(to_incr);

  if (to_incr->ops.incr)
    return to_incr->ops.incr(to_incr);
  else
    return true;
}

bool dds_loaned_sample_decr_refs(
  dds_loaned_sample_t *to_decr)
{
  assert(to_decr && to_decr->refs);

  if (to_decr->ops.decr && !to_decr->ops.decr(to_decr))
    return false;

  if (--to_decr->refs || !to_decr->ops.on_no_refs)
    return true;
  else
    return to_decr->ops.on_no_refs(to_decr);
}

dds_loan_manager_t *dds_loan_manager_create(
  uint32_t initial_cap)
{
  dds_loan_manager_t *mgr = dds_alloc(sizeof(dds_loan_manager_t));

  if (mgr)
  {
    mgr->n_samples_cap = initial_cap;
    mgr->samples = dds_alloc(sizeof(dds_loaned_sample_t*)*mgr->n_samples_cap);
    if (!mgr->samples)
      goto fail;
  }

  return mgr;

fail:
  dds_free(mgr);
  return NULL;
}

bool dds_loan_manager_fini(
  dds_loan_manager_t *to_fini)
{
  assert(to_fini);

  for (uint32_t i = 0; i < to_fini->n_samples_cap; i++)
  {
    dds_loaned_sample_t *s = to_fini->samples[i];

    if (s && s->ops.fini && !s->ops.fini(s))
      return false;
    else
      to_fini->samples[i] = NULL;
  }

  dds_free(to_fini->samples);
  dds_free(to_fini);

  return true;
}

bool dds_loan_manager_add_loan(
  dds_loan_manager_t *manager,
  dds_loaned_sample_t *to_add)
{
  assert(manager && to_add);

  //lookup
  uint32_t i = 0;
  for (; i < manager->n_samples_cap; i++)
  {
    if (!manager->samples[i])
      break;
  }

  //expand
  if (i == manager->n_samples_cap)
  {
    uint32_t newcap = manager->n_samples_cap*2;
    dds_loaned_sample_t **newsamples = dds_realloc(manager->samples, sizeof(dds_loaned_sample_t*)*newcap);
    if (!newsamples)
    {
      return false;
    }
    else
    {
      memset(newsamples+i, 0, sizeof(dds_loaned_sample_t*)*(newcap - i ));
      manager->samples = newsamples;
      manager->n_samples_cap = newcap;
    }
  }

  //add
  to_add->loan_idx = i;
  to_add->manager = manager;
  manager->samples[i] = to_add;

  return true;
}

bool dds_loan_manager_remove_loan(
  dds_loan_manager_t *manager,
  dds_loaned_sample_t *to_remove)
{
  assert(manager && to_remove);

  if (to_remove->loan_idx >= manager->n_samples_cap)
    return false;
  else if (to_remove != manager->samples[to_remove->loan_idx])
    return false;

  if (to_remove->ops.fini && !to_remove->ops.fini(to_remove))
    return false;

  manager->samples[to_remove->loan_idx] = NULL;
  return true;
}

dds_loaned_sample_t *dds_loan_manager_find_loan(
  const dds_loan_manager_t *manager,
  const void *sample)
{
  assert(manager && sample);

  for (uint32_t i = 0; i < manager->n_samples_cap; i++)
  {
    if (manager->samples[i] && manager->samples[i]->sample_ptr == sample)
      return manager->samples[i];
  }

  return NULL;
}

typedef struct dds_heap_loan {
  dds_loaned_sample_t c;
  //serdata/topic/writer?
} dds_heap_loan_t;

static bool heap_on_no_refs(
  dds_loaned_sample_t *to_on_no_refs)
{
  assert(to_on_no_refs && to_on_no_refs->refs == 0);

  dds_heap_loan_t *hl = (dds_heap_loan_t*)to_on_no_refs;

  //type specific cleanup/etc

  dds_free(hl);

  return true;
}

const dds_loaned_sample_ops_t dds_heap_loan_ops = {
  .fini = NULL,
  .incr = NULL,
  .decr = NULL,
  .on_no_refs = heap_on_no_refs
};

dds_loaned_sample_t* dds_heap_loan(uint32_t sz/*sertype/sample?*/)
{
  dds_loaned_sample_t *s = dds_alloc(sizeof(dds_heap_loan_t));

  s->ops = dds_heap_loan_ops;

  return s;
}
