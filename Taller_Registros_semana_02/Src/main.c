 #include "stm32f411xe.h"
#include <stm32f4xx.h>
 #include <stdint.h>

 //Variable global para visualizar estado en depuración

volatile uint8_t estado_pulsador = 0;


int main(void)
{

    /*Activar señales de reloj*/
    RCC->AHB1ENR &= ~(0b1 << 0);    /*limpiar bit 0*/
    RCC->AHB1ENR &= ~(0b1 << 2);    /*limpiar bit 2*/
    RCC->AHB1ENR |= (0b1 << 0);     /*encender bit 0*/      
    RCC->AHB1ENR |= (0b1 << 2);     /*activar bit 2*/

        /**/
    GPIOA->MODER &= ~(0b11 << 5*2);
    GPIOA->MODER |= (0b01 << 5*2);

    GPIOC->MODER &= ~(0b11 << 13*2);
    
    GPIOA->OTYPER &= ~(0b1 << 5);

    GPIOA->OSPEEDR &= ~(0b11 << 5*2);

    GPIOA->PUPDR &= ~(0b11 << 5*2);
    GPIOC->PUPDR &= ~(0b11 << 13*2);

    GPIOA->ODR &= ~(0b1 << 5);
    GPIOA->ODR |= (0b1 << 5);



    
    
    while (1) {

        if (GPIOC->IDR >> 13 & (0b1)){
            GPIOA->ODR &= ~(0b1 << 5);
        }
        else {
        GPIOA->ODR |= (0b1 << 5);
        }
    
    }

    return 0;
}
