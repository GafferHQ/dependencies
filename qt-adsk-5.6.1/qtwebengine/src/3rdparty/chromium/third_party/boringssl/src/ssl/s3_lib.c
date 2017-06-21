/* Copyright (C) 1995-1998 Eric Young (eay@cryptsoft.com)
 * All rights reserved.
 *
 * This package is an SSL implementation written
 * by Eric Young (eay@cryptsoft.com).
 * The implementation was written so as to conform with Netscapes SSL.
 *
 * This library is free for commercial and non-commercial use as long as
 * the following conditions are aheared to.  The following conditions
 * apply to all code found in this distribution, be it the RC4, RSA,
 * lhash, DES, etc., code; not just the SSL code.  The SSL documentation
 * included with this distribution is covered by the same copyright terms
 * except that the holder is Tim Hudson (tjh@cryptsoft.com).
 *
 * Copyright remains Eric Young's, and as such any Copyright notices in
 * the code are not to be removed.
 * If this package is used in a product, Eric Young should be given attribution
 * as the author of the parts of the library used.
 * This can be in the form of a textual message at program startup or
 * in documentation (online or textual) provided with the package.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    "This product includes cryptographic software written by
 *     Eric Young (eay@cryptsoft.com)"
 *    The word 'cryptographic' can be left out if the rouines from the library
 *    being used are not cryptographic related :-).
 * 4. If you include any Windows specific code (or a derivative thereof) from
 *    the apps directory (application code) you must include an acknowledgement:
 *    "This product includes software written by Tim Hudson (tjh@cryptsoft.com)"
 *
 * THIS SOFTWARE IS PROVIDED BY ERIC YOUNG ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * The licence and distribution terms for any publically available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution licence
 * [including the GNU Public Licence.]
 */
/* ====================================================================
 * Copyright (c) 1998-2007 The OpenSSL Project.  All rights reserved.
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
 *    for use in the OpenSSL Toolkit. (http://www.openssl.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    openssl-core@openssl.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.openssl.org/)"
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
 * Hudson (tjh@cryptsoft.com).
 *
 */
/* ====================================================================
 * Copyright 2002 Sun Microsystems, Inc. ALL RIGHTS RESERVED.
 *
 * Portions of the attached software ("Contribution") are developed by
 * SUN MICROSYSTEMS, INC., and are contributed to the OpenSSL project.
 *
 * The Contribution is licensed pursuant to the OpenSSL open source
 * license provided above.
 *
 * ECC cipher suite support in OpenSSL originally written by
 * Vipul Gupta and Sumit Gupta of Sun Microsystems Laboratories.
 *
 */
/* ====================================================================
 * Copyright 2005 Nokia. All rights reserved.
 *
 * The portions of the attached software ("Contribution") is developed by
 * Nokia Corporation and is licensed pursuant to the OpenSSL open source
 * license.
 *
 * The Contribution, originally written by Mika Kousa and Pasi Eronen of
 * Nokia Corporation, consists of the "PSK" (Pre-Shared Key) ciphersuites
 * support (see RFC 4279) to OpenSSL.
 *
 * No patent licenses or other rights except those expressly stated in
 * the OpenSSL open source license shall be deemed granted or received
 * expressly, by implication, estoppel, or otherwise.
 *
 * No assurances are provided by Nokia that the Contribution does not
 * infringe the patent or other intellectual property rights of any third
 * party or that the license provides you with all the necessary rights
 * to make use of the Contribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND. IN
 * ADDITION TO THE DISCLAIMERS INCLUDED IN THE LICENSE, NOKIA
 * SPECIFICALLY DISCLAIMS ANY LIABILITY FOR CLAIMS BROUGHT BY YOU OR ANY
 * OTHER ENTITY BASED ON INFRINGEMENT OF INTELLECTUAL PROPERTY RIGHTS OR
 * OTHERWISE. */

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <openssl/buf.h>
#include <openssl/dh.h>
#include <openssl/err.h>
#include <openssl/md5.h>
#include <openssl/mem.h>
#include <openssl/obj.h>

#include "internal.h"


const SSL3_ENC_METHOD SSLv3_enc_data = {
    ssl3_prf,
    tls1_setup_key_block,
    tls1_generate_master_secret,
    tls1_change_cipher_state,
    ssl3_final_finish_mac,
    ssl3_cert_verify_mac,
    SSL3_MD_CLIENT_FINISHED_CONST, 4,
    SSL3_MD_SERVER_FINISHED_CONST, 4,
    ssl3_alert_code,
    tls1_export_keying_material,
    0,
};

int ssl3_supports_cipher(const SSL_CIPHER *cipher) {
  return 1;
}

int ssl3_set_handshake_header(SSL *s, int htype, unsigned long len) {
  uint8_t *p = (uint8_t *)s->init_buf->data;
  *(p++) = htype;
  l2n3(len, p);
  s->init_num = (int)len + SSL3_HM_HEADER_LENGTH;
  s->init_off = 0;

  /* Add the message to the handshake hash. */
  return ssl3_finish_mac(s, (uint8_t *)s->init_buf->data, s->init_num);
}

int ssl3_handshake_write(SSL *s) { return ssl3_do_write(s, SSL3_RT_HANDSHAKE); }

int ssl3_new(SSL *s) {
  SSL3_STATE *s3;

  s3 = OPENSSL_malloc(sizeof *s3);
  if (s3 == NULL) {
    goto err;
  }
  memset(s3, 0, sizeof *s3);
  memset(s3->rrec.seq_num, 0, sizeof(s3->rrec.seq_num));

  s->s3 = s3;

  /* Set the version to the highest supported version for TLS. This controls the
   * initial state of |s->enc_method| and what the API reports as the version
   * prior to negotiation.
   *
   * TODO(davidben): This is fragile and confusing. */
  s->version = TLS1_2_VERSION;
  return 1;
err:
  return 0;
}

void ssl3_free(SSL *s) {
  if (s == NULL || s->s3 == NULL) {
    return;
  }

  BUF_MEM_free(s->s3->sniff_buffer);
  ssl3_cleanup_key_block(s);
  ssl3_release_read_buffer(s);
  ssl3_release_write_buffer(s);
  DH_free(s->s3->tmp.dh);
  EC_KEY_free(s->s3->tmp.ecdh);

  sk_X509_NAME_pop_free(s->s3->tmp.ca_names, X509_NAME_free);
  OPENSSL_free(s->s3->tmp.certificate_types);
  OPENSSL_free(s->s3->tmp.peer_ecpointformatlist);
  OPENSSL_free(s->s3->tmp.peer_ellipticcurvelist);
  OPENSSL_free(s->s3->tmp.peer_psk_identity_hint);
  BIO_free(s->s3->handshake_buffer);
  ssl3_free_digest_list(s);
  OPENSSL_free(s->s3->alpn_selected);

  OPENSSL_cleanse(s->s3, sizeof *s->s3);
  OPENSSL_free(s->s3);
  s->s3 = NULL;
}

static int ssl3_set_req_cert_type(CERT *c, const uint8_t *p, size_t len);

int SSL_session_reused(const SSL *ssl) {
  return ssl->hit;
}

int SSL_total_renegotiations(const SSL *ssl) {
  return ssl->s3->total_renegotiations;
}

int SSL_num_renegotiations(const SSL *ssl) {
  return SSL_total_renegotiations(ssl);
}

int SSL_CTX_need_tmp_RSA(const SSL_CTX *ctx) {
  return 0;
}

int SSL_need_rsa(const SSL *ssl) {
  return 0;
}

int SSL_CTX_set_tmp_rsa(SSL_CTX *ctx, const RSA *rsa) {
  return 1;
}

int SSL_set_tmp_rsa(SSL *ssl, const RSA *rsa) {
  return 1;
}

int SSL_CTX_set_tmp_dh(SSL_CTX *ctx, const DH *dh) {
  DH_free(ctx->cert->dh_tmp);
  ctx->cert->dh_tmp = DHparams_dup(dh);
  if (ctx->cert->dh_tmp == NULL) {
    OPENSSL_PUT_ERROR(SSL, SSL_CTX_set_tmp_dh, ERR_R_DH_LIB);
    return 0;
  }
  return 1;
}

int SSL_set_tmp_dh(SSL *ssl, const DH *dh) {
  DH_free(ssl->cert->dh_tmp);
  ssl->cert->dh_tmp = DHparams_dup(dh);
  if (ssl->cert->dh_tmp == NULL) {
    OPENSSL_PUT_ERROR(SSL, SSL_set_tmp_dh, ERR_R_DH_LIB);
    return 0;
  }
  return 1;
}

int SSL_CTX_set_tmp_ecdh(SSL_CTX *ctx, const EC_KEY *ec_key) {
  if (ec_key == NULL || EC_KEY_get0_group(ec_key) == NULL) {
    OPENSSL_PUT_ERROR(SSL, SSL_CTX_set_tmp_ecdh, ERR_R_PASSED_NULL_PARAMETER);
    return 0;
  }
  ctx->cert->ecdh_nid = EC_GROUP_get_curve_name(EC_KEY_get0_group(ec_key));
  return 1;
}

int SSL_set_tmp_ecdh(SSL *ssl, const EC_KEY *ec_key) {
  if (ec_key == NULL || EC_KEY_get0_group(ec_key) == NULL) {
    OPENSSL_PUT_ERROR(SSL, SSL_set_tmp_ecdh, ERR_R_PASSED_NULL_PARAMETER);
    return 0;
  }
  ssl->cert->ecdh_nid = EC_GROUP_get_curve_name(EC_KEY_get0_group(ec_key));
  return 1;
}

int SSL_CTX_enable_tls_channel_id(SSL_CTX *ctx) {
  ctx->tlsext_channel_id_enabled = 1;
  return 1;
}

int SSL_enable_tls_channel_id(SSL *ssl) {
  ssl->tlsext_channel_id_enabled = 1;
  return 1;
}

int SSL_CTX_set1_tls_channel_id(SSL_CTX *ctx, EVP_PKEY *private_key) {
  ctx->tlsext_channel_id_enabled = 1;
  if (EVP_PKEY_id(private_key) != EVP_PKEY_EC ||
      EVP_PKEY_bits(private_key) != 256) {
    OPENSSL_PUT_ERROR(SSL, SSL_CTX_set1_tls_channel_id,
                      SSL_R_CHANNEL_ID_NOT_P256);
    return 0;
  }
  EVP_PKEY_free(ctx->tlsext_channel_id_private);
  ctx->tlsext_channel_id_private = EVP_PKEY_up_ref(private_key);
  return 1;
}

int SSL_set1_tls_channel_id(SSL *ssl, EVP_PKEY *private_key) {
  ssl->tlsext_channel_id_enabled = 1;
  if (EVP_PKEY_id(private_key) != EVP_PKEY_EC ||
      EVP_PKEY_bits(private_key) != 256) {
    OPENSSL_PUT_ERROR(SSL, SSL_set1_tls_channel_id, SSL_R_CHANNEL_ID_NOT_P256);
    return 0;
  }
  EVP_PKEY_free(ssl->tlsext_channel_id_private);
  ssl->tlsext_channel_id_private = EVP_PKEY_up_ref(private_key);
  return 1;
}

size_t SSL_get_tls_channel_id(SSL *ssl, uint8_t *out, size_t max_out) {
  if (!ssl->s3->tlsext_channel_id_valid) {
    return 0;
  }
  memcpy(out, ssl->s3->tlsext_channel_id, (max_out < 64) ? max_out : 64);
  return 64;
}

int SSL_set_tlsext_host_name(SSL *ssl, const char *name) {
  OPENSSL_free(ssl->tlsext_hostname);
  ssl->tlsext_hostname = NULL;

  if (name == NULL) {
    return 1;
  }
  if (strlen(name) > TLSEXT_MAXLEN_host_name) {
    OPENSSL_PUT_ERROR(SSL, SSL_set_tlsext_host_name,
                      SSL_R_SSL3_EXT_INVALID_SERVERNAME);
    return 0;
  }
  ssl->tlsext_hostname = BUF_strdup(name);
  if (ssl->tlsext_hostname == NULL) {
    OPENSSL_PUT_ERROR(SSL, SSL_set_tlsext_host_name, ERR_R_MALLOC_FAILURE);
    return 0;
  }
  return 1;
}

long ssl3_ctrl(SSL *s, int cmd, long larg, void *parg) {
  int ret = 0;

  switch (cmd) {
    case SSL_CTRL_CHAIN:
      if (larg) {
        return ssl_cert_set1_chain(s->cert, (STACK_OF(X509) *)parg);
      } else {
        return ssl_cert_set0_chain(s->cert, (STACK_OF(X509) *)parg);
      }

    case SSL_CTRL_CHAIN_CERT:
      if (larg) {
        return ssl_cert_add1_chain_cert(s->cert, (X509 *)parg);
      } else {
        return ssl_cert_add0_chain_cert(s->cert, (X509 *)parg);
      }

    case SSL_CTRL_GET_CHAIN_CERTS:
      *(STACK_OF(X509) **)parg = s->cert->key->chain;
      ret = 1;
      break;

    case SSL_CTRL_SELECT_CURRENT_CERT:
      return ssl_cert_select_current(s->cert, (X509 *)parg);

    case SSL_CTRL_GET_CURVES: {
      const uint16_t *clist = s->s3->tmp.peer_ellipticcurvelist;
      size_t clistlen = s->s3->tmp.peer_ellipticcurvelist_length;
      if (parg) {
        size_t i;
        int *cptr = parg;
        int nid;
        for (i = 0; i < clistlen; i++) {
          nid = tls1_ec_curve_id2nid(clist[i]);
          if (nid != NID_undef) {
            cptr[i] = nid;
          } else {
            cptr[i] = TLSEXT_nid_unknown | clist[i];
          }
        }
      }
      return (int)clistlen;
    }

    case SSL_CTRL_SET_CURVES:
      return tls1_set_curves(&s->tlsext_ellipticcurvelist,
                             &s->tlsext_ellipticcurvelist_length, parg, larg);

    case SSL_CTRL_SET_SIGALGS:
      return tls1_set_sigalgs(s->cert, parg, larg, 0);

    case SSL_CTRL_SET_CLIENT_SIGALGS:
      return tls1_set_sigalgs(s->cert, parg, larg, 1);

    case SSL_CTRL_GET_CLIENT_CERT_TYPES: {
      const uint8_t **pctype = parg;
      if (s->server || !s->s3->tmp.cert_req) {
        return 0;
      }
      if (pctype) {
        *pctype = s->s3->tmp.certificate_types;
      }
      return (int)s->s3->tmp.num_certificate_types;
    }

    case SSL_CTRL_SET_CLIENT_CERT_TYPES:
      if (!s->server) {
        return 0;
      }
      return ssl3_set_req_cert_type(s->cert, parg, larg);

    case SSL_CTRL_BUILD_CERT_CHAIN:
      return ssl_build_cert_chain(s->cert, s->ctx->cert_store, larg);

    case SSL_CTRL_SET_VERIFY_CERT_STORE:
      return ssl_cert_set_cert_store(s->cert, parg, 0, larg);

    case SSL_CTRL_SET_CHAIN_CERT_STORE:
      return ssl_cert_set_cert_store(s->cert, parg, 1, larg);

    case SSL_CTRL_GET_SERVER_TMP_KEY:
      if (s->server || !s->session || !s->session->sess_cert) {
        return 0;
      } else {
        SESS_CERT *sc;
        EVP_PKEY *ptmp;
        int rv = 0;
        sc = s->session->sess_cert;
        if (!sc->peer_dh_tmp && !sc->peer_ecdh_tmp) {
          return 0;
        }
        ptmp = EVP_PKEY_new();
        if (!ptmp) {
          return 0;
        }
        if (sc->peer_dh_tmp) {
          rv = EVP_PKEY_set1_DH(ptmp, sc->peer_dh_tmp);
        } else if (sc->peer_ecdh_tmp) {
          rv = EVP_PKEY_set1_EC_KEY(ptmp, sc->peer_ecdh_tmp);
        }
        if (rv) {
          *(EVP_PKEY **)parg = ptmp;
          return 1;
        }
        EVP_PKEY_free(ptmp);
        return 0;
      }

    case SSL_CTRL_GET_EC_POINT_FORMATS: {
      const uint8_t **pformat = parg;
      if (!s->s3->tmp.peer_ecpointformatlist) {
        return 0;
      }
      *pformat = s->s3->tmp.peer_ecpointformatlist;
      return (int)s->s3->tmp.peer_ecpointformatlist_length;
    }

    default:
      break;
  }

  return ret;
}

long ssl3_ctx_ctrl(SSL_CTX *ctx, int cmd, long larg, void *parg) {
  switch (cmd) {
    case SSL_CTRL_SET_CURVES:
      return tls1_set_curves(&ctx->tlsext_ellipticcurvelist,
                             &ctx->tlsext_ellipticcurvelist_length, parg, larg);

    case SSL_CTRL_SET_SIGALGS:
      return tls1_set_sigalgs(ctx->cert, parg, larg, 0);

    case SSL_CTRL_SET_CLIENT_SIGALGS:
      return tls1_set_sigalgs(ctx->cert, parg, larg, 1);

    case SSL_CTRL_SET_CLIENT_CERT_TYPES:
      return ssl3_set_req_cert_type(ctx->cert, parg, larg);

    case SSL_CTRL_BUILD_CERT_CHAIN:
      return ssl_build_cert_chain(ctx->cert, ctx->cert_store, larg);

    case SSL_CTRL_SET_VERIFY_CERT_STORE:
      return ssl_cert_set_cert_store(ctx->cert, parg, 0, larg);

    case SSL_CTRL_SET_CHAIN_CERT_STORE:
      return ssl_cert_set_cert_store(ctx->cert, parg, 1, larg);

    case SSL_CTRL_EXTRA_CHAIN_CERT:
      if (ctx->extra_certs == NULL) {
        ctx->extra_certs = sk_X509_new_null();
        if (ctx->extra_certs == NULL) {
          return 0;
        }
      }
      sk_X509_push(ctx->extra_certs, (X509 *)parg);
      break;

    case SSL_CTRL_GET_EXTRA_CHAIN_CERTS:
      if (ctx->extra_certs == NULL && larg == 0) {
        *(STACK_OF(X509) **)parg = ctx->cert->key->chain;
      } else {
        *(STACK_OF(X509) **)parg = ctx->extra_certs;
      }
      break;

    case SSL_CTRL_CLEAR_EXTRA_CHAIN_CERTS:
      sk_X509_pop_free(ctx->extra_certs, X509_free);
      ctx->extra_certs = NULL;
      break;

    case SSL_CTRL_CHAIN:
      if (larg) {
        return ssl_cert_set1_chain(ctx->cert, (STACK_OF(X509) *)parg);
      } else {
        return ssl_cert_set0_chain(ctx->cert, (STACK_OF(X509) *)parg);
      }

    case SSL_CTRL_CHAIN_CERT:
      if (larg) {
        return ssl_cert_add1_chain_cert(ctx->cert, (X509 *)parg);
      } else {
        return ssl_cert_add0_chain_cert(ctx->cert, (X509 *)parg);
      }

    case SSL_CTRL_GET_CHAIN_CERTS:
      *(STACK_OF(X509) **)parg = ctx->cert->key->chain;
      break;

    case SSL_CTRL_SELECT_CURRENT_CERT:
      return ssl_cert_select_current(ctx->cert, (X509 *)parg);

    default:
      return 0;
  }

  return 1;
}

int SSL_CTX_set_tlsext_servername_callback(
    SSL_CTX *ctx, int (*callback)(SSL *ssl, int *out_alert, void *arg)) {
  ctx->tlsext_servername_callback = callback;
  return 1;
}

int SSL_CTX_set_tlsext_servername_arg(SSL_CTX *ctx, void *arg) {
  ctx->tlsext_servername_arg = arg;
  return 1;
}

int SSL_CTX_get_tlsext_ticket_keys(SSL_CTX *ctx, void *out, size_t len) {
  if (out == NULL) {
    return 48;
  }
  if (len != 48) {
    OPENSSL_PUT_ERROR(SSL, SSL_CTX_get_tlsext_ticket_keys,
                      SSL_R_INVALID_TICKET_KEYS_LENGTH);
    return 0;
  }
  uint8_t *out_bytes = out;
  memcpy(out_bytes, ctx->tlsext_tick_key_name, 16);
  memcpy(out_bytes + 16, ctx->tlsext_tick_hmac_key, 16);
  memcpy(out_bytes + 32, ctx->tlsext_tick_aes_key, 16);
  return 1;
}

int SSL_CTX_set_tlsext_ticket_keys(SSL_CTX *ctx, const void *in, size_t len) {
  if (in == NULL) {
    return 48;
  }
  if (len != 48) {
    OPENSSL_PUT_ERROR(SSL, SSL_CTX_set_tlsext_ticket_keys,
                      SSL_R_INVALID_TICKET_KEYS_LENGTH);
    return 0;
  }
  const uint8_t *in_bytes = in;
  memcpy(ctx->tlsext_tick_key_name, in_bytes, 16);
  memcpy(ctx->tlsext_tick_hmac_key, in_bytes + 16, 16);
  memcpy(ctx->tlsext_tick_aes_key, in_bytes + 32, 16);
  return 1;
}

int SSL_CTX_set_tlsext_ticket_key_cb(
    SSL_CTX *ctx, int (*callback)(SSL *ssl, uint8_t *key_name, uint8_t *iv,
                                  EVP_CIPHER_CTX *ctx, HMAC_CTX *hmac_ctx,
                                  int encrypt)) {
  ctx->tlsext_ticket_key_cb = callback;
  return 1;
}

struct ssl_cipher_preference_list_st *ssl_get_cipher_preferences(SSL *s) {
  if (s->cipher_list != NULL) {
    return s->cipher_list;
  }

  if (s->version >= TLS1_1_VERSION && s->ctx != NULL &&
      s->ctx->cipher_list_tls11 != NULL) {
    return s->ctx->cipher_list_tls11;
  }

  if (s->ctx != NULL && s->ctx->cipher_list != NULL) {
    return s->ctx->cipher_list;
  }

  return NULL;
}

const SSL_CIPHER *ssl3_choose_cipher(
    SSL *s, STACK_OF(SSL_CIPHER) *clnt,
    struct ssl_cipher_preference_list_st *server_pref) {
  const SSL_CIPHER *c, *ret = NULL;
  STACK_OF(SSL_CIPHER) *srvr = server_pref->ciphers, *prio, *allow;
  size_t i;
  int ok;
  size_t cipher_index;
  uint32_t alg_k, alg_a, mask_k, mask_a;
  /* in_group_flags will either be NULL, or will point to an array of bytes
   * which indicate equal-preference groups in the |prio| stack. See the
   * comment about |in_group_flags| in the |ssl_cipher_preference_list_st|
   * struct. */
  const uint8_t *in_group_flags;
  /* group_min contains the minimal index so far found in a group, or -1 if no
   * such value exists yet. */
  int group_min = -1;

  if (s->options & SSL_OP_CIPHER_SERVER_PREFERENCE) {
    prio = srvr;
    in_group_flags = server_pref->in_group_flags;
    allow = clnt;
  } else {
    prio = clnt;
    in_group_flags = NULL;
    allow = srvr;
  }

  ssl_get_compatible_server_ciphers(s, &mask_k, &mask_a);

  for (i = 0; i < sk_SSL_CIPHER_num(prio); i++) {
    c = sk_SSL_CIPHER_value(prio, i);

    ok = 1;

    /* Skip TLS v1.2 only ciphersuites if not supported */
    if ((c->algorithm_ssl & SSL_TLSV1_2) && !SSL_USE_TLS1_2_CIPHERS(s)) {
      ok = 0;
    }

    alg_k = c->algorithm_mkey;
    alg_a = c->algorithm_auth;

    ok = ok && (alg_k & mask_k) && (alg_a & mask_a);

    if (ok && sk_SSL_CIPHER_find(allow, &cipher_index, c)) {
      if (in_group_flags != NULL && in_group_flags[i] == 1) {
        /* This element of |prio| is in a group. Update the minimum index found
         * so far and continue looking. */
        if (group_min == -1 || (size_t)group_min > cipher_index) {
          group_min = cipher_index;
        }
      } else {
        if (group_min != -1 && (size_t)group_min < cipher_index) {
          cipher_index = group_min;
        }
        ret = sk_SSL_CIPHER_value(allow, cipher_index);
        break;
      }
    }

    if (in_group_flags != NULL && in_group_flags[i] == 0 && group_min != -1) {
      /* We are about to leave a group, but we found a match in it, so that's
       * our answer. */
      ret = sk_SSL_CIPHER_value(allow, group_min);
      break;
    }
  }

  return ret;
}

int ssl3_get_req_cert_type(SSL *s, uint8_t *p) {
  int ret = 0;
  const uint8_t *sig;
  size_t i, siglen;
  int have_rsa_sign = 0;
  int have_ecdsa_sign = 0;

  /* If we have custom certificate types set, use them */
  if (s->cert->client_certificate_types) {
    memcpy(p, s->cert->client_certificate_types,
           s->cert->num_client_certificate_types);
    return s->cert->num_client_certificate_types;
  }

  /* get configured sigalgs */
  siglen = tls12_get_psigalgs(s, &sig);
  for (i = 0; i < siglen; i += 2, sig += 2) {
    switch (sig[1]) {
      case TLSEXT_signature_rsa:
        have_rsa_sign = 1;
        break;

      case TLSEXT_signature_ecdsa:
        have_ecdsa_sign = 1;
        break;
    }
  }

  if (have_rsa_sign) {
    p[ret++] = SSL3_CT_RSA_SIGN;
  }

  /* ECDSA certs can be used with RSA cipher suites as well so we don't need to
   * check for SSL_kECDH or SSL_kECDHE. */
  if (s->version >= TLS1_VERSION && have_ecdsa_sign) {
      p[ret++] = TLS_CT_ECDSA_SIGN;
  }

  return ret;
}

static int ssl3_set_req_cert_type(CERT *c, const uint8_t *p, size_t len) {
  OPENSSL_free(c->client_certificate_types);
  c->client_certificate_types = NULL;
  c->num_client_certificate_types = 0;

  if (!p || !len) {
    return 1;
  }

  if (len > 0xff) {
    return 0;
  }

  c->client_certificate_types = BUF_memdup(p, len);
  if (!c->client_certificate_types) {
    return 0;
  }

  c->num_client_certificate_types = len;
  return 1;
}

/* If we are using default SHA1+MD5 algorithms switch to new SHA256 PRF and
 * handshake macs if required. */
uint32_t ssl_get_algorithm2(SSL *s) {
  static const uint32_t kMask = SSL_HANDSHAKE_MAC_DEFAULT;
  uint32_t alg2 = s->s3->tmp.new_cipher->algorithm2;
  if (s->enc_method->enc_flags & SSL_ENC_FLAG_SHA256_PRF &&
      (alg2 & kMask) == kMask) {
    return SSL_HANDSHAKE_MAC_SHA256;
  }
  return alg2;
}
