extern void uart_puts(const char* str);
extern void uart_putc(char c);
extern void uart_hex(unsigned long h);
extern char uart_getc();

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

void start_kernel(){
    while(1){
        uart_puts("opi-rv2> ");
        char cmd[prompt_size];
        int i = 0;
        
        // Dealing with input
        while(1){
            char c = uart_getc();
            if(c == '\r' || c == '\n'){
                uart_putc('\n');
                cmd[i] = '\0';
                break;
            }
            else if((c == '\b' || c == 127) && i > 0){
                uart_putc('\b');
                uart_putc(' ');
                uart_putc('\b');
                i--;
            }
            else if(i < prompt_size - 1){
                uart_putc(c);
                cmd[i++] = c;
            }
        }

        // Dealing with command
        if(cmd_cmp(cmd, "help")){
            uart_puts("Available commands:\n");
            uart_puts("help - Show all commands\n");
            uart_puts("hello - Print Hello, World!\n");
        }
        else if(cmd_cmp(cmd, "hello")){
            uart_puts("Hello, World!\n");
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

