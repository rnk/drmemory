/* **********************************************************
 * Copyright (c) 2011-2013 Google, Inc.  All rights reserved.
 * **********************************************************/

/* Dr. Memory: the memory debugger
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; 
 * version 2.1 of the License, and no later version.

 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Library General Public License for more details.

 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/***************************************************************************
 * symcache.c: cache symbol name lookups
 */

#ifndef USE_DRSYMS
# error requires USE_DRSYMS
#endif

#include "dr_api.h"
#include "symcache.h"
#include "drmgr.h"
#include "drsyms.h"
#include "utils.h"
#include <string.h>

/* General comments:
 * - The file is not assumed to be complete and instead contains negative
 *   entries.  This means it doesn't need to store runtime options used
 *   or anything if there are differences in which symbols we care about
 *   from run to run.  It also means we can think of it as a cache
 *   for repeated queries during a run as well as between runs.
 * - Using just module name for the file and not worrying about
 *   conflicts since this is just a performance improvement: thus some
 *   SxS or other modules may end up w/ competing cache files.
 * - i#617, we assume that we have all the entries of a symbol 
 *   if we can find one entry for that symbol in the symcache.
 * - Anyone creating synthetic symcaches (e.g., for i#192) needs to be aware
 *   of wildcard symcache entries.  i#722 added
 *   "std::_DebugHeapDelete<*>" whose matches are stored as
 *   "std::_DebugHeapDelete<>" duplicates.
 */

#define SYMCACHE_FILE_HEADER "Dr. Memory symbol cache version"

/* We need to bump the version number whenever we change the file format.
 * We do not need to bump it when we change the symbols we look up,
 * because we include negative entries in the file and make no assumptions
 * that it is a complete record of all lookups we'll need.
 */
#define SYMCACHE_VERSION 10

/* we need a separate hashtable per module */
#define SYMCACHE_MASTER_TABLE_HASH_BITS 6
#define SYMCACHE_MODULE_TABLE_HASH_BITS 6
#define SYMCACHE_OLIST_TABLE_HASH_BITS 5

/* Size of the buffer used to write the symbol cache.  This is stack allocated,
 * so it should not be increased.
 */
#define SYMCACHE_BUFFER_SIZE 4096

/* The number of digits used for the decimal representation of the file size of
 * the symcache file.
 */
#define SYMCACHE_SIZE_DIGITS 10

#define SYMCACHE_MAX_TMP_TRIES 1000

/* We key on full path to reduce chance of duplicate name (i#729).
 * If we do have duplicate preferred name, though, note that only one can
 * have a symcache file b/c our file namespace does not have versions
 * in it.
 */
static hashtable_t symcache_table;

/* simple synch scheme: hold this lock across all operations on outer or inner tables */
static void *symcache_lock;

static bool initialized;

/* Entry in the outer table */
typedef struct _mod_cache_t {
    /* strdup-ed modname since key now holds path */
    const char *modname;
    bool from_file; /* came from a cache file */
    bool appended; /* added to since read from file? */
    /* Table of offset_list_t entries */
    hashtable_t table;
    /* Values for consistency that we cache until ready to write to file */
    uint64 module_file_size;
#ifdef WINDOWS
    version_number_t file_version;
    version_number_t product_version;
    uint checksum;
    uint timestamp;
    size_t module_internal_size;
#else
    /* XXX: may want more consistency checks */
#endif
    bool has_debug_info; /* do we have DWARF/PECOFF/PDB symbols? */
} mod_cache_t;

typedef struct _offset_entry_t {
    size_t offs;
    struct _offset_entry_t *next;
} offset_entry_t;

/* For very few entries (which are the most common) we don't need a hashtable */
#define OFFSET_LIST_MIN_TABLE 3

/* Entry in the per-module table */
typedef struct _offset_list_t {
    uint num;
    /* We use both a linked list (for index-based iteration/lookup)
     * and a hashtable (to easily see whether an offset exists)
     */
    offset_entry_t *list;
    /* We want to append on add */
    offset_entry_t *list_last;
    /* The table is only allocated once we have OFFSET_LIST_MIN_TABLE entries.
     * Entries are offset+1 (b/c we have 0 offset).
     */
    hashtable_t *table;
    /* For improved iteration performance we cache the last index + entry */
    uint iter_idx;
    offset_entry_t *iter_entry;
} offset_list_t;

static char symcache_dir[MAXIMUM_PATH];
static size_t op_modsize_cache_threshold;

static void
symcache_free_entry(void *v)
{
    /* caller holds symcache_lock, or exit time */
    mod_cache_t *modcache = (mod_cache_t *) v;
    if (modcache != NULL) {
        hashtable_delete(&modcache->table);
        if (modcache->modname != NULL) {
            global_free((void *)modcache->modname, strlen(modcache->modname) + 1,
                        HEAPSTAT_HASHTABLE);
        }
        global_free(modcache, sizeof(*modcache), HEAPSTAT_HASHTABLE);
    }
}

static void
symcache_get_filename(const char *modname, char *symfile, size_t symfile_count)
{
    dr_snprintf(symfile, symfile_count, "%s/%s.txt", symcache_dir, modname);
    symfile[symfile_count-1] = '\0';
}

/* If an entry already exists and is 0, replaces it; else adds a new
 * offset for that symbol.
 *
 * Caller must hold symcache_lock.
 */
static bool
symcache_symbol_add(const char *modname, hashtable_t *symtable,
                    const char *symbol, size_t offs)
{
    offset_list_t *olist;
    offset_entry_t *e;
    olist = (offset_list_t *) hashtable_lookup(symtable, (void *)symbol);
    if (olist != NULL) {
        if (olist->num == 1 && olist->list->offs == 0) {
            /* replace a single 0 entry */
            if (olist->table != NULL) {
                ASSERT(olist->num >= OFFSET_LIST_MIN_TABLE, "table should be NULL");
                hashtable_remove(olist->table, (void *)(olist->list->offs + 1));
                hashtable_add(olist->table, (void *)(offs + 1), (void *)(offs + 1));
            }
            olist->list->offs = offs;
            return true;
        }
        if (olist->table != NULL) {
            if (hashtable_lookup(olist->table, (void *)(offs + 1)) != NULL) {
                LOG(2, "%s: ignoring dup entry %s\n", __FUNCTION__, symbol);
                return false;
            }
        } else {
            for (e = olist->list; e != NULL; e = e->next) {
                if (e->offs == offs) {
                    LOG(2, "%s: ignoring dup entry %s\n", __FUNCTION__, symbol);
                    return false;
                }
            }
        }
    } else {
        olist = (offset_list_t *) global_alloc(sizeof(*olist), HEAPSTAT_HASHTABLE);
        olist->num = 0;
        olist->list = NULL;
        olist->list_last = NULL;
        olist->table = NULL;
    }
    LOG(2, "%s: %s \"%s\" @ "PIFX"\n", __FUNCTION__, modname, symbol, offs);
    /* we could verify by an addr lookup but we still need consistency info
     * in the file for the negative entries so we don't bother
     */
    e = (offset_entry_t *) global_alloc(sizeof(*e), HEAPSTAT_HASHTABLE);
    e->offs = offs;
    e->next = NULL;
    /* append to avoid affecting iteration */
    if (olist->list_last == NULL) {
        ASSERT(olist->list == NULL, "last not set");
        olist->list = e;
        olist->list_last = e;
    } else {
        olist->list_last->next = e;
        olist->list_last = e;
    }
    olist->num++;
    if (olist->num >= OFFSET_LIST_MIN_TABLE) {
        if (olist->table == NULL) {
            /* enough entries that a table is worthwhile */
            olist->table = (hashtable_t *)
                global_alloc(sizeof(*olist->table), HEAPSTAT_HASHTABLE);
            hashtable_init(olist->table, SYMCACHE_OLIST_TABLE_HASH_BITS,
                           HASH_INTPTR, false/*strdup*/);
            for (e = olist->list; e != NULL; e = e->next)
                hashtable_add(olist->table, (void *)(e->offs + 1), (void *)(e->offs + 1));
        } else
            hashtable_add(olist->table, (void *)(offs + 1), (void *)(offs + 1));
    }
    hashtable_add(symtable, (void *)symbol, (void *)olist);
    /* clear any cached values */
    olist->iter_idx = 0;
    olist->iter_entry = NULL;
    return true;
}

/* caller must hold symcache_lock */
static void
symcache_write_symfile(const char *modname, mod_cache_t *modcache)
{
    uint i;
    file_t f;
    hashtable_t *symtable = &modcache->table;
    char buf[SYMCACHE_BUFFER_SIZE];
    size_t sofar = 0;
    ssize_t len;
    size_t bsz = BUFFER_SIZE_ELEMENTS(buf);
    size_t filesz_loc;
    char symfile[MAXIMUM_PATH];
    char symfile_tmp[MAXIMUM_PATH];
    int64 file_size;

    /* if from file, we assume it's a waste of time to re-write file:
     * the version matched after all, unless we appended to it.
     */
    if (modcache->from_file && !modcache->appended)
        return;
    if (symtable->entries == 0)
        return; /* nothing to write */

    /* Open the temp symcache that we will rename.  */
    symcache_get_filename(modname, symfile, BUFFER_SIZE_ELEMENTS(symfile));
    f = INVALID_FILE;
    i = 0;
    while (f == INVALID_FILE && i < SYMCACHE_MAX_TMP_TRIES) {
        dr_snprintf(symfile_tmp, BUFFER_SIZE_ELEMENTS(symfile_tmp),
                    "%s.%04d.tmp", symfile, i);
        NULL_TERMINATE_BUFFER(symfile_tmp);
        f = dr_open_file(symfile_tmp, DR_FILE_WRITE_REQUIRE_NEW);
        i++;
    }
    if (f == INVALID_FILE) {
        NOTIFY("WARNING: Unable to create symcache temp file %s"NL,
               symfile_tmp);
        return;
    }

    BUFFERED_WRITE(f, buf, bsz, sofar, len, "%s %d\n",
                   SYMCACHE_FILE_HEADER, SYMCACHE_VERSION);
    /* Leave room for file size for self-consistency check */
    filesz_loc = sofar;  /* XXX: Assumes that the buffer hasn't been flushed. */
    BUFFERED_WRITE(f, buf, bsz, sofar, len,
                   "%"STRINGIFY(SYMCACHE_SIZE_DIGITS)"u,", 0);
#ifdef WINDOWS
    BUFFERED_WRITE(f, buf, bsz, sofar, len,
                   UINT64_FORMAT_STRING","UINT64_FORMAT_STRING","
                   UINT64_FORMAT_STRING",%u,%u,%lu\n",
                   modcache->module_file_size, modcache->file_version.version,
                   modcache->product_version.version,
                   modcache->checksum, modcache->timestamp,
                   modcache->module_internal_size);
#else
    BUFFERED_WRITE(f, buf, bsz, sofar, len, UINT64_FORMAT_STRING"\n",
                   modcache->module_file_size);
#endif
    BUFFERED_WRITE(f, buf, bsz, sofar, len, "%u\n", modcache->has_debug_info);
    for (i = 0; i < HASHTABLE_SIZE(symtable->table_bits); i++) {
        hash_entry_t *he;
        for (he = symtable->table[i]; he != NULL; he = he->next) {
            offset_list_t *olist = (offset_list_t *) he->payload;
            offset_entry_t *e;
            if (olist == NULL)
                continue;
            /* skip symbol in dup entries to save space */
            BUFFERED_WRITE(f, buf, bsz, sofar, len, "%s", he->key);
            e = olist->list;
            while (e != NULL) {
                BUFFERED_WRITE(f, buf, bsz, sofar, len, ",0x%x\n", e->offs);
                e = e->next;
            }
        }
    }

    /* now update size */
    FLUSH_BUFFER(f, buf, sofar);
    if ((file_size = dr_file_tell(f)) < 0 ||
        dr_snprintf(buf, BUFFER_SIZE_ELEMENTS(buf),
                    "%"STRINGIFY(SYMCACHE_SIZE_DIGITS)"u", file_size) < 0 ||
        !dr_file_seek(f, filesz_loc, DR_SEEK_SET) ||
        dr_write_file(f, buf, SYMCACHE_SIZE_DIGITS) != SYMCACHE_SIZE_DIGITS) {
        /* If any steps fail, warn and give up. */
        NOTIFY("WARNING: Unable to write symcache file size."NL);
        dr_close_file(f);
        dr_delete_file(symfile_tmp);
        return;
    } else {
        ASSERT(strlen(buf) <= SYMCACHE_SIZE_DIGITS, "not enough space for file size");
    }

    dr_close_file(f);

    if (!dr_rename_file(symfile_tmp, symfile, /*replace*/true)) {
        NOTIFY_ERROR("WARNING: Failed to rename the symcache file."NL);
        dr_delete_file(symfile_tmp);
    }
}

#define MAX_SYMLEN 256

/* Sets modcache->has_debug_info */
static bool
symcache_read_symfile(const module_data_t *mod, const char *modname, mod_cache_t *modcache)
{
    hashtable_t *symtable = &modcache->table;
    bool res = false;
    const char *line, *next_line;
    char symbol[MAX_SYMLEN];
    size_t offs;
    uint64 map_size;
    size_t actual_size;
    bool ok;
    void *map = NULL;
    char symfile[MAXIMUM_PATH];
    file_t f;

    symcache_get_filename(modname, symfile, BUFFER_SIZE_ELEMENTS(symfile));
    f = dr_open_file(symfile, DR_FILE_READ);
    if (f == INVALID_FILE)
        goto symcache_read_symfile_done;
    LOG(2, "processing symbol cache file for %s\n", modname);
    /* we avoid having to do our own buffering by just mapping the whole file */
    ok = dr_file_size(f, &map_size);
    if (ok) {
        actual_size = (size_t) map_size;
        ASSERT(actual_size == map_size, "file size too large");
        map = dr_map_file(f, &actual_size, 0, NULL, DR_MEMPROT_READ, 0);
    }
    if (!ok || map == NULL || actual_size < map_size) {
        NOTIFY_ERROR("Error mapping symcache file for %s"NL, modname);
        goto symcache_read_symfile_done;
    }
    if (strncmp((char *)map, SYMCACHE_FILE_HEADER, strlen(SYMCACHE_FILE_HEADER)) != 0) {
        WARN("WARNING: symbol cache file is corrupted\n");
        goto symcache_read_symfile_done;
    }
    /* i#1057: We use dr_sscanf() because sscanf() from ntdll will call strlen()
     * and read off the end of the mapped file if it doesn't hit a null.
     */
    if (dr_sscanf((char *)map + strlen(SYMCACHE_FILE_HEADER) + 1, "%d",
                  (uint *)&offs) != 1 ||
        /* neither forward nor backward compatible */
        offs != SYMCACHE_VERSION) {
        WARN("WARNING: symbol cache file has wrong version\n");
        goto symcache_read_symfile_done;
    }
    line = strchr((char *) map, '\n');
    if (line != NULL)
        line++;

    if (line != NULL) {
        /* Module consistency checks */
        uint cache_file_size;
        uint64 module_file_size;
#ifdef WINDOWS
        version_number_t file_version;
        version_number_t product_version;
        uint checksum;
        uint timestamp;
        size_t module_internal_size;
        if (dr_sscanf(line, "%u,"UINT64_FORMAT_STRING","UINT64_FORMAT_STRING","
                      UINT64_FORMAT_STRING",%u,%u,%lu",
                      &cache_file_size, &module_file_size, &file_version.version,
                      &product_version.version, &checksum, &timestamp,
                      &module_internal_size) != 7) {
            WARN("WARNING: %s symbol cache file has bad consistency header\n", modname);
            goto symcache_read_symfile_done;
        }
        if (module_file_size != modcache->module_file_size ||
            file_version.version != modcache->file_version.version ||
            product_version.version != modcache->product_version.version ||
            checksum != modcache->checksum ||
            timestamp != modcache->timestamp ||
            module_internal_size != modcache->module_internal_size) {
            LOG(1, "module version mismatch: %s symbol cache file is stale\n", modname);
            LOG(2, "\t"UINT64_FORMAT_STRING" vs "UINT64_FORMAT_STRING", "
                UINT64_FORMAT_STRING" vs "UINT64_FORMAT_STRING", "
                UINT64_FORMAT_STRING" vs "UINT64_FORMAT_STRING", "
                "%u vs %u, %u vs %u, %lu vs %lu\n",
                module_file_size, modcache->module_file_size,
                file_version.version, modcache->file_version.version,
                product_version.version, modcache->product_version.version,
                checksum, modcache->checksum,
                timestamp, modcache->timestamp,
                module_internal_size, modcache->module_internal_size);
            goto symcache_read_symfile_done;
        }
#else
        if (dr_sscanf(line, "%u,"UINT64_FORMAT_STRING,
                      &cache_file_size, &module_file_size) != 2) {
            WARN("WARNING: %s symbol cache file has bad consistency header\n", modname);
            goto symcache_read_symfile_done;
        }
        if (module_file_size != modcache->module_file_size) {
            LOG(1, "module version mismatch: %s symbol cache file is stale\n", modname);
            goto symcache_read_symfile_done;
        }
#endif
        /* We could go further w/ CRC or even MD5 but not worth it for dev tool */
        if (cache_file_size != (uint)map_size) {
            WARN("WARNING: %s symbol cache file is corrupted\n", modname);
            goto symcache_read_symfile_done;
        }
    }
    line = strchr(line, '\n');
    if (line != NULL)
        line++;
    if (line != NULL) {
        uint has_debug_info;
        if (dr_sscanf(line, "%u", &has_debug_info) != 1) {
            WARN("WARNING: %s symbol cache file has bad consistency header\n", modname);
            goto symcache_read_symfile_done;
        }
        if (has_debug_info) {
            /* We assume that the current availability of debug info doesn't matter */
            modcache->has_debug_info = true;
        } else {
            /* We delay the costly check for symbols until we've read the symcache
             * b/c if its entry indicates symbols we don't need to look
             */
            if (module_has_debug_info(mod)) {
                LOG(1, "module now has debug info: %s symbol cache is stale\n", modname);
                goto symcache_read_symfile_done;
            }
        }
    }
    line = strchr(line, '\n');
    if (line != NULL)
        line++;

    symbol[0] = '\0';
    for (; line != NULL && line < ((char *)map) + map_size; line = next_line) {
        const char *comma = strchr(line, ',');
        const char *newline = strchr(line, '\n');
        size_t symlen = (comma != NULL ? comma - line : 0);
        if (newline == NULL) {
            next_line = ((char *)map) + map_size + 1; /* handle EOF w/o trailing \n */
        } else {
            next_line = newline + 1;
        }
        if (symlen > 0 && symlen < MAX_SYMLEN) {
            strncpy(symbol, line, symlen);
            symbol[symlen] = '\0';
        }
        if (comma != NULL && symlen < MAX_SYMLEN && symbol[0] != '\0' &&
            dr_sscanf(comma, ",0x%x", (uint *)&offs) == 1) {
            symcache_symbol_add(modname, symtable, symbol, offs);
        } else {
            WARN("WARNING: malformed symbol cache line \"%.*s\"\n",
                 next_line - line - 1, line);
            /* We abort in case there were two dueling writes to the file
             * and it somehow got past the self-consistency check,
             * putting a header in the middle of the file, and we can't
             * trust subsequent lines since they may belong to a different
             * version of the module
             */
            break; /* res should still be true */
        }
    }
    res = true;
 symcache_read_symfile_done:
    if (map != NULL)
        dr_unmap_file(map, actual_size);
    if (f != INVALID_FILE)
        dr_close_file(f);
    if (!res)
        modcache->has_debug_info = module_has_debug_info(mod);

    return res;
}

void
symcache_init(const char *symcache_dir_in,
              size_t modsize_cache_threshold)
{
    initialized = true;

    op_modsize_cache_threshold = modsize_cache_threshold;

    hashtable_init_ex(&symcache_table, SYMCACHE_MASTER_TABLE_HASH_BITS,
                      IF_WINDOWS_ELSE(HASH_STRING_NOCASE, HASH_STRING),
                      true/*strdup*/, false/*!synch*/,
                      symcache_free_entry, NULL, NULL);
    symcache_lock = dr_mutex_create();

    dr_snprintf(symcache_dir, BUFFER_SIZE_ELEMENTS(symcache_dir), 
                "%s", symcache_dir_in);
    NULL_TERMINATE_BUFFER(symcache_dir);
    if (!dr_directory_exists(symcache_dir)) {
        if (!dr_create_dir(symcache_dir)) {
            /* check again in case of a race (i#616) */
            if (!dr_directory_exists(symcache_dir)) {
                NOTIFY_ERROR("Unable to create symcache dir %s"NL, symcache_dir);
                ASSERT(false, "unable to create symcache dir");
                dr_abort();
            }
        }
    }
}

void
symcache_exit(void)
{
    uint i;
    ASSERT(initialized, "symcache was not initialized");
    dr_mutex_lock(symcache_lock);
    for (i = 0; i < HASHTABLE_SIZE(symcache_table.table_bits); i++) {
        hash_entry_t *he;
        for (he = symcache_table.table[i]; he != NULL; he = he->next) {
            mod_cache_t *modcache = (mod_cache_t *) he->payload;
            symcache_write_symfile(modcache->modname, modcache);
        }
    }
    hashtable_delete(&symcache_table);
    dr_mutex_unlock(symcache_lock);
    dr_mutex_destroy(symcache_lock);
}

static void
symcache_free_list(void *v)
{
    offset_list_t *olist = (offset_list_t *) v;
    offset_entry_t *tmp, *e = olist->list;
    if (olist->table != NULL) {
        hashtable_delete(olist->table);
        global_free(olist->table, sizeof(*olist->table), HEAPSTAT_HASHTABLE);
    }
    while (e != NULL) {
        tmp = e;
        e = e->next;
        global_free(tmp, sizeof(*tmp), HEAPSTAT_HASHTABLE);
    }
    global_free(olist, sizeof(*olist), HEAPSTAT_HASHTABLE);
}

void
symcache_module_load(void *drcontext, const module_data_t *mod, bool loaded)
{
    /* look for cache file for this module.
     * fill in hashtable: key is string, value is list of offsets
     */
    mod_cache_t *modcache;
    const char *modname = dr_module_preferred_name(mod);
    file_t f;
    if (modname == NULL)
        return; /* don't support caching */

    /* if smaller than threshold, not worth caching */
    if (mod->end - mod->start < op_modsize_cache_threshold) {
        LOG(1, "%s: module %s too small to cache\n", __FUNCTION__, modname);
        return;
    }

    ASSERT(initialized, "symcache was not initialized");

    /* support initializing prior to module events => called twice */
    dr_mutex_lock(symcache_lock);
    modcache = (mod_cache_t *) hashtable_lookup(&symcache_table,
                                                (void *)mod->full_path);
    dr_mutex_unlock(symcache_lock);
    if (modcache != NULL)
        return;

    modcache = (mod_cache_t *) global_alloc(sizeof(*modcache), HEAPSTAT_HASHTABLE);
    memset(modcache, 0, sizeof(*modcache));
    hashtable_init_ex(&modcache->table, SYMCACHE_MODULE_TABLE_HASH_BITS,
                      HASH_STRING, true/*strdup*/, true/*synch*/,
                      symcache_free_list, NULL, NULL);

    /* store consistency fields */
    f = dr_open_file(mod->full_path, DR_FILE_READ);
    if (f != INVALID_FILE) {
        bool ok = dr_file_size(f, &modcache->module_file_size);
        if (!ok)
            WARN("WARNING: unable to determine size of %s\n", mod->full_path);
        dr_close_file(f);
    } else
        WARN("WARNING: unable to open %s\n", mod->full_path);
#ifdef WINDOWS
    modcache->file_version = mod->file_version;
    modcache->product_version = mod->product_version;
    modcache->checksum = mod->checksum;
    modcache->timestamp = mod->timestamp;
    modcache->module_internal_size = mod->module_internal_size;
#endif

    modcache->modname = drmem_strdup(modname, HEAPSTAT_HASHTABLE);
    modcache->from_file = symcache_read_symfile(mod, modname, modcache);

    dr_mutex_lock(symcache_lock);
    if (!hashtable_add(&symcache_table, (void *)mod->full_path, (void *)modcache)) {
        /* this should be really rare to have dup paths (xref i#729) -- and
         * actually we now have a lookup up above so we should only get here
         * on a race while we let go of the lock
         */
        WARN("WARNING: duplicate module paths: only caching symbols from first\n");
        hashtable_delete(&modcache->table);
        global_free(modcache, sizeof(*modcache), HEAPSTAT_HASHTABLE);
    }
    dr_mutex_unlock(symcache_lock);
}

static bool
symcache_module_save_common(const module_data_t *mod, bool remove)
{
    mod_cache_t *modcache;
    const char *modname = dr_module_preferred_name(mod);
    if (modname == NULL)
        return false; /* don't support caching */
    ASSERT(initialized, "symcache was not initialized");
    dr_mutex_lock(symcache_lock);
    modcache = (mod_cache_t *) hashtable_lookup(&symcache_table, (void *)mod->full_path);
    if (modcache != NULL) {
        symcache_write_symfile(modname, modcache);
        if (remove)
            hashtable_remove(&symcache_table, (void *)mod->full_path);
    }
    dr_mutex_unlock(symcache_lock);
    return true;
}

bool
symcache_module_save_symcache(const module_data_t *mod)
{
    return symcache_module_save_common(mod, false/*keep*/);
}

void
symcache_module_unload(void *drcontext, const module_data_t *mod)
{
    symcache_module_save_common(mod, true/*remove*/);
}

static bool
symcache_module_has_data(const module_data_t *mod, bool require_syms)
{
    mod_cache_t *modcache;
    bool res = false;
    const char *modname = dr_module_preferred_name(mod);
    if (modname == NULL)
        return false; /* don't support caching */
    ASSERT(initialized, "symcache was not initialized");
    dr_mutex_lock(symcache_lock);
    modcache = (mod_cache_t *) hashtable_lookup(&symcache_table, (void *)mod->full_path);
    if (modcache != NULL)
        res = (modcache->table.entries > 0 && (!require_syms || modcache->has_debug_info));
    dr_mutex_unlock(symcache_lock);
    return res;
}

bool
symcache_module_is_cached(const module_data_t *mod)
{
    return symcache_module_has_data(mod, false/*don't need syms*/);
}

bool
symcache_module_has_debug_info(const module_data_t *mod)
{
    return symcache_module_has_data(mod, true/*need syms*/);
}

/* If an entry already exists and is 0, replaces it; else adds a new
 * offset for that symbol.
 */
bool
symcache_add(const module_data_t *mod, const char *symbol, size_t offs)
{
    mod_cache_t *modcache;
    const char *modname = dr_module_preferred_name(mod);
    if (modname == NULL)
        return false; /* don't support caching */
    ASSERT(initialized, "symcache was not initialized");
    dr_mutex_lock(symcache_lock);
    modcache = (mod_cache_t *) hashtable_lookup(&symcache_table, (void *)mod->full_path);
    if (modcache == NULL) {
        LOG(2, "%s: there is no cache for %s\n", __FUNCTION__, modname);
        dr_mutex_unlock(symcache_lock);
        return false;
    }
    if (symcache_symbol_add(modname, &modcache->table, symbol, offs) &&
        modcache->from_file)
        modcache->appended = true;
    dr_mutex_unlock(symcache_lock);
    return true;    
}

/* Returns true if the symbol is in the cache, which contains positive and
 * negative entries.  offs==0 indicates the symbol does not exist in the module.
 */
/* Some symbols have multiple offsets.  The majority have just one.
 * Rather than allocate an array and have the caller free it, or use
 * callbacks or an iterator across which we hold our lock, we have the
 * caller pass us an index as a stateless iterator.  The table is
 * unlikely to be changed in between so we avoid complexity there.
 */
bool
symcache_lookup(const module_data_t *mod, const char *symbol, uint idx,
                size_t *offs OUT, uint *num OUT)
{
    offset_list_t *olist;
    offset_entry_t *e;
    mod_cache_t *modcache;
    uint i;
    const char *modname = dr_module_preferred_name(mod);
    if (modname == NULL)
        return false; /* don't support caching */
    ASSERT(initialized, "symcache was not initialized");
    if (offs == NULL || num == NULL) {
        ASSERT(false, "invalid params");
        return false;
    }
    dr_mutex_lock(symcache_lock);
    modcache = (mod_cache_t *) hashtable_lookup(&symcache_table, (void *)mod->full_path);
    if (modcache == NULL) {
        dr_mutex_unlock(symcache_lock);
        return false;
    }
    olist = (offset_list_t *) hashtable_lookup(&modcache->table, (void *)symbol);
    if (olist == NULL) {
        dr_mutex_unlock(symcache_lock);
        return false;
    }
    *num = olist->num;
    if (olist->iter_entry != NULL && olist->iter_idx <= idx) {
        /* start at cached location */
        e = olist->iter_entry;
        i = olist->iter_idx;
    } else {
        /* start at the beginning */
        e = olist->list;
        i = 0;
    }
    for (; i < idx && e != NULL; i++, e = e->next)
        ; /* nothing */
    if (e != NULL)
        *offs = e->offs;
    else
        *offs = 0;
    olist->iter_idx = idx;
    olist->iter_entry = e;
    dr_mutex_unlock(symcache_lock);
    LOG(2, "sym lookup of %s in %s => symcache hit "PIFX"\n",
        symbol, mod->full_path, *offs);
    return true;
}
