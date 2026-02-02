## ¿Qué es? 
Una animación de fuego que habla directo con el servidor de pantalla usando el protocolo wayland. 

## Dependencias
Si estás en ubuntu alcanza con instalar:
```sudo apt install libwayland-dev libxkbcommon-dev```
## Modo de uso
Para compilar usar ```make```


Va a dejar un archivo "fueguito"


Para ejecutarlo (suponiendo que tu entorno usa wayland): "./fuegito"


Si estás bajo una sesión x11 entonces podes descargar weston (un compositor wayland) y usar eso para correr fueguito: 


```sudo apt install weston```
```weston -- ./fueguito```
