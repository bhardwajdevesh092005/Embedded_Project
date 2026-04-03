extern "C" {
    #include <stdint.h>
    #include <libopencm3/stm32/rcc.h>
    #include <libopencm3/stm32/gpio.h>
    #include "usart/usart.h" 
    #include "sdram/sdram.h" 
    #include "gfx.h"
    extern void clock_setup(void); 
    extern void lcd_spi_init(void);
    extern void lcd_draw_pixel(int x, int y, uint16_t color);
    extern void lcd_show_frame(void); // This pushes SDRAM to the Screen
}

// Linker fixes for USART logging
extern "C" int _write(int file, char *ptr, int len) {
    (void)file;
    for (int i = 0; i < len; i++) usart_send_blocking(USART1, ptr[i]);
    return len;
}


#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "model_data.h" 

#define SDRAM_BASE 0xD0000000
#define IMG_MAX 9216 
uint8_t* image_buffer = (uint8_t*)SDRAM_BASE;

// 🔥 SAFETY FIX: Move Arena to 1MB offset to avoid LCD Buffer collision
constexpr int kTensorArenaSize = 150 * 1024;
uint8_t* tensor_arena = (uint8_t*)(SDRAM_BASE + 0x100000); 

uint16_t gray_to_rgb565(uint8_t gray) {
    uint8_t r = (gray >> 3) & 0x1F;
    uint8_t g = (gray >> 2) & 0x3F;
    uint8_t b = (gray >> 3) & 0x1F;
    return (r << 11) | (g << 5) | b;
}

// ... [Includes and externs remain the same] ...

int main(void) {
    clock_setup(); 
    usart_clock_setup();
    usart_setup();
    
    // --- BREADCRUMB 1: GPIO INIT ---
    gpio_setup();
    gpio_set(GPIOG, GPIO13); // LED ON
    for(int i=0; i<2000000; i++) __asm__("nop");
    gpio_clear(GPIOG, GPIO13); // LED OFF (If you see this, GPIO is OK)

    sdram_init(); 
    lcd_spi_init(); 
    gfx_init(lcd_draw_pixel, GFX_WIDTH, GFX_HEIGHT);
    
    // --- BREADCRUMB 2: AI INIT ---
    const tflite::Model* model = tflite::GetModel(person_detect_tflite); 
    
    tflite::MicroMutableOpResolver<6> resolver;
    resolver.AddAveragePool2D(); resolver.AddConv2D();
    resolver.AddDepthwiseConv2D(); resolver.AddReshape(); resolver.AddSoftmax();

    // --- BREADCRUMB 3: ARENA ALLOCATION ---
    // If the board crashes here, your SDRAM base address is wrong
    tflite::MicroInterpreter interpreter(model, resolver, tensor_arena, kTensorArenaSize);
    
    gpio_set(GPIOG, GPIO13); // LED ON AGAIN
    if (interpreter.AllocateTensors() != kTfLiteOk) {
        usart_send_string("ALLOC_FAIL\r\n");
        while(1); 
    }
    gpio_clear(GPIOG, GPIO13); // LED OFF (If you see this, AI is READY)

    usart_send_string("READY_TO_RECEIVE\r\n");
    
    TfLiteTensor* input = interpreter.input(0);
    TfLiteTensor* output = interpreter.output(0); 
    int x_offset = (GFX_WIDTH - 96) / 2;
    int y_offset = (GFX_HEIGHT - 96) / 2;
    
    while (1) {
        // 1. Hunt for 0xAA
        if ((uint8_t)usart_read_char() != 0xAA) continue;
        
        // 2. Found 0xAA! Now look for 0x55
        if ((uint8_t)usart_read_char() != 0x55) continue;

        // 3. Header confirmed. Read Size (2 bytes)
        uint8_t size_low = usart_read_char();
        uint8_t size_high = usart_read_char();
        uint16_t expected_size = (size_high << 8) | size_low;

        if (expected_size != 9216) continue; // Safety check

        // 4. Read Pixels
        for (uint16_t i = 0; i < expected_size; i++) {
            image_buffer[i] = usart_read_char();
        }

        // 5. Process (LED ON)
        gpio_set(GPIOG, GPIO13);
        // ... (Drawing and AI Invoke logic) ...
        interpreter.Invoke();

        // 6. Display and Response
        lcd_show_frame(); 

        // Send Result (Only 3 bytes: 0xBB 0x66 Result)
        usart_send_blocking(USART1, 0xBB);
        usart_send_blocking(USART1, 0x66);
        usart_send_blocking(USART1, (output->data.int8[1] > output->data.int8[0]) ? 1 : 0);

        gpio_clear(GPIOG, GPIO13);
    }
}