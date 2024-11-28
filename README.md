# ChismeGPT
ChismeGPT es un sistema de manejo concurrente de usuarios basado en requerimientos y niveles de prioridad. Asegura la división de roles pospago y prepago, conexión y automatización de conexión de clientes.

## Componentes
1. ### core:
* Componente central responsable para manejar la data del usuario, prioridades y requerimientos.
* Procesa y maneja requerimientos.
```
./core <numero_mensajes>
```
## Instalación.
1. Clona el repositorio:
```
git clone https://github.com/se0klie/ChismeGPT.git
```
2. Navega hasta el directorio ChismeGPT en tu escritorio.
3. Compila el código.
```
make
```
## Uso.
1. Inicia el core con el número de mensajes recurrentes que desees observar.
```
./core <numero>
```
Ej: ./core 5
2. Inicia el programa mutli_client para simular el tráfico de clientes. Por default, se ingresarán 2 clientes con 1 mensaje cada uno.
```
./multi_client <num_clientes> <num_mensajes>
```
Ej: ./multi_client 5 5

