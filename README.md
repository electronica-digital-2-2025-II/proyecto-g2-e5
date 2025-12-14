[![Review Assignment Due Date](https://classroom.github.com/assets/deadline-readme-button-22041afd0340ce965d47ae6ef1cefeee28c7c493a6346c4f15d667ab976d596c.svg)](https://classroom.github.com/a/eiNgq3fR)
[![Open in Visual Studio Code](https://classroom.github.com/assets/open-in-vscode-2e0aaae1b6195c2367325f4f02e2d04e9abb55f0b24a779b69b11b9e10269abc.svg)](https://classroom.github.com/online_ide?assignment_repo_id=22046836&assignment_repo_type=AssignmentRepo)
# VitalSense
<!-- Cambiar el titulo "Proyecto" por el nombre del proyecto -->

# Integrantes

* [Julián Andrés Mancipe Muñoz](https://github.com/JuliTO65)
* [David Santiago Díaz Rivera](https://github.com/Davidx025)
* [María José Morales Villacres](https://github.com/MariaJoseM0)

<!-- Califico el informe unicamente a los integrantes que esten referenciados aqui y en el informe (PDF) -->

Indice:

1. [Descripción](#descripción)
2. [Informe](#informe)
3. [Implementación](#implementacion)
4. [Lista de anexos](#anexos)

## Descripción

<!-- Descripción general y lo mas completa posible del proyecto" -->


**VitalSense** es un sistema de **monitoreo biomédico portátil y de bajo costo** desarrollado como proyecto final de la asignatura **Electrónica Digital II** en la **Universidad Nacional de Colombia – Sede Bogotá**. El proyecto integra electrónica digital, sistemas embebidos y una aplicación móvil para adquirir, procesar y visualizar en tiempo real variables fisiológicas básicas, utilizando una arquitectura basada en **SoC FPGA**.

El objetivo principal de VitalSense es ofrecer una alternativa académica y tecnológica frente a los dispositivos *wearables* comerciales, cuyo alto costo limita su acceso, especialmente en contextos donde el monitoreo preventivo de la salud resulta crítico.

---

## 📌 Motivación

En Colombia, las enfermedades cardiovasculares representan una de las principales causas de mortalidad. A pesar del crecimiento del mercado de dispositivos de monitoreo de salud, su penetración sigue siendo baja debido a factores económicos y de accesibilidad. VitalSense surge como una respuesta a esta problemática, proponiendo un **prototipo funcional, abierto y de bajo costo**, enfocado en el monitoreo preventivo y el autocuidado.

---

## 🎯 Objetivo del proyecto

Desarrollar un módulo de monitoreo biomédico portátil que permita:

* Medir **frecuencia cardíaca (BPM)** y **saturación de oxígeno (SpO₂)** mediante fotopletismografía.
* Medir **temperatura corporal sin contacto** mediante sensores infrarrojos.
* Procesar las señales en tiempo real usando una plataforma **FPGA + ARM**.
* Transmitir los datos a un **PC** y a una **aplicación móvil vía Bluetooth**.
* Generar **alertas locales** ante valores fisiológicos fuera de rangos seguros.

---

## 🧠 Descripción general del sistema

VitalSense está compuesto por tres grandes capas:

1. **Hardware embebido (FPGA + sensores)**
2. **Software de procesamiento y control**
3. **Aplicación móvil para visualización**

La arquitectura fue diseñada para ser modular, escalable y orientada a la experimentación académica.

---

## 🧩 Arquitectura de hardware

### 🔹 Plataforma de procesamiento

* **Zybo Z7 (Zynq-7000 SoC)**

  * **Processing System (PS – ARM)**: adquisición de datos, procesamiento digital de señales y control general.
  * **Programmable Logic (PL – FPGA)**: control de periféricos, buzzer y UART dedicada para Bluetooth.

### 🔹 Sensores biomédicos

* **MAX30102**

  * Medición de frecuencia cardíaca y SpO₂.
  * Comunicación mediante **I2C**.
* **MLX90614 (GY-906)**

  * Medición de temperatura corporal sin contacto.
  * Comunicación **I2C / SMBus**.

### 🔹 Comunicación

* **I2C**: adquisición de datos desde sensores.
* **UART (PS)**: envío de información detallada a un PC (depuración y monitoreo).
* **UART Lite (PL) + Bluetooth HC-05**: transmisión inalámbrica a la aplicación móvil.

### 🔹 Actuadores

* **Buzzer** controlado desde la FPGA, activado cuando el BPM supera un umbral de seguridad.

---

## 💻 Software embebido

El software del sistema fue desarrollado en **C**, utilizando el **BSP de Xilinx**, e incluye:

* Inicialización y configuración de sensores mediante escritura directa de registros I2C.
* Implementación de drivers para **I2C** y **UART**.
* Algoritmos de **procesamiento digital de señales (DSP)** para:

  * Eliminación de componente DC.
  * Detección de picos cardíacos.
  * Cálculo de BPM y estimación de SpO₂.
* Promediado móvil para reducir ruido y mejorar estabilidad.
* Gestión de sesiones de medición y control por botones físicos.
* Protocolos de transmisión diferenciados para PC y Bluetooth.

---

## 📱 Aplicación móvil

La aplicación fue desarrollada en **MIT App Inventor** y permite:

* Visualizar en tiempo real BPM, SpO₂ y temperatura corporal.
* Confirmar el estado de la conexión Bluetooth.
* Mostrar alertas visuales ante valores anómalos.
* Facilitar el uso del sistema sin requerir conocimientos técnicos avanzados.

---

## 🚀 Estado del proyecto

* ✅ Prototipo funcional completamente integrado.
* ✅ Comunicación estable por I2C, UART y Bluetooth.
* ✅ Procesamiento en tiempo real de señales biomédicas.
* ⚠️ Proyecto **académico** (no es un dispositivo médico certificado).

---

## 🔮 Posibles mejoras

* Almacenamiento histórico de datos en la aplicación.
* Calibración automática y algoritmos de filtrado más avanzados.
* Integración de nuevos sensores (presión arterial, actividad física, ECG).
* Procesamiento avanzado en PC (Python / MATLAB).
* Sistema de alertas basado en tendencias y no solo en umbrales.

---


## Informe

### [Informe Final - Proyecto VitalSense](Informe/Informe_Final_Digital_II.pdf) 
<!-- Link que permita acceder al Informe, el cual debe estar subido a este repositorio -->

## Implementación

### Video explicativo del funcionamiento del proyecto 
 https://youtu.be/zWyNAepTANg?si=OjQUQD8bflupDY8l
<!-- Video explicativo del funcionamiento del proytecto -->


### Codigo main del proyecto
<!-- CREAR UN DIRECTORIO CON EL NOMBRE "src" DONDE INVLUYAN LAS FUENTE (.c Y .h) QUE CREARON PARA EL PROOYECTO-->
### [Codigo-main](src/main.c) 

### Diagrama de Bloques del ps
<!-- NO OLVIDAD SUBIR EL PDF GENERADOR EN DEL BLOCK DESIGN-->
### [BLOCK DESING](src/Diagrama_Bloques.pdf) 
