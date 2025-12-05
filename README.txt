README

------------------------------------------------------------
INSTALACIÓN Y USO DEL SOFTWARE – PROYECTO OPENGL
------------------------------------------------------------

1. REQUISITOS DEL SISTEMA
------------------------------------------------------------
- Windows 10 o superior
- Soporte para OpenGL 3.x o superior
- Procesador de 64 bits
- 4 GB RAM mínimo (8 GB recomendado)
- Tarjeta gráfica con drivers actualizados
- Espacio disponible en disco

2. PAQUETERÍAS UTILIZADAS (YA INCLUIDAS)
------------------------------------------------------------
Este software fue construido con:

- OpenGL 3.x
- GLFW (ventana, teclado, mouse)
- GLEW o GLAD (funciones modernas de OpenGL)
- GLM (matemáticas 3D)
- stb_image (carga de imágenes)
- Assimp o Model Loader (si se usan modelos 3D)

No necesitas instalar nada adicional.  
Todo viene integrado en el ejecutable y sus carpetas.

3. INSTALACIÓN DEL SOFTWARE (.EXE)
------------------------------------------------------------
1. Ejecutar el instalador (creado con InstallForge).
2. Presionar “Siguiente” hasta finalizar.
3. Seleccionar la carpeta de instalación.
4. Verificar que dentro de la carpeta final existan:
   - ProyectoOpenGL.exe
   - Carpeta assets/ con:
       * models/
       * textures/
       * shaders/

IMPORTANTE:
El ejecutable NO funcionará si “assets” no está junto a él.

4. EJECUCIÓN DEL PROGRAMA
------------------------------------------------------------
1. Abrir la carpeta de instalación.
2. Ejecutar:
   ProyectoOpenGL.exe

5. CONTROLES DEL PROGRAMA
------------------------------------------------------------
Movimiento:
  W → Adelante
  S → Atrás
  A → Izquierda
  D → Derecha

Cámara:
  Mouse → Rotar vista

Otros:
  ESC → Cerrar el programa

6. POSIBLES ERRORES Y SOLUCIONES
------------------------------------------------------------
Pantalla negra:
- Faltan shaders/texturas/modelos.
- La carpeta assets no está en la ubicación correcta.
- El equipo no soporta OpenGL 3.x o superior.

El programa no abre:
- Ejecutar como administrador.
- Reinstalar el programa.

Texturas o modelos no cargan:
- La carpeta assets está incompleta.
- No abrir el .exe desde otra ruta.

7. INFORMACIÓN TÉCNICA
------------------------------------------------------------
Software desarrollado en C++ usando OpenGL.
Incluye renderizado 3D, animaciones y modelos externos.
