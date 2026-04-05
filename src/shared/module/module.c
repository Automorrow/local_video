#include "module.h"

__attribute__((used, section(".embedi_init")))
static const module_init_entry_t _empty_init = { NULL, NULL };

__attribute__((used, section(".embedi_sub")))
static const module_sub_entry_t _empty_sub = { NULL, NULL };

__attribute__((used, section(".embedi_run")))
static const module_run_entry_t _empty_run = { NULL, NULL };

__attribute__((used, section(".embedi_exit")))
static const module_exit_entry_t _empty_exit = { NULL, NULL };

extern module_init_entry_t __start_embedi_init[];
extern module_init_entry_t __stop_embedi_init[];
extern module_sub_entry_t __start_embedi_sub[];
extern module_sub_entry_t __stop_embedi_sub[];
extern module_run_entry_t __start_embedi_run[];
extern module_run_entry_t __stop_embedi_run[];
extern module_exit_entry_t __start_embedi_exit[];
extern module_exit_entry_t __stop_embedi_exit[];

void module_init_all(void) {
    module_init_entry_t *entry;
    for (entry = __start_embedi_init; entry < __stop_embedi_init; entry++) {
        if (entry->fn) {
            entry->fn();
        }
    }
}

void module_sub_all(void) {
    module_sub_entry_t *entry;
    for (entry = __start_embedi_sub; entry < __stop_embedi_sub; entry++) {
        if (entry->fn) {
            entry->fn();
        }
    }
}

void module_run_all(void) {
    module_run_entry_t *entry;
    for (entry = __start_embedi_run; entry < __stop_embedi_run; entry++) {
        if (entry->fn) {
            entry->fn();
        }
    }
}

void module_exit_all(void) {
    module_exit_entry_t *entry;
    for (entry = __start_embedi_exit; entry < __stop_embedi_exit; entry++) {
        if (entry->fn) {
            entry->fn();
        }
    }
}
