/* Windows/MinGW linker script for module sections */
SECTIONS
{
    .embedi_init : {
        __start_embedi_init = .;
        KEEP(*(.embedi_init))
        __stop_embedi_init = .;
    }
    .embedi_sub : {
        __start_embedi_sub = .;
        KEEP(*(.embedi_sub))
        __stop_embedi_sub = .;
    }
    .embedi_run : {
        __start_embedi_run = .;
        KEEP(*(.embedi_run))
        __stop_embedi_run = .;
    }
    .embedi_exit : {
        __start_embedi_exit = .;
        KEEP(*(.embedi_exit))
        __stop_embedi_exit = .;
    }
}