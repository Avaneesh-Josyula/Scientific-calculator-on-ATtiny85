#define F_CPU 16500000L
#include <avr/io.h>
#include <util/delay.h>
#include <stdlib.h>
#include <string.h>
#include <math.h> // Pulling in the heavy math library

#define SDA PB0
#define SCL PB2
#define SR_CLK PB1
#define SR_LATCH PB3
#define SR_DATA PB4 
#define LCD_ADDR (0x27 << 1)

// --- YOUR CUSTOM KEYPAD MAPS ---
// Single chars represent the functions internally
// Mode 1: s=sin, c=cos, t=tan, e=exp, l=ln, C=Clear, B=Backspace, M=Mode, P=Pi
const char keymap_1[25] = {
    '0', '1', '2', '3', '4',
    '5', '6', '7', '8', '9',
    '+', '-', '*', '/', 's',
    'c', 't', 'e', 'l', 'C',
    'B', '.', '=', 'M', 'P'
};

// Mode 2: (= (, )= ), ^=pow, f=fact, S=asin, A=acos, T=atan, m=mod, L=log10
const char keymap_2[25] = {
    '0', '1', '2', '3', '4',
    '5', '6', '7', '8', '9',
    '(', ')', '^', 'f', 'S',
    'A', 'T', 'm', 'L', 'C',
    'B', '.', '=', 'M', 'P'
};

// --- I2C & LCD Basic Functions ---
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

// --- MATRIX SCANNER ---
uint8_t scan_matrix() {
    for (uint8_t r = 0; r < 5; r++) {
        DDRB |= (1<<SR_DATA); 
        uint8_t row_data = ~(1 << r); 
        for(int8_t i=7; i>=0; i--) {
            if(row_data & (1<<i)) PORTB |= (1<<SR_DATA); else PORTB &= ~(1<<SR_DATA);
            PORTB |= (1<<SR_CLK); _delay_us(1); PORTB &= ~(1<<SR_CLK);
        }
        PORTB &= ~(1<<SR_LATCH); _delay_us(1); PORTB |= (1<<SR_LATCH);
        _delay_us(50); 
        PORTB &= ~(1<<SR_LATCH); _delay_us(1); PORTB |= (1<<SR_LATCH);
        
        DDRB &= ~(1<<SR_DATA);  
        PORTB |= (1<<SR_DATA);  
        uint8_t col_data = 0;
        for(uint8_t i=0; i<8; i++) {
            col_data <<= 1; 
            if(PINB & (1<<SR_DATA)) col_data |= 1; 
            PORTB |= (1<<SR_CLK); _delay_us(1); PORTB &= ~(1<<SR_CLK);
        }
        uint8_t pressed_bits = (~col_data) & 0x1F;
        if(pressed_bits > 0) {
            for(uint8_t c=0; c<5; c++) {
                if(pressed_bits & (1<<c)) return (r * 5) + c + 1; 
            }
        }
    }
    return 0; 
}

// --- STATE VARIABLES ---
char input_buf[16] = "";
uint8_t input_idx = 0;
float op1 = 0, op2 = 0;
char current_op = 0;
char top_line[16] = "";
uint8_t calc_done = 0; 
uint8_t current_mode = 1; // Tracks which mode we are in!

int main() {
    DDRB |= (1<<SR_CLK) | (1<<SR_LATCH);
    _delay_ms(1500); 

    lcd_send(0x33,0); lcd_send(0x32,0); lcd_send(0x28,0); lcd_send(0x0C,0); lcd_send(0x01,0); 
    _delay_ms(5);
    
    lcd_print(" ATtiny SciCalc ", 0);
    lcd_print(" Mode 1 Active  ", 1);
    _delay_ms(1500);
    lcd_send(0x01,0); _delay_ms(2);

    uint8_t last_key = 0, stable_count = 0, key_registered = 0; 

    while(1) {
        uint8_t key = scan_matrix();
        
        if (key == last_key) { stable_count++; } 
        else { stable_count = 0; key_registered = 0; last_key = key; }

        if (stable_count >= 3 && key != 0 && !key_registered) {
            key_registered = 1; 
            
            // Map the key bsed on the CURRENT MODE
            char c = (current_mode == 1) ? keymap_1[key - 1] : keymap_2[key - 1];
            
            // --- MODE SHIFT (M) ---
            if (c == 'M') {
                current_mode = (current_mode == 1) ? 2 : 1;
                // Briefly flash the mode on screen
                lcd_print(current_mode == 1 ? "--- MODE 1 ---" : "--- MODE 2 ---", 0);
                _delay_ms(800);
                calc_done = 1; // Force display update
            }
            // --- BACKSPACE (B) ---
            else if (c == 'B') {
                if (input_idx > 0) {
                    input_buf[--input_idx] = '\0';
                }
            }
            // --- PI INJECTION (P) ---
            else if (c == 'P') {
                if (calc_done) { input_idx = 0; calc_done = 0; }
                strcpy(input_buf, "3.1415");
                input_idx = 6;
            }
            // --- NUMBER PAD ---
            else if ((c >= '0' && c <= '9') || c == '.') {
                if (calc_done) {
                    input_idx = 0; input_buf[0] = '\0'; top_line[0] = '\0'; calc_done = 0;
                }
                if (input_idx < 14) {
                    input_buf[input_idx++] = c; input_buf[input_idx] = '\0';
                }
            } 
            // --- CLEAR (C) ---
            else if (c == 'C') {
                input_idx = 0; input_buf[0] = '\0'; top_line[0] = '\0';
                op1 = 0; op2 = 0; current_op = 0; calc_done = 0;
            } 
            // --- TWO-PART OPERATORS (+, -, *, /, ^, m) ---
            else if (c == '+' || c == '-' || c == '*' || c == '/' || c == '^' || c == 'm') {
                if (input_idx > 0) op1 = atof(input_buf);
                current_op = c;
                input_idx = 0; input_buf[0] = '\0'; calc_done = 0;
                
                dtostrf(op1, 0, 2, top_line);
                uint8_t len = strlen(top_line);
                top_line[len++] = ' '; 
                if (c == 'm') top_line[len++] = '%'; else top_line[len++] = c; 
                top_line[len] = '\0';
            } 
            // --- INSTANT MATH FUNCTIONS (Mode 1 & 2) ---
            else if (c == 's' || c == 'c' || c == 't' || c == 'S' || c == 'A' || c == 'T' || c == 'e' || c == 'l' || c == 'L' || c == 'f') {
                if (input_idx > 0) op1 = atof(input_buf);
                
                // Mode 1: Basic Trig & Logs
                if (c == 's') { op1 = sin(op1 * (M_PI/180.0)); strcpy(top_line, "sin()"); } // Assumes input is degrees
                if (c == 'c') { op1 = cos(op1 * (M_PI/180.0)); strcpy(top_line, "cos()"); }
                if (c == 't') { op1 = tan(op1 * (M_PI/180.0)); strcpy(top_line, "tan()"); }
                if (c == 'e') { op1 = exp(op1); strcpy(top_line, "exp()"); }
                if (c == 'l') { op1 = log(op1); strcpy(top_line, "ln()"); } // Natural log
                
                // Mode 2: Inverse Trig, Log10 & Factorial
                if (c == 'S') { op1 = asin(op1) * (180.0/M_PI); strcpy(top_line, "asin()"); } // Returns degrees
                if (c == 'A') { op1 = acos(op1) * (180.0/M_PI); strcpy(top_line, "acos()"); }
                if (c == 'T') { op1 = atan(op1) * (180.0/M_PI); strcpy(top_line, "atan()"); }
                if (c == 'L') { op1 = log10(op1); strcpy(top_line, "log10()"); }
                if (c == 'f') { 
                    // Simple factorial loop
                    long result = 1;
                    for(int i = 1; i <= (int)op1; i++) result *= i;
                    op1 = result;
                    strcpy(top_line, "fact()");
                }
                
                dtostrf(op1, 0, 4, input_buf); 
                input_idx = strlen(input_buf);
                current_op = 0; calc_done = 1; 
            }
            // --- EQUALS (=) ---
            else if (c == '=') {
                if (input_idx > 0) op2 = atof(input_buf);
                
                if (current_op == '+') op1 += op2;
                else if (current_op == '-') op1 -= op2;
                else if (current_op == '*') op1 *= op2;
                else if (current_op == '/') { if(op2 != 0) op1 /= op2; }
                else if (current_op == '^') op1 = pow(op1, op2);
                else if (current_op == 'm') op1 = fmod(op1, op2);
                
                dtostrf(op1, 0, 4, input_buf);
                input_idx = strlen(input_buf);
                current_op = 0; calc_done = 1; 
            }

            // --- DISPLAY UPDATE ---
            lcd_print(top_line[0] == '\0' ? " " : top_line, 0);
            lcd_print(input_buf[0] == '\0' ? "0" : input_buf, 1);
        }
        _delay_ms(20); 
    }
}
