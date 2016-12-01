/*
     This file is part of GNUnet
     Copyright (C) 2013 GNUnet e.V.

     GNUnet is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License as published
     by the Free Software Foundation; either version 3, or (at your
     option) any later version.

     GNUnet is distributed in the hope that it will be useful, but
     WITHOUT ANY WARRANTY; without even the implied warranty of
     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
     General Public License for more details.

     You should have received a copy of the GNU General Public License
     along with GNUnet; see the file COPYING.  If not, write to the
     Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
     Boston, MA 02110-1301, USA.
*/

/**
 * @file credential/plugin_gnsrecord_credential.c
 * @brief gnsrecord plugin to provide the API for CREDENTIAL records
 * @author Adnan Husain
 */

#include "platform.h"
#include "gnunet_util_lib.h"
#include "gnunet_gnsrecord_lib.h"
#include "gnunet_credential_service.h"
#include "gnunet_gnsrecord_plugin.h"


/**
 * Convert the 'value' of a record to a string.
 *
 * @param cls closure, unused
 * @param type type of the record
 * @param data value in binary encoding
 * @param data_size number of bytes in @a data
 * @return NULL on error, otherwise human-readable representation of the value
 */
static char *
credential_value_to_string (void *cls,
                              uint32_t type,
                              const void *data,
                              size_t data_size)
{

  const char *cdata;

  switch (type)
  {
   case GNUNET_GNSRECORD_TYPE_CREDENTIAL:
   {
    struct GNUNET_CREDENTIAL_AttributeRecordData cred;
    char *cred_str;
    char *subject_pkey;
    char *issuer_pkey;
    uint32_t cf; // Credential flags
    if (data_size < sizeof (struct GNUNET_CREDENTIAL_AttributeRecordData))
        return NULL; /* malformed */
    memcpy (&cred,
              data,
              sizeof (cred));
    cdata = data;  
    subject_pkey = GNUNET_CRYPTO_ecdsa_public_key_to_string (&cred.subject_key);
    issuer_pkey = GNUNET_CRYPTO_ecdsa_public_key_to_string (&cred.issuer_key);
    cf = ntohl (cred.credential_flags);

     GNUNET_asprintf (&cred_str,
                     "%s %s %u %s",
                     subject_pkey,
                     issuer_pkey,
                     (unsigned int) cf,
                     &cdata[sizeof (cred)]);
      GNUNET_free (subject_pkey);
      GNUNET_free (issuer_pkey);



    return cred_str;
    }
  default:
    return NULL;
  }
}


/**
 * Convert human-readable version of a 'value' of a record to the binary
 * representation.
 *
 * @param cls closure, unused
 * @param type type of the record
 * @param s human-readable string
 * @param data set to value in binary encoding (will be allocated)
 * @param data_size set to number of bytes in @a data
 * @return #GNUNET_OK on success
 */
static int
credential_string_to_value (void *cls,
                     uint32_t type,
                     const char *s,
                     void **data,
                     size_t *data_size)
{
  if (NULL == s)
    return GNUNET_SYSERR;
  switch (type)
  {
    case GNUNET_GNSRECORD_TYPE_CREDENTIAL:
      { 
        struct GNUNET_CREDENTIAL_AttributeRecordData *cred;
        unsigned int cf; // credential flags

        size_t enclen = (sizeof (struct GNUNET_CRYPTO_EcdsaPublicKey)) * 8;
        if (enclen % 5 > 0)
          enclen += 5 - enclen % 5;
        enclen /= 5; /* 260/5 = 52 */
        char subject_pkey[enclen + 1];
        char issuer_pkey[enclen + 1];
        char name[253 + 1];

        if (5 != SSCANF (s,
                         "%52s %52s %u %253s",
                         subject_pkey,
                         issuer_pkey,
                         &cf,
                         name))
        {
          GNUNET_log (GNUNET_ERROR_TYPE_ERROR,
                      _("Unable to parse CRED record string `%s'\n"),
                      s);
          return GNUNET_SYSERR;
        }
        *data_size = sizeof (struct GNUNET_CREDENTIAL_AttributeRecordData) + strlen (name) + 1;
        *data = cred = GNUNET_malloc (*data_size);
        GNUNET_CRYPTO_ecdsa_public_key_from_string (subject_pkey,
                                                    strlen (subject_pkey),
                                                    &cred->subject_key);
        GNUNET_CRYPTO_ecdsa_public_key_from_string (issuer_pkey,
                                                    strlen (issuer_pkey),
                                                    &cred->issuer_key);
        cred->credential_flags = htonl (cf);
        GNUNET_memcpy (&cred[1],
                       name,
                       strlen (name));


        *data = GNUNET_strdup (s);
        *data_size = strlen (s);
        return GNUNET_OK;
      }
    default:
      return GNUNET_SYSERR;
  }
}


/**
 * Mapping of record type numbers to human-readable
 * record type names.
 */
static struct {
  const char *name;
  uint32_t number;
} name_map[] = {
  { "CRED", GNUNET_GNSRECORD_TYPE_CREDENTIAL },
  { NULL, UINT32_MAX }
};


/**
 * Convert a type name (i.e. "AAAA") to the corresponding number.
 *
 * @param cls closure, unused
 * @param gns_typename name to convert
 * @return corresponding number, UINT32_MAX on error
 */
static uint32_t
credential_typename_to_number (void *cls,
                               const char *gns_typename)
{
  unsigned int i;

  i=0;
  while ( (name_map[i].name != NULL) &&
          (0 != strcasecmp (gns_typename, name_map[i].name)) )
    i++;
  return name_map[i].number;
}


/**
 * Convert a type number (i.e. 1) to the corresponding type string (i.e. "A")
 *
 * @param cls closure, unused
 * @param type number of a type to convert
 * @return corresponding typestring, NULL on error
 */
static const char *
credential_number_to_typename (void *cls,
                               uint32_t type)
{
  unsigned int i;

  i=0;
  while ( (name_map[i].name != NULL) &&
          (type != name_map[i].number) )
    i++;
  return name_map[i].name;
}


/**
 * Entry point for the plugin.
 *
 * @param cls NULL
 * @return the exported block API
 */
void *
libgnunet_plugin_gnsrecord_credential_init (void *cls)
{
  struct GNUNET_GNSRECORD_PluginFunctions *api;

  api = GNUNET_new (struct GNUNET_GNSRECORD_PluginFunctions);
  api->value_to_string = &credential_value_to_string;
  api->string_to_value = &credential_string_to_value;
  api->typename_to_number = &credential_typename_to_number;
  api->number_to_typename = &credential_number_to_typename;
  return api;
}


/**
 * Exit point from the plugin.
 *
 * @param cls the return value from #libgnunet_plugin_block_test_init
 * @return NULL
 */
void *
libgnunet_plugin_gnsrecord_credential_done (void *cls)
{
  struct GNUNET_GNSRECORD_PluginFunctions *api = cls;

  GNUNET_free (api);
  return NULL;
}

/* end of plugin_gnsrecord_credential.c */