/*
 * Copyright(c) 2006 to 2022 ZettaScale Technology and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#include <stdlib.h>
#include <ctype.h>
#include <assert.h>
#include <string.h>

#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/log.h"
#include "dds/ddsrt/md5.h"
#include "dds/ddsrt/mh3.h"
#include "dds/ddsi/q_bswap.h"
#include "dds/ddsi/ddsi_config_impl.h"
#include "dds/ddsi/q_freelist.h"
#include "dds/ddsi/ddsi_tkmap.h"
#include "dds/ddsi/ddsi_cdrstream.h"
#include "dds/ddsi/q_radmin.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/ddsi/ddsi_serdata_default.h"

/* 8k entries in the freelist seems to be roughly the amount needed to send
   minimum-size (well, 4 bytes) samples as fast as possible over loopback
   while using large messages -- actually, it stands to reason that this would
   be the same as the WHC node pool size */
#define MAX_POOL_SIZE 8192
#define MAX_SIZE_FOR_POOL 256
#define DEFAULT_NEW_SIZE 128
#define CHUNK_SIZE 128

#ifndef NDEBUG
static int ispowerof2_size (size_t x)
{
  return x > 0 && !(x & (x-1));
}
#endif

static size_t alignup_size (size_t x, size_t a);

struct serdatapool * ddsi_serdatapool_new (void)
{
  struct serdatapool * pool;
  pool = ddsrt_malloc (sizeof (*pool));
  nn_freelist_init (&pool->freelist, MAX_POOL_SIZE, offsetof (struct ddsi_serdata_default, next));
  return pool;
}

static void serdata_free_wrap (void *elem)
{
#ifndef NDEBUG
  struct ddsi_serdata_default *d = elem;
  assert(ddsrt_atomic_ld32(&d->c.refc) == 0);
#endif
  dds_free(elem);
}

void ddsi_serdatapool_free (struct serdatapool * pool)
{
  nn_freelist_fini (&pool->freelist, serdata_free_wrap);
  ddsrt_free (pool);
}

static size_t alignup_size (size_t x, size_t a)
{
  size_t m = a-1;
  assert (ispowerof2_size (a));
  return (x+m) & ~m;
}

static void *serdata_default_append (struct ddsi_serdata_default **d, size_t n)
{
  char *p;
  if ((*d)->pos + n > (*d)->size)
  {
    size_t size1 = alignup_size ((*d)->pos + n, CHUNK_SIZE);
    *d = ddsrt_realloc (*d, offsetof (struct ddsi_serdata_default, data) + size1);
    (*d)->size = (uint32_t)size1;
  }
  assert ((*d)->pos + n <= (*d)->size);
  p = (*d)->data + (*d)->pos;
  (*d)->pos += (uint32_t)n;
  return p;
}

static void serdata_default_append_blob (struct ddsi_serdata_default **d, size_t sz, const void *data)
{
  char *p = serdata_default_append (d, sz);
  memcpy (p, data, sz);
}

static const unsigned char *serdata_default_keybuf(const struct ddsi_serdata_default *d)
{
  assert(d->key.buftype != KEYBUFTYPE_UNSET);
  return (d->key.buftype == KEYBUFTYPE_STATIC) ? d->key.u.stbuf : d->key.u.dynbuf;
}

static struct ddsi_serdata *fix_serdata_default(struct ddsi_serdata_default *d, uint32_t basehash)
{
  assert (d->key.keysize > 0); // we use a different function for implementing the keyless case
  d->c.hash = ddsrt_mh3 (serdata_default_keybuf(d), d->key.keysize, basehash); // FIXME: or the full buffer, regardless of actual size?
  return &d->c;
}

static struct ddsi_serdata *fix_serdata_default_nokey(struct ddsi_serdata_default *d, uint32_t basehash)
{
  d->c.hash = basehash;
  return &d->c;
}

static uint32_t serdata_default_get_size(const struct ddsi_serdata *dcmn)
{
  const struct ddsi_serdata_default *d = (const struct ddsi_serdata_default *) dcmn;
  return d->pos + (uint32_t)sizeof (struct CDRHeader);
}

static bool serdata_default_eqkey(const struct ddsi_serdata *acmn, const struct ddsi_serdata *bcmn)
{
  const struct ddsi_serdata_default *a = (const struct ddsi_serdata_default *)acmn;
  const struct ddsi_serdata_default *b = (const struct ddsi_serdata_default *)bcmn;
  assert (a->key.buftype != KEYBUFTYPE_UNSET && b->key.buftype != KEYBUFTYPE_UNSET);
  return a->key.keysize == b->key.keysize && memcmp (serdata_default_keybuf(a), serdata_default_keybuf(b), a->key.keysize) == 0;
}

static bool serdata_default_eqkey_nokey (const struct ddsi_serdata *acmn, const struct ddsi_serdata *bcmn)
{
  (void)acmn; (void)bcmn;
  return true;
}

static void serdata_default_free(struct ddsi_serdata *dcmn)
{
  struct ddsi_serdata_default *d = (struct ddsi_serdata_default *)dcmn;
  assert(ddsrt_atomic_ld32(&d->c.refc) == 0);

  if (d->key.buftype == KEYBUFTYPE_DYNALLOC)
    ddsrt_free(d->key.u.dynbuf);

  if (d->c.loan)
    dds_loaned_sample_decr_refs(d->c.loan);

  if (d->size > MAX_SIZE_FOR_POOL || !nn_freelist_push (&d->serpool->freelist, d))
    dds_free (d);
}

static void serdata_default_init(struct ddsi_serdata_default *d, const struct ddsi_sertype_default *tp, enum ddsi_serdata_kind kind, uint32_t xcdr_version)
{
  ddsi_serdata_init (&d->c, &tp->c, kind);
  d->pos = 0;
#ifndef NDEBUG
  d->fixed = false;
#endif
  if (xcdr_version != CDR_ENC_VERSION_UNDEF)
    d->hdr.identifier = ddsi_sertype_get_native_enc_identifier (xcdr_version, tp->encoding_format);
  else
    d->hdr.identifier = 0;
  d->hdr.options = 0;
  d->key.buftype = KEYBUFTYPE_UNSET;
  d->key.keysize = 0;
}

static struct ddsi_serdata_default *serdata_default_allocnew (struct serdatapool *serpool, uint32_t init_size)
{
  struct ddsi_serdata_default *d = ddsrt_malloc (offsetof (struct ddsi_serdata_default, data) + init_size);
  d->size = init_size;
  d->serpool = serpool;
  return d;
}

static struct ddsi_serdata_default *serdata_default_new_size (const struct ddsi_sertype_default *tp, enum ddsi_serdata_kind kind, uint32_t size, uint32_t xcdr_version)
{
  struct ddsi_serdata_default *d;
  if (size <= MAX_SIZE_FOR_POOL && (d = nn_freelist_pop (&tp->serpool->freelist)) != NULL)
    ddsrt_atomic_st32 (&d->c.refc, 1);
  else if ((d = serdata_default_allocnew (tp->serpool, size)) == NULL)
    return NULL;
  serdata_default_init (d, tp, kind, xcdr_version);
  return d;
}

static struct ddsi_serdata_default *serdata_default_new (const struct ddsi_sertype_default *tp, enum ddsi_serdata_kind kind, uint32_t xcdr_version)
{
  return serdata_default_new_size (tp, kind, DEFAULT_NEW_SIZE, xcdr_version);
}

static inline bool is_valid_xcdr_id (unsigned short cdr_identifier)
{
  /* PL_CDR_(L|B)E version 1 only supported for discovery data, using ddsi_serdata_plist */
  return (cdr_identifier == CDR_LE || cdr_identifier == CDR_BE
    || cdr_identifier == CDR2_LE || cdr_identifier == CDR2_BE
    || cdr_identifier == D_CDR2_LE || cdr_identifier == D_CDR2_BE
    || cdr_identifier == PL_CDR2_LE || cdr_identifier == PL_CDR2_BE);
}

enum gen_serdata_key_input_kind {
  GSKIK_SAMPLE,
  GSKIK_CDRSAMPLE,
  GSKIK_CDRKEY
};

static inline bool is_topic_fixed_key(uint32_t flagset, uint32_t xcdrv)
{
  if (xcdrv == CDR_ENC_VERSION_1)
    return flagset & DDS_TOPIC_FIXED_KEY;
  else if (xcdrv == CDR_ENC_VERSION_2)
    return flagset & DDS_TOPIC_FIXED_KEY_XCDR2;
  assert (0);
  return false;
}

static bool gen_serdata_key (const struct ddsi_sertype_default *type, struct ddsi_serdata_default_key *kh, enum gen_serdata_key_input_kind input_kind, void *input)
{
  const struct ddsi_sertype_default_desc *desc = &type->type;
  struct dds_istream *is = NULL;
  kh->buftype = KEYBUFTYPE_UNSET;
  if (desc->keys.nkeys == 0)
  {
    kh->buftype = KEYBUFTYPE_STATIC;
    kh->keysize = 0;
  }
  else if (input_kind == GSKIK_CDRKEY)
  {
    is = input;
    if (is->m_xcdr_version == CDR_ENC_VERSION_2)
    {
      kh->buftype = KEYBUFTYPE_DYNALIAS;
      assert (is->m_size < (1u << 30));
      kh->keysize = is->m_size & SERDATA_DEFAULT_KEYSIZE_MASK;
      kh->u.dynbuf = (unsigned char *) is->m_buffer;
    }
  }

  if (kh->buftype == KEYBUFTYPE_UNSET)
  {
    // Force the key in the serdata object to be serialized in XCDR2 format
    dds_ostream_t os;
    dds_ostream_init (&os, 0, CDR_ENC_VERSION_2);
    if (is_topic_fixed_key(desc->flagset, CDR_ENC_VERSION_2))
    {
      // FIXME: there are more cases where we don't have to allocate memory
      os.m_buffer = kh->u.stbuf;
      os.m_size = DDS_FIXED_KEY_MAX_SIZE;
    }
    switch (input_kind)
    {
      case GSKIK_SAMPLE:
        dds_stream_write_key (&os, input, type);
        break;
      case GSKIK_CDRSAMPLE:
        if (!dds_stream_extract_key_from_data (input, &os, type))
          return false;
        break;
      case GSKIK_CDRKEY:
        assert (is);
        assert (is->m_xcdr_version == CDR_ENC_VERSION_1);
        dds_stream_extract_key_from_key (is, &os, type);
        break;
    }
    assert (os.m_index < (1u << 30));
    kh->keysize = os.m_index & SERDATA_DEFAULT_KEYSIZE_MASK;
    if (is_topic_fixed_key (desc->flagset, CDR_ENC_VERSION_2))
      kh->buftype = KEYBUFTYPE_STATIC;
    else
    {
      kh->buftype = KEYBUFTYPE_DYNALLOC;
      kh->u.dynbuf = ddsrt_realloc (os.m_buffer, os.m_index); // don't waste bytes FIXME: maybe should be willing to waste a little
    }
  }
  return true;
}

static bool gen_serdata_key_from_sample (const struct ddsi_sertype_default *type, struct ddsi_serdata_default_key *kh, const char *sample)
{
  return gen_serdata_key (type, kh, GSKIK_SAMPLE, (void *) sample);
}

static bool gen_serdata_key_from_cdr (dds_istream_t * __restrict is, struct ddsi_serdata_default_key * __restrict kh, const struct ddsi_sertype_default * __restrict type, const bool just_key)
{
  return gen_serdata_key (type, kh, just_key ? GSKIK_CDRKEY : GSKIK_CDRSAMPLE, is);
}

/* Construct a serdata from a fragchain received over the network */
static struct ddsi_serdata_default *serdata_default_from_ser_common (const struct ddsi_sertype *tpcmn, enum ddsi_serdata_kind kind, const struct nn_rdata *fragchain, size_t size)
{
  const struct ddsi_sertype_default *tp = (const struct ddsi_sertype_default *)tpcmn;

  /* FIXME: check whether this really is the correct maximum: offsets are relative
     to the CDR header, but there are also some places that use a serdata as-if it
     were a stream, and those use offsets (m_index) relative to the start of the
     serdata */
  if (size > UINT32_MAX - offsetof (struct ddsi_serdata_default, hdr))
    return NULL;
  struct ddsi_serdata_default *d = serdata_default_new_size (tp, kind, (uint32_t) size, CDR_ENC_VERSION_UNDEF);
  if (d == NULL)
    return NULL;

  uint32_t off = 4; /* must skip the CDR header */

  assert (fragchain->min == 0);
  assert (fragchain->maxp1 >= off); /* CDR header must be in first fragment */

  memcpy (&d->hdr, NN_RMSG_PAYLOADOFF (fragchain->rmsg, NN_RDATA_PAYLOAD_OFF (fragchain)), sizeof (d->hdr));
  if (!is_valid_xcdr_id (d->hdr.identifier))
    goto err;

  while (fragchain)
  {
    assert (fragchain->min <= off);
    assert (fragchain->maxp1 <= size);
    if (fragchain->maxp1 > off)
    {
      /* only copy if this fragment adds data */
      const unsigned char *payload = NN_RMSG_PAYLOADOFF (fragchain->rmsg, NN_RDATA_PAYLOAD_OFF (fragchain));
      serdata_default_append_blob (&d, fragchain->maxp1 - off, payload + off - fragchain->min);
      off = fragchain->maxp1;
    }
    fragchain = fragchain->nextfrag;
  }

  const bool needs_bswap = !CDR_ENC_IS_NATIVE (d->hdr.identifier);
  d->hdr.identifier = CDR_ENC_TO_NATIVE (d->hdr.identifier);
  const uint32_t pad = ddsrt_fromBE2u (d->hdr.options) & 2;
  const uint32_t xcdr_version = ddsi_sertype_enc_id_xcdr_version (d->hdr.identifier);
  const uint32_t encoding_format = ddsi_sertype_enc_id_enc_format (d->hdr.identifier);
  if (encoding_format != tp->encoding_format)
    goto err;

  uint32_t actual_size;
  if (d->pos < pad || !dds_stream_normalize (d->data, d->pos - pad, needs_bswap, xcdr_version, tp, kind == SDK_KEY, &actual_size))
    goto err;

  dds_istream_t is;
  dds_istream_init (&is, actual_size, d->data, xcdr_version);
  if (!gen_serdata_key_from_cdr (&is, &d->key, tp, kind == SDK_KEY))
    goto err;
  // for (int n = 0; n < d->key.keysize; n++) {
  //   if (d->key.buftype == KEYBUFTYPE_DYNALLOC || d->key.buftype == KEYBUFTYPE_DYNALIAS)
  //     printf("%02x ", d->key.u.dynbuf[n]);
  //   else
  //     printf("%02x ", d->key.u.stbuf[n]);
  // }
  // printf("\n");
  return d;

err:
  ddsi_serdata_unref (&d->c);
  return NULL;
}

static struct ddsi_serdata_default *serdata_default_from_ser_iov_common (const struct ddsi_sertype *tpcmn, enum ddsi_serdata_kind kind, ddsrt_msg_iovlen_t niov, const ddsrt_iovec_t *iov, size_t size)
{
  const struct ddsi_sertype_default *tp = (const struct ddsi_sertype_default *)tpcmn;

  /* FIXME: check whether this really is the correct maximum: offsets are relative
     to the CDR header, but there are also some places that use a serdata as-if it
     were a stream, and those use offsets (m_index) relative to the start of the
     serdata */
  if (size > UINT32_MAX - offsetof (struct ddsi_serdata_default, hdr))
    return NULL;
  assert (niov >= 1);
  if (iov[0].iov_len < 4) /* CDR header */
    return NULL;
  struct ddsi_serdata_default *d = serdata_default_new_size (tp, kind, (uint32_t) size, CDR_ENC_VERSION_UNDEF);
  if (d == NULL)
    return NULL;

  memcpy (&d->hdr, iov[0].iov_base, sizeof (d->hdr));
  if (!is_valid_xcdr_id (d->hdr.identifier))
    goto err;
  serdata_default_append_blob (&d, iov[0].iov_len - 4, (const char *) iov[0].iov_base + 4);
  for (ddsrt_msg_iovlen_t i = 1; i < niov; i++)
    serdata_default_append_blob (&d, iov[i].iov_len, iov[i].iov_base);

  const bool needs_bswap = !CDR_ENC_IS_NATIVE (d->hdr.identifier);
  d->hdr.identifier = CDR_ENC_TO_NATIVE (d->hdr.identifier);
  const uint32_t pad = ddsrt_fromBE2u (d->hdr.options) & 2;
  const uint32_t xcdr_version = ddsi_sertype_enc_id_xcdr_version (d->hdr.identifier);
  const uint32_t encoding_format = ddsi_sertype_enc_id_enc_format (d->hdr.identifier);
  if (encoding_format != tp->encoding_format)
    goto err;

  uint32_t actual_size;
  if (d->pos < pad || !dds_stream_normalize (d->data, d->pos - pad, needs_bswap, xcdr_version, tp, kind == SDK_KEY, &actual_size))
    goto err;

  dds_istream_t is;
  dds_istream_init (&is, actual_size, d->data, xcdr_version);
  if (!gen_serdata_key_from_cdr (&is, &d->key, tp, kind == SDK_KEY))
    goto err;
  return d;

err:
  ddsi_serdata_unref (&d->c);
  return NULL;
}

static struct ddsi_serdata *serdata_default_from_ser (const struct ddsi_sertype *tpcmn, enum ddsi_serdata_kind kind, const struct nn_rdata *fragchain, size_t size)
{
  struct ddsi_serdata_default *d;
  if ((d = serdata_default_from_ser_common (tpcmn, kind, fragchain, size)) == NULL)
    return NULL;
  return fix_serdata_default (d, tpcmn->serdata_basehash);
}

static struct ddsi_serdata *serdata_default_from_ser_iov (const struct ddsi_sertype *tpcmn, enum ddsi_serdata_kind kind, ddsrt_msg_iovlen_t niov, const ddsrt_iovec_t *iov, size_t size)
{
  struct ddsi_serdata_default *d;
  if ((d = serdata_default_from_ser_iov_common (tpcmn, kind, niov, iov, size)) == NULL)
    return NULL;
  return fix_serdata_default (d, tpcmn->serdata_basehash);
}

static struct ddsi_serdata *serdata_default_from_ser_nokey (const struct ddsi_sertype *tpcmn, enum ddsi_serdata_kind kind, const struct nn_rdata *fragchain, size_t size)
{
  struct ddsi_serdata_default *d;
  if ((d = serdata_default_from_ser_common (tpcmn, kind, fragchain, size)) == NULL)
    return NULL;
  return fix_serdata_default_nokey (d, tpcmn->serdata_basehash);
}

static struct ddsi_serdata *serdata_default_from_ser_iov_nokey (const struct ddsi_sertype *tpcmn, enum ddsi_serdata_kind kind, ddsrt_msg_iovlen_t niov, const ddsrt_iovec_t *iov, size_t size)
{
  struct ddsi_serdata_default *d;
  if ((d = serdata_default_from_ser_iov_common (tpcmn, kind, niov, iov, size)) == NULL)
    return NULL;
  return fix_serdata_default_nokey (d, tpcmn->serdata_basehash);
}

static struct ddsi_serdata *ddsi_serdata_from_keyhash_cdr (const struct ddsi_sertype *tpcmn, const ddsi_keyhash_t *keyhash)
{
  const struct ddsi_sertype_default *tp = (const struct ddsi_sertype_default *)tpcmn;
  if (!is_topic_fixed_key (tp->type.flagset, CDR_ENC_VERSION_2))
  {
    /* keyhash is MD5 of a key value, so impossible to turn into a key value */
    return NULL;
  }
  else
  {
    const ddsrt_iovec_t iovec[2] = {
      { .iov_base = (unsigned char[]) { 0,0,0,0 }, .iov_len = 4}, // big-endian, unspecified padding
      { .iov_base = (void *) keyhash->value, .iov_len = (ddsrt_iov_len_t) sizeof (*keyhash) }
    };
    return serdata_default_from_ser_iov (tpcmn, SDK_KEY, 2, iovec, 4 + sizeof (*keyhash));
  }
}

static struct ddsi_serdata *ddsi_serdata_from_keyhash_cdr_nokey (const struct ddsi_sertype *tpcmn, const ddsi_keyhash_t *keyhash)
{
  const struct ddsi_sertype_default *tp = (const struct ddsi_sertype_default *)tpcmn;
  /* For keyless topic, the CDR encoding version is irrelevant */
  struct ddsi_serdata_default *d = serdata_default_new(tp, SDK_KEY, CDR_ENC_VERSION_UNDEF);
  if (d == NULL)
    return NULL;
  (void)keyhash;
  return fix_serdata_default_nokey(d, tp->c.serdata_basehash);
}

static struct ddsi_serdata *serdata_default_from_loaned_sample(const struct ddsi_sertype *tpcmn, enum ddsi_serdata_kind kind, const char *sample, dds_loaned_sample_t *loan, bool force_serialization)
{
  /*
    type = the type of data being serialized
    kind = the kind of data contained (key or normal)
    sample = the raw sample made into the serdata
    loan = the loaned buffer in use
    force_serialization = whether the contents of the loaned sample should be serialized
  */

  const struct ddsi_sertype_default *t = (const struct ddsi_sertype_default *)tpcmn;

  bool serialize_data = (force_serialization || 0 == tpcmn->fixed_size);

  struct ddsi_serdata_default *d = NULL;
  if (serialize_data) {
    //maybe if there is a loan and that loan is not the sample, use the loan block as the serialization buffer?
    d = (struct ddsi_serdata_default *)tpcmn->serdata_ops->from_sample (tpcmn, kind, sample);
  } else {
    d = serdata_default_new (t, kind, t->write_encoding_version);

    if(d == NULL || !gen_serdata_key_from_sample (t, &d->key, sample))
      return NULL;
  }

  if (d) {
    d->c.loan = loan;
    if (loan->sample_ptr != sample) {
      assert (loan->sample_state == LOANED_SAMPLE_STATE_UNITIALIZED);
      if (tpcmn->fixed_size) {
        loan->sample_state = LOANED_SAMPLE_STATE_RAW;
        memcpy(loan->sample_ptr, sample, loan->sample_size);
      } else {
        loan->sample_state = (kind == SDK_KEY ? LOANED_SAMPLE_STATE_SERIALIZED_KEY : LOANED_SAMPLE_STATE_SERIALIZED_DATA);
        memcpy(loan->sample_ptr, d->data, loan->sample_size);
      }
    } else {
      loan->sample_state = LOANED_SAMPLE_STATE_RAW;
    }
  }

  return (struct ddsi_serdata *)d;
}


static struct ddsi_serdata_default *serdata_default_from_sample_cdr_common (const struct ddsi_sertype *tpcmn, enum ddsi_serdata_kind kind, uint32_t xcdr_version, const void *sample)
{
  const struct ddsi_sertype_default *tp = (const struct ddsi_sertype_default *)tpcmn;
  struct ddsi_serdata_default *d = serdata_default_new(tp, kind, xcdr_version);
  if (d == NULL)
    return NULL;

  dds_ostream_t os;
  dds_ostream_from_serdata_default (&os, d);
  switch (kind)
  {
    case SDK_EMPTY:
      dds_ostream_add_to_serdata_default (&os, &d);
      break;
    case SDK_KEY:
      dds_stream_write_key (&os, sample, tp);
      dds_ostream_add_to_serdata_default (&os, &d);

      /* FIXME: detect cases where the XCDR1 and 2 representations are equal,
         so that we could alias the XCDR1 key from d->data */
      /* FIXME: use CDR encoding version used by the writer, not the write encoding
         of the sertype */
      if (tp->write_encoding_version == CDR_ENC_VERSION_2)
      {
        d->key.buftype = KEYBUFTYPE_DYNALIAS;
        // dds_ostream_add_to_serdata_default pads the size to a multiple of 4,
        // writing the number of padding bytes added in the 2 least-significant
        // bits of options (when interpreted as a big-endian 16-bit number) in
        // accordance with the XTypes spec.
        //
        // Those padding bytes are not part of the key!
        assert (ddsrt_fromBE2u (d->hdr.options) < 4);
        d->key.keysize = (d->pos - ddsrt_fromBE2u (d->hdr.options)) & SERDATA_DEFAULT_KEYSIZE_MASK;
        d->key.u.dynbuf = (unsigned char *) d->data;
      }
      else
      {
        /* We have a XCDR1 key, so this must be converted to XCDR2 to store
           it as key in this serdata. */
        if (!gen_serdata_key_from_sample (tp, &d->key, sample))
          return NULL;
      }
      break;
    case SDK_DATA:
      if (!dds_stream_write_sample (&os, sample, tp))
        return NULL;
      dds_ostream_add_to_serdata_default (&os, &d);
      if (!gen_serdata_key_from_sample (tp, &d->key, sample))
        return NULL;
      break;
  }
  return d;
}

static struct ddsi_serdata *serdata_default_from_sample_data_representation (const struct ddsi_sertype *tpcmn, enum ddsi_serdata_kind kind, dds_data_representation_id_t data_representation, const void *sample, bool key)
{
  assert (data_representation == DDS_DATA_REPRESENTATION_XCDR1 || data_representation == DDS_DATA_REPRESENTATION_XCDR2);
  struct ddsi_serdata_default *d;
  uint32_t xcdr_version = data_representation == DDS_DATA_REPRESENTATION_XCDR1 ? CDR_ENC_VERSION_1 : CDR_ENC_VERSION_2;
  if ((d = serdata_default_from_sample_cdr_common (tpcmn, kind, xcdr_version, sample)) == NULL)
    return NULL;
  return key ? fix_serdata_default (d, tpcmn->serdata_basehash) : fix_serdata_default_nokey (d, tpcmn->serdata_basehash);
}

static struct ddsi_serdata *serdata_default_from_sample_cdr (const struct ddsi_sertype *tpcmn, enum ddsi_serdata_kind kind, const void *sample)
{
  return serdata_default_from_sample_data_representation (tpcmn, kind, DDS_DATA_REPRESENTATION_XCDR1, sample, true);
}

static struct ddsi_serdata *serdata_default_from_sample_xcdr2 (const struct ddsi_sertype *tpcmn, enum ddsi_serdata_kind kind, const void *sample)
{
  return serdata_default_from_sample_data_representation (tpcmn, kind, DDS_DATA_REPRESENTATION_XCDR2, sample, true);
}

static struct ddsi_serdata *serdata_default_from_sample_cdr_nokey (const struct ddsi_sertype *tpcmn, enum ddsi_serdata_kind kind, const void *sample)
{
  return serdata_default_from_sample_data_representation (tpcmn, kind, DDS_DATA_REPRESENTATION_XCDR1, sample, false);
}

static struct ddsi_serdata *serdata_default_from_sample_xcdr2_nokey (const struct ddsi_sertype *tpcmn, enum ddsi_serdata_kind kind, const void *sample)
{
  return serdata_default_from_sample_data_representation (tpcmn, kind, DDS_DATA_REPRESENTATION_XCDR2, sample, false);
}


static struct ddsi_serdata *serdata_default_to_untyped (const struct ddsi_serdata *serdata_common)
{
  const struct ddsi_serdata_default *d = (const struct ddsi_serdata_default *)serdata_common;
  const struct ddsi_sertype_default *tp = (const struct ddsi_sertype_default *)d->c.type;

  assert (CDR_ENC_IS_NATIVE (d->hdr.identifier));
  struct ddsi_serdata_default *d_tl = serdata_default_new (tp, SDK_KEY, CDR_ENC_VERSION_2);
  if (d_tl == NULL)
    return NULL;
  d_tl->c.type = NULL;
  d_tl->c.hash = d->c.hash;
  d_tl->c.timestamp.v = INT64_MIN;
  /* These things are used for the key-to-instance map and only subject to eq, free and conversion to an invalid
     sample of some type for topics that can end up in a RHC, so, of the four kinds we have, only for CDR-with-key
     the payload is of interest. */
  if (d->c.ops == &ddsi_serdata_ops_cdr || d->c.ops == &ddsi_serdata_ops_xcdr2)
  {
    serdata_default_append_blob (&d_tl, d->key.keysize, serdata_default_keybuf (d));
    d_tl->key.buftype = KEYBUFTYPE_DYNALIAS;
    d_tl->key.keysize = d->key.keysize;
    d_tl->key.u.dynbuf = (unsigned char *) d_tl->data;
  }
  else
  {
    assert (d->c.ops == &ddsi_serdata_ops_cdr_nokey || d->c.ops == &ddsi_serdata_ops_xcdr2_nokey);
  }
  return (struct ddsi_serdata *)d_tl;
}

/* Fill buffer with 'size' bytes of serialised data, starting from 'off'; 0 <= off < off+sz <= alignup4(size(d)) */
static void serdata_default_to_ser (const struct ddsi_serdata *serdata_common, size_t off, size_t sz, void *buf)
{
  const struct ddsi_serdata_default *d = (const struct ddsi_serdata_default *)serdata_common;
  assert (off < d->pos + sizeof(struct CDRHeader));
  assert (sz <= alignup_size (d->pos + sizeof(struct CDRHeader), 4) - off);
  memcpy (buf, (char *)&d->hdr + off, sz);
}

static struct ddsi_serdata *serdata_default_to_ser_ref (const struct ddsi_serdata *serdata_common, size_t off, size_t sz, ddsrt_iovec_t *ref)
{
  const struct ddsi_serdata_default *d = (const struct ddsi_serdata_default *)serdata_common;
  assert (off < d->pos + sizeof(struct CDRHeader));
  assert (sz <= alignup_size (d->pos + sizeof(struct CDRHeader), 4) - off);
  ref->iov_base = (char *)&d->hdr + off;
  ref->iov_len = (ddsrt_iov_len_t)sz;
  return ddsi_serdata_ref(serdata_common);
}

static void serdata_default_to_ser_unref (struct ddsi_serdata *serdata_common, const ddsrt_iovec_t *ref)
{
  (void)ref;
  ddsi_serdata_unref(serdata_common);
}

static bool serdata_default_to_sample_cdr (const struct ddsi_serdata *serdata_common, void *sample, void **bufptr, void *buflim)
{
  const struct ddsi_serdata_default *d = (const struct ddsi_serdata_default *)serdata_common;
  const struct ddsi_sertype_default *tp = (const struct ddsi_sertype_default *) d->c.type;
  dds_istream_t is;
  if (bufptr) abort(); else { (void)buflim; } /* FIXME: haven't implemented that bit yet! */
  assert (CDR_ENC_IS_NATIVE (d->hdr.identifier));
  dds_istream_from_serdata_default(&is, d);
  if (d->c.kind == SDK_KEY)
    dds_stream_read_key (&is, sample, tp);
  else
    dds_stream_read_sample (&is, sample, tp);
  return true; /* FIXME: can't conversion to sample fail? */
}

static bool serdata_default_untyped_to_sample_cdr (const struct ddsi_sertype *sertype_common, const struct ddsi_serdata *serdata_common, void *sample, void **bufptr, void *buflim)
{
  const struct ddsi_serdata_default *d = (const struct ddsi_serdata_default *)serdata_common;
  const struct ddsi_sertype_default *tp = (const struct ddsi_sertype_default *) sertype_common;
  dds_istream_t is;
  assert (d->c.type == NULL);
  assert (d->c.kind == SDK_KEY);
  assert (d->c.ops == sertype_common->serdata_ops);
  assert (CDR_ENC_IS_NATIVE (d->hdr.identifier));
  if (bufptr) abort(); else { (void)buflim; } /* FIXME: haven't implemented that bit yet! */
  dds_istream_from_serdata_default(&is, d);
  dds_stream_read_key (&is, sample, tp);
  return true; /* FIXME: can't conversion to sample fail? */
}

static bool serdata_default_untyped_to_sample_cdr_nokey (const struct ddsi_sertype *sertype_common, const struct ddsi_serdata *serdata_common, void *sample, void **bufptr, void *buflim)
{
  (void)sertype_common; (void)sample; (void)bufptr; (void)buflim; (void)serdata_common;
  assert (serdata_common->type == NULL);
  assert (serdata_common->kind == SDK_KEY);
  return true;
}

static size_t serdata_default_print_cdr (const struct ddsi_sertype *sertype_common, const struct ddsi_serdata *serdata_common, char *buf, size_t size)
{
  const struct ddsi_serdata_default *d = (const struct ddsi_serdata_default *)serdata_common;
  const struct ddsi_sertype_default *tp = (const struct ddsi_sertype_default *)sertype_common;
  dds_istream_t is;
  dds_istream_from_serdata_default (&is, d);
  if (d->c.kind == SDK_KEY)
    return dds_stream_print_key (&is, tp, buf, size);
  else
    return dds_stream_print_sample (&is, tp, buf, size);
}

static void serdata_default_get_keyhash (const struct ddsi_serdata *serdata_common, struct ddsi_keyhash *buf, bool force_md5)
{
  const struct ddsi_serdata_default *d = (const struct ddsi_serdata_default *)serdata_common;
  const struct ddsi_sertype_default *tp = (const struct ddsi_sertype_default *)d->c.type;
  assert(buf);
  assert(d->key.buftype != KEYBUFTYPE_UNSET);

  // Convert native representation to what keyhashes expect
  // d->key could also be in big-endian, but that eliminates the possibility of aliasing d->data
  // on little endian machines and forces a double copy of the key to be present for SDK_KEY
  //
  // As nobody should be using the DDSI keyhash, the price to pay for the conversion looks like
  // a price worth paying

  uint32_t xcdrv = ddsi_sertype_enc_id_xcdr_version (d->hdr.identifier);

  /* serdata has a XCDR2 serialized key, so initializer the istream with this version
     and with the size of that key (d->key.keysize) */
  dds_istream_t is;
  dds_istream_init (&is, d->key.keysize, serdata_default_keybuf(d), CDR_ENC_VERSION_2);

  /* The output stream uses the XCDR version from the serdata, so that the keyhash in
     ostream is calculated using this CDR representation (XTypes spec 7.6.8, RTPS spec 9.6.3.8) */
  dds_ostreamBE_t os;
  dds_ostreamBE_init (&os, 0, xcdrv);
  dds_stream_extract_keyBE_from_key (&is, &os, tp);
  assert (is.m_index == d->key.keysize);

  /* We know the key size for XCDR2 encoding, but for XCDR1 there can be additional
     padding because of 8-byte alignment of key fields */
  if (xcdrv == CDR_ENC_VERSION_2)
    assert (os.x.m_index == d->key.keysize);

  /* Cannot use is_topic_fixed_key here, because in case there is a bounded string
     key field, it may contain a shorter string and fit in the 16 bytes */
  uint32_t actual_keysz = os.x.m_index;
  if (force_md5 || actual_keysz > DDS_FIXED_KEY_MAX_SIZE)
  {
    ddsrt_md5_state_t md5st;
    ddsrt_md5_init (&md5st);
    ddsrt_md5_append (&md5st, (ddsrt_md5_byte_t *) os.x.m_buffer, actual_keysz);
    ddsrt_md5_finish (&md5st, (ddsrt_md5_byte_t *) buf->value);
  }
  else
  {
    memset (buf->value, 0, DDS_FIXED_KEY_MAX_SIZE);
    memcpy (buf->value, os.x.m_buffer, actual_keysz);
  }
  dds_ostreamBE_fini (&os);
}

static struct ddsi_serdata * serdata_default_from_virtual_exchange(const struct ddsi_sertype *type, const ddsi_virtual_interface_exchange_unit_t *unit)
{
  const struct ddsi_sertype_default *tp = (const struct ddsi_sertype_default *)type;
  struct ddsi_serdata_default *d = serdata_default_new(
    tp,
    unit->metadata->sample_state == LOANED_SAMPLE_STATE_RAW ? SDK_DATA : LOANED_SAMPLE_STATE_UNITIALIZED,
    unit->metadata->encoding_version);

  //the loaned sample is not raw data, but serialized
  if (unit->metadata->sample_state == LOANED_SAMPLE_STATE_SERIALIZED_KEY ||
      unit->metadata->sample_state == LOANED_SAMPLE_STATE_SERIALIZED_DATA) {
    dds_istream_t is;
    dds_istream_init (&is, unit->metadata->sample_size, unit->loan->sample_ptr, unit->metadata->encoding_version);
    if (unit->metadata->sample_state == LOANED_SAMPLE_STATE_SERIALIZED_KEY)
      d->c.kind = SDK_KEY;
    else
      d->c.kind = SDK_DATA;
    dds_stream_read_sample (&is, d->data, tp);
  }

  d->c.hash = unit->metadata->hash;
  d->c.statusinfo = unit->metadata->statusinfo;
  d->c.timestamp.v = unit->metadata->timestamp;
  memcpy(d->key.u.stbuf, unit->metadata->keyhash.value, DDS_FIXED_KEY_MAX_SIZE);
  d->c.loan = unit->loan;
#if DDSRT_ENDIAN == DDSRT_LITTLE_ENDIAN
  d->hdr.identifier = CDR_LE; //same endianness as local
#endif

  return (struct ddsi_serdata *)d;
}

const struct ddsi_serdata_ops ddsi_serdata_ops_cdr = {
  .get_size = serdata_default_get_size,
  .eqkey = serdata_default_eqkey,
  .free = serdata_default_free,
  .from_ser = serdata_default_from_ser,
  .from_ser_iov = serdata_default_from_ser_iov,
  .from_keyhash = ddsi_serdata_from_keyhash_cdr,
  .from_sample = serdata_default_from_sample_cdr,
  .to_ser = serdata_default_to_ser,
  .to_sample = serdata_default_to_sample_cdr,
  .to_ser_ref = serdata_default_to_ser_ref,
  .to_ser_unref = serdata_default_to_ser_unref,
  .to_untyped = serdata_default_to_untyped,
  .untyped_to_sample = serdata_default_untyped_to_sample_cdr,
  .print = serdata_default_print_cdr,
  .get_keyhash = serdata_default_get_keyhash,
  .from_loaned_sample = serdata_default_from_loaned_sample,
  .from_virtual_exchange = serdata_default_from_virtual_exchange
};

const struct ddsi_serdata_ops ddsi_serdata_ops_xcdr2 = {
  .get_size = serdata_default_get_size,
  .eqkey = serdata_default_eqkey,
  .free = serdata_default_free,
  .from_ser = serdata_default_from_ser,
  .from_ser_iov = serdata_default_from_ser_iov,
  .from_keyhash = ddsi_serdata_from_keyhash_cdr,
  .from_sample = serdata_default_from_sample_xcdr2,
  .to_ser = serdata_default_to_ser,
  .to_sample = serdata_default_to_sample_cdr,
  .to_ser_ref = serdata_default_to_ser_ref,
  .to_ser_unref = serdata_default_to_ser_unref,
  .to_untyped = serdata_default_to_untyped,
  .untyped_to_sample = serdata_default_untyped_to_sample_cdr,
  .print = serdata_default_print_cdr,
  .get_keyhash = serdata_default_get_keyhash,
  .from_loaned_sample = serdata_default_from_loaned_sample,
  .from_virtual_exchange = serdata_default_from_virtual_exchange
};

const struct ddsi_serdata_ops ddsi_serdata_ops_cdr_nokey = {
  .get_size = serdata_default_get_size,
  .eqkey = serdata_default_eqkey_nokey,
  .free = serdata_default_free,
  .from_ser = serdata_default_from_ser_nokey,
  .from_ser_iov = serdata_default_from_ser_iov_nokey,
  .from_keyhash = ddsi_serdata_from_keyhash_cdr_nokey,
  .from_sample = serdata_default_from_sample_cdr_nokey,
  .to_ser = serdata_default_to_ser,
  .to_sample = serdata_default_to_sample_cdr,
  .to_ser_ref = serdata_default_to_ser_ref,
  .to_ser_unref = serdata_default_to_ser_unref,
  .to_untyped = serdata_default_to_untyped,
  .untyped_to_sample = serdata_default_untyped_to_sample_cdr_nokey,
  .print = serdata_default_print_cdr,
  .get_keyhash = serdata_default_get_keyhash,
  .from_loaned_sample = serdata_default_from_loaned_sample,
  .from_virtual_exchange = serdata_default_from_virtual_exchange
};

const struct ddsi_serdata_ops ddsi_serdata_ops_xcdr2_nokey = {
  .get_size = serdata_default_get_size,
  .eqkey = serdata_default_eqkey_nokey,
  .free = serdata_default_free,
  .from_ser = serdata_default_from_ser_nokey,
  .from_ser_iov = serdata_default_from_ser_iov_nokey,
  .from_keyhash = ddsi_serdata_from_keyhash_cdr_nokey,
  .from_sample = serdata_default_from_sample_xcdr2_nokey,
  .to_ser = serdata_default_to_ser,
  .to_sample = serdata_default_to_sample_cdr,
  .to_ser_ref = serdata_default_to_ser_ref,
  .to_ser_unref = serdata_default_to_ser_unref,
  .to_untyped = serdata_default_to_untyped,
  .untyped_to_sample = serdata_default_untyped_to_sample_cdr_nokey,
  .print = serdata_default_print_cdr,
  .get_keyhash = serdata_default_get_keyhash,
  .from_loaned_sample = serdata_default_from_loaned_sample,
  .from_virtual_exchange = serdata_default_from_virtual_exchange
};
