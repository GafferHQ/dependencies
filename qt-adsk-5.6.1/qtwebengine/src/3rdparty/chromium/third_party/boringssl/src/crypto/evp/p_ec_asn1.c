/* Written by Dr Stephen N Henson (steve@openssl.org) for the OpenSSL
 * project 2006.
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

#include <openssl/asn1t.h>
#include <openssl/bn.h>
#include <openssl/ec.h>
#include <openssl/err.h>
#include <openssl/mem.h>
#include <openssl/obj.h>
#include <openssl/x509.h>

#include "internal.h"


static int eckey_param2type(int *pptype, void **ppval, EC_KEY *ec_key) {
  const EC_GROUP *group;
  int nid;

  if (ec_key == NULL || (group = EC_KEY_get0_group(ec_key)) == NULL) {
    OPENSSL_PUT_ERROR(EVP, eckey_param2type, EVP_R_MISSING_PARAMETERS);
    return 0;
  }

  nid = EC_GROUP_get_curve_name(group);
  if (nid == NID_undef) {
    OPENSSL_PUT_ERROR(EVP, eckey_param2type, EVP_R_NO_NID_FOR_CURVE);
    return 0;
  }

  *ppval = (void*) OBJ_nid2obj(nid);
  *pptype = V_ASN1_OBJECT;
  return 1;
}

static int eckey_pub_encode(X509_PUBKEY *pk, const EVP_PKEY *pkey) {
  EC_KEY *ec_key = pkey->pkey.ec;
  void *pval = NULL;
  int ptype;
  uint8_t *penc = NULL, *p;
  int penclen;

  if (!eckey_param2type(&ptype, &pval, ec_key)) {
    OPENSSL_PUT_ERROR(EVP, eckey_pub_encode, ERR_R_EC_LIB);
    return 0;
  }
  penclen = i2o_ECPublicKey(ec_key, NULL);
  if (penclen <= 0) {
    goto err;
  }
  penc = OPENSSL_malloc(penclen);
  if (!penc) {
    goto err;
  }
  p = penc;
  penclen = i2o_ECPublicKey(ec_key, &p);
  if (penclen <= 0) {
    goto err;
  }
  if (X509_PUBKEY_set0_param(pk, OBJ_nid2obj(EVP_PKEY_EC), ptype, pval, penc,
                             penclen)) {
    return 1;
  }

err:
  if (ptype == V_ASN1_OBJECT) {
    ASN1_OBJECT_free(pval);
  } else {
    ASN1_STRING_free(pval);
  }
  if (penc) {
    OPENSSL_free(penc);
  }
  return 0;
}

static EC_KEY *eckey_type2param(int ptype, void *pval) {
  EC_KEY *eckey = NULL;

  if (ptype == V_ASN1_SEQUENCE) {
    ASN1_STRING *pstr = pval;
    const uint8_t *pm = pstr->data;
    int pmlen = pstr->length;

    eckey = d2i_ECParameters(NULL, &pm, pmlen);
    if (eckey == NULL) {
      OPENSSL_PUT_ERROR(EVP, eckey_type2param, EVP_R_DECODE_ERROR);
      goto err;
    }
  } else if (ptype == V_ASN1_OBJECT) {
    ASN1_OBJECT *poid = pval;

    /* type == V_ASN1_OBJECT => the parameters are given
     * by an asn1 OID */
    eckey = EC_KEY_new_by_curve_name(OBJ_obj2nid(poid));
    if (eckey == NULL) {
      goto err;
    }
  } else {
    OPENSSL_PUT_ERROR(EVP, eckey_type2param, EVP_R_DECODE_ERROR);
    goto err;
  }

  return eckey;

err:
  if (eckey) {
    EC_KEY_free(eckey);
  }
  return NULL;
}

static int eckey_pub_decode(EVP_PKEY *pkey, X509_PUBKEY *pubkey) {
  const uint8_t *p = NULL;
  void *pval;
  int ptype, pklen;
  EC_KEY *eckey = NULL;
  X509_ALGOR *palg;

  if (!X509_PUBKEY_get0_param(NULL, &p, &pklen, &palg, pubkey)) {
    return 0;
  }
  X509_ALGOR_get0(NULL, &ptype, &pval, palg);

  eckey = eckey_type2param(ptype, pval);
  if (!eckey) {
    OPENSSL_PUT_ERROR(EVP, eckey_pub_decode, ERR_R_EC_LIB);
    return 0;
  }

  /* We have parameters now set public key */
  if (!o2i_ECPublicKey(&eckey, &p, pklen)) {
    OPENSSL_PUT_ERROR(EVP, eckey_pub_decode, EVP_R_DECODE_ERROR);
    goto err;
  }

  EVP_PKEY_assign_EC_KEY(pkey, eckey);
  return 1;

err:
  if (eckey) {
    EC_KEY_free(eckey);
  }
  return 0;
}

static int eckey_pub_cmp(const EVP_PKEY *a, const EVP_PKEY *b) {
  int r;
  const EC_GROUP *group = EC_KEY_get0_group(b->pkey.ec);
  const EC_POINT *pa = EC_KEY_get0_public_key(a->pkey.ec),
                 *pb = EC_KEY_get0_public_key(b->pkey.ec);
  r = EC_POINT_cmp(group, pa, pb, NULL);
  if (r == 0) {
    return 1;
  } else if (r == 1) {
    return 0;
  } else {
    return -2;
  }
}

static int eckey_priv_decode(EVP_PKEY *pkey, PKCS8_PRIV_KEY_INFO *p8) {
  const uint8_t *p = NULL;
  void *pval;
  int ptype, pklen;
  EC_KEY *eckey = NULL;
  X509_ALGOR *palg;

  if (!PKCS8_pkey_get0(NULL, &p, &pklen, &palg, p8)) {
    return 0;
  }
  X509_ALGOR_get0(NULL, &ptype, &pval, palg);

  eckey = eckey_type2param(ptype, pval);

  if (!eckey) {
    goto ecliberr;
  }

  /* We have parameters now set private key */
  if (!d2i_ECPrivateKey(&eckey, &p, pklen)) {
    OPENSSL_PUT_ERROR(EVP, eckey_priv_decode, EVP_R_DECODE_ERROR);
    goto ecerr;
  }

  /* calculate public key (if necessary) */
  if (EC_KEY_get0_public_key(eckey) == NULL) {
    const BIGNUM *priv_key;
    const EC_GROUP *group;
    EC_POINT *pub_key;
    /* the public key was not included in the SEC1 private
     * key => calculate the public key */
    group = EC_KEY_get0_group(eckey);
    pub_key = EC_POINT_new(group);
    if (pub_key == NULL) {
      OPENSSL_PUT_ERROR(EVP, eckey_priv_decode, ERR_R_EC_LIB);
      goto ecliberr;
    }
    if (!EC_POINT_copy(pub_key, EC_GROUP_get0_generator(group))) {
      EC_POINT_free(pub_key);
      OPENSSL_PUT_ERROR(EVP, eckey_priv_decode, ERR_R_EC_LIB);
      goto ecliberr;
    }
    priv_key = EC_KEY_get0_private_key(eckey);
    if (!EC_POINT_mul(group, pub_key, priv_key, NULL, NULL, NULL)) {
      EC_POINT_free(pub_key);
      OPENSSL_PUT_ERROR(EVP, eckey_priv_decode, ERR_R_EC_LIB);
      goto ecliberr;
    }
    if (EC_KEY_set_public_key(eckey, pub_key) == 0) {
      EC_POINT_free(pub_key);
      OPENSSL_PUT_ERROR(EVP, eckey_priv_decode, ERR_R_EC_LIB);
      goto ecliberr;
    }
    EC_POINT_free(pub_key);
  }

  EVP_PKEY_assign_EC_KEY(pkey, eckey);
  return 1;

ecliberr:
  OPENSSL_PUT_ERROR(EVP, eckey_priv_decode, ERR_R_EC_LIB);
ecerr:
  if (eckey) {
    EC_KEY_free(eckey);
  }
  return 0;
}

static int eckey_priv_encode(PKCS8_PRIV_KEY_INFO *p8, const EVP_PKEY *pkey) {
  EC_KEY *ec_key;
  uint8_t *ep, *p;
  int eplen, ptype;
  void *pval;
  unsigned int tmp_flags, old_flags;

  ec_key = pkey->pkey.ec;

  if (!eckey_param2type(&ptype, &pval, ec_key)) {
    OPENSSL_PUT_ERROR(EVP, eckey_priv_encode, EVP_R_DECODE_ERROR);
    return 0;
  }

  /* set the private key */

  /* do not include the parameters in the SEC1 private key
   * see PKCS#11 12.11 */
  old_flags = EC_KEY_get_enc_flags(ec_key);
  tmp_flags = old_flags | EC_PKEY_NO_PARAMETERS;
  EC_KEY_set_enc_flags(ec_key, tmp_flags);
  eplen = i2d_ECPrivateKey(ec_key, NULL);
  if (!eplen) {
    EC_KEY_set_enc_flags(ec_key, old_flags);
    OPENSSL_PUT_ERROR(EVP, eckey_priv_encode, ERR_R_EC_LIB);
    return 0;
  }
  ep = (uint8_t *)OPENSSL_malloc(eplen);
  if (!ep) {
    EC_KEY_set_enc_flags(ec_key, old_flags);
    OPENSSL_PUT_ERROR(EVP, eckey_priv_encode, ERR_R_MALLOC_FAILURE);
    return 0;
  }
  p = ep;
  if (!i2d_ECPrivateKey(ec_key, &p)) {
    EC_KEY_set_enc_flags(ec_key, old_flags);
    OPENSSL_free(ep);
    OPENSSL_PUT_ERROR(EVP, eckey_priv_encode, ERR_R_EC_LIB);
    return 0;
  }
  /* restore old encoding flags */
  EC_KEY_set_enc_flags(ec_key, old_flags);

  if (!PKCS8_pkey_set0(p8, (ASN1_OBJECT *)OBJ_nid2obj(NID_X9_62_id_ecPublicKey),
                       0, ptype, pval, ep, eplen)) {
    return 0;
  }

  return 1;
}

static int int_ec_size(const EVP_PKEY *pkey) {
  return ECDSA_size(pkey->pkey.ec);
}

static int ec_bits(const EVP_PKEY *pkey) {
  BIGNUM *order = BN_new();
  const EC_GROUP *group;
  int ret;

  if (!order) {
    ERR_clear_error();
    return 0;
  }
  group = EC_KEY_get0_group(pkey->pkey.ec);
  if (!EC_GROUP_get_order(group, order, NULL)) {
    ERR_clear_error();
    return 0;
  }

  ret = BN_num_bits(order);
  BN_free(order);
  return ret;
}

static int ec_missing_parameters(const EVP_PKEY *pkey) {
  return EC_KEY_get0_group(pkey->pkey.ec) == NULL;
}

static int ec_copy_parameters(EVP_PKEY *to, const EVP_PKEY *from) {
  EC_GROUP *group = EC_GROUP_dup(EC_KEY_get0_group(from->pkey.ec));
  if (group == NULL ||
      EC_KEY_set_group(to->pkey.ec, group) == 0) {
    return 0;
  }
  EC_GROUP_free(group);
  return 1;
}

static int ec_cmp_parameters(const EVP_PKEY *a, const EVP_PKEY *b) {
  const EC_GROUP *group_a = EC_KEY_get0_group(a->pkey.ec),
                 *group_b = EC_KEY_get0_group(b->pkey.ec);
  if (EC_GROUP_cmp(group_a, group_b, NULL) != 0) {
    /* mismatch */
    return 0;
  }
  return 1;
}

static void int_ec_free(EVP_PKEY *pkey) { EC_KEY_free(pkey->pkey.ec); }

static int do_EC_KEY_print(BIO *bp, const EC_KEY *x, int off, int ktype) {
  uint8_t *buffer = NULL;
  const char *ecstr;
  size_t buf_len = 0, i;
  int ret = 0, reason = ERR_R_BIO_LIB;
  BIGNUM *order = NULL;
  BN_CTX *ctx = NULL;
  const EC_GROUP *group;
  const EC_POINT *public_key;
  const BIGNUM *priv_key;
  uint8_t *pub_key_bytes = NULL;
  size_t pub_key_bytes_len = 0;

  if (x == NULL || (group = EC_KEY_get0_group(x)) == NULL) {
    reason = ERR_R_PASSED_NULL_PARAMETER;
    goto err;
  }

  ctx = BN_CTX_new();
  if (ctx == NULL) {
    reason = ERR_R_MALLOC_FAILURE;
    goto err;
  }

  if (ktype > 0) {
    public_key = EC_KEY_get0_public_key(x);
    if (public_key != NULL) {
      pub_key_bytes_len = EC_POINT_point2oct(
          group, public_key, EC_KEY_get_conv_form(x), NULL, 0, ctx);
      if (pub_key_bytes_len == 0) {
        reason = ERR_R_MALLOC_FAILURE;
        goto err;
      }
      pub_key_bytes = OPENSSL_malloc(pub_key_bytes_len);
      if (pub_key_bytes == NULL) {
        reason = ERR_R_MALLOC_FAILURE;
        goto err;
      }
      pub_key_bytes_len =
          EC_POINT_point2oct(group, public_key, EC_KEY_get_conv_form(x),
                             pub_key_bytes, pub_key_bytes_len, ctx);
      if (pub_key_bytes_len == 0) {
        reason = ERR_R_MALLOC_FAILURE;
        goto err;
      }
      buf_len = pub_key_bytes_len;
    }
  }

  if (ktype == 2) {
    priv_key = EC_KEY_get0_private_key(x);
    if (priv_key && (i = (size_t)BN_num_bytes(priv_key)) > buf_len) {
      buf_len = i;
    }
  } else {
    priv_key = NULL;
  }

  if (ktype > 0) {
    buf_len += 10;
    if ((buffer = OPENSSL_malloc(buf_len)) == NULL) {
      reason = ERR_R_MALLOC_FAILURE;
      goto err;
    }
  }
  if (ktype == 2) {
    ecstr = "Private-Key";
  } else if (ktype == 1) {
    ecstr = "Public-Key";
  } else {
    ecstr = "ECDSA-Parameters";
  }

  if (!BIO_indent(bp, off, 128)) {
    goto err;
  }
  order = BN_new();
  if (order == NULL || !EC_GROUP_get_order(group, order, NULL) ||
      BIO_printf(bp, "%s: (%d bit)\n", ecstr, BN_num_bits(order)) <= 0) {
    goto err;
  }

  if ((priv_key != NULL) &&
      !ASN1_bn_print(bp, "priv:", priv_key, buffer, off)) {
    goto err;
  }
  if (pub_key_bytes != NULL) {
    BIO_hexdump(bp, pub_key_bytes, pub_key_bytes_len, off);
  }
  /* TODO(fork): implement */
  /*
  if (!ECPKParameters_print(bp, group, off))
    goto err; */
  ret = 1;

err:
  if (!ret) {
    OPENSSL_PUT_ERROR(EVP, do_EC_KEY_print, reason);
  }
  OPENSSL_free(pub_key_bytes);
  BN_free(order);
  BN_CTX_free(ctx);
  OPENSSL_free(buffer);
  return ret;
}

static int eckey_param_decode(EVP_PKEY *pkey, const uint8_t **pder,
                              int derlen) {
  EC_KEY *eckey;
  if (!(eckey = d2i_ECParameters(NULL, pder, derlen))) {
    OPENSSL_PUT_ERROR(EVP, eckey_param_decode, ERR_R_EC_LIB);
    return 0;
  }
  EVP_PKEY_assign_EC_KEY(pkey, eckey);
  return 1;
}

static int eckey_param_encode(const EVP_PKEY *pkey, uint8_t **pder) {
  return i2d_ECParameters(pkey->pkey.ec, pder);
}

static int eckey_param_print(BIO *bp, const EVP_PKEY *pkey, int indent,
                             ASN1_PCTX *ctx) {
  return do_EC_KEY_print(bp, pkey->pkey.ec, indent, 0);
}

static int eckey_pub_print(BIO *bp, const EVP_PKEY *pkey, int indent,
                           ASN1_PCTX *ctx) {
  return do_EC_KEY_print(bp, pkey->pkey.ec, indent, 1);
}


static int eckey_priv_print(BIO *bp, const EVP_PKEY *pkey, int indent,
                            ASN1_PCTX *ctx) {
  return do_EC_KEY_print(bp, pkey->pkey.ec, indent, 2);
}

static int eckey_opaque(const EVP_PKEY *pkey) {
  return EC_KEY_is_opaque(pkey->pkey.ec);
}

static int old_ec_priv_decode(EVP_PKEY *pkey, const uint8_t **pder,
                              int derlen) {
  EC_KEY *ec;
  if (!(ec = d2i_ECPrivateKey(NULL, pder, derlen))) {
    OPENSSL_PUT_ERROR(EVP, old_ec_priv_decode, EVP_R_DECODE_ERROR);
    return 0;
  }
  EVP_PKEY_assign_EC_KEY(pkey, ec);
  return 1;
}

static int old_ec_priv_encode(const EVP_PKEY *pkey, uint8_t **pder) {
  return i2d_ECPrivateKey(pkey->pkey.ec, pder);
}

const EVP_PKEY_ASN1_METHOD ec_asn1_meth = {
  EVP_PKEY_EC,
  EVP_PKEY_EC,
  0,
  "EC",
  "OpenSSL EC algorithm",

  eckey_pub_decode,
  eckey_pub_encode,
  eckey_pub_cmp,
  eckey_pub_print,

  eckey_priv_decode,
  eckey_priv_encode,
  eckey_priv_print,

  eckey_opaque,
  0 /* pkey_supports_digest */,

  int_ec_size,
  ec_bits,

  eckey_param_decode,
  eckey_param_encode,
  ec_missing_parameters,
  ec_copy_parameters,
  ec_cmp_parameters,
  eckey_param_print,
  0,

  int_ec_free,
  old_ec_priv_decode,
  old_ec_priv_encode
};
