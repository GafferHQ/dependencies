/* Written by Dr Stephen N Henson (steve@openssl.org) for the OpenSSL project
 * 2006.
 */
/* ====================================================================
 * Copyright (c) 2006 The OpenSSL Project.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit. (http://www.OpenSSL.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    licensing@OpenSSL.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.OpenSSL.org/)"
 *
 * THIS SOFTWARE IS PROVIDED BY THE OpenSSL PROJECT ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OpenSSL PROJECT OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 *
 * This product includes cryptographic software written by Eric Young
 * (eay@cryptsoft.com).  This product includes software written by Tim
 * Hudson (tjh@cryptsoft.com). */

#include <openssl/evp.h>

#include <openssl/asn1.h>
#include <openssl/asn1t.h>
#include <openssl/digest.h>
#include <openssl/dsa.h>
#include <openssl/err.h>
#include <openssl/mem.h>
#include <openssl/obj.h>
#include <openssl/x509.h>

#include "../dsa/internal.h"
#include "internal.h"


static int dsa_pub_decode(EVP_PKEY *pkey, X509_PUBKEY *pubkey) {
  const uint8_t *p, *pm;
  int pklen, pmlen;
  int ptype;
  void *pval;
  ASN1_STRING *pstr;
  X509_ALGOR *palg;
  ASN1_INTEGER *public_key = NULL;

  DSA *dsa = NULL;

  if (!X509_PUBKEY_get0_param(NULL, &p, &pklen, &palg, pubkey)) {
    return 0;
  }
  X509_ALGOR_get0(NULL, &ptype, &pval, palg);

  if (ptype == V_ASN1_SEQUENCE) {
    pstr = pval;
    pm = pstr->data;
    pmlen = pstr->length;

    dsa = d2i_DSAparams(NULL, &pm, pmlen);
    if (dsa == NULL) {
      OPENSSL_PUT_ERROR(EVP, dsa_pub_decode, EVP_R_DECODE_ERROR);
      goto err;
    }
  } else if (ptype == V_ASN1_NULL || ptype == V_ASN1_UNDEF) {
    dsa = DSA_new();
    if (dsa == NULL) {
      OPENSSL_PUT_ERROR(EVP, dsa_pub_decode, ERR_R_MALLOC_FAILURE);
      goto err;
    }
  } else {
    OPENSSL_PUT_ERROR(EVP, dsa_pub_decode, EVP_R_PARAMETER_ENCODING_ERROR);
    goto err;
  }

  public_key = d2i_ASN1_INTEGER(NULL, &p, pklen);
  if (public_key == NULL) {
    OPENSSL_PUT_ERROR(EVP, dsa_pub_decode, EVP_R_DECODE_ERROR);
    goto err;
  }

  dsa->pub_key = ASN1_INTEGER_to_BN(public_key, NULL);
  if (dsa->pub_key == NULL) {
    OPENSSL_PUT_ERROR(EVP, dsa_pub_decode, EVP_R_BN_DECODE_ERROR);
    goto err;
  }

  ASN1_INTEGER_free(public_key);
  EVP_PKEY_assign_DSA(pkey, dsa);
  return 1;

err:
  ASN1_INTEGER_free(public_key);
  DSA_free(dsa);
  return 0;
}

static int dsa_pub_encode(X509_PUBKEY *pk, const EVP_PKEY *pkey) {
  DSA *dsa;
  ASN1_STRING *pval = NULL;
  uint8_t *penc = NULL;
  int penclen;

  dsa = pkey->pkey.dsa;
  dsa->write_params = 0;

  int ptype;
  if (dsa->p && dsa->q && dsa->g) {
    pval = ASN1_STRING_new();
    if (!pval) {
      OPENSSL_PUT_ERROR(EVP, dsa_pub_encode, ERR_R_MALLOC_FAILURE);
      goto err;
    }
    pval->length = i2d_DSAparams(dsa, &pval->data);
    if (pval->length <= 0) {
      OPENSSL_PUT_ERROR(EVP, dsa_pub_encode, ERR_R_MALLOC_FAILURE);
      goto err;
    }
    ptype = V_ASN1_SEQUENCE;
  } else {
    ptype = V_ASN1_UNDEF;
  }

  penclen = i2d_DSAPublicKey(dsa, &penc);
  if (penclen <= 0) {
    OPENSSL_PUT_ERROR(EVP, dsa_pub_encode, ERR_R_MALLOC_FAILURE);
    goto err;
  }

  if (X509_PUBKEY_set0_param(pk, OBJ_nid2obj(EVP_PKEY_DSA), ptype, pval,
                             penc, penclen)) {
    return 1;
  }

err:
  OPENSSL_free(penc);
  ASN1_STRING_free(pval);

  return 0;
}

static int dsa_priv_decode(EVP_PKEY *pkey, PKCS8_PRIV_KEY_INFO *p8) {
  const uint8_t *p, *pm;
  int pklen, pmlen;
  int ptype;
  void *pval;
  ASN1_STRING *pstr;
  X509_ALGOR *palg;
  ASN1_INTEGER *privkey = NULL;
  BN_CTX *ctx = NULL;

  /* In PKCS#8 DSA: you just get a private key integer and parameters in the
   * AlgorithmIdentifier the pubkey must be recalculated. */

  STACK_OF(ASN1_TYPE) *ndsa = NULL;
  DSA *dsa = NULL;

  if (!PKCS8_pkey_get0(NULL, &p, &pklen, &palg, p8)) {
    return 0;
  }
  X509_ALGOR_get0(NULL, &ptype, &pval, palg);

  /* Check for broken DSA PKCS#8, UGH! */
  if (*p == (V_ASN1_SEQUENCE | V_ASN1_CONSTRUCTED)) {
    ASN1_TYPE *t1, *t2;
    ndsa = d2i_ASN1_SEQUENCE_ANY(NULL, &p, pklen);
    if (ndsa == NULL) {
      goto decerr;
    }
    if (sk_ASN1_TYPE_num(ndsa) != 2) {
      goto decerr;
    }

    /* Handle Two broken types:
     * SEQUENCE {parameters, priv_key}
     * SEQUENCE {pub_key, priv_key}. */

    t1 = sk_ASN1_TYPE_value(ndsa, 0);
    t2 = sk_ASN1_TYPE_value(ndsa, 1);
    if (t1->type == V_ASN1_SEQUENCE) {
      p8->broken = PKCS8_EMBEDDED_PARAM;
      pval = t1->value.ptr;
    } else if (ptype == V_ASN1_SEQUENCE) {
      p8->broken = PKCS8_NS_DB;
    } else {
      goto decerr;
    }

    if (t2->type != V_ASN1_INTEGER) {
      goto decerr;
    }

    privkey = t2->value.integer;
  } else {
    const uint8_t *q = p;
    privkey = d2i_ASN1_INTEGER(NULL, &p, pklen);
    if (privkey == NULL) {
      goto decerr;
    }
    if (privkey->type == V_ASN1_NEG_INTEGER) {
      p8->broken = PKCS8_NEG_PRIVKEY;
      ASN1_INTEGER_free(privkey);
      privkey = d2i_ASN1_UINTEGER(NULL, &q, pklen);
      if (privkey == NULL) {
        goto decerr;
      }
    }
    if (ptype != V_ASN1_SEQUENCE) {
      goto decerr;
    }
  }

  pstr = pval;
  pm = pstr->data;
  pmlen = pstr->length;
  dsa = d2i_DSAparams(NULL, &pm, pmlen);
  if (dsa == NULL) {
    goto decerr;
  }
  /* We have parameters. Now set private key */
  dsa->priv_key = ASN1_INTEGER_to_BN(privkey, NULL);
  if (dsa->priv_key == NULL) {
    OPENSSL_PUT_ERROR(EVP, dsa_priv_decode, ERR_LIB_BN);
    goto dsaerr;
  }
  /* Calculate public key. */
  dsa->pub_key = BN_new();
  if (dsa->pub_key == NULL) {
    OPENSSL_PUT_ERROR(EVP, dsa_priv_decode, ERR_R_MALLOC_FAILURE);
    goto dsaerr;
  }
  ctx = BN_CTX_new();
  if (ctx == NULL) {
    OPENSSL_PUT_ERROR(EVP, dsa_priv_decode, ERR_R_MALLOC_FAILURE);
    goto dsaerr;
  }

  if (!BN_mod_exp(dsa->pub_key, dsa->g, dsa->priv_key, dsa->p, ctx)) {
    OPENSSL_PUT_ERROR(EVP, dsa_priv_decode, ERR_LIB_BN);
    goto dsaerr;
  }

  EVP_PKEY_assign_DSA(pkey, dsa);
  BN_CTX_free(ctx);
  sk_ASN1_TYPE_pop_free(ndsa, ASN1_TYPE_free);
  ASN1_INTEGER_free(privkey);

  return 1;

decerr:
  OPENSSL_PUT_ERROR(EVP, dsa_priv_decode, EVP_R_DECODE_ERROR);

dsaerr:
  BN_CTX_free(ctx);
  ASN1_INTEGER_free(privkey);
  sk_ASN1_TYPE_pop_free(ndsa, ASN1_TYPE_free);
  DSA_free(dsa);
  return 0;
}

static int dsa_priv_encode(PKCS8_PRIV_KEY_INFO *p8, const EVP_PKEY *pkey) {
  ASN1_STRING *params = NULL;
  ASN1_INTEGER *prkey = NULL;
  uint8_t *dp = NULL;
  int dplen;

  if (!pkey->pkey.dsa || !pkey->pkey.dsa->priv_key) {
    OPENSSL_PUT_ERROR(EVP, dsa_priv_encode, EVP_R_MISSING_PARAMETERS);
    goto err;
  }

  params = ASN1_STRING_new();
  if (!params) {
    OPENSSL_PUT_ERROR(EVP, dsa_priv_encode, ERR_R_MALLOC_FAILURE);
    goto err;
  }

  params->length = i2d_DSAparams(pkey->pkey.dsa, &params->data);
  if (params->length <= 0) {
    OPENSSL_PUT_ERROR(EVP, dsa_priv_encode, ERR_R_MALLOC_FAILURE);
    goto err;
  }
  params->type = V_ASN1_SEQUENCE;

  /* Get private key into integer. */
  prkey = BN_to_ASN1_INTEGER(pkey->pkey.dsa->priv_key, NULL);

  if (!prkey) {
    OPENSSL_PUT_ERROR(EVP, dsa_priv_encode, ERR_LIB_BN);
    goto err;
  }

  dplen = i2d_ASN1_INTEGER(prkey, &dp);

  ASN1_INTEGER_free(prkey);

  if (!PKCS8_pkey_set0(p8, (ASN1_OBJECT *)OBJ_nid2obj(NID_dsa), 0,
                       V_ASN1_SEQUENCE, params, dp, dplen)) {
    goto err;
  }

  return 1;

err:
  OPENSSL_free(dp);
  ASN1_STRING_free(params);
  ASN1_INTEGER_free(prkey);
  return 0;
}

static int int_dsa_size(const EVP_PKEY *pkey) {
  return DSA_size(pkey->pkey.dsa);
}

static int dsa_bits(const EVP_PKEY *pkey) {
  return BN_num_bits(pkey->pkey.dsa->p);
}

static int dsa_missing_parameters(const EVP_PKEY *pkey) {
  DSA *dsa;
  dsa = pkey->pkey.dsa;
  if (dsa->p == NULL || dsa->q == NULL || dsa->g == NULL) {
    return 1;
  }
  return 0;
}

static int dup_bn_into(BIGNUM **out, BIGNUM *src) {
  BIGNUM *a;

  a = BN_dup(src);
  if (a == NULL) {
    return 0;
  }
  BN_free(*out);
  *out = a;

  return 1;
}

static int dsa_copy_parameters(EVP_PKEY *to, const EVP_PKEY *from) {
  if (!dup_bn_into(&to->pkey.dsa->p, from->pkey.dsa->p) ||
      !dup_bn_into(&to->pkey.dsa->q, from->pkey.dsa->q) ||
      !dup_bn_into(&to->pkey.dsa->g, from->pkey.dsa->g)) {
    return 0;
  }

  return 1;
}

static int dsa_cmp_parameters(const EVP_PKEY *a, const EVP_PKEY *b) {
  return BN_cmp(a->pkey.dsa->p, b->pkey.dsa->p) == 0 &&
         BN_cmp(a->pkey.dsa->q, b->pkey.dsa->q) == 0 &&
         BN_cmp(a->pkey.dsa->g, b->pkey.dsa->g) == 0;
}

static int dsa_pub_cmp(const EVP_PKEY *a, const EVP_PKEY *b) {
  return BN_cmp(b->pkey.dsa->pub_key, a->pkey.dsa->pub_key) == 0;
}

static void int_dsa_free(EVP_PKEY *pkey) { DSA_free(pkey->pkey.dsa); }

static void update_buflen(const BIGNUM *b, size_t *pbuflen) {
  size_t i;

  if (!b) {
    return;
  }
  i = BN_num_bytes(b);
  if (*pbuflen < i) {
    *pbuflen = i;
  }
}

static int do_dsa_print(BIO *bp, const DSA *x, int off, int ptype) {
  uint8_t *m = NULL;
  int ret = 0;
  size_t buf_len = 0;
  const char *ktype = NULL;

  const BIGNUM *priv_key, *pub_key;

  priv_key = NULL;
  if (ptype == 2) {
    priv_key = x->priv_key;
  }

  pub_key = NULL;
  if (ptype > 0) {
    pub_key = x->pub_key;
  }

  ktype = "DSA-Parameters";
  if (ptype == 2) {
    ktype = "Private-Key";
  } else if (ptype == 1) {
    ktype = "Public-Key";
  }

  update_buflen(x->p, &buf_len);
  update_buflen(x->q, &buf_len);
  update_buflen(x->g, &buf_len);
  update_buflen(priv_key, &buf_len);
  update_buflen(pub_key, &buf_len);

  m = (uint8_t *)OPENSSL_malloc(buf_len + 10);
  if (m == NULL) {
    OPENSSL_PUT_ERROR(EVP, do_dsa_print, ERR_R_MALLOC_FAILURE);
    goto err;
  }

  if (priv_key) {
    if (!BIO_indent(bp, off, 128) ||
        BIO_printf(bp, "%s: (%d bit)\n", ktype, BN_num_bits(x->p)) <= 0) {
      goto err;
    }
  }

  if (!ASN1_bn_print(bp, "priv:", priv_key, m, off) ||
      !ASN1_bn_print(bp, "pub: ", pub_key, m, off) ||
      !ASN1_bn_print(bp, "P:   ", x->p, m, off) ||
      !ASN1_bn_print(bp, "Q:   ", x->q, m, off) ||
      !ASN1_bn_print(bp, "G:   ", x->g, m, off)) {
    goto err;
  }
  ret = 1;

err:
  OPENSSL_free(m);
  return ret;
}

static int dsa_param_decode(EVP_PKEY *pkey, const uint8_t **pder, int derlen) {
  DSA *dsa;
  dsa = d2i_DSAparams(NULL, pder, derlen);
  if (dsa == NULL) {
    OPENSSL_PUT_ERROR(EVP, dsa_param_decode, ERR_R_DSA_LIB);
    return 0;
  }
  EVP_PKEY_assign_DSA(pkey, dsa);
  return 1;
}

static int dsa_param_encode(const EVP_PKEY *pkey, uint8_t **pder) {
  return i2d_DSAparams(pkey->pkey.dsa, pder);
}

static int dsa_param_print(BIO *bp, const EVP_PKEY *pkey, int indent,
                           ASN1_PCTX *ctx) {
  return do_dsa_print(bp, pkey->pkey.dsa, indent, 0);
}

static int dsa_pub_print(BIO *bp, const EVP_PKEY *pkey, int indent,
                         ASN1_PCTX *ctx) {
  return do_dsa_print(bp, pkey->pkey.dsa, indent, 1);
}

static int dsa_priv_print(BIO *bp, const EVP_PKEY *pkey, int indent,
                          ASN1_PCTX *ctx) {
  return do_dsa_print(bp, pkey->pkey.dsa, indent, 2);
}

static int old_dsa_priv_decode(EVP_PKEY *pkey, const uint8_t **pder,
                               int derlen) {
  DSA *dsa;
  dsa = d2i_DSAPrivateKey(NULL, pder, derlen);
  if (dsa == NULL) {
    OPENSSL_PUT_ERROR(EVP, old_dsa_priv_decode, ERR_R_DSA_LIB);
    return 0;
  }
  EVP_PKEY_assign_DSA(pkey, dsa);
  return 1;
}

static int old_dsa_priv_encode(const EVP_PKEY *pkey, uint8_t **pder) {
  return i2d_DSAPrivateKey(pkey->pkey.dsa, pder);
}

static int dsa_sig_print(BIO *bp, const X509_ALGOR *sigalg,
                         const ASN1_STRING *sig, int indent, ASN1_PCTX *pctx) {
  DSA_SIG *dsa_sig;
  const uint8_t *p;

  if (!sig) {
    return BIO_puts(bp, "\n") > 0;
  }

  p = sig->data;
  dsa_sig = d2i_DSA_SIG(NULL, &p, sig->length);
  if (dsa_sig == NULL) {
    return X509_signature_dump(bp, sig, indent);
  }

  int rv = 0;
  size_t buf_len = 0;
  uint8_t *m = NULL;

  update_buflen(dsa_sig->r, &buf_len);
  update_buflen(dsa_sig->s, &buf_len);
  m = OPENSSL_malloc(buf_len + 10);
  if (m == NULL) {
    OPENSSL_PUT_ERROR(EVP, dsa_sig_print, ERR_R_MALLOC_FAILURE);
    goto err;
  }

  if (BIO_write(bp, "\n", 1) != 1 ||
      !ASN1_bn_print(bp, "r:   ", dsa_sig->r, m, indent) ||
      !ASN1_bn_print(bp, "s:   ", dsa_sig->s, m, indent)) {
    goto err;
  }
  rv = 1;

err:
  OPENSSL_free(m);
  DSA_SIG_free(dsa_sig);
  return rv;
}

const EVP_PKEY_ASN1_METHOD dsa_asn1_meth = {
  EVP_PKEY_DSA,
  EVP_PKEY_DSA,
  0,

  "DSA",
  "OpenSSL DSA method",

  dsa_pub_decode,
  dsa_pub_encode,
  dsa_pub_cmp,
  dsa_pub_print,

  dsa_priv_decode,
  dsa_priv_encode,
  dsa_priv_print,

  NULL /* pkey_opaque */,
  NULL /* pkey_supports_digest */,

  int_dsa_size,
  dsa_bits,

  dsa_param_decode,
  dsa_param_encode,
  dsa_missing_parameters,
  dsa_copy_parameters,
  dsa_cmp_parameters,
  dsa_param_print,
  dsa_sig_print,

  int_dsa_free,
  old_dsa_priv_decode,
  old_dsa_priv_encode,
};
