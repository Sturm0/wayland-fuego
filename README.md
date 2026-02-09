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

Para controlar la intensidad del fuego usar "+" y "-"

## Detalle técnico
Usa decoraciones del lado del servidor a través de xdg-decoration-unstable-v1, si tu compositor no soporta esa extensión entonces no vas a tener decoraciones. 
Al 9/2/2026  mutter (gnome), muffin , weston y gamescope no soportan xdg-decoration.
Por si no sabes a que me refiero con la "decoración" : https://en.wikipedia.org/wiki/Window_(computing)#Window_decoration
