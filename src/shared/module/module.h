#ifndef MODULE_H
#define MODULE_H

#include "../../include/local_video.h"

#ifdef WINDOWS_BUILD

/* Windows: use constructor functions for automatic registration (PE format has no linker script sections) */

#define MAX_MODULE_ENTRIES 16

/* Global registration tables */
extern module_init_entry_t g_module_init_table[];
extern int g_module_init_count;
extern module_sub_entry_t g_module_sub_table[];
extern int g_module_sub_count;
extern module_run_entry_t g_module_run_table[];
extern int g_module_run_count;
extern module_exit_entry_t g_module_exit_table[];
extern int g_module_exit_count;

/* Registration helper macros using constructor attribute for auto-registration */
#define MODULE_INIT(func, mod_name) \
    static void _lv_reg_init_##func(void) { \
        int _idx = g_module_init_count++; \
        if (_idx < MAX_MODULE_ENTRIES) { \
            g_module_init_table[_idx].name = mod_name; \
            g_module_init_table[_idx].fn = func; \
        } \
    } \
    __attribute__((constructor)) static void _lv_auto_init_##func(void) { _lv_reg_init_##func(); }

#define MODULE_SUB(func, mod_name) \
    static void _lv_reg_sub_##func(void) { \
        int _idx = g_module_sub_count++; \
        if (_idx < MAX_MODULE_ENTRIES) { \
            g_module_sub_table[_idx].name = mod_name; \
            g_module_sub_table[_idx].fn = func; \
        } \
    } \
    __attribute__((constructor)) static void _lv_auto_sub_##func(void) { _lv_reg_sub_##func(); }

#define MODULE_RUN(func, mod_name) \
    static void _lv_reg_run_##func(void) { \
        int _idx = g_module_run_count++; \
        if (_idx < MAX_MODULE_ENTRIES) { \
            g_module_run_table[_idx].name = mod_name; \
            g_module_run_table[_idx].fn = func; \
        } \
    } \
    __attribute__((constructor)) static void _lv_auto_run_##func(void) { _lv_reg_run_##func(); }

#define MODULE_EXIT(func, mod_name) \
    static void _lv_reg_exit_##func(void) { \
        int _idx = g_module_exit_count++; \
        if (_idx < MAX_MODULE_ENTRIES) { \
            g_module_exit_table[_idx].name = mod_name; \
            g_module_exit_table[_idx].fn = func; \
        } \
    } \
    __attribute__((constructor)) static void _lv_auto_exit_##func(void) { _lv_reg_exit_##func(); }

#else

/* Linux: use linker script sections for automatic module discovery */

#define MODULE_INIT(func, mod_name) \
    __attribute__((used, section(".embedi_init"))) \
    static const module_init_entry_t _lv_init_##func = { \
        mod_name, func \
    }

#define MODULE_SUB(func, mod_name) \
    __attribute__((used, section(".embedi_sub"))) \
    static const module_sub_entry_t _lv_sub_##func = { \
        mod_name, func \
    }

#define MODULE_RUN(func, mod_name) \
    __attribute__((used, section(".embedi_run"))) \
    static const module_run_entry_t _lv_run_##func = { \
        mod_name, func \
    }

#define MODULE_EXIT(func, mod_name) \
    __attribute__((used, section(".embedi_exit"))) \
    static const module_exit_entry_t _lv_exit_##func = { \
        mod_name, func \
    }

#endif /* WINDOWS_BUILD */

void module_init_all(void);
void module_sub_all(void);
void module_run_all(void);
void module_exit_all(void);

#endif /* MODULE_H */
