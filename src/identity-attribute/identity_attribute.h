/*
   This file is part of GNUnet.
   Copyright (C) 2012-2015 GNUnet e.V.

   GNUnet is free software: you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published
   by the Free Software Foundation, either version 3 of the License,
   or (at your option) any later version.

   GNUnet is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Affero General Public License for more details.
   */
/**
 * @author Martin Schanzenbach
 * @file identity-attribute/identity_attribute.h
 * @brief GNUnet Identity attributes
 *
 */
#ifndef IDENTITY_ATTRIBUTE_H
#define IDENTITY_ATTRIBUTE_H

#include "gnunet_identity_provider_service.h"

struct Attribute
{
  /**
   * Attribute type
   */
  uint32_t attribute_type;

  /**
   * Attribute version
   */
  uint32_t attribute_version;

  /**
   * Name length
   */
  uint32_t name_len;
  
  /**
   * Data size
   */
  uint32_t data_size;

  //followed by data_size Attribute value data
};

#endif
