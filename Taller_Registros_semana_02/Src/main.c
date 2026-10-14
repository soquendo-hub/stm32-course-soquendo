 #include "stm32f411xe.h"
#include <stm32f4xx.h>
 #include <stdint.h>

 //Variable global para visualizar estado en depuración

volatile uint8_t boton = 0;


int main(void)
{
    /*LED2 -> PA5  -> GPIOA -> 5*/
    /*USER -> PC13 -> GPIOC -> 13*/

    /*Activar señales de reloj*/

    /*
    En el registro RCC_AHB1ENR:
    GPIOA -> bit 0
    GPIOC -> bit 2
    */

    RCC->AHB1ENR &= ~(0b1 << 0);    /*limpiar bit 0*/
    RCC->AHB1ENR &= ~(0b1 << 2);    /*limpiar bit 2*/
    RCC->AHB1ENR |= (0b1 << 0);     /*encender la señal de reloj para el GPIOA*/      
    RCC->AHB1ENR |= (0b1 << 2);     /*encender la señal de reloj para el GPIOC*/

    /*GPIO REGISTERS*/
    /*CONFIGURAR PA5 COMO SALIDA*/
    
    GPIOA->MODER &= ~(0b11 << 5*2);     /*LIMPIAR MODER5*/
    GPIOA->MODER |= (0b01 << 5*2);      /*MODO SALIDA|01: General purpose output mode*/

    /*CONFIGURAR PC13 COMO ENTRADA*/
    GPIOC->MODER &= ~(0b11 << 13*2);    /*MODO ENTRADA|00: Input (reset state)*/
    
    /*CONFIGURAR TIPO DE SALIDA PARA EL PA5*/
    GPIOA->OTYPER &= ~(0b1 << 5);       /*0: Output push-pull (reset state)*/

    /*CONFIGURAR VELOCIDAD DE SALIDA PARA EL PA5*/
    GPIOA->OSPEEDR &= ~(0b11 << 5*2);   /*00: Low speed*/

    /*CONFIGURACIÓN PULL-UP/PULL-DOWN*/
    /*
    *EN EL DIAGRAMA DE REFERENCIA DEL NUCLEOF411 LOS PERIFERICOS
    *LD2 Y USER YA TRAEN UNA RESISTENCIA
    *Sin pull interna (ya tiene pull-up externa)
    */
    GPIOA->PUPDR &= ~(0b11 << 5*2);     /*00: No pull-up, pull-down*/
    GPIOC->PUPDR &= ~(0b11 << 13*2);    /*00: No pull-up, pull-down*/

    /*CONFIGURAR PUERTO DE SALIDA (ENCENDER(1) O APAGAR(0))*/
    GPIOA->ODR &= ~(0b1 << 5);      /*LIMPIAR*/
    GPIOA->ODR |= (0b1 << 5);       /*1:ENCENDER*/

    
    while (1) {

        if (GPIOC->IDR >> 13 & (0b1)){      /*COMPARAR EL ESTADO DEL PUERTO DE ENTRADA*/
            /*Si USER no está pulsado (1) entonces:*/
            GPIOA->ODR &= ~(0b1 << 5);      /*APAGAR PUERTO DE SALIDA|0:APAGAR*/
            boton = 0;
        }
        else {  
            /*Si está pulsado (0)*/
        GPIOA->ODR |= (0b1 << 5);       /*1:ENCENDER*/
        boton = 1;
        }
    
    }

    return 0;
}
