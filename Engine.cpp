#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>
#include <string>
#include <map>
#include <chrono>
#include <filesystem>
#include <random>
#include <algorithm>
#include <utility>
#define STB_TRUETYPE_IMPLEMENTATION  // force following include to generate implementation
#include "extern/stb_truetype.h"
#include "extern/SDL2/SDL.h"

//
// Types
//

using U8 = std::uint8_t;
using I16 = std::int16_t;
using I32 = std::int32_t;
using U32 = std::uint32_t;
using F32 = float;
using String = std::string;
template <typename T> using Vector = std::vector<T>;
template <typename K, typename V> using Map = std::map<K, V>;

struct Size {
    short w;
    short h;
    Size(): w(0), h(0) {}
    Size(short a, short b): w(a), h(b) {} 
    Size(int a, int b): w(a), h(b) {} 
    Size(double a, double b): w(a), h(b) {} 
    Size operator*(const Size& s) { return Size(w * s.w, h * s.h); }
    Size operator*(const float d) { return Size(d * w, d * h); }
    Size operator/(const Size& s) { return Size(w / s.w, h / s.h); }
};

struct Point {
    short x;
    short y;
    Point(): x(0), y(0) {}
    Point(short a, short b): x(a), y(b) {} 
    Point(short a, short b, Size max_size): x(a), y(b) { wrap(max_size); }
    Point(int a, int b): x(a), y(b) {} 
    Point(int pos) { x = pos & 0x0000FFFF; y = (pos & 0xFFFF0000) >> 16; } 
    Point(double a, double b): x(a), y(b) {} 
    Point operator+(const Point& p) { return Point(x + p.x, y + p.y); }
    Point operator-(const Point& p) { return Point(x - p.x, y - p.y); }
    Point operator*(Point& p) { return Point(x * p.x, y * p.y); }
    Point operator/(Point& p) { return Point(x / p.x, y / p.y); }
    bool operator==(const Point& p) { return x == p.x && y == p.y; }
    bool operator!=(Point& p) { return x != p.x || y != p.y; }
    short distance(const Point& p) { return std::abs(x - p.x) + std::abs(y - p.y); }
    operator int() { return *((int*)this); }
    void wrap(Size max_size) {
        if (x < 0) x += ((-x / max_size.w) + 1) * max_size.w;
        else if (x >= max_size.w) x %= max_size.w;
        if (y < 0) y += ((-y / max_size.h) + 1) * max_size.h;
        else if (y >= max_size.h) y %= max_size.h;
    }
};

struct Box {
    Box(): a({0, 0}), b({0, 0}) {}
    Box(Point first, Point second): a(first), b(second) {}
    Box(Point first, Size second): a(first), b({first.x+second.w, first.y+second.h}) {}
    Point a;
    Point b;
    Point center() { return {a.x + 0.5 * (b.x - a.x), a.y + 0.5 * (b.y - a.y)}; }
    Size size() {return Size(std::abs(b.x - a.x), std::abs(b.y - a.y));}
    bool inside(Point p) { return p.x >= a.x && p.y >= a.y && p.x <= b.x && p.y <= b.y; }
};

struct Color {
    Color(const Color& c) { *((unsigned*)this) = unsigned(c); };
    Color() {}
    Color(U8 r, U8 g, U8 b, U8 a=255): blue(b), green(g), red(r), alpha(a) {}
    Color(unsigned u) { *this = u; }
    Color(int i) { *this = i; }
    Color operator-(U8 d) { return Color(red > d ? red - d : 0, green > d ? green - d : 0, blue > d ? blue - d : 0, alpha); }
    Color operator+(U8 d) { return Color(red + d < 255 ? red + d : 255, green + d < 255 ? green + d : 255, blue + d < 255 ? blue + d : 255, alpha); }
    Color& operator=(int i) { *((int*)this) = i; return *this; } 
    Color& operator=(unsigned u) { *((unsigned*)this) = u; return *this; } 
    Color& operator=(Color c) { *((unsigned*)this) = unsigned(c); return *this; } 
    operator int() { return *((int*)this); }
    operator unsigned() const { return *((unsigned*)this); }
    U8 blue = 0;
    U8 green = 0;
    U8 red = 0;
    U8 alpha = 0;
};






//
// Utility methods
//

static void print(const std::string& s) { puts(s.c_str()); }
static void wait(int us) { SDL_Delay(us / 1000); }
static long long now() { return std::chrono::time_point_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now()).time_since_epoch().count(); }

static double random_fast() {
    static unsigned long long r = 0;
    static bool init = false;
    if (!init) {
        unsigned seed = (unsigned)now();
        auto generator = std::default_random_engine(seed);
        std::uniform_int_distribution<int> distribution(0, 2147483647);
        r = distribution(generator);
        init = true;
    }
    r = (r * 48271) % 2147483648;
    return (double)r / 2147483648;
}

static double random_uniform(double min, double max) {
    static unsigned seed = (unsigned)now();
    static std::default_random_engine generator(seed);
    std::uniform_real_distribution<double> distribution(min, max);
    return distribution(generator);
}

static double random_gauss(double mean, double dev) {
    static unsigned seed = (unsigned)now();
    static std::default_random_engine generator(seed);
    std::normal_distribution<double> distribution(mean, dev);
    return distribution(generator);
}



using FileHandle = void*;
static FileHandle file_open(const std::string& path) {
    fclose(fopen(path.c_str(), "a"));
    return (FileHandle)fopen(path.c_str(), "rb+");
}

static void file_close(FileHandle file) { fclose((FILE*)file); }
static void file_read(FileHandle file, char* buffer, int num_bytes) { fread(buffer, num_bytes, 1, (FILE*)file); }
static void file_write(FileHandle file, char* buffer, int num_bytes) { fwrite(buffer, num_bytes, 1, (FILE*)file); }

static String file_readline(FileHandle file) {
    static char buffer[4096];
    fgets(buffer, 4096, (FILE*)file);
    std::string ret = std::string(buffer);
    return ret.substr(0, ret.size() - 1);
}

void file_writeline(FileHandle file, const std::string& s) {
    std::string w = s + "\n";
    fwrite(w.c_str(), w.size(), 1, (FILE*)file);
}

bool file_isend(FileHandle file) { return feof((FILE*)file); }

bool file_exists(const std::string& path) {
    FILE* file = fopen(path.c_str(), "r");
    if (file) {
        fclose(file);
    }
    return file != nullptr;
}


std::vector<std::string> filelist(const std::string& path, const std::string& filter) {
    std::vector<std::string> ret;
    for (auto& file : std::filesystem::recursive_directory_iterator(path)) {
        std::string filepath = file.path().string();
        if (filter.empty() || filepath.find(filter) != std::string::npos) {
            ret.push_back(filepath);
        }
    }
    return ret;
}

std::string filename(const std::string& filepath) {
    std::string f = filepath;
    if (f.find("\\") != std::string::npos) {
        f = f.substr(f.find_last_of("\\") + 1);
    } else if (f.find("/") != std::string::npos) {
        f = f.substr(f.find_last_of("/") + 1);
    }
    if (f.find(".") != std::string::npos) {
        f = f.substr(0, f.find_last_of("."));
    }
    return f;
}

std::vector<std::string> split(const std::string &s, char delim) {
    std::vector<std::string> result;
    std::string cpy(s);
    std::string d(1, delim);
    char* pch = strtok ((char*)cpy.c_str(), d.c_str());
    while (pch) {
        std::string item(pch);
        if (delim != ' ' || !item.empty()) {
            result.push_back(item);
        }
        pch = strtok(nullptr, d.c_str());
    }
    if (s.empty() || s.back() == delim) {
        result.emplace_back();
    }
    return result;
}

void replace(std::string& s, const std::string& from, const std::string& to) {
    if (from.empty()) {
        return;
    }
    size_t start_pos = 0;
    while ((start_pos = s.find(from, start_pos)) != std::string::npos) {
        s.replace(start_pos, from.length(), to);
        start_pos += to.length(); // In case 'to' contains 'from', like replacing 'x' with 'yx'
    }
}

template <typename T>
void vector_remove(Vector<T>& v, const T& val) {
    v.erase(std::remove(v.begin(), v.end(), val), v.end());
}




static void audio_callback(void*, Uint8 *stream, int len);

struct Audio {
    struct AudioFile {
        SDL_AudioSpec spec;
        Uint32 length;
        Uint8* buffer;
        int length_remaining = 0;
        bool loop;
    };

    void load_wav(const String& filepath, const String& name, bool music) {
        static bool audio_init = false;
        if (!audio_init) {
            SDL_AudioSpec want, have;
            SDL_memset(&want, 0, sizeof(want)); /* or SDL_zero(want) */
            want.freq = 48000;
            want.format = audio_format;
            want.channels = 2;
            want.samples = 1024;
            want.callback = audio_callback;
            dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, SDL_AUDIO_ALLOW_ANY_CHANGE);
            SDL_PauseAudioDevice(dev, 0);
            audio_init = true;
        }
        AudioFile* handle = new AudioFile();
        handle->loop = music;
        SDL_LoadWAV(filepath.c_str(), &handle->spec, &handle->buffer, &handle->length);
        audio_files[name] = handle;
    }   

    void play_wav(const String& name, bool) {
        AudioFile* handle = audio_files[name];
        if (!handle) {
            return;
        }
        if (!handle->length_remaining && std::find(queued_audio.begin(), queued_audio.end(), handle) == queued_audio.end()) {
            handle->length_remaining = handle->length;
            queued_audio.push_back(handle);
        }
    }

    Map<String, AudioFile*> audio_files;
    Vector<AudioFile*> queued_audio;
    SDL_AudioFormat audio_format = AUDIO_S16LSB;
    SDL_AudioDeviceID dev;   
};

static Audio* g_audio = nullptr;

static void audio_callback(void*, Uint8 *stream, int len) {
    SDL_memset(stream, 0, len);
    Vector<Audio::AudioFile*> remove;
    Vector<Audio::AudioFile*>& queued_audio = g_audio->queued_audio;
    for (auto& audio : queued_audio) {
        unsigned play_len = audio->length_remaining > len ? len : audio->length_remaining;
        SDL_MixAudioFormat(stream, audio->buffer + audio->length - audio->length_remaining, g_audio->audio_format, play_len, 50);
        audio->length_remaining -= play_len;
        if (!audio->length_remaining) {
            remove.push_back(audio);
        } 
    }
    for (auto& audio : remove) {
        queued_audio.erase(std::remove(queued_audio.begin(), queued_audio.end(), audio), queued_audio.end());
    }
}







struct Input {
    struct Listener {
        public:
        Listener(void (*f)()): func(f) {}
        void mouse_clicked(Point) { func(); };
        void mouse_moved(Point) {};
        void key_pressed(const std::string& /*key*/) { func(); };
        //virtual void keyReleased(const std::string& /*key*/) {};
        void (*func)(); 
    };

    void enable() { enabled = true; }
    void disable() { enabled = false;}
    void clear_temp_listeners() { clear_temps = true; }
    void remove_mouse_listener(Listener* l) { remove_list.push_back(l); }
    void add_key_listeners(Listener* l, const std::vector<std::string>& keys, bool temp = false) {
        for (auto& key : keys) {
            if (temp) {
                temp_presses[key] = l;
            } else {
                presses[key] = l;
            }
        }
    }
    void add_move_listener(Listener* l) { mouse_moves.push_back(l); }
    void remove_move_listener(Listener* l) { mouse_moves.erase(std::remove(mouse_moves.begin(), mouse_moves.end(), l), mouse_moves.end()); };
    void add_mouse_listener(Listener* l, Box b, bool temp = false) {
        if (temp) {
            temp_clicks[l] = b;
        } else { 
            if (clicks.find(l) == clicks.end()) {
                clicks[l] = b;
            } 
        }
    }
    bool shift_held() { return shift_active; }

    void handleInputs() {
        std::vector<std::string> pressed;
        std::vector<std::string> released;
        pressed_keys(pressed, released);
        for (auto& hold : held) {
            pressed.push_back(hold.first);
        }
        for (auto& key : pressed) {
            if (temp_presses.find(key) != temp_presses.end() && !clear_temps) {
                temp_presses[key]->key_pressed(key);
            } else if (enabled && presses.find(key) != presses.end()) {
                presses[key]->key_pressed(key);
                if (key != "WheelUp" && key != "WheelDown") {
                    held[key] = 1;
                }
            } else if (key == "MouseLeft") {
                Point p = mouse_pos();
                Map<Listener*, Box>& active_clicks = enabled ? clicks : temp_clicks;
                for (auto& click : active_clicks) {
                    auto& box = click.second;
                    if (p.x >= box.a.x && p.y >= box.a.y && p.x <= box.b.x && p.y <= box.b.y) {
                        if (enabled || !clear_temps) {
                            click.first->mouse_clicked(p);
                        }
                    }
                }
            }
            if (key == "Left Shift") {
                shift_active = true;
            }
        }
        for (auto& key : released) {
            if (key == "Left Shift") {
                shift_active = false;
            }
            held.erase(key);
        }
        Point current_mouse_pos = mouse_pos();
        if (enabled && current_mouse_pos != last_mouse_pos) {
            for (auto& l : mouse_moves) {
                l->mouse_moved(current_mouse_pos);
            }
            last_mouse_pos = current_mouse_pos; 
        }
        for (auto l : remove_list) {
            clicks.erase(l);
        }
        remove_list.clear();
        if (clear_temps) {
            temp_presses.clear();
            temp_clicks.clear();
            clear_temps = false;
        }
    }

    void pressed_keys(std::vector<std::string>& pressed, std::vector<std::string>& released) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_MOUSEWHEEL: {
                    pressed.push_back(event.wheel.y > 0 ? "WheelUp" : "WheelDown");
                    break;
                }
                case SDL_KEYDOWN: {
                    pressed.push_back(SDL_GetKeyName(event.key.keysym.sym));
                    break;
                }
                case SDL_KEYUP: {
                    released.push_back(SDL_GetKeyName(event.key.keysym.sym));
                    break;
                }
                case SDL_MOUSEBUTTONDOWN: {
                    if (event.button.button == SDL_BUTTON_LEFT) {
                        pressed.push_back("MouseLeft");
                    }
                    break; 
                }
                case SDL_WINDOWEVENT: {
                    switch (event.window.event) {
                    case SDL_WINDOWEVENT_CLOSE:
                        exit(0);
                        break;
                    default:
                        break;
                    }
                    break;
                }
                default:
                    break;
            }
        }
    }   

    Point mouse_pos() {
        int mx, my;
        SDL_GetMouseState(&mx, &my);
        return {mx, my};
    }

    Map<std::string, int> held;
    Vector<Listener*> mouse_moves;
    Map<std::string, Listener*> temp_presses;
    Map<std::string, Listener*> presses;
    Map<Listener*, Box> temp_clicks;
    Map<Listener*, Box> clicks;
    Vector<Listener*> remove_list;
    bool enabled = true;
    bool clear_temps = false;
    bool shift_active = false;
    Point last_mouse_pos;
};

static Input* g_input = nullptr;








using TextureID = I16;

struct MapConfig {
    struct Item {
        Item(const String& n, double p): name(n), perc(p) {}
        String name = "";
        double perc = 0.0;
        TextureID id = -1;
        Size size = {0, 0};
    };
    
    struct Biome {
        Biome(const String& n, const String& nw, I16 mh, I16 wh, bool b): name(n), name_wall(nw), max_height(mh), wall_height(wh), blocking(b) {}
        String name;
        String name_wall;
        I16 max_height;
        I16 wall_height;
        bool blocking;
        TextureID id = -1;
        Vector<Item> items;
    };

    struct Elevation {
        Elevation(double p): perc(p) {}
        double perc;
        Vector<Biome> biomes;
        Vector<char> temperatures;
    };
    
    Vector<Elevation> elevations;
    I16 num_cells = 0;
    I16 sample_factor = 0;
    I16 sample_distance = 0;
};

struct Texture {
    Texture(Color* p, Size s, bool t=false): pixels(p), size(s), transparent(t) {}
    Color* pixels = nullptr;
    Size size;
    I16 id;
    bool transparent = false;
};

struct Widget {
    Widget(Size s): size(s) {}
    ~Widget() {
        if (listener) {
            g_input->remove_mouse_listener(listener);
        }
        for (Widget* c : children) delete c;
    }
    Size size;
    Point pos;
    Texture* texture = nullptr;
    Widget* parent = nullptr;
    Vector<Widget*> children;
    Vector<Vector<Texture*>> letters;
    Point letters_offset;
    bool remove = false;
    Input::Listener* listener = nullptr;
};
    
struct Camera {
    Camera(int a, int b): x(a), y(b) {}
    Camera(double a, double b): x(a), y(b) {}
    int x;
    int y;
    Camera operator+(const Point& p) { return Camera(x + p.x, y + p.y); }
    Camera operator-(const Point& p) { return Camera(x - p.x, y - p.y); }
    Camera operator+(const Camera& p) { return Camera(x - p.x, y - p.y); }
    Camera operator-(const Camera& p) { return Camera(x - p.x, y - p.y); }
};

template<class T, size_t N>
constexpr size_t size(T (&)[N]) { return N; }

struct UI {    
    Widget* tilemap_widget = nullptr; 
    TextureID* tiles_ground = nullptr;
    Size tile_dim = {0, 0};
    Size map_size = {0, 0};
    Camera camera_pos = {0, 0};
    Point move_vector = {0, 0};
    bool infinite_scrolling = true;
    static inline constexpr char LETTER_MIN = 32;
    static inline constexpr char LETTER_MAX = 127;
    SDL_Window* window = nullptr;
    Color* pixels;
    Size size;
    long long last_update = now();
    I32 fps = 0;
    Texture* id_to_texture[15000] = {0};
    Map<String, Texture*> name_to_texture;
    Vector<Texture*> letter_to_texture[1024];
    TextureID currentID = 1;
    int zoom_idx = 3;
    float zoom = 1.0;
    constexpr static inline float zoom_levels[6] = {0.125, 0.25, 0.5, 1.0, 2.0, 4.0};
    Vector<Widget*> top_widgets;
    MapConfig* map_config = new MapConfig();

    Box visible_tiles() {
        constexpr short pad = 1;
        I16 xstart = (camera_pos.x) / (zoom * tile_dim.w) - pad;
        I16 xend = (camera_pos.x + size.w) / (zoom * tile_dim.w) + pad;
        I16 ystart = (camera_pos.y) / (zoom * tile_dim.h) - pad;
        I16 yend = (camera_pos.y + size.h) / (zoom * tile_dim.h) + pad;
        return {Point(xstart, ystart), Point(xend, yend)};
    }

    void draw_tilemap() {
        if (move_vector.x == move_vector.y) {
            move_vector.x = sqrt(move_vector.x * move_vector.x + move_vector.y * move_vector.y);
            move_vector.x = move_vector.y;
        }
        camera_pos = camera_pos + move_vector;
        fix_camera();
        move_vector = {0, 0};
        Box canvas(tilemap_widget->pos, tilemap_widget->size);
        const Box visible = visible_tiles();
        const Size tile_size(tile_dim.w * zoom, tile_dim.h * zoom);
        for (I16 y = visible.a.y; y <= visible.b.y; y++) {
            for (I16 x = visible.a.x; x <= visible.b.x; x++) {
                Point p(x, y, map_size);
                Point start(tilemap_widget->pos.x - camera_pos.x + x * tile_size.w, tilemap_widget->pos.y - camera_pos.y + y * tile_size.h);
                TextureID ground_id = tiles_ground[p.y * map_size.w + p.x];
                Texture* texture_ground = id_to_texture[ground_id < 0 ? -ground_id : ground_id];
                if (texture_ground) {
                    blit(texture_ground->pixels, tile_size, start, canvas, false, zoom);
                }
            }
        }
    }

    void zoomin_cam() {
        if (++zoom_idx >= sizeof(zoom_levels) / sizeof(*zoom_levels)) {
            --zoom_idx;
            return;
        }
        camera_pos.x = (camera_pos.x + size.w / 4) * 2;
        camera_pos.y = (camera_pos.y + size.h / 4) * 2;
        zoom = zoom_levels[zoom_idx];
        fix_camera();
    }

    void zoomout_cam() {
        if (--zoom_idx < 0) {
            ++zoom_idx;
            return;
        }
        camera_pos.x = (camera_pos.x - size.w / 2) / 2;
        camera_pos.y = (camera_pos.y - size.h / 2) / 2;
        zoom = zoom_levels[zoom_idx];
        fix_camera();
    }

    void fix_camera() {
        Camera camera_max = {(zoom * tile_dim.w) * map_size.w - size.w, (zoom * tile_dim.h) * map_size.h - size.h};
        if (infinite_scrolling) {
            while (camera_pos.x <= -camera_max.x) camera_pos.x += (camera_max.x + size.w);
            while (camera_pos.y <= -camera_max.y) camera_pos.y += (camera_max.y + size.h);
            while (camera_pos.x >= 2 * camera_max.x) camera_pos.x -= (camera_max.x + size.w);
            while (camera_pos.y >= 2 * camera_max.y) camera_pos.y -= (camera_max.y + size.h);
        } else {
            if (camera_pos.x < 0) camera_pos.x = 0;
            if (camera_pos.y < 0) camera_pos.y = 0;
            camera_pos.x = camera_pos.x < camera_max.x ? camera_pos.x : camera_max.x;
            camera_pos.y = camera_pos.y < camera_max.y ? camera_pos.y : camera_max.y;
        }
    }

    void move_cam(Point p) {
        if ((p.x != 0 && move_vector.x == 0)) {
            move_vector.x = p.x;
        } 
        if ((p.y != 0 && move_vector.y == 0)) {
            move_vector.y = p.y;
        }
    }

    void move_cam_to_tile(Point tile_pos) {
        camera_pos = {tile_pos.x * tile_dim.w * zoom, tile_pos.y * tile_dim.h * zoom};
        Box visible = visible_tiles();
        Camera mid = {zoom * tile_dim.w * (visible.b.x - visible.a.x) / 2, zoom * tile_dim.h * (visible.b.y - visible.a.y) / 2};
        camera_pos = camera_pos - mid;
        fix_camera();
    }

    Widget* create_tilemap(Size widget_size, Size tilemap_size, Size tile_size) {
        tilemap_widget = new Widget(widget_size);
        map_size = tilemap_size;
        tile_dim = tile_size;
        tiles_ground = new TextureID[map_size.w * map_size.h];
        std::memset(tiles_ground, 0, map_size.w * map_size.h * sizeof(TextureID));
        return tilemap_widget;
    }

    void set_tile(I16 x, I16 y, const String& texture_name, bool ground) {
        if (ground) {
            tiles_ground[y * map_size.w + x] = name_to_texture[texture_name]->id;
        }
    }

    UI(Size s) {
        size = s;
        if (!window) {
            #ifdef _WIN32
            SDL_setenv("SDL_AUDIODRIVER", "directsound", true); 
            #endif
            SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
            window = SDL_CreateWindow("", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, s.w, s.h, 0);
            //if (fullscreen) {
            //    SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN);
            //}
        }
        pixels = (Color*)SDL_GetWindowSurface(window)->pixels;
    }

    void update() {
        std::vector<Widget*> widgets(top_widgets);
        I32 idx = 0;
        while (idx < widgets.size()) {
            Widget* w = widgets[idx];
            if (w->remove) {
                if (w->parent) {
                    vector_remove(w->parent->children, w);
                } else {
                    vector_remove(top_widgets, w);
                }
                delete w;
                ++idx;
                continue;
            }
            if (w->texture) {
                blit(w->texture->pixels, w->texture->size, w->pos, Box(w->pos, w->size), w->texture->transparent);
            }
            if (w == tilemap_widget) {
                draw_tilemap();
            }
            Point p = w->pos + w->letters_offset;
            for (auto& line : w->letters) {
                I16 line_height = 0;
                for (Texture* letter : line) {
                    blit(letter->pixels, letter->size, p, Box(w->pos, w->size), true);
                    p.x += letter->size.w;
                    line_height = letter->size.h > line_height ? letter->size.h : line_height;
                }
                p.x = w->pos.x;
                p.y += line_height;
            }
            for (Widget* child : w->children) {
                widgets.push_back(child);
            }
            ++idx;
        }
        long long t_now = now();
        long long t = t_now - last_update;
        SDL_UpdateWindowSurface(window);
        fps = t > 0 ? (double) 1 / ((double)now() / (1000 * 1000)) : 0; 
        last_update = now();
    }

    void load_letters(int height, Color color) {
        String fontpath = "./mono.ttf";
        letter_to_texture[height].resize(LETTER_MAX + 1);
        unsigned char* ttf_buffer = new unsigned char[1<<25];
        FILE* fontfile = fopen(fontpath.c_str(), "rb");
        fread(ttf_buffer, 1, 1<<25, fontfile);
        fclose(fontfile);
        stbtt_fontinfo font;
        stbtt_InitFont(&font, ttf_buffer, stbtt_GetFontOffsetForIndex(ttf_buffer,0));
        float scale = stbtt_ScaleForPixelHeight(&font, (float)height);
        int ascent, descent, lineGap;
        stbtt_GetFontVMetrics(&font, &ascent, &descent, &lineGap);  
        ascent = roundf(ascent * scale);
        descent = roundf(descent * scale);
        for (char c = LETTER_MIN; c < LETTER_MAX; c++) {
            int leftSideBearing;
            int advanceWidth;
            stbtt_GetCodepointHMetrics(&font, c, &advanceWidth, &leftSideBearing);
            advanceWidth *= c == ' ' ? scale : 0;
            leftSideBearing *= scale;
            int c_x1, c_y1, c_x2, c_y2;
            stbtt_GetCodepointBitmapBox(&font, c, scale, scale, &c_x1, &c_y1, &c_x2, &c_y2);
            int y_char = ascent + c_y1;
            int w = c_x2 - c_x1;
            int h = c_y2 - c_y1;
            unsigned char* bitmap = new unsigned char[w * h];
            stbtt_MakeCodepointBitmap(&font, bitmap, w, h, w, scale, scale, c);
            Size s(w + leftSideBearing + advanceWidth, y_char + h);
            Color* out = new Color[s.w * s.h];
            for (int y = y_char; y < s.h; y++) {
                for (int x = leftSideBearing; x < w + leftSideBearing; x++) {
                    out[y * s.w + x] = color;
                    out[y * s.w + x].alpha = bitmap[(y - y_char) * w + (x - leftSideBearing)];
                }
            }
            delete[] bitmap;
            letter_to_texture[height][c] = new Texture(out, s, true);
        }
        delete[] ttf_buffer;
    }

    void register_texture(const String& name, Texture* t, bool scale) {
        if (name_to_texture.find(name) != name_to_texture.end()) {
            return;
        }
        id_to_texture[currentID] = t;
        name_to_texture[name] = t;
        t->id = currentID;
        currentID++;
    }

    Texture* get(const std::string& name) {
        if (name_to_texture.find(name) != name_to_texture.end()) {
            return name_to_texture[name]; 
        }
        return nullptr;
    }

    Texture* get(char letter, int size) {
        if (letter_to_texture[size].empty()) {
            load_letters(size, Color(255, 255, 255, 255));
        }
        return letter_to_texture[size][letter]; 
    }

    void blit(Color* texture, Size texture_size, Point start, Box canvas, bool transparent=false, float zoom=1.0f) {
        Point texture_end(start.x + texture_size.w, start.y + texture_size.h);
        Point texture_start(0, 0);
        Point texture_endcut(0, 0);
        if (start.x < canvas.a.x) {
            texture_start.x = (canvas.a.x - start.x);
            start.x = canvas.a.x;
        } else if (texture_end.x > canvas.b.x) {
            texture_endcut.x = texture_end.x - canvas.b.x;
        }
        if (start.y < canvas.a.y) {
            texture_start.y = (canvas.a.y - start.y);
            start.y = canvas.a.y;
        } else if (texture_end.y > canvas.b.y) {
            texture_endcut.y = texture_end.y - canvas.b.y;
        }
        
        unsigned* texture_pixels = (unsigned*)(texture + (unsigned)(texture_start.x / zoom)); 
        unsigned* screen_pixels = (unsigned*)(pixels + start.y * size.w + start.x);
        I16 upper_bound_x = texture_size.w - texture_start.x - texture_endcut.x;
        I16 upper_bound_y = texture_size.h - texture_start.y - texture_endcut.y;
        if (upper_bound_y <= 0 || upper_bound_x <= 0) {
            return;
        }
        
        if (transparent) {
            for (I16 y = 0; y < upper_bound_y; y++) {
                for (I16 x = 0; x < upper_bound_x; x++) {
                    unsigned color1 = screen_pixels[y * size.w + x];
                    unsigned color2 = texture_pixels[int(y / zoom) * int(texture_size.w / zoom) + int(x / zoom)];
                    unsigned rb = (color1 & 0xff00ff) + (((color2 & 0xff00ff) - (color1 & 0xff00ff)) * ((color2 & 0xff000000) >> 24) >> 8);
                    unsigned g  = (color1 & 0x00ff00) + (((color2 & 0x00ff00) - (color1 & 0x00ff00)) * ((color2 & 0xff000000) >> 24) >> 8);
                    screen_pixels[y * size.w + x] = (rb & 0xff00ff) | (g & 0x00ff00);
                }
            }
        } else {
           for (I16 y = 0; y < upper_bound_y; y++) {
                for (I16 x = 0; x < upper_bound_x; x++) {
                    screen_pixels[y * size.w + x] = texture_pixels[int(y / zoom) * int(texture_size.w / zoom) + int(x / zoom)];
                }
           }
        }
        
    }

    void add_widget(Widget* parent, Widget* child, Point offset) {
        if (!parent) {
            child->pos = offset;
            top_widgets.push_back(child);
            return;
        }
        std::function<void(Widget*, Point)> offset_position = [&](Widget* w, Point offset) {
            w->pos = w->pos + offset;
            for (auto& child : w->children) {
                offset_position(child, offset);
            }
            return;
        };
        offset_position(child, parent->pos + offset);
        parent->children.push_back(child);
        child->parent = parent;
    }

    void set_texture(Widget* w, const String& name) {
        w->texture = name_to_texture[name];
    }

    void set_text(Widget* w, const String& txt, I16 txt_height, Point offset) {
        w->letters_offset = offset;
        Vector<Vector<Texture*>>& lines = w->letters;
        lines.clear();
        lines.resize(1);
        std::vector<Texture*> word;
        int current_line = 0;
        short word_length = 0;
        short line_length = 0;
        short line_height = 0;
        short total_height = 0;

        for (int i = 0; i < (int)txt.size(); i++) {
            bool newline = i < (int)txt.size()-1 ? txt[i] == 10 : false;
            if (newline) {
                total_height += line_height;
                lines[current_line].insert(lines[current_line].end(), word.begin(), word.end());
                lines.push_back(std::vector<Texture*>());
                line_length = 0;
                line_height = 0;
                current_line++;
                word.clear();
                word_length = 0;
                continue;
            }
            Texture* t = get(txt[i], txt_height);
            if (t) {
                word.push_back(t);
                word_length += t->size.w;
                line_height = t->size.h > line_height ? t->size.h : line_height;
            }
            if (txt[i] == ' ' || i == (int)(txt.size()-1)) {
                if (line_length + word_length < w->size.w) {
                    lines[current_line].insert(lines[current_line].end(), word.begin(), word.end());
                    line_length += word_length;
                } else {
                    total_height += line_height;
                    if (total_height + line_height > w->size.h) { 
                        //std::cout << "Text too long!" << std::endl;
                        // add excess text handling here
                        break;
                    }
                    lines.push_back(word);
                    line_length = word_length;
                    line_height = 0;
                    current_line++;
                }
                word.clear();
                word_length = 0;
            }
        }
    }

    struct Anchor {
        static constexpr int PERC_FACTOR = 100000;
        Anchor(Point p, double pc, char t): pos(p), perc(PERC_FACTOR * pc), temp(t) {}
        Point pos;
        long long perc;
        char temp;
    };

    void randomize_map() {
        MapConfig& config = *map_config;
        Size num_cells = {config.num_cells, config.num_cells};
        Size cell_size = map_size / num_cells;
        int max_samples = cell_size.w * cell_size.h * config.sample_factor; // 3
        int sample_dist = config.sample_distance; // 2
        
        char* heightmap = new char[map_size.w * map_size.h];
        std::memset(heightmap, 0, map_size.w * map_size.h);
        MapConfig::Biome& mountain_biome = config.elevations.back().biomes[0];
        unsigned char max_height = mountain_biome.max_height - 1;
        F32 height_cutoff = 1 - config.elevations.back().perc;
        short wall_height = mountain_biome.wall_height;

        const int climate_cluster_factor = 4;
        double r;
        Vector<Anchor> anchors;
        for (short y = 0; y < num_cells.h; y++) {
            for (short x = 0; x < num_cells.w; x++) {
                if (x % climate_cluster_factor == 0 && y % climate_cluster_factor == 0) {
                    r = random_uniform(20, 80);
                } else {
                    short use_x = x - (x % climate_cluster_factor);
                    short use_y = (y - (y % climate_cluster_factor)) * num_cells.w; 
                    r = anchors[use_y + use_x].temp;
                }
                anchors.emplace_back(
                    Point((x + random_fast()) * cell_size.w, (y + random_fast()) * cell_size.h), 
                    random_fast(),
                    r
                ); 
            }
        }
        /*
        for (short y_map = 0; y_map < map_size.h; y_map++) {
            for (short x_map = 0; x_map < map_size.w; x_map++) {
                tiles_ground->get(x_map, y_map) = 0;
                tiles_above->get(x_map, y_map) = 0;
                heightmap->get(x_map, y_map) = 0;
            }
        }
        */
        //parallel_for(0, num_cells.h-1, [&](int y_cell) {
        for (I16 y_cell = 0; y_cell < num_cells.h; y_cell++) {        
            for (I16 x_cell = 0; x_cell < num_cells.w; x_cell++) {
                Box sample_cells(Point(x_cell - sample_dist, y_cell - sample_dist), Point(x_cell + sample_dist, y_cell + sample_dist));
                Vector<Anchor> current_anchors;
                for (short y_cells = sample_cells.a.y; y_cells <= sample_cells.b.y; y_cells++) {
                    for (short x_cells = sample_cells.a.x; x_cells <= sample_cells.b.x; x_cells++) {
                        short x_cur = x_cells;
                        short x_offset = 0;
                        if (x_cur < 0) {
                            x_offset = -map_size.w;
                            x_cur += num_cells.w;
                        } else if (x_cur > num_cells.w - 1) {
                            x_offset = map_size.w;
                            x_cur -= num_cells.w;
                        }
                        short y_cur = y_cells;
                        short y_offset = 0;
                        if (y_cur < 0) {
                            y_offset = -map_size.h;
                            y_cur += num_cells.h;
                        } else if (y_cur > num_cells.h - 1) {
                            y_offset = map_size.h;
                            y_cur -= num_cells.h;
                        }
                        current_anchors.emplace_back(anchors[y_cur * num_cells.w + x_cur]);
                        current_anchors.back().pos.x += x_offset;
                        current_anchors.back().pos.y += y_offset;
                    }
                }
                for (I16 y_map = y_cell * cell_size.h; y_map < (I16)((y_cell + 1) * cell_size.h); y_map++) {
                    for (I16 x_map = x_cell * cell_size.w; x_map < (I16)((x_cell + 1) * cell_size.w); x_map++) {
                        unsigned long long sum_val = 0;
                        unsigned long long sum_temp = 0;
                        unsigned long long total_samples = 0;
                        for (int i = 0; i < (int)current_anchors.size(); i++) {
                            int diffx = current_anchors[i].pos.x - x_map;
                            int diffy = current_anchors[i].pos.y - y_map;
                            int dist = ((diffx ^ (diffx >> 31)) - (diffx >> 31)) + ((diffy ^ (diffy >> 31)) - (diffy >> 31));
                            int num_samples = max_samples - dist * dist;
                            num_samples = 1 + (num_samples & -((num_samples >> 31) ^ 1));
                            sum_val += num_samples * current_anchors[i].perc;
                            sum_temp += num_samples * current_anchors[i].temp;
                            total_samples += num_samples;
                        }
                        double total_val = (double)sum_val / (total_samples * Anchor::PERC_FACTOR);
                        double total_temp = (double)sum_temp / total_samples;

                        double current_val = 0;
                        for (MapConfig::Elevation& elevation : config.elevations) {
                            current_val += elevation.perc;
                            if (total_val - current_val <= 0.001) {
                                int biome_index = 0;
                                if (elevation.biomes.size() > 1) {
                                    for (biome_index = 0; biome_index < (int)elevation.biomes.size()-1; biome_index++) {
                                        if (total_temp < elevation.temperatures[biome_index]) {
                                            break;
                                        }
                                    }
                                }
                                MapConfig::Biome& biome = elevation.biomes[biome_index];
                                if (biome.max_height > 0) {
                                    char height = max_height * (total_val - height_cutoff) / (1 - height_cutoff);
                                    heightmap[y_map * map_size.w + x_map] = height < 0 ? 0 : height;
                                }
                                if (biome.id < 0) {
                                    Texture* t = get(biome.name);
                                    biome.id = t->id;
                                }
                                tiles_ground[y_map * map_size.w + x_map] = biome.id;
                                //map->set_ground(biome.id(), {x_map, y_map}, biome.blocking);
                                double val = random_fast();
                                current_val = 0;
                                for (auto& item : biome.items) {
                                    current_val += item.perc;
                                    if (val - current_val <= 0.01) {
                                        if (item.id < 0) {
                                            Texture* t = get(item.name);
                                            item.id = t->id;
                                            item.size = t->size;
                                        }
                                        //map->set_tile(item.id(), {x_map, y_map}, item.size() / tile_dim);
                                        break;
                                    }
                                }
                                break;
                            }
                        }
                    }
                }
            }
        }
        TextureID mountain_id = get(mountain_biome.name_wall)->id;
        for (I16 y_map = 1; y_map < map_size.h - wall_height; y_map++) {
            for (I16 x_map = 1; x_map < map_size.w - 1; x_map++) {
                if (heightmap[y_map * map_size.w + x_map] > heightmap[(y_map+1) * map_size.w + x_map]) {
                    for (int i = 1; i <= wall_height; i++) { 
                        tiles_ground[(y_map + i) * map_size.w + x_map] = mountain_id;
                    }
                }
            }
        }
    }
};

static UI* g_ui = nullptr;




extern "C" {

void init(I16 width, I16 height) {
    g_audio = new Audio();
    g_input = new Input();
    g_ui = new UI({width, height});
}

void run() {
    while(1) {
        g_input->handleInputs();
        g_ui->update();
    }
}



void bind_key(const char* key, void (*action)()) { g_input->add_key_listeners(new Input::Listener(action), {key}); }




void* create_widget(I16 width, I16 height) { return new Widget({width, height}); }

void remove_widget(Widget* w) { w->remove = true; }

void add_widget(Widget* parent, Widget* child, I16 offset_x, I16 offset_y) { g_ui->add_widget(parent, child, {offset_x, offset_y}); }

void set_widget_texture(Widget* w, const char* texture_name) { g_ui->set_texture(w, texture_name); }

void set_widget_text(Widget* w, const char* text, I16 text_height, I16 offset_x, I16 offset_y) { g_ui->set_text(w, text, text_height, {offset_x, offset_y}); }

void set_widget_callback(Widget* w, void (*f)()) {
    w->listener = new Input::Listener(f);
    g_input->add_mouse_listener(w->listener, Box(w->pos, w->size));
}



void* create_tilemap_widget(I16 widget_width, I16 widget_height, I16 map_width, I16 map_height, I16 tile_width, I16 tile_height) {
    return g_ui->create_tilemap({widget_width, widget_height}, {map_width, map_height}, {tile_width, tile_height});
}

void tilemap_move(int x_move, int y_move) { g_ui->move_cam({x_move, y_move}); }

void tilemap_zoomin() { g_ui->zoomin_cam(); }

void tilemap_zoomout() { g_ui->zoomout_cam();}

void tilemap_randomize() { g_ui->randomize_map(); }

void set_tile(I16 x, I16 y, const char* texture_name, bool ground) { g_ui->set_tile(x, y, texture_name, ground); }

void mapconfig_add_elevation(F32 quantity) { g_ui->map_config->elevations.emplace_back(quantity); }

void mapconfig_add_biome(I16 elevation, const char* name, I16 max_temp, const char* name_wall, I16 max_height, I16 wall_height, bool blocking) {
    g_ui->map_config->elevations[elevation].biomes.emplace_back(name, name_wall, max_height, wall_height, blocking);
    g_ui->map_config->elevations[elevation].temperatures.emplace_back(max_temp);
}

void mapconfig_add_vegetation(I16 elevation, const char* biome, const char* vegetation, F32 quantity) {
    for (auto& b : g_ui->map_config->elevations[elevation].biomes) { // this has quadratic runtime
        if (b.name == biome) {
            b.items.emplace_back(vegetation, quantity);
            break;
        }
    }
}

void mapconfig_set_parameters(I16 num_cells, I16 sample_distance, I16 sample_factor) {
    g_ui->map_config->num_cells = num_cells;
    g_ui->map_config->sample_distance = sample_distance;
    g_ui->map_config->sample_factor = sample_factor;
}




void texture_from_bitmap(const char* name, Color* bitmap, I16 width, I16 height) {
    Color* new_bitmap = new Color[width * height];
    std::memcpy(new_bitmap, bitmap, width * height * sizeof(Color));
    g_ui->register_texture(name, new Texture(new_bitmap, {width, height}), false);
}

bool texture_registered(const char* name) {
    return g_ui->name_to_texture.find(name) != g_ui->name_to_texture.end();
}



void load_sound(const char* path, const char* name) {
    g_audio->load_wav(path, name, false);
}

void play_sound(const char* name) {
    g_audio->play_wav(name, false);
}


}
