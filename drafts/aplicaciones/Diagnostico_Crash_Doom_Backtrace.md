# 🔍 Diagnóstico y Solución: Crash / Reinicio Continuo en DOOM Cartridge (app1)

## 📌 1. Resumen Ejecutivo
El motor de DOOM se reinicia constantemente inmediatamente después de terminar `doomgeneric_Create` y entrar en el primer ciclo de `doomgeneric_Tick()` dentro de `loop()`.

- **Tipo de Excepción:** `Guru Meditation Error: Core 1 panic'ed (LoadProhibited). Exception was unhandled.`
- **Dirección Fallida:** `EXCVADDR: 0x00000000` (desreferencia de puntero nulo / memoria corrupta).
- **Causa Raíz:** **Puntero colgado en la pila (*Stack Use-After-Return* / *Dangling Pointer*)**.
  En `DoomLauncher.cpp`, el arreglo de argumentos de línea de comandos `doom_argv` fue declarado como una variable **local** dentro de `setup()`.
  El motor DOOM guarda la referencia global `myargv = argv`. Al finalizar `setup()` e iniciar `loop()`, el marco de pila (*stack frame*) de `setup()` es destruido y reutilizado, convirtiendo `myargv` en basura/NULL. Al ejecutar el primer frame del juego, DOOM consulta los parámetros con `M_CheckParm()` y colapsa al intentar leer `myargv[1]`.

---

## 🔬 2. Decodificación Técnica del Backtrace

Al decodificar la traza de pila (*stack backtrace*) reportada por el ESP32-S3 contra el binario `firmware.elf`:

```text
Backtrace decodificado:
[0] 0x400553c8: strcasecmp() en ROM libc
[1] 0x42011366: M_CheckParmWithArgs en lib/doomgeneric/m_argv.c:49
[2] 0x4201137d: M_CheckParm en lib/doomgeneric/m_argv.c:70
[3] 0x4200fc2f: G_DoPlayDemo en lib/doomgeneric/g_game.c:2200
[4] 0x42010090: G_Ticker en lib/doomgeneric/g_game.c:883
[5] 0x4200d430: RunTic en lib/doomgeneric/d_net.c:94
[6] 0x4200c4fb: TryRunTics en lib/doomgeneric/d_loop.c:811
[7] 0x4200c94e: doomgeneric_Tick en lib/doomgeneric/d_main.c:410
[8] 0x4202540d: loop() en src/DoomLauncher.cpp:210
```

### ¿Qué ocurre paso a paso?
1. En `DoomLauncher.cpp` (líneas 173-177):
   ```cpp
   // VARIABLE LOCAL EN EL STACK DE setup()
   char* doom_argv[] = {(char*)"doom", (char*)"-iwad", (char*)iwad_path, NULL};
   doomgeneric_Create(3, doom_argv);
   ```
2. En `lib/doomgeneric/doomgeneric.c`:
   ```c
   void doomgeneric_Create(int argc, char **argv) {
       myargc = argc;
       myargv = argv; // <── Guarda el puntero directo a la pila de setup()
       ...
   }
   ```
3. `setup()` termina su ejecución. FreeRTOS cede el control a `loop()`. El área de memoria de la pila donde residía `doom_argv` es sobreescrita por variables locales de `loop()`, `s_gamepad.read()` y `doomgeneric_Tick()`.
4. En `loop()`, se llama a `doomgeneric_Tick()`. DOOM arranca la demo inicial llamando a `G_DoPlayDemo()`.
5. `G_DoPlayDemo()` (línea 2200 de `g_game.c`) ejecuta:
   ```c
   if (playeringame[1] || M_CheckParm("-solo-net") > 0 || M_CheckParm("-netdemo") > 0)
   ```
6. `M_CheckParm()` llama a `M_CheckParmWithArgs()` (`m_argv.c:49`):
   ```c
   for (i = 1; i < myargc - num_args; i++) {
       if (!strcasecmp(check, myargv[i])) // <── myargv[1] contiene 0x00000000 (NULL)
           return i;
   }
   ```
7. `strcasecmp` intenta leer la dirección `0x00000000` (`EXCVADDR: 0x00000000`) y dispara el Kernel Panic `LoadProhibited`.

---

## 💡 3. Solución Propuesta (Sin modificar código todavía)

Para solucionar de raíz este problema, los argumentos pasados a DOOM deben tener **duración estática / global** (almacenados en la sección `.data` / `.bss` y no en la pila efímera).

### Cambio requerido en [DoomLauncher.cpp](file:///home/kaber420/Documentos/proyectos/espOS32/firmware/src/DoomLauncher.cpp):

Declarar `s_doom_argv` y las cadenas con alcance estático:

```cpp
// En DoomLauncher.cpp (alcance estático o global fuera de la pila)
static char s_iwad_path_buf[64];
static char* s_doom_argv[4];

// Dentro de setup():
if (iwad_path != NULL) {
    strncpy(s_iwad_path_buf, iwad_path, sizeof(s_iwad_path_buf) - 1);
    s_doom_argv[0] = (char*)"doom";
    s_doom_argv[1] = (char*)"-iwad";
    s_doom_argv[2] = s_iwad_path_buf;
    s_doom_argv[3] = NULL;
    doomgeneric_Create(3, s_doom_argv);
} else {
    s_doom_argv[0] = (char*)"doom";
    s_doom_argv[1] = NULL;
    doomgeneric_Create(1, s_doom_argv);
}
```

---

## 📋 4. Pasos a Seguir
1. Revisar este borrador.
2. Dar la autorización cuando desees que apliquemos la corrección en `DoomLauncher.cpp`, recompilemos con `pio run -e doom` y verifiquemos el arranque en la placa.
