/* core/common/system.h */
#ifndef SYSTEM_H
#define SYSTEM_H

/* configura el reloj del sistema y los divisores de los buses */
void SystemClock_Config(void);
/* detiene interrupciones y se queda en un bucle si falla una inicialización */
void Error_Handler(void);

#endif
