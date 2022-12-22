/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2014 Blender Foundation. All rights reserved. */
#pragma once

/** \file
 * \ingroup bke
 *
 * API to perform operations over all ID pointers used by a given data-block.
 *
 * \note `BKE_lib_` files are for operations over data-blocks themselves, although they might
 * alter Main as well (when creating/renaming/deleting an ID e.g.).
 *
 * \section Function Names
 *
 * \warning Descriptions below is ideal goal, current status of naming does not yet fully follow it
 * (this is WIP).
 *
 * - `BKE_lib_query_` should be used for functions in that file.
 */

#include "BLI_sys_types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct ID;
struct IDProperty;
struct Main;

/* Tips for the callback for cases it's gonna to modify the pointer. */
enum {
  IDWALK_CB_NOP = 0,
  IDWALK_CB_NEVER_NULL = (1 << 0),
  IDWALK_CB_NEVER_SELF = (1 << 1),

  /**
   * Indicates whether this is direct (i.e. by local data) or indirect (i.e. by linked data) usage.
   * \note Object proxies are half-local, half-linked...
   */
  IDWALK_CB_INDIRECT_USAGE = (1 << 2),

  /**
   * That ID is used as mere sub-data by its owner (only case currently: those root nodetrees in
   * materials etc., and the Scene's master collections).
   * This means callback shall not *do* anything, only use this as informative data if it needs it.
   */
  IDWALK_CB_EMBEDDED = (1 << 3),

  /**
   * That ID is not really used by its owner, it's just an internal hint/helper.
   * This marks the 'from' pointers issue, like Key->from.
   * How to handle that kind of cases totally depends on what caller code is doing... */
  IDWALK_CB_LOOPBACK = (1 << 4),

  /** That ID is used as library override's reference by its owner. */
  IDWALK_CB_OVERRIDE_LIBRARY_REFERENCE = (1 << 5),

  /** That ID pointer is not overridable. */
  IDWALK_CB_OVERRIDE_LIBRARY_NOT_OVERRIDABLE = (1 << 6),

  /**
   * Indicates that this is an internal runtime ID pointer, like e.g. `ID.newid` or `ID.original`.
   * \note Those should be ignored in most cases, and won't be processed/generated anyway unless
   * `IDWALK_DO_INTERNAL_RUNTIME_POINTERS` option is enabled.
   */
  IDWALK_CB_INTERNAL = (1 << 7),

  /**
   * This ID usage is fully refcounted.
   * Callback is responsible to deal accordingly with #ID.us if needed.
   */
  IDWALK_CB_USER = (1 << 8),
  /**
   * This ID usage is not refcounted, but at least one user should be generated by it (to avoid
   * e.g. losing the used ID on save/reload).
   * Callback is responsible to deal accordingly with #ID.us if needed.
   */
  IDWALK_CB_USER_ONE = (1 << 9),
};

enum {
  IDWALK_RET_NOP = 0,
  /** Completely stop iteration. */
  IDWALK_RET_STOP_ITER = 1 << 0,
  /** Stop recursion, that is, do not loop over ID used by current one. */
  IDWALK_RET_STOP_RECURSION = 1 << 1,
};

typedef struct LibraryIDLinkCallbackData {
  void *user_data;
  /** Main database used to call `BKE_library_foreach_ID_link()`. */
  struct Main *bmain;
  /**
   * 'Real' ID, the one that might be in bmain, only differs from self_id when the later is an
   * embedded one.
   */
  struct ID *id_owner;
  /**
   * ID from which the current ID pointer is being processed. It may be an embedded ID like master
   * collection or root node tree.
   */
  struct ID *id_self;
  struct ID **id_pointer;
  int cb_flag;
} LibraryIDLinkCallbackData;

/**
 * Call a callback for each ID link which the given ID uses.
 *
 * \return a set of flags to control further iteration (0 to keep going).
 */
typedef int (*LibraryIDLinkCallback)(LibraryIDLinkCallbackData *cb_data);

/* Flags for the foreach function itself. */
enum {
  IDWALK_NOP = 0,
  /** The callback will never modify the ID pointers it processes. */
  IDWALK_READONLY = (1 << 0),
  /** Recurse into 'descendant' IDs.
   * Each ID is only processed once. Order of ID processing is not guaranteed.
   *
   * Also implies IDWALK_READONLY, and excludes IDWALK_DO_INTERNAL_RUNTIME_POINTERS.
   *
   * NOTE: When enabled, embedded IDs are processed separately from their owner, as if they were
   * regular IDs. Owner ID is not available then in the #LibraryForeachIDData callback data.
   */
  IDWALK_RECURSE = (1 << 1),
  /** Include UI pointers (from WM and screens editors). */
  IDWALK_INCLUDE_UI = (1 << 2),
  /** Do not process ID pointers inside embedded IDs. Needed by depsgraph processing e.g. */
  IDWALK_IGNORE_EMBEDDED_ID = (1 << 3),

  /** Also process internal ID pointers like `ID.newid` or `ID.orig_id`.
   *  WARNING: Dangerous, use with caution. */
  IDWALK_DO_INTERNAL_RUNTIME_POINTERS = (1 << 9),
};

typedef struct LibraryForeachIDData LibraryForeachIDData;

/**
 * Check whether current iteration over ID usages should be stopped or not.
 * \return true if the iteration should be stopped, false otherwise.
 */
bool BKE_lib_query_foreachid_iter_stop(struct LibraryForeachIDData *data);
void BKE_lib_query_foreachid_process(struct LibraryForeachIDData *data,
                                     struct ID **id_pp,
                                     int cb_flag);
int BKE_lib_query_foreachid_process_flags_get(struct LibraryForeachIDData *data);
int BKE_lib_query_foreachid_process_callback_flag_override(struct LibraryForeachIDData *data,
                                                           int cb_flag,
                                                           bool do_replace);

#define BKE_LIB_FOREACHID_PROCESS_ID(_data, _id, _cb_flag) \
  { \
    CHECK_TYPE_ANY((_id), ID *, void *); \
    BKE_lib_query_foreachid_process((_data), (ID **)&(_id), (_cb_flag)); \
    if (BKE_lib_query_foreachid_iter_stop((_data))) { \
      return; \
    } \
  } \
  ((void)0)

#define BKE_LIB_FOREACHID_PROCESS_IDSUPER(_data, _id_super, _cb_flag) \
  { \
    CHECK_TYPE(&((_id_super)->id), ID *); \
    BKE_lib_query_foreachid_process((_data), (ID **)&(_id_super), (_cb_flag)); \
    if (BKE_lib_query_foreachid_iter_stop((_data))) { \
      return; \
    } \
  } \
  ((void)0)

#define BKE_LIB_FOREACHID_PROCESS_FUNCTION_CALL(_data, _func_call) \
  { \
    _func_call; \
    if (BKE_lib_query_foreachid_iter_stop((_data))) { \
      return; \
    } \
  } \
  ((void)0)

/**
 * Process embedded ID pointers (root node-trees, master collections, ...).
 *
 * Those require specific care, since they are technically sub-data of their owner, yet in some
 * cases they still behave as regular IDs.
 */
void BKE_library_foreach_ID_embedded(struct LibraryForeachIDData *data, struct ID **id_pp);
void BKE_lib_query_idpropertiesForeachIDLink_callback(struct IDProperty *id_prop, void *user_data);

/**
 * Loop over all of the ID's this data-block links to.
 */
void BKE_library_foreach_ID_link(
    struct Main *bmain, struct ID *id, LibraryIDLinkCallback callback, void *user_data, int flag);
/**
 * Re-usable function, use when replacing ID's.
 */
void BKE_library_update_ID_link_user(struct ID *id_dst, struct ID *id_src, int cb_flag);

/**
 * Return the number of times given \a id_user uses/references \a id_used.
 *
 * \note This only checks for pointer references of an ID, shallow usages
 * (like e.g. by RNA paths, as done for FCurves) are not detected at all.
 *
 * \param id_user: the ID which is supposed to use (reference) \a id_used.
 * \param id_used: the ID which is supposed to be used (referenced) by \a id_user.
 * \return the number of direct usages/references of \a id_used by \a id_user.
 */
int BKE_library_ID_use_ID(struct ID *id_user, struct ID *id_used);

/**
 * Say whether given \a id_owner may use (in any way) a data-block of \a id_type_used.
 *
 * This is a 'simplified' abstract version of #BKE_library_foreach_ID_link() above,
 * quite useful to reduce useless iterations in some cases.
 */
bool BKE_library_id_can_use_idtype(struct ID *id_owner, short id_type_used);

/**
 * Given the id_owner return the type of id_types it can use as a filter_id.
 */
uint64_t BKE_library_id_can_use_filter_id(const struct ID *id_owner);

/**
 * Check whether given ID is used locally (i.e. by another non-linked ID).
 */
bool BKE_library_ID_is_locally_used(struct Main *bmain, void *idv);
/**
 * Check whether given ID is used indirectly (i.e. by another linked ID).
 */
bool BKE_library_ID_is_indirectly_used(struct Main *bmain, void *idv);
/**
 * Combine #BKE_library_ID_is_locally_used() and #BKE_library_ID_is_indirectly_used()
 * in a single call.
 */
void BKE_library_ID_test_usages(struct Main *bmain,
                                void *idv,
                                bool *is_used_local,
                                bool *is_used_linked);

/**
 * Tag all unused IDs (a.k.a 'orphaned').
 *
 * By default only tag IDs with `0` user count.
 * If `do_tag_recursive` is set, it will check dependencies to detect all IDs that are not actually
 * used in current file, including 'archipelagos` (i.e. set of IDs referencing each other in
 * loops, but without any 'external' valid usages.
 *
 * Valid usages here are defined as ref-counting usages, which are not towards embedded or
 * loop-back data.
 *
 * \param r_num_tagged: If non-NULL, must be a zero-initialized array of #INDEX_ID_MAX integers.
 * Number of tagged-as-unused IDs is then set for each type, and as total in
 * #INDEX_ID_NULL item.
 */
void BKE_lib_query_unused_ids_tag(struct Main *bmain,
                                  int tag,
                                  bool do_local_ids,
                                  bool do_linked_ids,
                                  bool do_tag_recursive,
                                  int *r_num_tagged);

/**
 * Detect orphaned linked data blocks (i.e. linked data not used (directly or indirectly)
 * in any way by any local data), including complex cases like 'linked archipelagoes', i.e.
 * linked data-blocks that use each other in loops,
 * which prevents their deletion by 'basic' usage checks.
 *
 * \param do_init_tag: if \a true, all linked data are checked, if \a false,
 * only linked data-blocks already tagged with #LIB_TAG_DOIT are checked.
 */
void BKE_library_unused_linked_data_set_tag(struct Main *bmain, bool do_init_tag);
/**
 * Untag linked data blocks used by other untagged linked data-blocks.
 * Used to detect data-blocks that we can forcefully make local
 * (instead of copying them to later get rid of original):
 * All data-blocks we want to make local are tagged by caller,
 * after this function has ran caller knows data-blocks still tagged can directly be made local,
 * since they are only used by other data-blocks that will also be made fully local.
 */
void BKE_library_indirectly_used_data_tag_clear(struct Main *bmain);

#ifdef __cplusplus
}
#endif
