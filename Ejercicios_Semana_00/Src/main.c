#include <stdint.h>

int main(void)
{
    /*
    Ejercicio 0.1 - Primer contacto con el depurador
    */
    uint8_t mi_variable = 42;

    /*
    Ejercicio 0.2 - El mismo número, diferentes presentaciones
    */
    uint8_t dec = 65; //65 decimal
    uint8_t hex = 0x41; // (4x16¹)+(1x16⁰)=64+1=65
    uint8_t bin = 0b01000001; // (1x2⁶)+(1x2⁰)=64+1=65

    /*
    Ejercicio 0.3 - ¿Qué tan grande es una variable?
    */
    uint8_t  a = 255;
    uint16_t b = 255;
    uint32_t c = 255;
    uint8_t  d = 256; //este debería irse a cero
    uint8_t  e = 257; //creo que esete debería irse a 1
    //cuando el valor que quiero almacenar es más grande de lo que el contenedor puede contener se "reinicia el contador"

    /*
    Ejercicio 0.4 - El signo importa
    */

    uint8_t a_2 = 200; //almacena valores enteros de o a 255, 200 está dentro del rango
    int8_t  b_2 = 200; //almacena valores enteros desde -128 hasta 127, 200 está fuera del rango 256(combinaciones)-200=-56, se le asigna -56
    int8_t  c_2 = -1;  //supongo que se almacena el -1

    /*
    Ejercicio 0.5 - Límites y desbordamiento
    */

    int8_t x = 127;
    x = x + 1; 
    //A la variable x se le asigna el valor que contenía más una unidad, 
    // //pero el valor se desborda entonces se le asigna -128?... creo

    uint8_t y = 255;
    y = y + 1; 
    //Aquí también se desborda y se le asigna 0

    /*
    Ejercicio 0.6 — Aritmética mental en hexadecimal 
    Decimal - Binario - Hexadecimal
        10      1010        A
        11      1011        B
        12      1100        C
        13      1101        D
        14      1110        E
        15      1111        F
    */

    uint8_t result = 0;
    result = 0x0F + 0X01;   // (15X16⁰)+(1X16⁰)=15+1=16
    result = 0xFF + 0X01;   // (15X16¹)+(15X16⁰)+(1X16⁰)=240+15+1=256
    result = 0xA0 + 0x5F;   // (10X16¹)+(5X16¹)+(15X16⁰)=160+80+15=255
    result = 0xA0 + 0x60;   // (10X16¹)+(6X16¹)=160+96=256 se desborda pasa a ser cero 0x00

    /*
    Ejercicio 0.7 . Construyendo un número bit a bit 
    */

    uint8_t x_1 = 0;

    x_1 = 0x01;   //1
    x_1 = 0x02;   //2
    x_1 = 0x04;   //4
    x_1 = 0x08;   //8
    x_1 = 0x10;   //16
    x_1 = 0x20;   //32
    x_1 = 0x40;   //64
    x_1 = 0x80;   //128

    /*
    Ejercicio 0.8 - El operador bang y la lógica booleana
    ! invierte la lógica de verdadero a falso y de falso a verdadero
    si x = 0 entonces es falso, si x es diferente de 0 es verdadero
    ~ not toma los 0 y los convierte en 1 ej: 00011101 --> 11100010 
    */

    uint8_t a_3 = 5;      //0b00000101
    uint8_t b_3 = 0;      //0b00000000
    uint8_t c_3 = 255;    //0b11111111
    uint8_t r1 = !a_3;    // como a es diferente a cero es verdadero, con ! pasa a ser falso es decir 0 0x00
    uint8_t r2 = !b_3;    // falso pasa a ser verdadero 1 0x01
    uint8_t r3 = !c_3;    // de nuevo 0 0x00
    uint8_t r4 = ~c_3;    //0b0000000 0 0x00
    uint8_t r5 = ~a_3;    //0b11111010 250 0xFA

    /* Bucle infinito */
	while (1){
        
    }
    return 0;
}
