# AGENTS.md — Reglas de trabajo del proyecto

## 1. Contexto del proyecto

El proyecto ubicado en la carpeta `Compiler` es un compilador en desarrollo enfocado en el lenguaje C++ y elaborado en ese mismo lenguaje. El equipo encargado de esto no es necesariamente experto en C++ ni en el tema de compiladores, por lo tanto **el código encontrado no debe tomarse como correcto, óptimo y/o seguro para producción**.

Estas instrucciones se aplican de forma literal durante la fase de análisis del proyecto.

## 2. Distinción de comportamientos

Toda afirmación sobre el sistema debe distinguir explícitamente entre:

| Tipo | Significado |
|---|---|
| **Comportamiento esperado** | Lo que la especificación, el requisito o la intención declara que debería ocurrir. |
| **Comportamiento documentado** | Lo que la documentación (`doc.md`, comentarios, mensajes de error, nombres) afirma que ocurre. |
| **Comportamiento observado** | Lo que el código realmente hace al ejecutarse o al inspeccionarse línea por línea. |

Ninguno de los tres sustituye a los otros: no se debe asumir que lo documentado es lo observado, ni que lo esperado es lo implementado.

## 3. Reglas obligatorias

1. **No adoptar patrones existentes de forma automática.** No se deben convertir ni tomar de forma literal u obligatoria los patrones de diseño y la estructura de código presentes en el proyecto para la implementación de código nuevo.

2. **Identificar defectos, errores de lógica y de diseño.** Deben quedar registrados para tenerlos en cuenta en implementaciones futuras. Dichos errores deben ser **únicamente leídos y tomados en cuenta para evitar cometer nuevos errores**; no se corrigen.

3. **El código existente jamás debe ser modificado y/o alterado para su corrección.** Esta prohibición no admite excepciones por conveniencia, salvo la autorización explícita registrada en la sección 7: únicamente archivos CMake, únicamente para establecer conexiones entre fases; y el archivo llamdo main.cpp con el unico proposito de implemntar nuevas funciones. Los archivos `*.cpp` y `*.hpp` con el nombre de Lexer, Regex, Tokens, Parser y AST  no se tocan jamás.

4. **Las nuevas implementaciones deben mitigar errores arrastrados desde fases anteriores**, pero siempre evitando a toda costa la sobreingeniería y el desorden de código, y sin mezclar las fases ya existentes con las nuevas.

5. **Jamás se debe ampliar el alcance** para modificar el código ya existente como medio para implementar código nuevo.

6. **Organización de archivos:** los archivos e implementaciones deben organizarse en las carpetas señaladas explícitamente por el programador. Dichas carpetas siempre deben incluir un archivo CMake para facilitar la comunicación entre las fases anteriores y las nuevas.

7. **Prohibido suponer.** No se deben establecer variantes, funciones ni bloques enteros de código solamente basándose en suposiciones sobre lo que el código existente puede hacer. Siempre se debe verificar su funcionalidad y sus posibles riesgos para evitar crear nuevos estragos y errores.

8. **Prohibido escribir código sin verificarlo antes.**

9. **Toda nueva implementación debe ser testeada** para garantizar su correcto funcionamiento.

10. **Ante funciones confusas o contradictorias entre sí**, se deben evaluar todos los posibles caminos y elegir o crear una única solución acorde al problema, tomando en cuenta los posibles estragos que puedan afectar el correcto funcionamiento de la implementación.

11. **Preservar siempre la compatibilidad correcta entre archivos**, pero jamás confundir compatibilidad con corrección.

12. **Evitar a toda costa codigo dificil de leer**. El codigo escrito siempre debe de seguir los siguientes lineamientos: Sintaxis facil de leer, facilitando la identificacion de la logica de cada bloque. Funciones complejas del lenguajes explicadas con comentarios claros y breves. Jamas implementar caracteristicas complejas del lenguaje C++ si no son explicitamente necesarias. Ante todo mantener la facilidad de lectura, pero jamas poniendo por denajo la calidad, seguridad y robustez de codigo.

## 4. Clasificación de hallazgos

Todo hallazgo debe clasificarse en una, y solo una, de las siguientes categorías:

| Categoría | Criterio |
|---|---|
| **Confirmado** | Respaldado por documentación y pruebas contundentes. |
| **Probable** | Existencia de claros indicios de error, pero aún falta verificación. |
| **Desconocido** | No hay información suficiente para respaldar cualquier afirmación. |
| **Decisión del equipo** | Requiere una elección del equipo de desarrollo. |
| **Fuera de alcance** | No afecta de forma directa al objetivo actual, y jamás debe abordarse sin una autorización explícita. |

Un hallazgo no puede presentarse como hecho sin indicar su categoría y la evidencia que lo respalda.

## 5. Verificación antes de implementar

Antes de escribir cualquier código nuevo, el agente debe poder responder, con evidencia del código y no con suposiciones:

- ¿Qué hace hoy el código relacionado, según inspección directa?
- ¿Qué dice la documentación y en qué difiere de lo observado?
- ¿Qué defectos existentes pueden afectar la nueva implementación y cómo se mitigan sin modificar el código existente?
- ¿Qué carpeta asignó explícitamente el programador y tiene su `CMakeLists.txt`?
- ¿Qué prueba demuestra que la nueva implementación funciona?

Si alguna respuesta no puede sustentarse, el trabajo se detiene y se reporta como **Desconocido** o **Probable**, según corresponda, en lugar de implementar sobre una suposición.

## 6. Referencia

`doc.md` contiene la verificación de la fase de análisis semántico y los hallazgos `V-01` a `V-12`. Debe usarse como referencia para no releer el proyecto entero, pero **no como verdad absoluta**: todo lo allí afirmado sigue sujeto a las reglas de este documento y a su clasificación correspondiente.

`doc.md` es de **solo lectura**: no debe actualizarse, reescribirse ni corregirse. Su única función es servir de apoyo para evitar releer partes completas del proyecto con hallazgos ya confirmados.

## 7. Autorizaciones explícitas vigentes

Esta sección prevalece sobre cualquier prohibición general de las secciones anteriores que la contradiga, y solo en el alcance aquí indicado:

| Alcance | Estado | Límite exacto |
|---|---|---|
| Archivos CMake existentes (`CMakeLists.txt`, `*.cmake`) | **Autorizado** | Solo para establecer conexiones entre fases (añadir fuentes, bibliotecas, rutas de inclusión y enlaces). Nunca para cambiar lógica, opciones de compilación ni estructura. |
| Archivos de código existentes (`*.cpp`, `*.hpp`), salvo `app/main.cpp` | **Prohibido** | No se tocan, ni para corregir, ni para "conectar", ni para adaptar a lo nuevo. |
| `app/main.cpp` | **Autorizado (alcance limitado)** | Únicamente para invocar la fase de análisis semántico y devolver el código de salida de la compuerta (`3`). Sin cambios en el resto de la orquestación ni en las fases existentes. Registrado el 2026-10-29. |
| `doc.md` | **Solo lectura** | No se actualiza (sección 6). |
| Cualquier otro cambio sobre lo existente | **Requiere autorización nueva** | Debe pedirse y registrarse aquí antes de ejecutarlo. |

Toda autorización no listada en esta tabla se considera **no concedida**. Si una tarea parece necesitarla, el trabajo se detiene y se solicita autorización explícita.
