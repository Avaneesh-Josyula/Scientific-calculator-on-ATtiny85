#define F_CPU 16500000L
#include <avr/io.h>
#include <util/delay.h>

#define SDA PB0
#define SCL PB2
#define LCD_ADDR (0x27 << 1)

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
    uint8_t h = (data & 0xF0) | mode | 0x08; 
    uint8_t l = ((data << 4) & 0xF0) | mode | 0x08;
    i2c_start(); i2c_write(LCD_ADDR); i2c_write(h|0x04); i2c_stop(); 
    i2c_start(); i2c_write(LCD_ADDR); i2c_write(h); i2c_stop();
    i2c_start(); i2c_write(LCD_ADDR); i2c_write(l|0x04); i2c_stop(); 
    i2c_start(); i2c_write(LCD_ADDR); i2c_write(l); i2c_stop();
    _delay_us(50);
}
void lcd_print(const char* str) {
    lcd_send(0x80, 0); // Line 1
    while(*str) lcd_send(*str++, 1);
}

int main() {
    _delay_ms(1500); 
    lcd_send(0x33,0); lcd_send(0x32,0); lcd_send(0x28,0); lcd_send(0x0C,0); lcd_send(0x01,0); 
    _delay_ms(5);
    
    while(1) {
        lcd_send(0x01,0); _delay_ms(2); // Clear
        lcd_print("LCD Online");
        _delay_ms(1000);
        lcd_send(0x01,0); _delay_ms(2); // Clear (Blink effect)
        _delay_ms(500);
    }
}
