#define _POSIX_C_SOURCE 200112L
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>
#include <wayland-client.h>
#include "xdg-shell-client-protocol.h"
#include <iostream>
#include <assert.h>
#include <xkbcommon/xkbcommon.h>
#include <cstdint>
#include <algorithm>

int intensidad_maxima_fuego = 36; // máximo indice en palette_xrgb8888
int ancho_fuego = 40;
int alto_fuego = 40;
int* arreglo_intensidades;
int decaimiento_máximo = 4; // modificando esto podes hacer que el fuego "decaiga" más rapido, osea, una llama más débil 


static const uint32_t palette_xrgb8888[] = {0x00070707, 0x001F0707, 0x002F0F07, 0x00470F07, 0x00571707
                                          , 0x00671F07, 0x00771F07, 0x008F2707, 0x009F2F07, 0x00AF3F07
                                          , 0x00BF4707, 0x00C74707, 0x00DF4F07, 0x00DF5707, 0x00DF5707
                                          , 0x00D75F07, 0x00D75F07, 0x00D7670F, 0x00CF6F0F, 0x00CF770F
                                          , 0x00CF7F0F, 0x00CF8717, 0x00C78717, 0x00C78F17, 0x00C7971F
                                          , 0x00BF9F1F, 0x00BF9F1F, 0x00BFA727, 0x00BFA727, 0x00BFAF2F
                                          , 0x00B7AF2F, 0x00B7B72F, 0x00B7B737, 0x00CFD06F, 0x00DFDF9F
                                          , 0x00EFEFC7, 0x00FFFFFF};

int* crear_estructura_de_datos_fuego() {
    return new int[ancho_fuego*alto_fuego]();
}
void crear_origen_fuego(int intensidad = intensidad_maxima_fuego) {
    for (int columna = 0; columna < ancho_fuego; columna++)
    {
        int ultimo_pixel = ancho_fuego*alto_fuego;
        arreglo_intensidades[ultimo_pixel-ancho_fuego + columna] = intensidad;
    }
}
void actualizar_intesidad_fuego_por_pixel(int indice_pixel_actual) {
    int indice_pixel_de_abajo = indice_pixel_actual + ancho_fuego;
    
    if (indice_pixel_de_abajo >= ancho_fuego*alto_fuego) {
        return;
    }

    int decaimiento = rand()%decaimiento_máximo;
    //esto hace que en vez de necesariamente modificar la intensidad que estoy "mirando" modifique una de los costados, lo cual da la sensación de que hay viento 
    int intensidad_a_modificar = indice_pixel_actual-decaimiento >= 0 && indice_pixel_actual-decaimiento < ancho_fuego*alto_fuego ? indice_pixel_actual-decaimiento : indice_pixel_actual;
    arreglo_intensidades[intensidad_a_modificar] = arreglo_intensidades[indice_pixel_de_abajo] - decaimiento >= 0 ? arreglo_intensidades[indice_pixel_de_abajo] - decaimiento : 0;
}


void calcular_propagación_fuego() {
    for (int columna = 0; columna < ancho_fuego; columna++)
    {
        for (int fila = alto_fuego-1; fila >= 0; fila--)
        {
            int indice_pixel = columna + ancho_fuego*fila;
            actualizar_intesidad_fuego_por_pixel(indice_pixel);
        }
    }
}

/* Código para memoria compartida */
static void
randname(char *buf)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    long r = ts.tv_nsec;
    for (int i = 0; i < 6; ++i) {
        buf[i] = 'A'+(r&15)+(r&16)*2;
        r >>= 5;
    }
}

static int
create_shm_file(void)
{
    int retries = 100;
    do {
        char name[] = "/wl_shm-XXXXXX";
        randname(name + sizeof(name) - 7);
        --retries;
        int fd = shm_open(name, O_RDWR | O_CREAT | O_EXCL, 0600);
        if (fd >= 0) {
            shm_unlink(name);
            return fd;
        }
    } while (retries > 0 && errno == EEXIST);
    return -1;
}

static int
allocate_shm_file(size_t size)
{
    int fd = create_shm_file();
    if (fd < 0)
        return -1;
    int ret;
    do {
        ret = ftruncate(fd, size);
    } while (ret < 0 && errno == EINTR);
    if (ret < 0) {
        close(fd);
        return -1;
    }
    return fd;
}

/* Wayland code */
struct client_state {
    /* Globals */
    struct wl_display *wl_display;
    struct wl_registry *wl_registry;
    struct wl_shm *wl_shm;
    struct wl_compositor *wl_compositor;
    struct xdg_wm_base *xdg_wm_base;
    struct wl_seat *wl_seat;
    /* Objetos */
    struct wl_surface *wl_surface;
    struct xdg_surface *xdg_surface;
    struct xdg_toplevel *xdg_toplevel;
    struct wl_keyboard *wl_keyboard;
    /* Estado */
    float offset;
    uint32_t last_frame;
    bool closed;
    struct xkb_state *xkb_state;
    struct xkb_context *xkb_context;
    struct xkb_keymap *xkb_keymap;
    uint32_t ultimo_frame_dibujado; // el ultimo frame en el que avancé la animación de fuego
    int width, height;
};

static void
xdg_toplevel_configure(void *data,
		struct xdg_toplevel *xdg_toplevel, int32_t width, int32_t height,
		struct wl_array *states)
{
	struct client_state *state = (struct client_state *)data;
	if (width == 0 || height == 0) {
		/* Compositor is deferring to us */
		return;
	}
	state->width = width;
	state->height = height;
}

static void
xdg_toplevel_close(void *data, struct xdg_toplevel *toplevel)
{
	struct client_state *state = (struct client_state *)data;
	state->closed = true;
}

static const struct xdg_toplevel_listener xdg_toplevel_listener = {
	.configure = xdg_toplevel_configure,
	.close = xdg_toplevel_close,
};

static void
wl_buffer_release(void *data, struct wl_buffer *wl_buffer)
{
    /* Lo manda el compositor cuando ya no esta usando este búfer */ 
    wl_buffer_destroy(wl_buffer);
}

static const struct wl_buffer_listener wl_buffer_listener = {
    .release = wl_buffer_release,
};

static struct wl_buffer* draw_frame(struct client_state *state)
{
    int width = state->width, height = state->height;
    int stride = width * 4;
    int size = stride * height;

    int fd = allocate_shm_file(size);
    if (fd == -1) {
        return NULL;
    }

    uint32_t* data = (uint32_t*)mmap(NULL, size,
            PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (data == MAP_FAILED) {
        close(fd);
        return NULL;
    }

    struct wl_shm_pool *pool = wl_shm_create_pool(state->wl_shm, fd, size);
    struct wl_buffer *buffer = wl_shm_pool_create_buffer(pool, 0,
            width, height, stride, WL_SHM_FORMAT_XRGB8888);
    wl_shm_pool_destroy(pool);
    close(fd);
    
    int minimo = std::min(width,height);
    int escala = std::max(minimo/40 , 1); 
    int offset = width > height ? (width-height)/2 : 0;
    
    // arreglo_intensidades tiene la intensidad del fuego para cada pixel 
    for (int y = 0; y < minimo; ++y) {
        for (int x = 0; x < minimo; ++x) 
        {
            int y_n =  std::clamp(y/escala,0,39);
            int x_n =  std::clamp(x/escala,0,39);
            data[y*width+x+offset] = palette_xrgb8888[arreglo_intensidades[y_n *ancho_fuego+  x_n]];
            
        }
        
    }
    munmap(data, size);
    wl_buffer_add_listener(buffer, &wl_buffer_listener, NULL);
    return buffer;
}

static void
xdg_surface_configure(void *data, struct xdg_surface *xdg_surface, uint32_t serial)
{
    struct client_state* state = (struct client_state*)data;
    xdg_surface_ack_configure(xdg_surface, serial);

    struct wl_buffer *buffer = draw_frame(state);
    wl_surface_attach(state->wl_surface, buffer, 0, 0);
    wl_surface_commit(state->wl_surface);
}

static const struct xdg_surface_listener xdg_surface_listener = {
    .configure = xdg_surface_configure,
};

static void
xdg_wm_base_ping(void *data, struct xdg_wm_base *xdg_wm_base, uint32_t serial)
{
    xdg_wm_base_pong(xdg_wm_base, serial);
}

static const struct xdg_wm_base_listener xdg_wm_base_listener = {
    .ping = xdg_wm_base_ping,
};

static void wl_surface_frame_done(void *data, struct wl_callback *cb, uint32_t time);

static const struct wl_callback_listener wl_surface_frame_listener = {
	.done = wl_surface_frame_done,
};

static void
wl_surface_frame_done(void *data, struct wl_callback *cb, uint32_t time)
{
	/* Destroy this callback */
	wl_callback_destroy(cb);

	/* Request another frame */
	struct client_state* state = (struct client_state*)data;
	cb = wl_surface_frame(state->wl_surface);
	wl_callback_add_listener(cb, &wl_surface_frame_listener, state);

	/* Submit a frame for this event */
    if (time > 0 && ((time - state->ultimo_frame_dibujado) > 66)) // porque si no va muy rápido :) 
        {
            calcular_propagación_fuego();
            struct wl_buffer *buffer = draw_frame(state);
            wl_surface_attach(state->wl_surface, buffer, 0, 0);
            wl_surface_damage_buffer(state->wl_surface, 0, 0, INT32_MAX, INT32_MAX);
            
            state->ultimo_frame_dibujado = time;
        }
    wl_surface_commit(state->wl_surface);
	state->last_frame = time;
}


static void
wl_keyboard_keymap(void *data, struct wl_keyboard *wl_keyboard,
               uint32_t format, int32_t fd, uint32_t size)
{
       struct client_state* client_state = (struct client_state*)data;
       assert(format == WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1);

       char* map_shm = (char*)mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
       assert(map_shm != MAP_FAILED);

       struct xkb_keymap *xkb_keymap = xkb_keymap_new_from_string(
                       client_state->xkb_context, map_shm,
                       XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS);
       munmap(map_shm, size);
       close(fd);

       struct xkb_state *xkb_state = xkb_state_new(xkb_keymap);
       xkb_keymap_unref(client_state->xkb_keymap);
       xkb_state_unref(client_state->xkb_state);
       client_state->xkb_keymap = xkb_keymap;
       client_state->xkb_state = xkb_state;
}

static void
wl_keyboard_enter(void *data, struct wl_keyboard *wl_keyboard,
               uint32_t serial, struct wl_surface *surface,
               struct wl_array *keys)
{
       struct client_state* client_state = (struct client_state*)data;
       fprintf(stderr, "keyboard enter; keys pressed are:\n");
       
       for (uint32_t* key = (uint32_t*)(keys->data); (const char*)key < (const char*) keys->data + keys->size ; key++)
       {
               char buf[128];
               xkb_keysym_t sym = xkb_state_key_get_one_sym(
                               client_state->xkb_state, *key + 8);
               xkb_keysym_get_name(sym, buf, sizeof(buf));
               fprintf(stderr, "sym: %-12s (%d), ", buf, sym);
               xkb_state_key_get_utf8(client_state->xkb_state,
                               *key + 8, buf, sizeof(buf));
               fprintf(stderr, "utf8: '%s'\n", buf);
       }
       
       
}


static void
wl_keyboard_key(void *data, struct wl_keyboard *wl_keyboard,
               uint32_t serial, uint32_t time, uint32_t key, uint32_t state)
{
       struct client_state* client_state = (struct client_state*)data;
       char buf[128];
       uint32_t keycode = key + 8;
       xkb_keysym_t sym = xkb_state_key_get_one_sym(
                       client_state->xkb_state, keycode);
        if (sym == XKB_KEY_plus && state == WL_KEYBOARD_KEY_STATE_PRESSED && arreglo_intensidades[ancho_fuego*alto_fuego-1] != intensidad_maxima_fuego) {
            for (int columna = 0; columna < ancho_fuego; columna++)
            {
                int ultimo_pixel = ancho_fuego*alto_fuego;
                arreglo_intensidades[ultimo_pixel-ancho_fuego + columna] +=1;
            }
            decaimiento_máximo -= 1;
            
        }
        if (sym == XKB_KEY_minus && state == WL_KEYBOARD_KEY_STATE_PRESSED && arreglo_intensidades[ancho_fuego*alto_fuego-1] > 0) {
            
            for (int columna = 0; columna <= ancho_fuego; columna++)
            {
                int ultimo_pixel = ancho_fuego*alto_fuego;
                arreglo_intensidades[ultimo_pixel-ancho_fuego + columna] -=1;
            }
            decaimiento_máximo +=1;
        }
}


static void
wl_keyboard_leave(void *data, struct wl_keyboard *wl_keyboard,
               uint32_t serial, struct wl_surface *surface)
{
       fprintf(stderr, "keyboard leave\n");
}

static void
wl_keyboard_modifiers(void *data, struct wl_keyboard *wl_keyboard,
               uint32_t serial, uint32_t mods_depressed,
               uint32_t mods_latched, uint32_t mods_locked,
               uint32_t group)
{
       struct client_state* client_state = (struct client_state *)data;
       xkb_state_update_mask(client_state->xkb_state,
               mods_depressed, mods_latched, mods_locked, 0, 0, group);
}

static void
wl_keyboard_repeat_info(void *data, struct wl_keyboard *wl_keyboard, int32_t rate, int32_t delay)
{
       // esto quizá eventualmente lo hago (mentira) 
}


static const struct wl_keyboard_listener wl_keyboard_listener = {
       .keymap = wl_keyboard_keymap,
       .enter = wl_keyboard_enter,
       .leave = wl_keyboard_leave,
       .key = wl_keyboard_key,
       .modifiers = wl_keyboard_modifiers,
       .repeat_info = wl_keyboard_repeat_info,
};



static void
wl_seat_capabilities(void *data, struct wl_seat *wl_seat, uint32_t capabilities)
{
       struct client_state* state = (struct client_state*)data;
      
       bool have_keyboard = capabilities & WL_SEAT_CAPABILITY_KEYBOARD;

       if (have_keyboard && state->wl_keyboard == NULL) {
               state->wl_keyboard = wl_seat_get_keyboard(state->wl_seat);
               wl_keyboard_add_listener(state->wl_keyboard,
                               &wl_keyboard_listener, state);
       } else if (!have_keyboard && state->wl_keyboard != NULL) {
               wl_keyboard_release(state->wl_keyboard);
               state->wl_keyboard = NULL;
       }    
}

static void
wl_seat_name(void *data, struct wl_seat *wl_seat, const char *name)
{
       std::cerr << "nombre del asiento: " << name << std::endl;
}

static const struct wl_seat_listener wl_seat_listener = {
       .capabilities = wl_seat_capabilities,
       .name = wl_seat_name,
};


static void
registry_global(void *data, struct wl_registry *wl_registry,
        uint32_t name, const char *interface, uint32_t version)
{
    struct client_state *state = (struct client_state *)data;
    if (strcmp(interface, wl_shm_interface.name) == 0) {
        state->wl_shm = (struct wl_shm*)wl_registry_bind(
                wl_registry, name, &wl_shm_interface, 1);
    } else if (strcmp(interface, wl_compositor_interface.name) == 0) {
        state->wl_compositor = (struct wl_compositor*)wl_registry_bind(wl_registry, name, &wl_compositor_interface,4);
    } else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
        state->xdg_wm_base = (struct xdg_wm_base*)wl_registry_bind(wl_registry, name, &xdg_wm_base_interface, 1);
        xdg_wm_base_add_listener(state->xdg_wm_base, &xdg_wm_base_listener, state);
    } else if (strcmp(interface, wl_seat_interface.name) == 0) {
              state->wl_seat = (struct wl_seat*)wl_registry_bind(wl_registry, name, &wl_seat_interface, 7);
              wl_seat_add_listener(state->wl_seat,&wl_seat_listener, state);
        }
}

static void
registry_global_remove(void *data,
        struct wl_registry *wl_registry, uint32_t name)
{
    /* This space deliberately left blank, XD */
}

static const struct wl_registry_listener wl_registry_listener = {
    .global = registry_global,
    .global_remove = registry_global_remove,
};

int
main(int argc, char *argv[])
{
    struct client_state state = { 0 };
    state.width = 640;
	state.height = 480;
    state.wl_display = wl_display_connect(NULL);
    state.wl_registry = wl_display_get_registry(state.wl_display);
    state.xkb_context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    wl_registry_add_listener(state.wl_registry, &wl_registry_listener, &state);
    wl_display_roundtrip(state.wl_display);

    arreglo_intensidades = crear_estructura_de_datos_fuego();
    crear_origen_fuego();

    state.wl_surface = wl_compositor_create_surface(state.wl_compositor);
    state.xdg_surface = xdg_wm_base_get_xdg_surface(
            state.xdg_wm_base, state.wl_surface);
    xdg_surface_add_listener(state.xdg_surface, &xdg_surface_listener, &state);
    state.xdg_toplevel = xdg_surface_get_toplevel(state.xdg_surface);
    xdg_toplevel_add_listener(state.xdg_toplevel,&xdg_toplevel_listener, &state);
    xdg_toplevel_set_title(state.xdg_toplevel, "Cliente de ejemplo");
    wl_surface_commit(state.wl_surface);

    struct wl_callback *cb = wl_surface_frame(state.wl_surface);
    wl_callback_add_listener(cb, &wl_surface_frame_listener, &state);
    
    
    
    
    while (wl_display_dispatch(state.wl_display)) {
        /* This space deliberately left blank */ 
    }
    
    return 0;
}
