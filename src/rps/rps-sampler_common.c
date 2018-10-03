/*
     This file is part of GNUnet.
     Copyright (C)

     GNUnet is free software: you can redistribute it and/or modify it
     under the terms of the GNU Affero General Public License as published
     by the Free Software Foundation, either version 3 of the License,
     or (at your option) any later version.

     GNUnet is distributed in the hope that it will be useful, but
     WITHOUT ANY WARRANTY; without even the implied warranty of
     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
     Affero General Public License for more details.

     You should have received a copy of the GNU Affero General Public License
     along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/**
 * @file rps/rps-sampler_common.c
 * @brief Code common to client and service sampler
 * @author Julius Bünger
 */
#include "platform.h"
#include "gnunet_util_lib.h"
#include "gnunet_statistics_service.h"

#include "rps-sampler_common.h"
#include "gnunet-service-rps_sampler_elem.h"

#include <math.h>
#include <inttypes.h>

#include "rps-test_util.h"

#define LOG(kind, ...) GNUNET_log_from(kind,"rps-sampler_common",__VA_ARGS__)

/**
 * @brief Context for a callback. Contains callback and closure.
 *
 * Meant to be an entry in an DLL.
 */
struct SamplerNotifyUpdateCTX
{
  /**
   * @brief The Callback to call on updates
   */
  SamplerNotifyUpdateCB notify_cb;

  /**
   * @brief The according closure.
   */
  void *cls;

  /**
   * @brief Next element in DLL.
   */
  struct SamplerNotifyUpdateCTX *next;

  /**
   * @brief Previous element in DLL.
   */
  struct SamplerNotifyUpdateCTX *prev;
};


/**
 * Closure to _get_n_rand_peers_ready_cb()
 */
struct RPS_SamplerRequestHandle
{
  /**
   * DLL
   */
  struct RPS_SamplerRequestHandle *next;
  struct RPS_SamplerRequestHandle *prev;

  /**
   * Number of peers we are waiting for.
   */
  uint32_t num_peers;

  /**
   * Number of peers we currently have.
   */
  uint32_t cur_num_peers;

  /**
   * Pointer to the array holding the ids.
   */
  struct GNUNET_PeerIdentity *ids;

  /**
   * Head and tail for the DLL to store the tasks for single requests
   */
  struct GetPeerCls *gpc_head;
  struct GetPeerCls *gpc_tail;

  /**
   * Sampler.
   */
  struct RPS_Sampler *sampler;

  /**
   * Callback to be called when all ids are available.
   */
  RPS_sampler_n_rand_peers_ready_cb callback;

  /**
   * Closure given to the callback
   */
  void *cls;
};


/**
 * @brief Add a callback that will be called when the next peer is inserted
 * into the sampler
 *
 * @param sampler The sampler on which update it will be called
 * @param notify_cb The callback
 * @param cls Closure given to the callback
 *
 * @return The context containing callback and closure
 */
struct SamplerNotifyUpdateCTX *
sampler_notify_on_update (struct RPS_Sampler *sampler,
                          SamplerNotifyUpdateCB notify_cb,
                          void *cls)
{
  struct SamplerNotifyUpdateCTX *notify_ctx;

  LOG (GNUNET_ERROR_TYPE_DEBUG,
      "Inserting new context for notification\n");
  notify_ctx = GNUNET_new (struct SamplerNotifyUpdateCTX);
  notify_ctx->notify_cb = notify_cb;
  notify_ctx->cls = cls;
  GNUNET_CONTAINER_DLL_insert (sampler->notify_ctx_head,
                               sampler->notify_ctx_tail,
                               notify_ctx);
  return notify_ctx;
}


/**
 * Get the size of the sampler.
 *
 * @param sampler the sampler to return the size of.
 * @return the size of the sampler
 */
unsigned int
RPS_sampler_get_size (struct RPS_Sampler *sampler)
{
  return sampler->sampler_size;
}


/**
 * @brief Notify about update of the sampler.
 *
 * Call the callbacks that are waiting for notification on updates to the
 * sampler.
 *
 * @param sampler The sampler the updates are waiting for
 */
static void
notify_update (struct RPS_Sampler *sampler)
{
  struct SamplerNotifyUpdateCTX *tmp_notify_head;
  struct SamplerNotifyUpdateCTX *tmp_notify_tail;

  LOG (GNUNET_ERROR_TYPE_DEBUG,
      "Calling callbacks waiting for update notification.\n");
  tmp_notify_head = sampler->notify_ctx_head;
  tmp_notify_tail = sampler->notify_ctx_tail;
  sampler->notify_ctx_head = NULL;
  sampler->notify_ctx_tail = NULL;
  for (struct SamplerNotifyUpdateCTX *notify_iter = tmp_notify_head;
       NULL != tmp_notify_head;
       notify_iter = tmp_notify_head)
  {
    GNUNET_assert (NULL != notify_iter->notify_cb);
    GNUNET_CONTAINER_DLL_remove (tmp_notify_head,
                                 tmp_notify_tail,
                                 notify_iter);
    notify_iter->notify_cb (notify_iter->cls);
    GNUNET_free (notify_iter);
  }
}


/**
 * Update every sampler element of this sampler with given peer
 *
 * @param sampler the sampler to update.
 * @param id the PeerID that is put in the sampler
 */
  void
RPS_sampler_update (struct RPS_Sampler *sampler,
                    const struct GNUNET_PeerIdentity *id)
{
  to_file (sampler->file_name,
           "Got %s",
           GNUNET_i2s_full (id));

  for (uint32_t i = 0; i < sampler->sampler_size; i++)
  {
    RPS_sampler_elem_next (sampler->sampler_elements[i],
                           id);
  }
  notify_update (sampler);
}


/**
 * Reinitialise all previously initialised sampler elements with the given value.
 *
 * Used to get rid of a PeerID.
 *
 * @param sampler the sampler to reinitialise a sampler element in.
 * @param id the id of the sampler elements to update.
 */
  void
RPS_sampler_reinitialise_by_value (struct RPS_Sampler *sampler,
                                   const struct GNUNET_PeerIdentity *id)
{
  uint32_t i;

  for (i = 0; i < sampler->sampler_size; i++)
  {
    if (0 == GNUNET_CRYPTO_cmp_peer_identity(id,
          &(sampler->sampler_elements[i]->peer_id)) )
    {
      LOG (GNUNET_ERROR_TYPE_DEBUG, "Reinitialising sampler\n");
      to_file (sampler->sampler_elements[i]->file_name,
               "--- non-active");
      RPS_sampler_elem_reinit (sampler->sampler_elements[i]);
    }
  }
}


/**
 * Counts how many Samplers currently hold a given PeerID.
 *
 * @param sampler the sampler to count ids in.
 * @param id the PeerID to count.
 *
 * @return the number of occurrences of id.
 */
  uint32_t
RPS_sampler_count_id (struct RPS_Sampler *sampler,
                      const struct GNUNET_PeerIdentity *id)
{
  uint32_t count;
  uint32_t i;

  count = 0;
  for ( i = 0 ; i < sampler->sampler_size ; i++ )
  {
    if ( 0 == GNUNET_CRYPTO_cmp_peer_identity (&sampler->sampler_elements[i]->peer_id, id)
        && EMPTY != sampler->sampler_elements[i]->is_empty)
      count++;
  }
  return count;
}


/**
 * Grow or shrink the size of the sampler.
 *
 * @param sampler the sampler to resize.
 * @param new_size the new size of the sampler
 */
static void
sampler_resize (struct RPS_Sampler *sampler, unsigned int new_size)
{
  unsigned int old_size;
  uint32_t i;

  // TODO check min and max size

  old_size = sampler->sampler_size;

  if (old_size > new_size)
  { /* Shrinking */

    LOG (GNUNET_ERROR_TYPE_DEBUG,
         "Shrinking sampler %d -> %d\n",
         old_size,
         new_size);

    to_file (sampler->file_name,
         "Shrinking sampler %d -> %d",
         old_size,
         new_size);

    for (i = new_size ; i < old_size ; i++)
    {
      to_file (sampler->file_name,
               "-%" PRIu32 ": %s",
               i,
               sampler->sampler_elements[i]->file_name);
      RPS_sampler_elem_destroy (sampler->sampler_elements[i]);
    }

    GNUNET_array_grow (sampler->sampler_elements,
                       sampler->sampler_size,
                       new_size);
    LOG (GNUNET_ERROR_TYPE_DEBUG,
         "sampler->sampler_elements now points to %p\n",
         sampler->sampler_elements);

  }
  else if (old_size < new_size)
  { /* Growing */
    LOG (GNUNET_ERROR_TYPE_DEBUG,
         "Growing sampler %d -> %d\n",
         old_size,
         new_size);

    to_file (sampler->file_name,
         "Growing sampler %d -> %d",
         old_size,
         new_size);

    GNUNET_array_grow (sampler->sampler_elements,
        sampler->sampler_size,
        new_size);

    for (i = old_size ; i < new_size ; i++)
    { /* Add new sampler elements */
      sampler->sampler_elements[i] = RPS_sampler_elem_create ();

      to_file (sampler->file_name,
               "+%" PRIu32 ": %s",
               i,
               sampler->sampler_elements[i]->file_name);
    }
  }
  else
  {
    LOG (GNUNET_ERROR_TYPE_DEBUG, "Size remains the same -- nothing to do\n");
    return;
  }

  GNUNET_assert (sampler->sampler_size == new_size);
}


/**
 * Grow or shrink the size of the sampler.
 *
 * @param sampler the sampler to resize.
 * @param new_size the new size of the sampler
 */
void
RPS_sampler_resize (struct RPS_Sampler *sampler, unsigned int new_size)
{
  GNUNET_assert (0 < new_size);
  sampler_resize (sampler, new_size);
}


/**
 * Empty the sampler.
 *
 * @param sampler the sampler to empty.
 * @param new_size the new size of the sampler
 */
static void
sampler_empty (struct RPS_Sampler *sampler)
{
  sampler_resize (sampler, 0);
}


/**
 * Callback to _get_rand_peer() used by _get_n_rand_peers().
 *
 * Checks whether all n peers are available. If they are,
 * give those back.
 */
static void
check_n_peers_ready (void *cls,
                     const struct GNUNET_PeerIdentity *id)
{
  struct RPS_SamplerRequestHandle *req_handle = cls;
  (void) id;

  req_handle->cur_num_peers++;
  LOG (GNUNET_ERROR_TYPE_DEBUG,
      "Got %" PRIX32 ". of %" PRIX32 " peers\n",
      req_handle->cur_num_peers, req_handle->num_peers);

  if (req_handle->num_peers == req_handle->cur_num_peers)
  { /* All peers are ready -- return those to the client */
    GNUNET_assert (NULL != req_handle->callback);

    LOG (GNUNET_ERROR_TYPE_DEBUG,
        "returning %" PRIX32 " peers to the client\n",
        req_handle->num_peers);
    req_handle->callback (req_handle->ids, req_handle->num_peers, req_handle->cls);

    RPS_sampler_request_cancel (req_handle);
  }
}


/**
 * Get n random peers out of the sampled peers.
 *
 * We might want to reinitialise this sampler after giving the
 * corrsponding peer to the client.
 * Random with or without consumption?
 *
 * @param sampler the sampler to get peers from.
 * @param cb callback that will be called once the ids are ready.
 * @param cls closure given to @a cb
 * @param for_client #GNUNET_YES if result is used for client,
 *                   #GNUNET_NO if used internally
 * @param num_peers the number of peers requested
 */
struct RPS_SamplerRequestHandle *
RPS_sampler_get_n_rand_peers (struct RPS_Sampler *sampler,
                              uint32_t num_peers,
                              RPS_sampler_n_rand_peers_ready_cb cb,
                              void *cls)
{
  uint32_t i;
  struct RPS_SamplerRequestHandle *req_handle;
  struct GetPeerCls *gpc;

  GNUNET_assert (0 != sampler->sampler_size);
  if (0 == num_peers)
    return NULL;

  // TODO check if we have too much (distinct) sampled peers
  req_handle = GNUNET_new (struct RPS_SamplerRequestHandle);
  req_handle->num_peers = num_peers;
  req_handle->cur_num_peers = 0;
  req_handle->ids = GNUNET_new_array (num_peers, struct GNUNET_PeerIdentity);
  req_handle->sampler = sampler;
  req_handle->callback = cb;
  req_handle->cls = cls;
  GNUNET_CONTAINER_DLL_insert (sampler->req_handle_head,
                               sampler->req_handle_tail,
                               req_handle);

  LOG (GNUNET_ERROR_TYPE_DEBUG,
      "Scheduling requests for %" PRIu32 " peers\n", num_peers);

  for (i = 0; i < num_peers; i++)
  {
    gpc = GNUNET_new (struct GetPeerCls);
    gpc->req_handle = req_handle;
    gpc->cont = check_n_peers_ready;
    gpc->cont_cls = req_handle;
    gpc->id = &req_handle->ids[i];

    GNUNET_CONTAINER_DLL_insert (req_handle->gpc_head,
                                 req_handle->gpc_tail,
                                 gpc);
    // maybe add a little delay
    gpc->get_peer_task = GNUNET_SCHEDULER_add_now (sampler->get_peers,
						   gpc);
  }
  return req_handle;
}

/**
 * Cancle a request issued through #RPS_sampler_n_rand_peers_ready_cb.
 *
 * @param req_handle the handle to the request
 */
void
RPS_sampler_request_cancel (struct RPS_SamplerRequestHandle *req_handle)
{
  struct GetPeerCls *i;

  while (NULL != (i = req_handle->gpc_head) )
  {
    GNUNET_CONTAINER_DLL_remove (req_handle->gpc_head,
                                 req_handle->gpc_tail,
                                 i);
    if (NULL != i->get_peer_task)
    {
      GNUNET_SCHEDULER_cancel (i->get_peer_task);
    }
    if (NULL != i->notify_ctx)
    {
      GNUNET_CONTAINER_DLL_remove (req_handle->sampler->notify_ctx_head,
                                   req_handle->sampler->notify_ctx_tail,
                                   i->notify_ctx);
      GNUNET_free (i->notify_ctx);
    }
    GNUNET_free (i);
  }
  GNUNET_free (req_handle->ids);
  GNUNET_CONTAINER_DLL_remove (req_handle->sampler->req_handle_head,
                               req_handle->sampler->req_handle_tail,
                               req_handle);
  GNUNET_free (req_handle);
}


/**
 * Cleans the sampler.
 */
  void
RPS_sampler_destroy (struct RPS_Sampler *sampler)
{
  if (NULL != sampler->req_handle_head)
  {
    LOG (GNUNET_ERROR_TYPE_WARNING,
        "There are still pending requests. Going to remove them.\n");
    while (NULL != sampler->req_handle_head)
    {
      RPS_sampler_request_cancel (sampler->req_handle_head);
    }
  }
  sampler_empty (sampler);
  GNUNET_free (sampler);
}


/* end of rps-sampler_common.c */
