#ifndef MODULE_H
#define MODULE_H

#include "local_video.h"

#define MODULE_INIT(fn, module_name) \
    __attribute__((used, section(".embedi_init"))) \
    static const module_init_entry_t _lv_init_##fn = { \
        module_name, fn \
    }

#define MODULE_SUB(fn, module_name) \
    __attribute__((used, section(".embedi_sub"))) \
    static const module_sub_entry_t _lv_sub_##fn = { \
        module_name, fn \
    }

#define MODULE_RUN(fn, module_name) \
    __attribute__((used, section(".embedi_run"))) \
    static const module_run_entry_t _lv_run_##fn = { \
        module_name, fn \
    }

#define MODULE_EXIT(fn, module_name) \
    __attribute__((used, section(".embedi_exit"))) \
    static const module_exit_entry_t _lv_exit_##fn = { \
        module_name, fn \
    }

void module_init_all(void);
void module_sub_all(void);
void module_run_all(void);
void module_exit_all(void);

#endif /* MODULE_H */
