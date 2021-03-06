/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Initial Developer of the Original Code is PageMail, Inc.
 *
 * Portions created by the Initial Developer are 
 * Copyright (c) 2010, PageMail, Inc. All Rights Reserved.
 *
 * Contributor(s): 
 * 
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** 
 *
 * @file        gpsee_datastores.c      Arbitrary per-runtime data stores for GPSEE. 
 *                                      Stores key/value pairs where each is a
 *                                      pointer-sized value.
 * @author      Wes Garland
 * @date        May 2010
 * @version     $Id: gpsee_datastores.c,v 1.3 2011/12/05 19:13:36 wes Exp $
 */

#include "gpsee.h"

/** @addtogroup  datastores 
 *  @{
 */

/** Internal structure storing the data. Subject to change without notice. */
struct dataStore /* Completes forward declaration in gpsee.h */
{
  size_t                size;           /**< Number of slots allocated for data */
  struct
  {
    const void          *key;           /**< Key */
    void                *value;         /**< Value */
  }                     *data;          /**< Slots in which data is stored */
  gpsee_monitor_t       monitor;        /**< Monitor we must be in to read/write the data store internals */
  gpsee_runtime_t       *grt;           /**< GPSEE Runtime which owns data store (monitor) */

  uint32                flags;          /**< Per-store flags (e.g. GPSEE_DS_BINARY_KEYS) */
};

/**
 *  Retrieve a value from a GPSEE Data Store. 
 *
 *  @param      store   The data store
 *  @param      key     The key describing the value to retrieve
 *  @returns    The value stored, or NULL
 *
 *  @note       When used on data stores created with GPSEE_DS_OTM_KEYS, this routine will
 *              return the first key match it happens to find; order is not guaranteed.
 */
void *gpsee_ds_get(gpsee_dataStore_t store, const void *key)
{
  size_t i;
  void          *ret = NULL;

  GPSEE_ASSERT(store != NULL);
  GPSEE_ASSERT(store->monitor != NULL);

  gpsee_enterMonitor(store->monitor);
  for (i=0; i < store->size; i++)
  {
    if (store->data[i].key == key)
    {
      ret = store->data[i].value;
      goto out;
    }
  }

  out:
  gpsee_leaveMonitor(store->monitor);
  return ret;
}

/**
 *  Put a value into a GPSEE Data Store. Failure to insert does not corrupt 
 *  existing data.  Unless the GPSEE_DS_OTM_KEYS flag is used, inserting a value
 *  which exists will overwrite the existing value.
 *  
 *  @param      store   The data store
 *  @param      key     The key describing the value to store. Cannot use NULL as a key.
 *  @param      value   The value to store
 *
 *  @returns    JS_FALSE on OOM.
 */
JSBool gpsee_ds_put(gpsee_dataStore_t store, const void *key, void *value)
{
  size_t        i, newSize;
  void          *newData;

  GPSEE_ASSERT(store->monitor);

  gpsee_enterMonitor(store->monitor);

  if (!(store->flags & GPSEE_DS_OTM_KEYS))
  {
    for (i=0; i < store->size; i++)
    {
      if (store->data[i].key == key)
      {
        /* Slot already had same key: overwrite */
        store->data[i].value = value;
        gpsee_leaveMonitor(store->monitor);
        return JS_TRUE;
      }
      else if (store->data[i].key == NULL)
      {
        /* Found empty slot */
        store->data[i].key   = key;
        store->data[i].value = value;
        gpsee_leaveMonitor(store->monitor);
        return JS_TRUE;
      }
    }
  }

  /* No overwrite, no empty slot */
  if (store->size < 100)
  {
    if (store->size)
      newSize = store->size * 2;
    else
      newSize = 1;
  }
  else
  {
    newSize = store->size + 128;
    newSize -= newSize % 128;
  }

  newData = realloc(store->data, newSize * sizeof(store->data[0]));
  if (!newData)
  {
    gpsee_leaveMonitor(store->monitor);
    return JS_FALSE;
  }

  if (newData != store->data)
    store->data = newData;

  memset(&store->data[store->size], 0, (newSize - store->size) * sizeof(store->data[0]));

  store->data[store->size].key          = key;
  store->data[store->size].value        = value;
  store->size = newSize;
  gpsee_leaveMonitor(store->monitor);
  return JS_TRUE;
}

/**
 *  Remove a key/value from a GPSEE Data Store.
 *
 *  @param      store   The data store
 *  @param      key     The key describing the value to remove
 *
 *  @return     returns The value removed, or NULL if not found
 *
 *  @note       It is not possible to differentiate between not-found and NULL-valued success.
 */
void *gpsee_ds_remove(gpsee_dataStore_t store, const void *key)
{
  size_t        i;
  void          *value = NULL;

  gpsee_enterMonitor(store->monitor);

  for (i=0; i < store->size; i++)
  {
    if (store->data[i].key == key)
    {
      value = store->data[i].value;
#ifdef GPSEE_DEBUG_BUILD
      memset(&store->data[i], 0xdd, sizeof(store->data[0]));
#endif
      store->data[i].key = NULL;
      goto out;
    }
  }

  out:
  gpsee_leaveMonitor(store->monitor);
  return value;
}

/**
 *  Remove a key/value from a GPSEE Data Store.
 *
 *  @param      store   The data store
 *  @param      key     The key describing the value to remove
 *  @param      value   The value to remove
 *
 *  @return     returns JS_TRUE if the value was found and the key/value pair removed; otherwise returns JS_FALSE.
 */
JSBool gpsee_ds_match_remove(gpsee_dataStore_t store, const void *key, const void *value)
{
  size_t        i;
  JSBool        found = JS_FALSE;

  gpsee_enterMonitor(store->monitor);

  for (i=0; i < store->size; i++)
  {
    if ((store->data[i].key == key) && (store->data[i].value == value))
    {
#ifdef GPSEE_DEBUG_BUILD
      memset(&store->data[i], 0xdd, sizeof(store->data[0]));
#endif
      store->data[i].key = NULL;
      found = JS_TRUE;
      goto out;
    }
  }

  out:
  gpsee_leaveMonitor(store->monitor);
  return found;
}

/**
 *  Remove all key/value pairs from a GPSEE Data Store.
 *
 *  @param      store   The data store to empty.
 */
void gpsee_ds_empty(gpsee_dataStore_t store)
{
  gpsee_enterMonitor(store->monitor);
  memset(store->data, 0, sizeof(store->data[0]) * store->size);
  gpsee_leaveMonitor(store->monitor);
}

/** 
 *  @see        gpsee_ds_create()
 */
static gpsee_dataStore_t ds_create(size_t initialSizeHint)
{
  gpsee_dataStore_t     store;

#ifdef GPSEE_DEBUG_BUILD
  initialSizeHint = min(initialSizeHint, 1);      /* Draw out realloc errors */
#endif

  store = calloc(sizeof(*store), 1);
  if (!store)
    return NULL;

  if (initialSizeHint)
  {
    store->data = calloc(sizeof(store->data[0]), initialSizeHint);
    if (!store->data)
    {
      free(store);
      return NULL;
    }
  }

  store->size = initialSizeHint;

  return store;
}

/**
 *  Create a new GPSEE Data Store.
 *
 *  @param      grt                     GPSEE runtime to which data store belongs. It is 
 *                                      permissible to specify a NULL grt if the GPESE_DS_UNLOCKED
 *                                      flag is specified.
 *  @param      initialSizeHint         How many slots to allocate immediately. Engine 
 *                                      is free to ignore. Used to give hint as to how
 *                                      many values will be immediately inserted.
 *  @params     flags                   A bitmask describing data store capabilities
 *  @return     The new data store, or NULL on failure.
 *
 *  @see GPSEE_DS_UNLOCKED, GPSEE_DS_OTM_KEYS
 */
gpsee_dataStore_t gpsee_ds_create(gpsee_runtime_t *grt, uint32 flags, size_t initialSizeHint)
{
  gpsee_dataStore_t     store = ds_create(initialSizeHint);

  if (!store)
    return NULL;

  if (flags & GPSEE_DS_UNLOCKED)
    store->monitor = gpsee_getNilMonitor();
  else
    store->monitor = gpsee_createMonitor(grt);
  store->grt = grt;     /* Cached for delete */
  store->flags = flags;

  return store;
}

/**
 *  Destroy a GPSEE Data Store. All references to the data store (besides the final
 *  one used for this routine) must be cleaned up or otherwise invalidated before this
 *  routine is called.
 *
 *  @param      store                   The data store to destroy
 */
void gpsee_ds_destroy(gpsee_dataStore_t store)
{
  gpsee_monitor_t       monitor = store->monitor;
  gpsee_runtime_t   *grt    = store->grt;

  gpsee_enterMonitor(monitor);
#ifdef GPSEE_DEBUG_BUILD
  memset(store->data, 0xdb, sizeof(store->data) * store->size);
#endif
  free(store->data);
#ifdef GPSEE_DEBUG_BUILD
  memset(store, 0xdc, sizeof(*store));
#endif
  free(store);
  gpsee_leaveMonitor(monitor);
  gpsee_destroyMonitor(grt, monitor);

  return;
}

/**
 *  Iterate over all data in a GPSEE Data Store, executing the callback function cb.
 *  If any invocation of cb returns JS_FALSE, we immediately return JS_FALSE. If no 
 *  invocation of cb returns JS_FALSE, we return JS_TRUE.
 *
 *  @param      cx      JavaScript Context passed to callback
 *  @param      store   The GPSEE data store
 *  @param      cb      The function to call for each (key, value) pair
 *  @param      private A private pointer to pass to the callback function cb
 *
 *  @returns    JS_TRUE if no invocation of cb of returned JS_FALSE.
 *
 *  @note       Each invocation of cb will have the arguments (cx, key, value, private),
 *              where cx is the passed context, private is the passed private pointer, 
 *              and (key, value) are an entry in the data store.
 *
 *  @note       This routine enters the data store's monitor and does not leave it until
 *              we return.  This implies that that cb should not be a long-running or
 *              blocking function.
 */
JSBool gpsee_ds_forEach(JSContext *cx, gpsee_dataStore_t store, gpsee_ds_forEach_fn cb, void *private)
{
  size_t        i;

  gpsee_enterMonitor(store->monitor);
  for (i=0; i < store->size; i++)
  {
    if (!store->data[i].key)
      continue;

    if (cb(cx, store->data[i].key, store->data[i].value, private) == JS_FALSE)
    {
      gpsee_leaveMonitor(store->monitor);
      return JS_FALSE;
    }
  }

  gpsee_leaveMonitor(store->monitor);
  return JS_TRUE;
}

/** Determine whether or not a data store is actually storing any data or not.
 *
 *  @param      cx      JavaScript Context passed to callback
 *  @param      store   The GPSEE data store
 * 
 *  @returns	JS_TRUE when the data store is not empty, JS_FALSE when it is.
 */
JSBool gpsee_ds_hasData(JSContext *cx, gpsee_dataStore_t store)
{
  size_t        i;
  JSBool	b = JS_FALSE;

  gpsee_enterMonitor(store->monitor);
  for (i=0; i < store->size; i++)
  {
    if (!store->data[i].key)
      continue;
    b = JS_TRUE;
    break;
  }

  return b;
}

/** @} */
