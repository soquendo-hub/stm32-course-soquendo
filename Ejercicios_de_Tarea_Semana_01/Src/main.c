#include <stdint.h>

int main(void)
{
    
    /*
    Ejercicio 1.1 - 
    Operadores aritmeticos y division entera
    */
    
    uint8_t a = 17;                 /*a toma el valor 17*/
    uint8_t b = 5;                  /*b toma el valor 5*/
    uint8_t div_result = a / b;     /*Realiza division 
    entera, lo que significa que la parte decimal 
    simplemente se descarta: div_result deberia tomar
    el valor 3 */
    uint8_t mod_result = a % b;     /*mod_result toma el
    valor 2 que es lo que le falataria a 15 para llegar a 17*/
    uint8_t mul_result = a * b;     /*toma el valor 85*/

    /*Ejercicio 1.2 - Desbordamiento en aritmetica*/

    uint8_t x = 200;        
    uint8_t y = 100;
    uint8_t sum = x +y;

    /*recordando que el contenedor toma valores de 0 a 255
    almacenar en una variable 200 + 100 = 300 se desborda.
    yo lo veo de la siguiente manera, el 256 pasa a ser cero,
    entonces 300-256=44, entoces toma el valor 44*/
    
    /*Ejercicio 1.3 - Operadores de desplazamiento como
    multiplicacion y division*/

    uint8_t val = 3;

    uint8_t left1  = val << 1;      /*toma el valor 6*/
    uint8_t left2  = val << 2;      /*toma el valor 12*/
    uint8_t left3  = val << 3;      /*toma el valor 24*/
    uint8_t right1 = val >> 1;      /*toma el valor 1*/
    /*el desplazamiento a la izquierda es como multiplicar
    por 2 y a la derecha es dividir entre 2*/
    
    /*Ejercicio 1.4 - Evaluacion booleana e if/else */
    uint8_t v_false = 0;
    uint8_t v_true  = 10;
    uint8_t v_comp  = 55;
    uint8_t result  = 0;

    if (v_true) {
        result = 1;
    }

    if (v_false) {
        result = 1;
    } else {
        result = 0;
    }

    if (v_comp == v_true) {
        result = 1;
    } else {
        result = 0;
    }

    /*C considera falso lo que sea 0 y verdadero lo que sea
    diferente de cero*/


    /*Ejercicio 1.5 - El bucle for como contador*/

    uint8_t counter_1 = 0;
    uint8_t counter_2 = 0;
    uint8_t counter_3 = 0;
    uint8_t counter_4 = 0;


    for (uint8_t i_1 = 0; i_1 <= 9; i_1++) 
    /*i_1 toma los valores 0, 1, 2, 3, 4, 5, 6, 7, 8 y 9 */
    {
        counter_1++;
        /*counter_1++ se ejecuta 10 veces, una vez por cada
        valor que toma i_1, al final counter_1 vale 10*/
    }

    for (uint8_t i_2 = 0; i_2 <= 9; i_2 += 2)
    /*i_2 toma los valores 0, 2, 4, 6 y 8*/
    {
        counter_2++;
        /*i_2 toma 5 valores y por cada valor que toma i_2
        se le suma una unidad a counter_2, finalmente toma
        el valor 5*/
    }

    for (uint8_t i_3 = 10; i_3 >= 1; i_3--)
    /*i_3 toma los valores 10, 9, 8, 7, 6, 5, 4, 3, 2 y 1*/
    {
        counter_3++;
        /*al final counter_3 vale 10*/
    }

    for (uint8_t i_4 = 0; i_4 <= 15; i_4 += 3)
    /*i_4 toma los siguientes valores:
    0, 3, 6, 9, 12, 15, . toma 6 valores*/
    {
        counter_4++;
        /*al final counter_4 vale 6*/
    }

    /*Ejercicio 1.6 - El bucle while y un acumulador simple*/

    uint8_t  sum_1 = 0;
    uint16_t sum_2 = 0;
    uint8_t  sum_3 = 0;

    uint8_t  i_5 = 1;
    uint16_t i_6 = 1;
    uint8_t  i_7 = 1;

    while (i_5 <= 10)
    /*mientras que i_5 sea menor o igual que 10 */
    {
        sum_1 += i_5;
        /*a sum_1 se le suma el valor que tiene i_5*/
        i_5++;
        /*luego el valor de i_5 se incremente en una unidad*/
    } 
    /*es decir que i_5 va a tomar los valores de uno hasta
    10 uno a uno y en cada ciclo se suma cada uno de esos 
    valores en sum_1*/
    /*sum_1 al final vale (10*(10+1))/2 = 55*/

    while (i_6 <= 100)
    {
        sum_2 += i_6;

        i_6++;
    }
    /*la suma deberia dar 5050 y no se desborda porque 
    el rango de uint16_t es de 0 a 65535*/

    while (i_7 <= 100)
    {
        sum_3 += i_7;

        i_7++;
    }
    /*como uint8_t solo puede contener 256 valores
    hay un desbordamiento, si se hace la division entera de 
    5050 entre 256 nos da 19 y quedarian faltando 186.
    lo que es lo mismo que 5050 mod 256 = 186. ese es el 
    valor que tomaria suma por el desbordamiento*/

    /*Ejercicio 1.7 - El bucle do-while*/

   uint8_t resultado_1 = 0;
   uint8_t resultado_2 = 0;

    while (0)
    {
        resultado_1 = 42;
    }

    do 
    {
        resultado_2 = 42;
    }

    while (0);

    /*resultado_1 permanece en 0 porque el la condicion
    del while es falsa, luego aunque la condicion del 
    do-while tambien es falsa, se ejecuta primero el codigo
    asignandole el valor 42 a resultado_2, con el do-while 
    puedo garantizar que se ejecute almenos una vez aunque 
    la condicion sea falsa*/

    /*Ejercicio 1.8 - El switch-case como arbol de decisiones*/

    uint8_t input_1 = 2;
    uint8_t input_2 = 3;

    uint8_t output_1 = 0;
    uint8_t output_2 = 0;

    switch (input_1)
    {
        case 1:
            output_1 = 10;
            break;
        case 2:     /*aqui coincide el valor*/
            output_1 = 20;
            break;
        case 3:
            output_1 = 30;
            break;
        case 4:
            output_1 = 40;
            break;
        default:
            output_1 = 0;
            break;
    }
    /* entiendo entonces que whitch compara input_1 con el
    valor de cada caso, si coincide se ejecuta el codigo:
    en este caso se le asigna el valor 20 a output_1 y se
    ejecuta break para salir del switch*/

    switch (input_2)
    {
        case 1:
            output_2= 10;
            break;
        case 2:
            output_2= 20;
            break;
        case 3:
            output_2= 30;      /*aqui coincide el valor*/
            /*se elimino el break*/
        case 4:
            output_2= 40;
            break;
        default:
            output_2= 0;
            break;
    }

    /*en un primer momento esperaria que al coincidir para 
    el caso 3, output tomara el valor 30 y como no hay break
    el switch seguiria hasta default, pero eso no ocurre.
    se ejecuta como si el caso 4 coincidiera aunque no se 
    compare con este
    */

    /*Blucle infinito*/
    while (1){
        
    }
}


