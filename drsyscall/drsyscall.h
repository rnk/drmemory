/* **********************************************************
 * Copyright (c) 2012-2013 Google, Inc.  All rights reserved.
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

#ifndef _DR_SYSCALL_H_
#define _DR_SYSCALL_H_ 1

/* Dr. Syscall: DynamoRIO System Call Extension */

/* Framework-shared header */
#include "drmemory_framework.h"

/**
 * @file drsyscall.h
 * @brief Header for Dr. Syscall: System Call Monitoring Extension
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \addtogroup drsyscall Dr. Syscall: System Call Monitoring Extension
 */
/*@{*/ /* begin doxygen group */

/* Users of drsyscall need to use the drmgr versions of these events to ensure
 * that drsyscall's actions occur at the right time.
 */
#ifndef dr_get_tls_field
# define dr_get_tls_field DO_NOT_USE_tls_field_USE_drmgr_tls_field_instead
# define dr_set_tls_field DO_NOT_USE_tls_field_USE_drmgr_tls_field_instead
# define dr_insert_read_tls_field DO_NOT_USE_tls_field_USE_drmgr_tls_field_instead
# define dr_insert_write_tls_field DO_NOT_USE_tls_field_USE_drmgr_tls_field_instead
# define dr_register_thread_init_event DO_NOT_USE_thread_event_USE_drmgr_events_instead
# define dr_unregister_thread_init_event DO_NOT_USE_thread_event_USE_drmgr_events_instead
# define dr_register_thread_exit_event DO_NOT_USE_thread_event_USE_drmgr_events_instead
# define dr_unregister_thread_exit_event DO_NOT_USE_thread_event_USE_drmgr_events_instead
# define dr_register_pre_syscall_event DO_NOT_USE_pre_syscall_USE_drmgr_events_instead
# define dr_unregister_pre_syscall_event DO_NOT_USE_pre_syscall_USE_drmgr_events_instead
# define dr_register_post_syscall_event DO_NOT_USE_post_syscall_USE_drmgr_events_instead
# define dr_unregister_post_syscall_event DO_NOT_USE_post_syscall_USE_drmgr_events_instead
#endif /* dr_get_tls_field */
/***************************************************************************
 * ENUMS AND TYPES
 */

/** Priority of drsyscall events. */
enum {
    /**
     * Priority of the drsyscall pre-syscall and post-syscall events
     * that are meant to take place before the corresponding events of
     * a regular user of drsyscall.  Dynamic iteration is not allowed
     * until these events have taken place.  Users of drsyscall should
     * arrange their pre-syscall and post-syscall event callbacks to
     * be called after the drsyscall event, unless they want to modify
     * the pre-syscall arguments before they're stored or they want to
     * modify the application's context, in which case their event
     * should go beforehand.
     */
    DRMGR_PRIORITY_PRESYS_DRSYS       = -100,
    /** See the comment for DRMGR_PRIORITY_PRESYS_DRSYS. */
    DRMGR_PRIORITY_POSTSYS_DRSYS      = -100,
    /**
     * Priority of the drsyscall last-chance post-syscall event.  This
     * event must take place after any dynamic iteration of system
     * call arguments, which means it must be after the post-syscall
     * event in all users of drsyscall.
     */
    DRMGR_PRIORITY_POSTSYS_DRSYS_LAST =  10000,
    /**
     * Priority of the drsyscall module load event.  This
     * event must take place before any user of drsyscall in order
     * to populate the tables used by drsys_name_to_number().
     */
    DRMGR_PRIORITY_MODLOAD_DRSYS      = -100,
};

/**
 * Name of drsyscall pre-syscall and post-syscall events that occur
 * prior to iteration being allowed.
 */
#define DRMGR_PRIORITY_NAME_DRSYS "drsyscall"

/** Name of drsyscall post-syscall last-chance event. */
#define DRMGR_PRIORITY_NAME_DRSYS_LAST "drsyscall_last"

/** Opaque "system call handle" type.  See #drsys_syscall_t. */
struct _drsys_syscall_t;
/**
 * Opaque "system call handle" type used to refer to a particular system call.
 * The system call handle can be obtained from drsys_cur_syscall(),
 * drsys_iterate_syscalls(), drsys_name_to_syscall(),
 * drsys_number_to_syscall(), or syscall_arg_t.syscall.
 */
typedef struct _drsys_syscall_t drsys_syscall_t;

/** Representation of a system call number. */
typedef struct _drsys_sysnum_t {
    /**
     * Either the sole system call number by itself (in which case \p
     * secondary will be zero), or the primary component of a two-part
     * system call number \p number.secondary.
     */
    int number;
    int secondary;   /**< Secondary component of \p number.secondary, or zero. */
} drsys_sysnum_t;

/**
 * Indicates whether a parameter is an input or an output.  Used as a
 * bitmask, so multiple of these can be set.
 */
typedef enum {
    DRSYS_PARAM_IN     = 0x01,  /**< Input parameter. */
    DRSYS_PARAM_OUT    = 0x02,  /**< Output parameter. */
    /**
     * May be IN or OUT.  Used only in pre-syscall to indicate the
     * size of an entire data structure, when only some fields are
     * actually read or writen.  Those fields will be presented as
     * separate IN or OUT arguments which will of course overlap this
     * one.
     */
    DRSYS_PARAM_BOUNDS = 0x04,
    /**
     * Not used for memory iteration, only for type iteration, where
     * the type of the return value is indicated if it is other than a
     * status or error code.
     */
    DRSYS_PARAM_RETVAL = 0x08,
    /**
     * If this flag is not set, the parameter is passed as a pointer to
     * the specified type.  If this flag is set, the parameter's value
     * is passed in.
     */
    DRSYS_PARAM_INLINED= 0x10,
} drsys_param_mode_t;

/* Keep this in synch with param_type_names[] */
/**
 * Indicates the data type of a parameter. 
 * For the non-memarg iterators, a pointer type is implied whenever the
 * mode is DRSYS_PARAM_OUT.  Thus, a system call parameter of type DRSYS_TYPE_INT
 * and mode DRSYS_PARAM_OUT can be assumed to be a pointer to an int.
 */
typedef enum {
    DRSYS_TYPE_INVALID,     /**< This type field is not used for this iteration type. */
    DRSYS_TYPE_UNKNOWN,     /**< Unknown type. */

    /* Inlined */
    DRSYS_TYPE_VOID,   	    /**< Void type. */
    DRSYS_TYPE_BOOL,   	    /**< Boolean type. */
    DRSYS_TYPE_INT,    	    /**< Integer type of unspecified signedness. */
    DRSYS_TYPE_SIGNED_INT,  /**< Signed integer type. */
    DRSYS_TYPE_UNSIGNED_INT,/**< Unsigned integer type. */
    DRSYS_TYPE_HANDLE,      /**< Windows-only: kernel/GDI/user handle type. */
    DRSYS_TYPE_NTSTATUS,    /**< Windows-only: NTSTATUS Native API/RTL type. */
    DRSYS_TYPE_ATOM,        /**< Windows-only: ATOM type. */
    DRSYS_TYPE_POINTER,     /**< Pointer to an unspecified type. */

    /* Structs */
    DRSYS_TYPE_STRUCT,      /**< Unspecified structure type. */
    DRSYS_TYPE_CSTRING,     /**< Null-terminated string of characters (C string). */
    DRSYS_TYPE_CWSTRING,    /**< Null-terminated string of wide characters. */
    DRSYS_TYPE_CARRAY,      /**< Non-null-terminated string of characters. */
    DRSYS_TYPE_CWARRAY,     /**< Non-null-terminated string of wide characters. */
    DRSYS_TYPE_CSTRARRAY,   /**< Double-null-terminated array of C strings. */
    DRSYS_TYPE_UNICODE_STRING,      /**< UNICODE_STRING structure. */
    DRSYS_TYPE_LARGE_STRING,        /**< LARGE_STRING structure. */
    DRSYS_TYPE_OBJECT_ATTRIBUTES,   /**< OBJECT_ATTRIBUTES structure. */
    DRSYS_TYPE_SECURITY_DESCRIPTOR, /**< SECURITY_DESCRIPTOR structure. */
    DRSYS_TYPE_SECURITY_QOS,        /**< SECURITY_QUALITY_OF_SERVICE structure */
    DRSYS_TYPE_PORT_MESSAGE,        /**< PORT_MESSAGE structure. */
    DRSYS_TYPE_CONTEXT,             /**< CONTEXT structure. */
    DRSYS_TYPE_EXCEPTION_RECORD,    /**< EXCEPTION_RECORD structure. */
    DRSYS_TYPE_DEVMODEW,            /**< DEVMODEW structure. */
    DRSYS_TYPE_WNDCLASSEXW,         /**< WNDCLASSEXW structure. */
    DRSYS_TYPE_CLSMENUNAME,         /**< CLSMENUNAME structure. */
    DRSYS_TYPE_MENUITEMINFOW,       /**< MENUITEMINFOW structure. */
    DRSYS_TYPE_ALPC_PORT_ATTRIBUTES,/**< ALPC_PORT_ATTRIBUTES structure. */
    DRSYS_TYPE_ALPC_SECURITY_ATTRIBUTES,/**< ALPC_SECURITY_ATTRIBUTES structure. */
    DRSYS_TYPE_LOGFONTW,            /**< LOGFONTW structure. */
    DRSYS_TYPE_NONCLIENTMETRICSW,   /**< NONCLIENTMETRICSW structure. */
    DRSYS_TYPE_ICONMETRICSW,        /**< ICONMETRICSW structure. */
    DRSYS_TYPE_SERIALKEYSW,         /**< SERIALKEYSW structure. */
    DRSYS_TYPE_SOCKADDR,            /**< struct sockaddr. */ 
    DRSYS_TYPE_MSGHDR,              /**< struct msghdr. */
    DRSYS_TYPE_MSGBUF,              /**< struct msgbuf. */
    DRSYS_TYPE_LARGE_INTEGER,       /**< LARGE_INTEGER structure. */
    DRSYS_TYPE_ULARGE_INTEGER,      /**< ULARGE_INTEGER structure. */
    DRSYS_TYPE_IO_STATUS_BLOCK,     /**< IO_STATUS_BLOCK structure. */

    DRSYS_TYPE_FUNCTION,            /**< Function of unspecified signature. */
    /* Additional types may be added in the future. */
    DRSYS_TYPE_LAST = DRSYS_TYPE_FUNCTION,
} drsys_param_type_t;

/** Describes a system call parameter or memory region. */
typedef struct _drsys_arg_t {
    /* System call context *****************************************/
    /** The system call handle. */
    drsys_syscall_t *syscall;
    /** The system call number. */
    drsys_sysnum_t sysnum;
    /** The current thread's drcontext.  Set for the dynamic iterators only. */
    void *drcontext;
    /**
     * Whether operating pre-system call (if true) or post-system call (if false).
     * Set for the dynamic iterators only (drsys_iterate_args() and
     * drsys_iterate_memargs()).
     */
    bool pre;
    /**
     * The application state, cached at the pre- or post-system call event.
     * This contains DR_MC_CONTROL|DR_MC_INTEGER.
     * Set for the dynamic iterators only.
     */
    dr_mcontext_t *mc;

    /* System call argument information ****************************/
    /** The ordinal of the parameter.  Set to -1 for a return value. */
    int ordinal;
    /** The mode (whether read or written) of the parameter. */
    drsys_param_mode_t mode;
    /** The type of the parameter. */
    drsys_param_type_t type;
    /** A string further describing the type of the parameter.  May be NULL. */
    const char *type_name;
    /**
     * For the memarg iterator, the type of the containing structure.  This is
     * only set for some types when the sub-fields of the sructure are separated
     * into different pieces due to gaps in the structure and the containing
     * structure has its own type enum value.
     * If not valid, it is set to DRSYS_TYPE_INVALID.
     * Invalid for the arg iterator.
     */
    drsys_param_type_t containing_type;
    /** A string describing the parameter.  This may be NULL. */
    const char *arg_name;
    /**
     * If not set to DR_REG_NULL, indicates which register the parameter's
     * value is stored in.
     */
    reg_id_t reg;
    /**
     * Indicates whether the start_addr and value fields are valid.  For memarg
     * iteration, this is always true, as a failure to read will result in not
     * calling the callback for that memarg.  For arg iteration this field can
     * be false.  For static iteration this field is always false.
     */
    bool valid;
    /**
     * For the memarg iterator, holds the address of the start of the memory
     * region represented by this parameter.
     * For the arg iterator, if this parameter is in memory, holds the
     * address of the memory location; if this parameter is a register,
     * holds NULL (and the register is in the \p reg field).
     */
    void *start_addr;
    /**
     * For the arg iterator, holds the value of the parameter.
     * Unused for the memarg iterator.
     */
    ptr_uint_t value;
    /**
     * For the memarg iterator, specifies the size of the memory region.
     * For the arg iterator, specifies the size of the parameter.
     */
    size_t size;
} drsys_arg_t;

/** Indicates the category of system call.  Relevant to Windows only. */
typedef enum {
    DRSYS_SYSCALL_TYPE_KERNEL,   /**< The kernel proper (ntoskrnl for Windows). */
    DRSYS_SYSCALL_TYPE_USER,     /**< A user-related system call. */
    DRSYS_SYSCALL_TYPE_GRAPHICS, /**< A graphics-related system call. */
} drsys_syscall_type_t;

/** Specifies parameters controlling the behavior of Dr. Syscall to drsys_init(). */
typedef struct _drsys_options_t {
    /** For compatibility. Set to sizeof(drsys_options_t). */
    size_t struct_size;

    /* For analyzing unknown system calls */
    /**
     * Dr. Syscall does not have information on every system call.  For unknown
     * syscalls, if this parameter is set, then a pre- and post-syscall memory
     * comparison will be used to identify output parameters.  Input parameters
     * will remain unknown.  When using this parameter, we recommend
     * providing callbacks for is_byte_addressable(), is_byte_defined(),
     * and is_register_defined(), if possible, to achieve greater accuracy.
     */
    bool analyze_unknown_syscalls;
    /**
     * If analyze_unknown_syscalls is on and this parameter is on, when changes
     * are detected, the containing dword (32 bits) are considered to have
     * changed.
     */
    bool syscall_dword_granularity;
    /**
     * If analyze_unknown_syscalls is on and this parameter is on, sentinels are
     * used to detect writes and reduce false positives, in particular for
     * uninitialized reads.  However, enabling this option can potentially
     * result in incorrect behavior if definedness information is incorrect or
     * application threads read syscall parameter info simultaneously.
     */
    bool syscall_sentinels;
    /**
     * Provides a query routine for whether a byte is addressable, i.e.,
     * allocated and safe to access.
     * If analyze_unknown_syscalls is on, the quality of unknown parameter
     * analysis increases substantially if information on whether registers
     * and memory contain valid, initialized information is available.
     */
    bool (*is_byte_addressable)(byte *addr);
    /**
     * Provides a query routine for whether a byte is defined, i.e.,
     * allocated, safe to access, and initialized.
     * If analyze_unknown_syscalls is on, the quality of unknown parameter
     * analysis increases substantially if information on whether registers
     * and memory contain valid, initialized information is available.
     */
    bool (*is_byte_defined)(byte *addr);
    /**
     * Provides a query routine for whether a byte is undefined, i.e.,
     * allocated and safe to access yet uninitialized.
     * If analyze_unknown_syscalls is on, the quality of unknown parameter
     * analysis increases substantially if information on whether registers
     * and memory contain valid, initialized information is available.
     * If this is not provided but is_byte_addressable and is_byte_defined both
     * are, those two will be called in concert to provide this information.
     */
    bool (*is_byte_undefined)(byte *addr);
    /**
     * Provides a query routine for whether a register is defined, i.e.,
     * contains a fully initialized value.
     * If analyze_unknown_syscalls is on, the quality of unknown parameter
     * analysis increases substantially if information on whether registers
     * and memory contain valid, initialized information is available.
     */
    bool (*is_register_defined)(reg_id_t reg);


    /** This is an internal-only option that is reserved for developer use. */
    bool verify_sysnums;
    /** This is an internal-only option that is reserved for developer use. */
    app_pc (*lookup_internal_symbol)(const module_data_t *mod, const char *sym);
    /** This is an internal-only option that is reserved for developer use. */
    bool syscall_driver;
} drsys_options_t;


/** Type of iterator callbacks. */
typedef bool (*drsys_iter_cb_t)(drsys_arg_t *arg, void *user_data);

/***************************************************************************
 * TOP-LEVEL
 */

DR_EXPORT
/**
 * Initializes the Dr. Syscall extension.  Must be called prior to any
 * of the other routines.  Can be called multiple times (by separate components,
 * normally) but each call must be paired with a corresponding call to
 * drsys_exit().
 *
 * @param[in] client_id  The id of the client using drsys, as passed to dr_init().
 * @param[in] options    Allows changing the default behavior of Dr. Syscall.
 *
 * \return success code.
 */
drmf_status_t
drsys_init(client_id_t client_id, drsys_options_t *options);

DR_EXPORT
/**
 * Cleans up the Dr. Syscall extension.
 */
drmf_status_t
drsys_exit(void);


DR_EXPORT
/**
 * Instructs Dr. Syscall that this system call will be queried and must be
 * tracked.  In particular, Dr. Syscall only records pre-system call arguments
 * for system calls that are filtered.
 *
 * @param[in] sysnum  The system call number to track.
 *
 * \return success code.
 */
drmf_status_t
drsys_filter_syscall(drsys_sysnum_t sysnum);

DR_EXPORT
/**
 * Instructs Dr. Syscall that all system calls may be queried and must be
 * tracked.  In particular, Dr. Syscall only records pre-system call arguments
 * for system calls that are filtered.
 *
 * \return success code.
 */
drmf_status_t
drsys_filter_all_syscalls(void);

/***************************************************************************
 * STATELESS QUERIES
 */

DR_EXPORT
/**
 * Given a system call name, retrieves a handle to the system call to
 * be used for further queries.  The handle is valid until drsys_exit()
 * is called.
 * On Windows, multiple versions of the name are accepted.
 * For ntoskrnl system calls, the Nt or Zw varieties are supported.
 * For secondary system calls like NtUserCallOneParam.RELEASEDC, the
 * full name as well as just the secondary name (RELEASEDC) are accepted.
 * The lookup is case-insensitive on Windows.
 * This can be called in dr_init() for all system calls, even if the
 * libraries containing their wrappers have not yet been loaded.
 *
 * @param[in]  name    The system call name to look up.
 * @param[out] sysnum  The system call handle.
 *
 * \return success code.
 */
drmf_status_t
drsys_name_to_syscall(const char *name, drsys_syscall_t **syscall OUT);

DR_EXPORT
/**
 * Given a system call number, retrieves a handle to the system call to
 * be used for further queries.  The handle is valid until drsys_exit()
 * is called.
 * This can be called in dr_init() for all system calls, even if the
 * libraries containing their wrappers have not yet been loaded.
 *
 * @param[in]  sysnum   The system call number to look up.
 * @param[out] syscall  The system call handle.
 *
 * \return success code.
 */
drmf_status_t
drsys_number_to_syscall(drsys_sysnum_t sysnum, drsys_syscall_t **syscall OUT);

DR_EXPORT
/**
 * Given a system call handle, retrieves the canonical system call name.
 * The system call handle can be obtained from drsys_cur_syscall(),
 * drsys_iterate_syscalls(), drsys_name_to_syscall(),
 * drsys_number_to_syscall(), or syscall_arg_t.syscall.
 *
 * @param[in]  syscall  The handle for the system call to query.
 * @param[out] name     The system call name.
 *
 * \return success code.
 */
drmf_status_t
drsys_syscall_name(drsys_syscall_t *syscall, const char **name OUT);

DR_EXPORT
/**
 * Given a system call handle, retrieves the system call number.
 * The system call handle can be obtained from drsys_cur_syscall(),
 * drsys_iterate_syscalls(), drsys_name_to_syscall(),
 * drsys_number_to_syscall(), or syscall_arg_t.syscall.
 *
 * @param[in]  syscall  The handle for the system call to query.
 * @param[out] sysnum   The system call number.
 *
 * \return success code.
 */
drmf_status_t
drsys_syscall_number(drsys_syscall_t *syscall, drsys_sysnum_t *sysnum OUT);

DR_EXPORT
/**
 * Identifies the type of system call.
 * The system call handle can be obtained from drsys_cur_syscall(),
 * drsys_iterate_syscalls(), drsys_name_to_syscall(),
 * drsys_number_to_syscall(), or syscall_arg_t.syscall.
 *
 * @param[in]  syscall  The handle for the system call to query.
 * @param[out] type     The system call type.
 *
 * \return success code.
 */
drmf_status_t
drsys_syscall_type(drsys_syscall_t *syscall, drsys_syscall_type_t *type OUT);

DR_EXPORT
/**
 * Identifies whether the system call details for the given syscall are known.
 * The system call handle can be obtained from drsys_cur_syscall(),
 * drsys_iterate_syscalls(), drsys_name_to_syscall(),
 * drsys_number_to_syscall(), or syscall_arg_t.syscall.
 *
 * @param[in]  syscall  The handle for the system call to query.
 * @param[out] known    Whether known.
 *
 * \return success code.
 */
drmf_status_t
drsys_syscall_is_known(drsys_syscall_t *syscall, bool *known OUT);

DR_EXPORT
/**
 * Identifies whether the given value is a successful return value
 * for the given system call.
 * The system call handle can be obtained from drsys_cur_syscall(),
 * drsys_iterate_syscalls(), drsys_name_to_syscall(),
 * drsys_number_to_syscall(), or syscall_arg_t.syscall.
 *
 * On Windows, System calls that return an error code like
 * STATUS_BUFFER_TOO_SMALL but that still write an OUT param are
 * considered to have succeeded.
 *
 * @param[in]  syscall  The handle for the system call to query.
 * @param[in]  result   The system call return value.
 * @param[out] success  Whether the value indicates success.
 *
 * \return success code.
 */
drmf_status_t
drsys_syscall_succeeded(drsys_syscall_t *syscall, reg_t result, bool *success OUT);

DR_EXPORT
/**
 * Identifies the type of the return value for the specified system call.
 * The system call handle can be obtained from drsys_cur_syscall(),
 * drsys_iterate_syscalls(), drsys_name_to_syscall(),
 * drsys_number_to_syscall(), or syscall_arg_t.syscall.
 *
 * @param[in]  syscall  The handle for the system call to query.
 * @param[out] type     The system call return type.
 *
 * \return success code.
 */
drmf_status_t
drsys_syscall_return_type(drsys_syscall_t *syscall, drsys_param_type_t *type OUT);

#ifdef WINDOWS
DR_EXPORT
/**
 * Identifies whether the given process handle refers to the current process.
 *
 * @param[in]  h        The handle to query.
 * @param[out] current  Whether the handle refers to the current process.
 *
 * \return success code.
 */
drmf_status_t
drsys_handle_is_current_process(HANDLE h, bool *current);
#endif

/**
 * Returns whether the two system call numbers are equal.
 *
 * @param[in]  num1   The first number to compare.
 * @param[in]  num2   The second number to compare.
 *
 * \return whether equal.
 */
static inline bool
drsys_sysnums_equal(drsys_sysnum_t *num1, drsys_sysnum_t *num2)
{
    if (num1 == NULL || num2 == NULL)
        return DRMF_ERROR_INVALID_PARAMETER;
    return (num1->number == num2->number &&
            num1->secondary == num2->secondary);
}

/***************************************************************************
 * DYNAMIC QUERIES
 *
 * Must be called from syscall events.
 */

DR_EXPORT
/**
 * Retrieves the system call handle for the current in-progress system call.
 * The handle is only valid through the end of the post-system-call event
 * for the system call.
 *
 * @param[in]  drcontext  The current DynamoRIO thread context.
 * @param[out] syscall    The system call handle.
 *
 * \return success code.
 */
drmf_status_t
drsys_cur_syscall(void *drcontext, drsys_syscall_t **syscall OUT);

DR_EXPORT
/**
 * Identifies the value of a system call argument as passed to the
 * current in-progress system call.  The value is cached in the
 * pre-syscall event only for those system calls that are filtered via
 * drsys_filter_syscall() drsys_filter_all_syscalls().  Must be called
 * from a system call pre- or post-event.
 *
 * @param[in]  drcontext  The current DynamoRIO thread context.
 * @param[in]  argnum     The ordinal of the parameter to query.
 * @param[out] value      The value of the parameter.
 *
 * \return success code.
 */
drmf_status_t
drsys_pre_syscall_arg(void *drcontext, uint argnum, ptr_uint_t *value OUT);

DR_EXPORT
/**
 * Identifies the machine context of the application at the point
 * of the current in-progress system call.  The data is cached in the
 * pre-syscall event only for those system calls that are filtered via
 * drsys_filter_syscall() drsys_filter_all_syscalls().  Must be called
 * from a system call pre- or post-event.
 *
 * This is a copy of the machine context, for convenience.  It should
 * not be modified.  To change the context, or to change system call
 * parameters or return value, the client must use a separate system
 * call event that is ordered prior to DRMGR_PRIORITY_PRESYS_DRSYS or
 * DRMGR_PRIORITY_POSTSYS_DRSYS.
 *
 * @param[in]  drcontext  The current DynamoRIO thread context.
 * @param[out] mc         The cached machine context.
 *
 * \return success code.
 */
drmf_status_t
drsys_get_mcontext(void *drcontext, dr_mcontext_t **mc OUT);


/***************************************************************************
 * STATIC CALLBACK-BASED ITERATORS
 */

DR_EXPORT
/**
 * Iterates over all system call numbers and calls the given callback
 * for each one.  The argument types of each system call can then be
 * enumerated by calling drsys_iterate_arg_types() and passing the
 * given system call handle \p syscall.
 *
 * This will enumerate all system calls even if the libraries
 * containing their wrappers have not yet been loaded.  System calls
 * whose parameter details are unknown are included (see
 * drsys_syscall_is_known()).
 *
 * @param[in] cb         The callback to invoke for each system call number.
 *                       The callback's return value indicates whether to
 *                       continue the iteration.
 * @param[in] user_data  A custom parameter passed to \p cb.
 *
 * \return success code.
 */
drmf_status_t
drsys_iterate_syscalls(bool (*cb)(drsys_sysnum_t sysnum,
                                  drsys_syscall_t *syscall, void *user_data),
                       void *user_data);

DR_EXPORT
/**
 * Statically iterates over all system call parameters for the given
 * system call.
 * The system call handle can be obtained from drsys_cur_syscall(),
 * drsys_iterate_syscalls(), drsys_name_to_syscall(),
 * drsys_number_to_syscall(), or syscall_arg_t.syscall.
 *
 * Only the top-level types are enumerated (i.e., fields of structures
 * are not recursively followed).  As this is a static iteration, only
 * the types are known and not any values.
 * The return value is included at the end of the iteration, with a
 * drsys_arg_t.ordinal value of -1.
 *
 * @param[in] syscall    The handle for the system call to query.
 * @param[in] cb         The callback to invoke for each system call parameter.
 *                       The callback's return value indicates whether to
 *                       continue the iteration.
 * @param[in] user_data  A custom parameter passed to \p cb.
 *
 * \return success code.
 */
drmf_status_t
drsys_iterate_arg_types(drsys_syscall_t *syscall, drsys_iter_cb_t cb, void *user_data);

/***************************************************************************
 * DYNAMIC CALLBACK-BASED ITERATORS
 */

DR_EXPORT
/**
 * Dynamically iterates over all system call parameters for the
 * current in-progress system call.  Only the top-level types are
 * enumerated (i.e., fields of structures are not recursively
 * followed).  The return value is included.
 * Must be called from a system call pre- or post-event.
 *
 * @param[in] drcontext  The current DynamoRIO thread context.
 * @param[in] cb         The callback to invoke for each system call parameter.
 *                       The callback's return value indicates whether to
 *                       continue the iteration.
 * @param[in] user_data  A custom parameter passed to \p cb.
 *
 * \return success code.
 */
drmf_status_t
drsys_iterate_args(void *drcontext, drsys_iter_cb_t cb, void *user_data);

DR_EXPORT
/**
 * Dynamically iterates over all memory regions read or written by the
 * current in-progress system call.  Does descend into fields of data
 * structures.
 *
 * Must be called from a system call pre- or post-event.  If this is
 * called from a post-system call event, it must also be called from
 * the pre-system call event, as some information required for
 * post-system call values is stored during pre-system call iteration.
 *
 * In pre-syscall, for OUT parameters, may treat a region containing
 * padding between structure fields as a single region.  Otherwise,
 * splits up any region with padding into multiple iteration steps.
 *
 * For unknown syscalls, may call \p cb for each byte of memory even for
 * adjacent bytes, as it uses a heuristic to try and detect written memory.
 *
 * Does NOT iterate over the primary parameter values themselves, even if
 * they are located in memory: use drsys_iterate_args() for that.
 *
 * If unable to read the value of a parameter, will skip potential memory
 * regions.
 *
 * Some memory regions may overlap each other.  This occurs when the
 * full capacity of a structure is passed to \p cb with a mode of
 * DRSYS_PARAM_BOUNDS and the fields of the structure are subsequently
 * enumerated separately.
 *
 * @param[in] drcontext  The current DynamoRIO thread context.
 * @param[in] cb         The callback to invoke for each memory region.
 *                       The callback's return value indicates whether to
 *                       continue the iteration.
 * @param[in] user_data  A custom parameter passed to \p cb.
 *
 * \return success code.
 */
drmf_status_t
drsys_iterate_memargs(void *drcontext, drsys_iter_cb_t cb, void *user_data);


/***************************************************************************
 * SYNCHRONOUS ITERATORS
 */

/* XXX i#1092: add start/next/stop synchronous layer on top of
 * callback-based iterators
 */


/*@}*/ /* end doxygen group */

#ifdef __cplusplus
}
#endif

#endif /* _DR_SYSCALL_H_ */
