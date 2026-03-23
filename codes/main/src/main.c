#define F_CPU 16500000L
#include <avr/io.h>
#include <util/delay.h>
#include <stdlib.h>
#include <string.h>
#include <avr/pgmspace.h>

#define SDA PB0
#define SCL PB2
#define SR_CLK PB1
#define SR_LATCH PB3
#define SR_DATA PB4 
#define LCD_ADDR (0x27 << 1)

// --- DUAL-MODE KEYPAD MAPS ---
const char keymap_1[25] = {
    '1', '2', '3', '4', '5', 
    '6', '7', '8', '9', '0', 
    '+', '-', '*', '/', '^', 
    's', 'c', 't', '(', ')', 
    '.', 'M', 'B', 'C', '='  
};

const char keymap_2[25] = {
    '1', '2', '3', '4', '5', 
    '6', '7', '8', '9', '0', 
    'R', 'f', 'P', 'm', 'a', // m=modulus, a=abs
    'I', 'O', 'A', '(', ')', 
    '.', 'M', 'B', 'C', '='  
};

// --- LIGHTWEIGHT TRIG LOOKUP TABLES ---
const uint16_t sin_table[91] PROGMEM = {
    0, 175, 349, 523, 698, 872, 1045, 1219, 1392, 1564,
    1736, 1908, 2079, 2250, 2419, 2588, 2756, 2924, 3090, 3256,
    3420, 3584, 3746, 3907, 4067, 4226, 4384, 4540, 4695, 4848,
    5000, 5150, 5299, 5446, 5592, 5736, 5878, 6018, 6157, 6293,
    6428, 6561, 6691, 6820, 6947, 7071, 7193, 7314, 7431, 7547,
    7660, 7771, 7880, 7986, 8090, 8192, 8290, 8387, 8480, 8572,
    8660, 8746, 8829, 8910, 8988, 9063, 9135, 9205, 9272, 9336,
    9397, 9455, 9511, 9563, 9613, 9659, 9703, 9744, 9781, 9816,
    9848, 9877, 9903, 9925, 9945, 9962, 9976, 9986, 9994, 9998, 10000
};

const uint16_t asin_table[21] PROGMEM = {
    0, 287, 574, 863, 1154, 1448, 1746, 2049, 2358, 2674, 
    3000, 3337, 3687, 4054, 4443, 4859, 5313, 5821, 6416, 7181, 9000
};

uint8_t math_error = 0;

// --- I2C & LCD Drivers ---
void i2c_start() { DDRB |= (1<<SDA); _delay_us(5); DDRB |= (1<<SCL); _delay_us(5); }
void i2c_stop() { DDRB |= (1<<SDA); _delay_us(5); DDRB &= ~(1<<SCL); _delay_us(5); DDRB &= ~(1<<SDA); _delay_us(5); }
void i2c_write(uint8_t byte) {
    for(uint8_t i=0; i<8; i++) {
        if(byte & 0x80) DDRB &= ~(1<<SDA); else DDRB |= (1<<SDA);
        _delay_us(5); DDRB &= ~(1<<SCL); _delay_us(5); DDRB |= (1<<SCL); _delay_us(5);
        byte <<= 1;
    }
    DDRB &= ~(1<<SDA); DDRB &= ~(1<<SCL); _delay_us(5); DDRB |= (1<<SCL); _delay_us(5);
}
void lcd_send(uint8_t data, uint8_t mode) {
    uint8_t h = (data & 0xF0) | mode | 0x08; uint8_t l = ((data << 4) & 0xF0) | mode | 0x08;
    i2c_start(); i2c_write(LCD_ADDR); i2c_write(h|0x04); i2c_stop(); i2c_start(); i2c_write(LCD_ADDR); i2c_write(h); i2c_stop();
    i2c_start(); i2c_write(LCD_ADDR); i2c_write(l|0x04); i2c_stop(); i2c_start(); i2c_write(LCD_ADDR); i2c_write(l); i2c_stop();
    _delay_us(50);
}
void lcd_print(const char* str, uint8_t line) {
    lcd_send(line == 0 ? 0x80 : 0xC0, 0); 
    for(uint8_t i=0; i<16; i++) {
        if(*str) lcd_send(*str++, 1);
        else lcd_send(' ', 1); 
    }
}

// --- FIXED-POINT MATH BASE ---
void format_fixed(int32_t val, char* buf) {
    if(val < 0) { *buf++ = '-'; val = -val; }
    int32_t int_p = val / 1000; int32_t frac_p = val % 1000;
    ltoa(int_p, buf, 10);
    while(*buf) buf++; 
    *buf++ = '.';
    if(frac_p < 100) *buf++ = '0'; 
    if(frac_p < 10) *buf++ = '0';
    ltoa(frac_p, buf, 10);
}

int32_t get_sin_fixed(int16_t angle_deg) {
    angle_deg = angle_deg % 360;
    if (angle_deg < 0) angle_deg += 360;
    int16_t mapped; int32_t sign = 1;
    if (angle_deg <= 90) mapped = angle_deg;
    else if (angle_deg <= 180) mapped = 180 - angle_deg;
    else if (angle_deg <= 270) { mapped = angle_deg - 180; sign = -1; }
    else { mapped = 360 - angle_deg; sign = -1; }
    return sign * (pgm_read_word(&sin_table[mapped]) / 10); 
}

int32_t sqrt_fixed(int32_t x) {
    if (x <= 0) return 0;
    uint32_t res = 0, add = 0x40000000, val = x * 1000;   
    while (add > 0) {
        if (val >= res + add) { val -= res + add; res = (res >> 1) + add; } 
        else { res >>= 1; }
        add >>= 2;
    }
    return res;
}

int32_t get_asin_fixed(int32_t x) {
    int32_t sign = 1;
    if (x < 0) { sign = -1; x = -x; }
    if (x > 1000) { math_error = 1; return 0; } 
    int32_t idx = x / 50;
    int32_t rem = x % 50;
    if (idx >= 20) return sign * 90000; 
    int32_t y1 = pgm_read_word(&asin_table[idx]);
    int32_t y2 = pgm_read_word(&asin_table[idx+1]);
    int32_t interpolated = y1 + ((y2 - y1) * rem) / 50;
    return sign * (interpolated * 10); 
}

int32_t get_acos_fixed(int32_t x) {
    if (x < -1000 || x > 1000) { math_error = 1; return 0; }
    return 90000 - get_asin_fixed(x); 
}

int32_t get_atan_fixed(int32_t x) {
    int64_t x_sq = ((int64_t)x * x) / 1000; 
    int32_t den = sqrt_fixed(1000 + x_sq);
    if (den == 0) return 0;
    int32_t ratio = ((int64_t)x * 1000) / den;
    return get_asin_fixed(ratio);
}

// ==========================================
// --- SHUNTING-YARD EVALUATOR ---
// ==========================================
uint8_t precedence(char op) {
    if(op == '+' || op == '-') return 1;
    if(op == '*' || op == '/' || op == 'm') return 2; // m added here
    if(op == '^') return 3;
    if(op == 's' || op == 'c' || op == 't' || op == 'f' || op == 'R' || op == 'I' || op == 'O' || op == 'A' || op == 'a') return 4; // a added here
    return 0;
}

void apply_op(int32_t* vals, uint8_t* v_ptr, char op) {
    if (*v_ptr == 0 || math_error) return;
    
    // Unary Functions
    if (op == 's' || op == 'c' || op == 't' || op == 'f' || op == 'R' || op == 'I' || op == 'O' || op == 'A' || op == 'a') { 
        int32_t a = vals[*v_ptr - 1];
        if (op == 's') a = get_sin_fixed(a / 1000); 
        if (op == 'c') a = get_sin_fixed((a / 1000) + 90);
        if (op == 't') {
           int32_t c_val = get_sin_fixed((a/1000)+90);
           if (c_val == 0) { math_error = 1; return; } 
           int32_t s_val = get_sin_fixed(a/1000);
           a = (int32_t)(((int64_t)s_val * 1000) / c_val);
        }
        if (op == 'I') a = get_asin_fixed(a);
        if (op == 'O') a = get_acos_fixed(a);
        if (op == 'A') a = get_atan_fixed(a);
        if (op == 'a') { // Absolute Value
           if (a < 0) a = -a; 
        }
        if (op == 'R') { 
           if (a < 0) { math_error = 1; return; } 
           a = sqrt_fixed(a);
        }
        if (op == 'f') { 
           if (a < 0) { math_error = 1; return; }
           int32_t whole_num = a / 1000;
           int32_t result = 1000;
           for(int i = 1; i <= whole_num; i++) result = (int32_t)(((int64_t)result * (i * 1000)) / 1000);
           a = result;
        }
        vals[*v_ptr - 1] = a;
    } 
    // Binary Functions
    else { 
        if (*v_ptr < 2) return;
        int32_t b = vals[--(*v_ptr)];
        int32_t a = vals[*v_ptr - 1];
        
        if (op == '+') a += b;
        if (op == '-') a -= b;
        if (op == '*') a = (int32_t)(((int64_t)a * b) / 1000);
        if (op == '/') {
            if (b == 0) { math_error = 1; return; } 
            a = (int32_t)((((int64_t)a) * 1000) / b); 
        }
        if (op == 'm') { // Modulus
            if (b == 0) { math_error = 1; return; }
            a = a % b;
        }
        if (op == '^') {
            int32_t exp = b / 1000; 
            if (exp < 0) { math_error = 1; return; } 
            int32_t res = 1000;
            for(int i=0; i<exp; i++) res = (int32_t)(((int64_t)res * a) / 1000);
            a = res;
        }
        vals[*v_ptr - 1] = a;
    }
}

int32_t evaluate_expression(char* expr) {
    int32_t val_stack[8]; uint8_t v_ptr = 0;
    char op_stack[8]; uint8_t o_ptr = 0;
    char* p = expr;
    math_error = 0; 

    while(*p && !math_error) {
        if (*p >= '0' && *p <= '9') {
            int32_t num = 0, frac = 0, div = 1; uint8_t in_frac = 0;
            while((*p >= '0' && *p <= '9') || *p == '.') {
                if (*p == '.') in_frac = 1;
                else if (in_frac) { if(div<1000) {frac=frac*10+(*p-'0'); div*=10;} }
                else num = num*10 + (*p-'0');
                p++;
            }
            while(div < 1000) { frac*=10; div*=10; }
            val_stack[v_ptr++] = num*1000 + frac;
            continue;
        }
        if (*p == 's' || *p == 'c' || *p == 't' || *p == 'f' || *p == 'R' || *p == 'I' || *p == 'O' || *p == 'A' || *p == 'a' || *p == '(') {
            op_stack[o_ptr++] = *p;
        }
        else if (*p == ')') {
            while(o_ptr > 0 && op_stack[o_ptr-1] != '(') apply_op(val_stack, &v_ptr, op_stack[--o_ptr]);
            if (o_ptr > 0) o_ptr--; 
            if (o_ptr > 0 && (op_stack[o_ptr-1] == 's' || op_stack[o_ptr-1] == 'c' || op_stack[o_ptr-1] == 't' || op_stack[o_ptr-1] == 'f' || op_stack[o_ptr-1] == 'R' || op_stack[o_ptr-1] == 'I' || op_stack[o_ptr-1] == 'O' || op_stack[o_ptr-1] == 'A' || op_stack[o_ptr-1] == 'a')) {
                apply_op(val_stack, &v_ptr, op_stack[--o_ptr]);
            }
        }
        else if (*p == '+' || *p == '-' || *p == '*' || *p == '/' || *p == '^' || *p == 'm') {
            if (*p == '-' && (p == expr || *(p-1) == '(')) val_stack[v_ptr++] = 0;
            while(o_ptr > 0 && precedence(op_stack[o_ptr-1]) >= precedence(*p)) apply_op(val_stack, &v_ptr, op_stack[--o_ptr]);
            op_stack[o_ptr++] = *p;
        }
        p++;
    }
    while(o_ptr > 0 && !math_error) apply_op(val_stack, &v_ptr, op_stack[--o_ptr]);
    return v_ptr > 0 ? val_stack[0] : 0;
}

// --- LEAN UI FORMATTER ---
void render_display(char* buf, char* lcd_top) {
    int len = strlen(buf); 
    char out[17] = "                ";
    if(len <= 16) strcpy(out, buf); 
    else strncpy(out, buf + (len - 16), 16);
    lcd_print(lcd_top, 0); 
    lcd_print(out, 1);
}

// --- HARDWARE SCANNER ---
uint8_t scan_matrix() {
    for (uint8_t r = 0; r < 5; r++) {
        DDRB |= (1<<SR_DATA); uint8_t row_data = ~(1 << r); 
        for(int8_t i=7; i>=0; i--) {
            if(row_data & (1<<i)) PORTB |= (1<<SR_DATA); else PORTB &= ~(1<<SR_DATA);
            PORTB |= (1<<SR_CLK); _delay_us(1); PORTB &= ~(1<<SR_CLK);
        }
        PORTB &= ~(1<<SR_LATCH); _delay_us(1); PORTB |= (1<<SR_LATCH); _delay_us(50); 
        PORTB &= ~(1<<SR_LATCH); _delay_us(1); PORTB |= (1<<SR_LATCH);
        
        DDRB &= ~(1<<SR_DATA); PORTB |= (1<<SR_DATA);  
        uint8_t col_data = 0;
        for(uint8_t i=0; i<8; i++) {
            col_data <<= 1; if(PINB & (1<<SR_DATA)) col_data |= 1; 
            PORTB |= (1<<SR_CLK); _delay_us(1); PORTB &= ~(1<<SR_CLK);
        }
        uint8_t pressed_bits = (~col_data) & 0x1F;
        if(pressed_bits > 0) {
            for(uint8_t c=0; c<5; c++) if(pressed_bits & (1<<c)) return (r * 5) + c + 1; 
        }
    }
    return 0; 
}

// --- MAIN CONTROLLER ---
int main() {
    DDRB |= (1<<SR_CLK) | (1<<SR_LATCH);
    _delay_ms(1500); 
    lcd_send(0x33,0); lcd_send(0x32,0); lcd_send(0x28,0); lcd_send(0x0C,0); lcd_send(0x01,0); 
    _delay_ms(5);

    char input_buf[32] = ""; char top_line[16] = "";
    uint8_t last_key = 0, stable_count = 0, key_registered = 0, calc_done = 0; 
    uint8_t current_mode = 1;
    
    render_display(input_buf, top_line);

    while(1) {
        uint8_t key = scan_matrix();
        if (key == last_key) { stable_count++; } 
        else { stable_count = 0; key_registered = 0; last_key = key; }

        if (stable_count >= 3 && key != 0 && !key_registered) {
            key_registered = 1; 
            char c = (current_mode == 1) ? keymap_1[key - 1] : keymap_2[key - 1]; 
            
            if (strcmp(input_buf, "N/A") == 0) { input_buf[0] = '\0'; top_line[0] = '\0'; }

            if (calc_done && c != '+' && c != '-' && c != '*' && c != '/' && c != '^' && c != 'm') {
                input_buf[0] = '\0'; top_line[0] = '\0'; 
            }
            calc_done = 0;

            if (c == 'M') { // MODE SHIFT
                current_mode = (current_mode == 1) ? 2 : 1;
                lcd_print(current_mode == 1 ? "Mode 1" : "Mode 2", 0);
                _delay_ms(800);
            }
            else if (c == 'C') { // CLEAR ALL
                input_buf[0] = '\0'; top_line[0] = '\0';
            }
            else if (c == 'B') { // BACKSPACE
                int len = strlen(input_buf);
                if (len > 0) input_buf[len - 1] = '\0';
            }
            else if (c == '=') { // EVALUATE
                if (strlen(input_buf) > 0) {
                    int ilen = strlen(input_buf);
                    if(ilen <= 15) { strcpy(top_line, input_buf); strcat(top_line, "="); }
                    else { strcpy(top_line, "="); }

                    int32_t result = evaluate_expression(input_buf);
                    
                    if (math_error) {
                        strcpy(input_buf, "N/A");
                    } else {
                        format_fixed(result, input_buf);
                    }
                    calc_done = 1;
                }
            }
            else if (c == 'P') { // INSERT PI
                int len = strlen(input_buf);
                if (len < 25) strcat(input_buf, "3.141");
            }
            else { // TYPING 
                int len = strlen(input_buf);
                if (len < 30) {
                    input_buf[len] = c; 
                    input_buf[len+1] = '\0';
                }
            }
            render_display(input_buf, top_line);
        }
        _delay_ms(20); 
    }
}
