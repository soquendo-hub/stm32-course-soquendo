#include <stdint.h>
#include "stm32f411xe.h"
#include "stm32f4xx.h"
// Headers definitions
void init_gpio(void);
void init_tim3(void);

int main(void)
{

    //CONFIGURAR EL HARDWARE ANTES DE LA FUNCIÓN PRINCIPAL
    init_gpio();    //CONFIGURANTO LOS PINES GPIO

    init_tim3();    //CARGAR LA CONFG DEL TIM3
    
    /*Loop forever */
    while (1) {
    
        
    }
}

/**/
void init_gpio(void){
    //Encender la señal de reloj para el GPIOA
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN_Msk;

    //Configurar el PIN A5 como salida
    GPIOA->MODER &= ~(0b11 << GPIO_PUPDR_PUPD5_Pos);    //LIMPIAR LA POSICIÓN
    GPIOA->MODER |= (0b01 << GPIO_MODER_MODER5_Pos);    //CONFIGURAR A5 COMO SALIDA

    GPIOA->OTYPER &= ~(GPIO_OTYPER_OT5);                //LIMPIAR Y CONFIGURAR COMO SALIDA

    GPIOA->OSPEEDR &= ~(0b11 << GPIO_OSPEEDR_OSPEED5_Pos);   //LIMPIAR POSICIÓN
    GPIOA->OSPEEDR |= (0b01 << GPIO_OSPEEDR_OSPEED5_Pos);    //CONG VEL MEDIA

    GPIOA->PUPDR &= ~(0b11 << GPIO_PUPDR_PUPD5_Pos);    //LIMPIAMOS, NOPULL-UP/DOWN

    GPIOA->ODR |= (GPIO_ODR_OD5);                        //ENCENDER LED

}

void init_tim3(void){ //GENERAR INTERRUPCIÓN CADA 0.25s
    //ENCENDER SEÑAL DE RELOJ
    RCC->APB1ENR |= RCC_APB1ENR_TIM3EN_Msk;

    //CONFG TIM3
    

    TIM3->CNT = 0;              //CONTADOR INICIA EN 0
    TIM3->PSC = (1600 - 1);     //INCREMENTO CADA 0.1us
    TIM3->ARR = (2500 -1);      //CONTAR HASTA 2500
    
    TIM3->CR1 &= ~(0b11 << TIM_CR1_CKD_Pos);    //LIMPIA, LA SEÑAL DE 16MH PASA SIN CAMBIOS AL PSC
    TIM3->CR1 |= (TIM_CR1_ARPE);    //ACTIVAR LA PRECARGA ;D
    TIM3->CR1 &= ~(TIM_CR1_DIR);    //CONFG CONTEO ASCENDENTE

    TIM3->DIER |= (TIM_DIER_UIE);   //ACTIVAR INTERRUPCIÓN POR EVENTO DE ACTUALIZACIÓN

    NVIC_EnableIRQ(TIM3_IRQn);  //REGISTRAR LA INTERRUPCIÓN TIM3 EN EL NVIC

    TIM3->CR1 |= TIM_CR1_CEN;       //ENCENDER EL CONTADOR


    //ACTIVAR EL CONTADOR
}


/*
*FUNCIÓN QUE "ADMINISTRA" LA INTERRUPCIÓN DEL TIM3
*/
void TIM3_IRQHandler(void){
    //VERIFICAR QUE LA BADNERA ESTÉ EN ALTO
    if (TIM3->SR && TIM_SR_UIF) {
        TIM3->SR &= ~TIM_SR_UIF;    //BAJANDO LA BANDERA

        //HACEMOS "ALGO" EN LA INTERRUPCIÓN
        GPIOA->ODR ^= (GPIO_ODR_OD5);    //HACEMOS UN TOOGLE EN LA POSICIÓN 5 DEL GPIOA
    }



}