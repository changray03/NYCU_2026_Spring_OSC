extern void uart_puts(const char* str);
extern void uart_putc(char c);
extern void uart_hex(unsigned long h);
extern char uart_getc();

extern struct sbiret sbi_get_spec_version();
extern struct sbiret sbi_get_impl_id();
extern struct sbiret sbi_get_impl_version();
struct sbiret {
	long error;
	long value;
};

#define prompt_size 128

_Bool cmd_cmp(const char* cmd, const char* target){
    int i = 0;
    while(cmd[i] != '\0' && target[i] != '\0'){
        if(cmd[i] != target[i])
            return 0;
        i++;
    }
    return cmd[i] == '\0' && target[i] == '\0';
}

extern char _start[];

void start_kernel(){
    uart_puts("Kernel actual address: ");
    uart_hex((unsigned long)_start);
    uart_putc('\n');
    uart_putc('\n');
    while(1){
        uart_puts("opi-rv2> ");
        char cmd[prompt_size];
        int i = 0;
        
        // Dealing with input
        while(1){
            char c = uart_getc();
            if(c == 0x1B){
                uart_getc();
                uart_getc();
                continue;
            }
            if(c == '\r' || c == '\n'){
                uart_putc('\n');
                cmd[i] = '\0';
                break;
            }
            else if(c == '\b' || c == 127){
                if(i > 0){
                    uart_putc('\b');
                    uart_putc(' ');
                    uart_putc('\b');
                    i--;
                }
            }
            else{
                if(i < prompt_size - 1 && c >= 32 && c <= 126){
                    uart_putc(c);
                    cmd[i++] = c;
                }
            }
        }

        // Dealing with command
        if(cmd_cmp(cmd, "help")){
            uart_puts("Available commands:\n");
            uart_puts("\thelp - show all commands\n");
            uart_puts("\thello - print Hello, World!\n");
            uart_puts("\tinfo - print system info\n");
        }
        else if(cmd_cmp(cmd, "hello")){
            uart_puts("Hello, World!\n");
        }
        else if(cmd_cmp(cmd, "info")){
            uart_puts("System information:\n");
            struct sbiret spec_version = sbi_get_spec_version();
            struct sbiret impl_id = sbi_get_impl_id();
            struct sbiret impl_version = sbi_get_impl_version();
            
            if(spec_version.error == 0){
                uart_puts("\tSBI Spec Version: ");
                uart_hex(spec_version.value);
                uart_putc('\n');
            }
            else{
                uart_puts("\tFailed to get SBI Spec Version\n");
            }

            if(impl_id.error == 0){
                uart_puts("\tSBI Impl ID: ");
                uart_hex(impl_id.value);
                uart_putc('\n');
            }
            else{
                uart_puts("\tFailed to get SBI Impl ID\n");
            }

            if(impl_version.error == 0){
                uart_puts("\tSBI Impl Version: ");
                uart_hex(impl_version.value);
                uart_putc('\n');
            }
            else{
                uart_puts("\tFailed to get SBI Impl Version\n");
            }
        }
        else{
            if(i == 0){
                continue;
            }
            uart_puts("Unknown command: ");
            uart_puts(cmd);
            uart_putc('\n');
        }
    }
}

